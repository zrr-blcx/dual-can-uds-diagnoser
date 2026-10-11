"""High-level UDS client for the core ISO 14229 services."""

from __future__ import annotations

from diagnoser.transport.isotp import IsoTpTransport
from diagnoser.uds.constants import (
    SECURITY_REQUEST_SEED,
    SECURITY_SEND_KEY,
    SID_DIAGNOSTIC_SESSION_CONTROL,
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
from diagnoser.uds.security import DEFAULT_SECURITY_SALT, seed_to_key


class UdsClient:
    def __init__(self, transport: IsoTpTransport, timeout: float | None = None) -> None:
        self.transport = transport
        self.timeout = timeout

    def _exchange(self, request: bytes, expected_sid: int) -> bytes:
        self.transport.send(request)
        response = self.transport.receive(timeout=self.timeout)
        return parse_response(response, expected_sid)

    def send_raw(self, request: bytes) -> bytes:
        self.transport.send(request)
        return self.transport.receive(timeout=self.timeout)

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

    def request_seed(self, level: int = 0x01) -> bytes:
        """Request a seed and return only the seed bytes."""

        response = self.security_access(level)
        return response[1:5]

    def send_key(self, key: bytes, level: int = 0x01) -> bytes:
        if len(key) != 4:
            raise ValueError("security key must be four bytes")
        return self.security_access(level + 1, key)

    def unlock(self, salt: int = DEFAULT_SECURITY_SALT) -> bytes:
        """Run the 0x27 seed/key exchange and return the final response."""

        seed = int.from_bytes(self.request_seed(), "big")
        if seed == 0:
            return bytes([0x02])
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

