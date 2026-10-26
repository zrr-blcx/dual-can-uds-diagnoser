from __future__ import annotations

import queue

import can


class QueueBus(can.BusABC):
    """A deterministic CAN device stub with an injectable receive queue."""

    def __init__(self) -> None:
        self.incoming: queue.Queue[can.Message] = queue.Queue()
        self.sent: list[can.Message] = []
        self.shutdown_count = 0
        super().__init__(channel="queue-bus", bitrate=500000)

    def send(self, msg: can.Message, timeout: float | None = None) -> None:
        del timeout
        self.sent.append(msg)

    def _recv_internal(self, timeout: float | None):
        try:
            return self.incoming.get(timeout=timeout), True
        except queue.Empty:
            return None, True

    def inject(self, arbitration_id: int, data: bytes) -> None:
        self.incoming.put(
            can.Message(
                arbitration_id=arbitration_id,
                data=data,
                is_extended_id=False,
            )
        )

    def shutdown(self) -> None:
        self.shutdown_count += 1
        super().shutdown()
