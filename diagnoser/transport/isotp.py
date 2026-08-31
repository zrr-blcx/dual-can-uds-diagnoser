"""ISO 15765-2 multi-frame packing, unpacking, and transport."""

from __future__ import annotations

import time
from collections.abc import Callable

import can

PCI_SF = 0x0
PCI_FF = 0x1
PCI_CF = 0x2
PCI_FC = 0x3

FC_CTS = 0x0
FC_WAIT = 0x1
FC_OVERFLOW = 0x2

DEFAULT_PADDING = 0xCC
MAX_SF_LENGTH = 7
MAX_FF_LENGTH = 6


def encode_single_frame(data: bytes, padding: int = DEFAULT_PADDING) -> bytes:
    if len(data) > MAX_SF_LENGTH:
        raise ValueError("single frame payload exceeds 7 bytes")
    frame = bytearray([(PCI_SF << 4) | len(data)])
    frame.extend(data)
    return _pad(frame, padding)


def encode_first_frame(
    total_length: int, first_chunk: bytes, padding: int = DEFAULT_PADDING
) -> bytes:
    if total_length <= MAX_SF_LENGTH:
        raise ValueError("first frame is only used for payloads over 7 bytes")
    if total_length > 0x0FFF:
        raise ValueError("ISO-TP total length exceeds 4095 bytes")
    if len(first_chunk) > MAX_FF_LENGTH:
        raise ValueError("first frame data chunk exceeds 6 bytes")
    frame = bytearray(
        [
            (PCI_FF << 4) | ((total_length >> 8) & 0x0F),
            total_length & 0xFF,
        ]
    )
    frame.extend(first_chunk)
    return _pad(frame, padding)


def encode_consecutive_frame(
    sequence_index: int, chunk: bytes, padding: int = DEFAULT_PADDING
) -> bytes:
    if not 0 <= sequence_index <= 0x0F:
        raise ValueError("consecutive frame sequence index must be 0..15")
    if len(chunk) > MAX_SF_LENGTH:
        raise ValueError("consecutive frame data chunk exceeds 7 bytes")
    frame = bytearray([(PCI_CF << 4) | sequence_index])
    frame.extend(chunk)
    return _pad(frame, padding)


def encode_flow_control(
    flow_status: int = FC_CTS,
    block_size: int = 0,
    stmin: int = 0,
    padding: int = DEFAULT_PADDING,
) -> bytes:
    if not 0 <= flow_status <= 0x0F:
        raise ValueError("flow status must be 0..15")
    frame = bytearray([(PCI_FC << 4) | flow_status, block_size & 0xFF, stmin & 0xFF])
    return _pad(frame, padding)


def pack_message(data: bytes, padding: int = DEFAULT_PADDING) -> list[bytes]:
    """Pack a diagnostic payload into one or more CAN frames."""
    if not data:
        raise ValueError("cannot pack an empty diagnostic payload")
    if len(data) <= MAX_SF_LENGTH:
        return [encode_single_frame(data, padding)]

    frames = [encode_first_frame(len(data), data[:MAX_FF_LENGTH], padding)]
    remaining = data[MAX_FF_LENGTH:]
    index = 1
    while remaining:
        frames.append(
            encode_consecutive_frame(index, remaining[:MAX_SF_LENGTH], padding)
        )
        remaining = remaining[MAX_SF_LENGTH:]
        index = (index + 1) % 16
    return frames


def unpack_frames(frames: list[bytes]) -> bytes:
    """Reassemble a diagnostic payload from received ISO-TP frames."""
    if not frames:
        raise ValueError("no frames to unpack")
    first = frames[0]
    pci_type = (first[0] >> 4) & 0x0F
    if pci_type == PCI_SF:
        length = first[0] & 0x0F
        return bytes(first[1 : 1 + length])
    if pci_type != PCI_FF:
        raise ValueError("first frame must be SF or FF")

    total_length = ((first[0] & 0x0F) << 8) | first[1]
    payload = bytearray(first[2 : 2 + MAX_FF_LENGTH])
    expected_index = 1
    for frame in frames[1:]:
        if (frame[0] >> 4) & 0x0F != PCI_CF:
            raise ValueError("expected a consecutive frame")
        sequence_index = frame[0] & 0x0F
        if sequence_index != expected_index:
            raise ValueError(
                f"unexpected CF sequence index 0x{sequence_index:X}, expected 0x{expected_index:X}"
            )
        payload.extend(frame[1:])
        expected_index = (expected_index + 1) % 16
    if len(payload) < total_length:
        raise ValueError("incomplete multi-frame payload")
    return bytes(payload[:total_length])


class IsoTpTransport:
    """A blocking ISO-TP sender/receiver on top of python-can."""

    def __init__(
        self,
        bus: can.BusABC,
        txid: int,
        rxid: int,
        timeout: float = 2.0,
        padding: int = DEFAULT_PADDING,
        stmin_ms: int = 0,
        block_size: int = 0,
        is_extended_id: bool = False,
    ) -> None:
        self.bus = bus
        self.txid = txid
        self.rxid = rxid
        self.timeout = timeout
        self.padding = padding
        self.stmin_s = stmin_ms / 1000.0
        self.block_size = block_size
        self.is_extended_id = is_extended_id

    def _send_frame(self, data: bytes) -> None:
        self.bus.send(
            can.Message(
                arbitration_id=self.txid,
                data=data,
                is_extended_id=self.is_extended_id,
                is_fd=False,
            )
        )

    def _wait_frame(
        self,
        predicate: Callable[[can.Message], bool],
        timeout: float,
    ) -> can.Message:
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("timed out waiting for ISO-TP frame")
            message = self.bus.recv(timeout=remaining)
            if message is not None and message.arbitration_id == self.rxid and predicate(message):
                return message

    def send(self, payload: bytes) -> None:
        frames = pack_message(payload, padding=self.padding)
        if len(frames) == 1:
            self._send_frame(frames[0])
            return

        self._send_frame(frames[0])
        fc = self._wait_frame(
            lambda m: (m.data[0] >> 4) & 0x0F == PCI_FC,
            self.timeout,
        )
        flow_status = fc.data[0] & 0x0F
        if flow_status != FC_CTS:
            raise TimeoutError(f"flow control rejected transmission, status={flow_status}")
        for frame in frames[1:]:
            if self.stmin_s > 0:
                time.sleep(self.stmin_s)
            self._send_frame(frame)

    def receive(self, timeout: float | None = None) -> bytes:
        deadline = time.monotonic() + (timeout if timeout is not None else self.timeout)

        def remaining() -> float:
            return max(0.0, deadline - time.monotonic())

        message = self._wait_frame(lambda m: True, remaining())
        pci_type = (message.data[0] >> 4) & 0x0F
        if pci_type == PCI_SF:
            length = message.data[0] & 0x0F
            return bytes(message.data[1 : 1 + length])
        if pci_type != PCI_FF:
            raise ValueError(f"unexpected PCI type {pci_type} while receiving")

        total_length = ((message.data[0] & 0x0F) << 8) | message.data[1]
        payload = bytearray(message.data[2 : 2 + MAX_FF_LENGTH])
        expected_index = 1
        self._send_frame(encode_flow_control(FC_CTS, self.block_size, 0))
        while len(payload) < total_length:
            frame = self._wait_frame(
                lambda m: (m.data[0] >> 4) & 0x0F == PCI_CF
                and m.data[0] & 0x0F == expected_index,
                remaining(),
            )
            payload.extend(frame.data[1:])
            expected_index = (expected_index + 1) % 16
        return bytes(payload[:total_length])

    def close(self) -> None:
        if hasattr(self.bus, "shutdown"):
            self.bus.shutdown()


def _pad(frame: bytearray, padding: int) -> bytes:
    while len(frame) < 8:
        frame.append(padding & 0xFF)
    return bytes(frame[:8])
