"""An in-process CAN bus that routes frames by arbitration ID."""

from __future__ import annotations

import queue
import threading
from typing import Any

import can


class MemoryBus(can.BusABC):
    _channels: dict[str, dict[object, "queue.Queue[can.Message]"]] = {}
    _lock = threading.Lock()

    def __init__(
        self,
        channel: str = "memory-can",
        node_id: int | None = None,
        bitrate: int = 500000,
        **kwargs: Any,
    ) -> None:
        self.node_id = node_id
        with MemoryBus._lock:
            routes = MemoryBus._channels.setdefault(channel, {})
            key: object = node_id if node_id is not None else object()
            self._queue = routes.setdefault(key, queue.Queue())
        super().__init__(channel=channel, bitrate=bitrate, **kwargs)

    def send(self, message: can.Message) -> None:
        with MemoryBus._lock:
            targets = [
                route
                for node_key, route in MemoryBus._channels[
                    self.channel
                ].items()
                if isinstance(node_key, int) and node_key == message.arbitration_id
            ]
        for target in targets:
            target.put(message)

    def _recv_internal(self, timeout: float | None):
        try:
            return self._queue.get(timeout=timeout), True
        except queue.Empty:
            return None, True

    def shutdown(self) -> None:
        with MemoryBus._lock:
            routes = MemoryBus._channels.get(self.channel, {})
            key: object = self.node_id if self.node_id is not None else None
            for route_key in list(routes):
                if route_key == key or (
                    self.node_id is None and not isinstance(route_key, int)
                ):
                    del routes[route_key]
