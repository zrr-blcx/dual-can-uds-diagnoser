"""High-level UDS client for the core ISO 14229 services."""

from __future__ import annotations

import time
from dataclasses import dataclass
from typing import Any

from diagnoser.transport.isotp import IsoTpTransport
from diagnoser.uds.constants import (
    DEFAULT_P2_MS,
    DEFAULT_P2_STAR_MS,
    DEFAULT_RETRIES,
    NRC_RESPONSE_PENDING,
    SECURITY_REQUEST_SEED,
    SECURITY_SEND_KEY,
    SID_DIAGNOSTIC_SESSION_CONTROL,
    SID_NEGATIVE_RESPONSE,
    SID_READ_DATA_BY_IDENTIFIER,
    SID_ROUTINE_CONTROL,
    SID_SECURITY_ACCESS,
    SID_TESTER_PRESENT,
    SID_WRITE_DATA_BY_IDENTIFIER,
)
from diagnoser.uds.encoding import (
    build_read_data_by_identifier,
    build_routine_control,
    build_security_access,
    build_session_control,
    build_write_data_by_identifier,
    parse_response,
)
from diagnoser.uds.errors import NegativeResponse
from diagnoser.uds.nrc import nrc_name
from diagnoser.uds.security import DEFAULT_SECURITY_SALT, seed_to_key


@dataclass(frozen=True)
class ExchangeInfo:
    """Timing and protocol metadata for one request/response exchange."""

    request: bytes
    response: bytes
    tx_id: int | None
    rx_id: int | None
    tx_time: float
    rx_time: float
    elapsed_ms: float
    retries: int = 0
    pending_responses: int = 0

    @property
    def negative(self) -> bool:
        return bool(self.response) and self.response[0] == SID_NEGATIVE_RESPONSE

    @property
    def service(self) -> int | None:
        if self.negative and len(self.response) > 1:
            return self.response[1]
        return None

    @property
    def nrc(self) -> int | None:
        if self.negative and len(self.response) > 2:
            return self.response[2]
        return None

    @property
    def nrc_name(self) -> str | None:
        return nrc_name(self.nrc) if self.nrc is not None else None

    def as_dict(self) -> dict[str, Any]:
        return {
            "tx_id": f"0x{self.tx_id:03X}" if self.tx_id is not None else None,
            "rx_id": f"0x{self.rx_id:03X}" if self.rx_id is not None else None,
            "request": self.request.hex(" ").upper(),
            "response": self.response.hex(" ").upper(),
            "elapsed_ms": round(self.elapsed_ms, 3),
            "retries": self.retries,
            "pending_responses": self.pending_responses,
            "negative": self.negative,
            "service": f"0x{self.service:02X}" if self.service is not None else None,
            "nrc": f"0x{self.nrc:02X}" if self.nrc is not None else None,
            "nrc_name": self.nrc_name,
        }


class UdsClient:
    """Blocking UDS client with P2/P2* timing, retry, and 0x78 handling."""

    def __init__(
        self,
        transport: IsoTpTransport,
        timeout: float | None = None,
        p2_ms: float = DEFAULT_P2_MS,
        p2_star_ms: float = DEFAULT_P2_STAR_MS,
        retries: int = DEFAULT_RETRIES,
        retry_delay_s: float = 0.0,
        handle_response_pending: bool = True,
    ) -> None:
        if p2_ms <= 0:
            raise ValueError("p2_ms must be positive")
        if p2_star_ms <= 0:
            raise ValueError("p2_star_ms must be positive")
        if retries < 0:
            raise ValueError("retries must not be negative")

        self.transport = transport
        self.p2_ms = float(p2_ms)
        self.p2_star_ms = float(p2_star_ms)
        self.retries = int(retries)
        self.retry_delay_s = float(retry_delay_s)
        self.handle_response_pending = bool(handle_response_pending)
        self.timeout = (
            float(timeout) if timeout is not None else self.p2_ms / 1000.0
        )
        self.last_exchange: ExchangeInfo | None = None

    def _exchange(self, request: bytes, expected_sid: int) -> bytes:
        exchange = self._transact(request)
        self.last_exchange = exchange
        try:
            return parse_response(exchange.response, expected_sid)
        except NegativeResponse as exc:
            exc.exchange = exchange
            raise

    def _transact(self, request: bytes) -> ExchangeInfo:
        last_error: TimeoutError | None = None

        for attempt in range(self.retries + 1):
            if attempt > 0 and self.retry_delay_s > 0:
                time.sleep(self.retry_delay_s)
            try:
                return self._transact_once(request, attempt)
            except TimeoutError as exc:
                last_error = exc

        if last_error is not None:
            raise last_error
        raise TimeoutError("no response")

    def _transact_once(self, request: bytes, attempt: int) -> ExchangeInfo:
        self.transport.send(request)
        tx_time = time.monotonic()
        timeout = self.timeout
        pending_responses = 0

        while True:
            response = self.transport.receive(timeout=timeout)
            if (
                self.handle_response_pending
                and len(response) >= 3
                and response[0] == SID_NEGATIVE_RESPONSE
                and response[2] == NRC_RESPONSE_PENDING
            ):
                pending_responses += 1
                timeout = self.p2_star_ms / 1000.0
                continue
            rx_time = time.monotonic()
            return ExchangeInfo(
                request=bytes(request),
                response=bytes(response),
                tx_id=getattr(self.transport, "txid", None),
                rx_id=getattr(self.transport, "rxid", None),
                tx_time=tx_time,
                rx_time=rx_time,
                elapsed_ms=(rx_time - tx_time) * 1000.0,
                retries=attempt,
                pending_responses=pending_responses,
            )

    def send_raw(self, request: bytes) -> bytes:
        self.transport.send(request)
        tx_time = time.monotonic()
        response = self.transport.receive(timeout=self.timeout)
        rx_time = time.monotonic()
        self.last_exchange = ExchangeInfo(
            request=bytes(request),
            response=bytes(response),
            tx_id=getattr(self.transport, "txid", None),
            rx_id=getattr(self.transport, "rxid", None),
            tx_time=tx_time,
            rx_time=rx_time,
            elapsed_ms=(rx_time - tx_time) * 1000.0,
        )
        return response

    def change_session(self, session: int) -> bytes:
        return self._exchange(
            build_session_control(session), SID_DIAGNOSTIC_SESSION_CONTROL
        )

    def read_data_by_identifier(self, did: int) -> bytes:
        return self._exchange(
            build_read_data_by_identifier(did), SID_READ_DATA_BY_IDENTIFIER
        )

    def write_data_by_identifier(self, did: int, data: bytes) -> bytes:
        return self._exchange(
            build_write_data_by_identifier(did, data), SID_WRITE_DATA_BY_IDENTIFIER
        )

    def security_access(self, subfunction: int, data: bytes = b"") -> bytes:
        return self._exchange(
            build_security_access(subfunction, data), SID_SECURITY_ACCESS
        )

    def request_seed(self, level: int = SECURITY_REQUEST_SEED) -> bytes:
        """Request a seed and return only the seed bytes."""

        response = self.security_access(level)
        return response[1:5]

    def send_key(self, key: bytes, level: int = SECURITY_REQUEST_SEED) -> bytes:
        if len(key) != 4:
            raise ValueError("security key must be four bytes")
        return self.security_access(level + 1, key)

    def unlock(self, salt: int = DEFAULT_SECURITY_SALT) -> bytes:
        """Run the 0x27 seed/key exchange and return the final response."""

        seed = int.from_bytes(self.request_seed(), "big")
        if seed == 0:
            return bytes([SECURITY_SEND_KEY])
        return self.send_key(seed_to_key(seed, salt).to_bytes(4, "big"))

    def routine_control(
        self, subfunction: int, routine_id: int, data: bytes = b""
    ) -> bytes:
        return self._exchange(
            build_routine_control(subfunction, routine_id, data),
            SID_ROUTINE_CONTROL,
        )[1:]

    def tester_present(self) -> bytes:
        return self._exchange(bytes([SID_TESTER_PRESENT, 0x00]), SID_TESTER_PRESENT)
