"""CAN bus load simulation."""

from __future__ import annotations

import threading
import time

import can

CAN_FRAME_OVERHEAD_BITS = 47


def frame_bits(dlc: int) -> int:
    return CAN_FRAME_OVERHEAD_BITS + dlc * 8


class BusLoadSimulator:
    def __init__(
        self,
        bus: can.BusABC,
        arbitration_id: int = 0x100,
        dlc: int = 8,
        baudrate: int = 500000,
        target_load: float = 0.9,
        payload: bytes | None = None,
        sequence_prefix: bool = False,
    ) -> None:
        self.bus = bus
        self.arbitration_id = arbitration_id
        self.dlc = dlc
        self.payload = payload if payload is not None else bytes(dlc)
        self.target_load = target_load
        self.baudrate = baudrate
        self.sequence_prefix = bool(sequence_prefix)
        self._period = frame_bits(dlc) / baudrate / target_load
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None
        self._sent = 0
        self._missed_deadlines = 0
        self._started_at = 0.0
        self._lock = threading.Lock()

    def start(self) -> None:
        if self._thread is not None:
            return
        self._stop.clear()
        self._sent = 0
        self._started_at = time.perf_counter()
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def _run(self) -> None:
        next_send = time.perf_counter()
        while not self._stop.is_set():
            now = time.perf_counter()
            if now < next_send:
                time.sleep(next_send - now)
            else:
                late = now - next_send
                if late >= self._period:
                    missed = int(late / self._period)
                    with self._lock:
                        self._missed_deadlines += missed
                    next_send += missed * self._period

            with self._lock:
                sequence = self._sent
            payload = self.payload
            if self.sequence_prefix:
                prefix = sequence.to_bytes(4, "big")
                payload = (prefix + bytes(max(0, self.dlc - 4)))[: self.dlc]
            self.bus.send(
                can.Message(
                    arbitration_id=self.arbitration_id,
                    data=payload,
                    is_extended_id=False,
                )
            )
            with self._lock:
                self._sent += 1
            next_send += self._period

    def stop(self) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=2.0)
            self._thread = None

    def measured_load(self) -> float:
        elapsed = time.perf_counter() - self._started_at
        if elapsed <= 0:
            return 0.0
        with self._lock:
            sent = self._sent
        return (sent * frame_bits(self.dlc)) / elapsed / self.baudrate

    @property
    def missed_deadlines(self) -> int:
        with self._lock:
            return self._missed_deadlines
