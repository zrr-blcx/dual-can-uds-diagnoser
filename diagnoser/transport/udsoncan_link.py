"""udsoncan connection adapter over the in-repo ISO-TP transport."""

from __future__ import annotations

from diagnoser.transport.isotp import IsoTpTransport


class UdsOnCanLink:
    """Expose :class:`IsoTpTransport` through the udsoncan connection API.

    The transport (and its python-can bus) is owned by the caller; ``close``
    only marks this connection as closed.
    """

    def __init__(self, transport: IsoTpTransport) -> None:
        self.transport = transport
        self._open = True

    def open(self) -> None:
        self._open = True

    def close(self) -> None:
        self._open = False

    def is_open(self) -> bool:
        return self._open

    def send(self, data: bytes) -> None:
        self.transport.send(bytes(data))

    def wait_frame(
        self, timeout: float | None = None, exception: bool = False
    ) -> bytes | None:
        try:
            return bytes(self.transport.receive(timeout=timeout))
        except TimeoutError:
            if exception:
                raise
            return None

    def empty_rxqueue(self) -> None:
        """Drop already-buffered frames before a new request."""

        while True:
            try:
                self.transport.receive(timeout=0.001)
            except (TimeoutError, ValueError):
                return
