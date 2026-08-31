"""UDS request/response encoding helpers."""

from __future__ import annotations

from diagnoser.uds.constants import (
    SID_DIAGNOSTIC_SESSION_CONTROL,
    SID_NEGATIVE_RESPONSE,
    SID_READ_DATA_BY_IDENTIFIER,
    SID_ROUTINE_CONTROL,
    SID_WRITE_DATA_BY_IDENTIFIER,
)
from diagnoser.uds.errors import NegativeResponse, UnexpectedResponse


def build_session_control(session: int) -> bytes:
    return bytes([SID_DIAGNOSTIC_SESSION_CONTROL, session & 0xFF])


def build_read_data_by_identifier(did: int) -> bytes:
    return bytes([SID_READ_DATA_BY_IDENTIFIER]) + did.to_bytes(2, "big")


def build_write_data_by_identifier(did: int, data: bytes) -> bytes:
    return bytes([SID_WRITE_DATA_BY_IDENTIFIER]) + did.to_bytes(2, "big") + data


def build_routine_control(subfunction: int, routine_id: int, data: bytes = b"") -> bytes:
    return (
        bytes([SID_ROUTINE_CONTROL, subfunction & 0xFF])
        + routine_id.to_bytes(2, "big")
        + data
    )


def parse_response(response: bytes, expected_sid: int) -> bytes:
    if not response:
        raise UnexpectedResponse("empty UDS response")
    if response[0] == SID_NEGATIVE_RESPONSE:
        service = response[1] if len(response) > 1 else 0
        nrc = response[2] if len(response) > 2 else 0
        raise NegativeResponse(service, nrc)
    if response[0] not in (expected_sid, expected_sid + 0x40):
        raise UnexpectedResponse(
            f"expected SID 0x{expected_sid:02X}, got 0x{response[0]:02X}"
        )
    return response[1:]

