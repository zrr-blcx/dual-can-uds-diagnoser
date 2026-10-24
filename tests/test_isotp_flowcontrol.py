"""Flow-control semantics for the Python ISO-TP transport.

These tests use a scripted peer on the other end of a virtual bus to verify
that IsoTpTransport.send() waits for a new flow-control frame at each BS block
boundary, and that receive() advertises blocks with the configured BS/STmin.
"""

from __future__ import annotations

import queue
import threading
import time

import can

from diagnoser.transport.isotp import (
    FC_CTS,
    FC_OVERFLOW,
    FC_WAIT,
    PCI_CF,
    PCI_FC,
    PCI_FF,
    PCI_SF,
    IsoTpTransport,
    encode_consecutive_frame,
    encode_first_frame,
    encode_flow_control,
    pack_message,
)


class ScriptedBus(can.BusABC):
    """A deterministic virtual bus. Sent frames land in a queue the test drains;
    the test pushes frames into 'incoming' for the transport to receive."""

    def __init__(self) -> None:
        super().__init__(channel=0)
        self.outgoing: list[can.Message] = []
        self.incoming: queue.Queue[can.Message] = queue.Queue()

    def send(self, msg: can.Message) -> None:
        self.outgoing.append(msg)

    def recv(self, timeout: float | None = None) -> can.Message | None:
        if timeout is None:
            return self.incoming.get()
        try:
            return self.incoming.get(timeout=timeout)
        except queue.Empty:
            return None

    def shutdown(self) -> None:
        pass


def _pci(msg: can.Message) -> int:
    return (msg.data[0] >> 4) & 0x0F


def _make_transport(bus: ScriptedBus, block_size: int = 8, stmin_ms: int = 0) -> IsoTpTransport:
    return IsoTpTransport(
        bus,
        txid=0x7E0,
        rxid=0x7E8,
        timeout=0.5,
        padding=0xCC,
        stmin_ms=stmin_ms,
        block_size=block_size,
    )


def test_send_waits_for_fc_at_each_block() -> None:
    bus = ScriptedBus()
    tx = _make_transport(bus, block_size=8)

    # Peer drives by counting CFs: after FF grant block 1 (8 CF), after CF 8
    # grant block 2. The transport must not emit CF 9 before the second FC.
    peer_seen_cf = [0]
    second_fc_sent = [False]

    def peer() -> None:
        while True:
            time.sleep(0.002)
            if bus.outgoing and _pci(bus.outgoing[0]) == PCI_FF:
                break
        bus.incoming.put(can.Message(arbitration_id=0x7E8, data=encode_flow_control(FC_CTS, 8, 0)))
        # Wait until 8 CFs have left before granting the next block.
        while peer_seen_cf[0] < 8:
            time.sleep(0.002)
        second_fc_sent[0] = True
        bus.incoming.put(can.Message(arbitration_id=0x7E8, data=encode_flow_control(FC_CTS, 8, 0)))

    def observer() -> None:
        seen = 0
        while True:
            time.sleep(0.002)
            if len(bus.outgoing) > seen:
                peer_seen_cf[0] += sum(
                    1 for m in bus.outgoing[seen:] if _pci(m) == PCI_CF
                )
                seen = len(bus.outgoing)

    t = threading.Thread(target=peer)
    o = threading.Thread(target=observer)
    o.daemon = True
    t.start()
    o.start()
    try:
        payload = bytes(range(100))
        tx.send(payload)
        cf_sns = [m.data[0] & 0x0F for m in bus.outgoing if _pci(m) == PCI_CF]
        assert len(cf_sns) == 14
        assert cf_sns == list(range(1, 15))
        assert second_fc_sent[0]
    finally:
        t.join(timeout=1.0)
        o.join(timeout=1.0)


def test_receive_advertises_two_blocks() -> None:
    bus = ScriptedBus()
    rx = _make_transport(bus, block_size=8)

    payload = bytes(range(100))
    frames = pack_message(payload)
    fc_count = [0]

    def script_incoming() -> None:
        # The peer transmits towards our rxid.
        bus.incoming.put(can.Message(arbitration_id=0x7E8, data=frames[0]))  # FF
        # After the receiver advertises the first block, feed 8 CFs.
        time.sleep(0.02)
        for f in frames[1:9]:
            bus.incoming.put(can.Message(arbitration_id=0x7E8, data=f))
        time.sleep(0.05)
        for f in frames[9:]:
            bus.incoming.put(can.Message(arbitration_id=0x7E8, data=f))

    t = threading.Thread(target=script_incoming)
    t.daemon = True
    t.start()
    try:
        got = rx.receive()
        assert got == payload
        fcs = [m for m in bus.outgoing if _pci(m) == PCI_FC]
        assert len(fcs) == 2  # one after FF, one after block 1
        for fc in fcs:
            assert fc.data[0] & 0x0F == FC_CTS
            assert fc.data[1] == 8  # BS advertised
    finally:
        t.join(timeout=1.0)


def test_send_aborts_on_overflow_fc() -> None:
    bus = ScriptedBus()
    tx = _make_transport(bus, block_size=8)

    payload = bytes(range(20))

    def peer() -> None:
        while True:
            time.sleep(0.005)
            if bus.outgoing and _pci(bus.outgoing[0]) == PCI_FF:
                break
        bus.incoming.put(can.Message(arbitration_id=0x7E8, data=encode_flow_control(FC_OVERFLOW, 0, 0)))

    t = threading.Thread(target=peer)
    t.daemon = True
    t.start()
    try:
        try:
            tx.send(payload)
            raise AssertionError("expected overflow to abort the transfer")
        except TimeoutError:
            pass
    finally:
        t.join(timeout=1.0)


def test_send_handles_wait_before_cts() -> None:
    bus = ScriptedBus()
    tx = _make_transport(bus, block_size=8)

    payload = bytes(range(20))

    def peer() -> None:
        while True:
            time.sleep(0.005)
            if bus.outgoing and _pci(bus.outgoing[0]) == PCI_FF:
                break
        bus.incoming.put(can.Message(arbitration_id=0x7E8, data=encode_flow_control(FC_WAIT, 0, 0)))
        time.sleep(0.02)
        bus.incoming.put(can.Message(arbitration_id=0x7E8, data=encode_flow_control(FC_CTS, 0, 0)))

    t = threading.Thread(target=peer)
    t.daemon = True
    t.start()
    try:
        tx.send(payload)
        cf_sns = [m.data[0] & 0x0F for m in bus.outgoing if _pci(m) == PCI_CF]
        assert len(cf_sns) == 2  # 20B -> FF + 2 CF
    finally:
        t.join(timeout=1.0)


def test_wait_refreshes_flow_control_deadline() -> None:
    bus = ScriptedBus()
    tx = IsoTpTransport(
        bus,
        txid=0x7E0,
        rxid=0x7E8,
        timeout=0.10,
        padding=0xCC,
        block_size=8,
    )

    def peer() -> None:
        while True:
            time.sleep(0.005)
            if bus.outgoing and _pci(bus.outgoing[0]) == PCI_FF:
                break
        time.sleep(0.06)
        bus.incoming.put(
            can.Message(
                arbitration_id=0x7E8,
                data=encode_flow_control(FC_WAIT, 0, 0),
            )
        )
        time.sleep(0.06)
        bus.incoming.put(
            can.Message(
                arbitration_id=0x7E8,
                data=encode_flow_control(FC_CTS, 0, 0),
            )
        )

    t = threading.Thread(target=peer)
    t.daemon = True
    t.start()
    try:
        tx.send(bytes(range(20)))
        assert sum(1 for m in bus.outgoing if _pci(m) == PCI_CF) == 2
    finally:
        t.join(timeout=1.0)
