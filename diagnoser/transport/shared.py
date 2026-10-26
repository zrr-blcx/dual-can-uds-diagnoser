"""Share one physical CAN device between multiple diagnostic endpoints."""

from __future__ import annotations

import queue
import threading

import can


class SharedCanBus:
    """Route received CAN IDs to independent endpoint queues.

    The underlying python-can bus has exactly one reader. Each diagnostic
    transport receives only frames matching its response ID, while sends from
    all transports and the bus-load generator are serialized on one device.
    """

    def __init__(
        self,
        bus: can.BusABC,
        *,
        queue_capacity: int = 1024,
        reader_timeout_s: float = 0.05,
    ) -> None:
        if queue_capacity <= 0:
            raise ValueError("queue_capacity must be positive")
        if reader_timeout_s <= 0:
            raise ValueError("reader_timeout_s must be positive")

        self.bus = bus
        self.queue_capacity = queue_capacity
        self.reader_timeout_s = reader_timeout_s
        self._endpoints: dict[int, queue.Queue[can.Message]] = {}
        self._endpoint_lock = threading.Lock()
        self._send_lock = threading.Lock()
        self._stop = threading.Event()
        self._closed = False
        self._reader = threading.Thread(
            target=self._read_loop,
            name="shared-can-reader",
            daemon=True,
        )
        self._reader.start()

    def endpoint(self, arbitration_id: int) -> "SharedCanBusEndpoint":
        if self._closed:
            raise RuntimeError("shared CAN bus is closed")
        route: queue.Queue[can.Message] = queue.Queue(self.queue_capacity)
        with self._endpoint_lock:
            if arbitration_id in self._endpoints:
                raise ValueError(
                    f"CAN ID 0x{arbitration_id:X} already has an endpoint"
                )
            self._endpoints[arbitration_id] = route
        return SharedCanBusEndpoint(self, arbitration_id, route)

    def send(self, message: can.Message, timeout: float | None = None) -> None:
        if self._closed:
            raise RuntimeError("shared CAN bus is closed")
        with self._send_lock:
            self.bus.send(message, timeout=timeout)

    def shutdown(self) -> None:
        if self._closed:
            return
        self._closed = True
        self._stop.set()
        self._reader.join(timeout=2.0)
        self.bus.shutdown()

    def _read_loop(self) -> None:
        while not self._stop.is_set():
            message = self.bus.recv(timeout=self.reader_timeout_s)
            if message is None:
                continue
            with self._endpoint_lock:
                route = self._endpoints.get(message.arbitration_id)
            if route is not None:
                self._put_latest(route, message)

    def _put_latest(
        self,
        route: queue.Queue[can.Message],
        message: can.Message,
    ) -> None:
        try:
            route.put_nowait(message)
        except queue.Full:
            try:
                route.get_nowait()
            except queue.Empty:
                pass
            try:
                route.put_nowait(message)
            except queue.Full:
                pass

    def _remove_endpoint(
        self,
        arbitration_id: int,
        route: queue.Queue[can.Message],
    ) -> None:
        with self._endpoint_lock:
            if self._endpoints.get(arbitration_id) is route:
                del self._endpoints[arbitration_id]


class SharedCanBusEndpoint:
    """A filtered send/receive view of a :class:`SharedCanBus`."""

    def __init__(
        self,
        shared: SharedCanBus,
        arbitration_id: int,
        route: queue.Queue[can.Message],
    ) -> None:
        self.shared = shared
        self.arbitration_id = arbitration_id
        self._route = route
        self._closed = False

    def send(self, message: can.Message, timeout: float | None = None) -> None:
        self.shared.send(message, timeout=timeout)

    def recv(self, timeout: float | None = None) -> can.Message | None:
        if self._closed:
            return None
        try:
            return self._route.get(timeout=timeout)
        except queue.Empty:
            return None

    def shutdown(self) -> None:
        if self._closed:
            return
        self._closed = True
        self.shared._remove_endpoint(self.arbitration_id, self._route)
