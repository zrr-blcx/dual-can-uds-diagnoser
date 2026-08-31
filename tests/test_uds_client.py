import pytest

from diagnoser.uds.client import UdsClient
from diagnoser.uds.errors import NegativeResponse, UnexpectedResponse


class FakeTransport:
    def __init__(self, response: bytes) -> None:
        self.response = response
        self.sent: list[bytes] = []

    def send(self, payload: bytes) -> None:
        self.sent.append(payload)

    def receive(self, timeout: float | None = None) -> bytes:
        return self.response


def test_change_session_request() -> None:
    transport = FakeTransport(b"\x50\x03\x00\x32\x00\x00")
    client = UdsClient(transport)
    response = client.change_session(0x03)
    assert transport.sent == [b"\x10\x03"]
    assert response[0] == 0x03


def test_read_did_request() -> None:
    transport = FakeTransport(b"\x62\xF1\x90\x32\x30\x32\x36")
    client = UdsClient(transport)
    response = client.read_data_by_identifier(0xF190)
    assert transport.sent == [b"\x22\xF1\x90"]
    assert response == b"\xF1\x90\x32\x30\x32\x36"


def test_write_did_request() -> None:
    transport = FakeTransport(b"\x6E\xF1\x91")
    client = UdsClient(transport)
    response = client.write_data_by_identifier(0xF191, b"\x01\x02")
    assert transport.sent == [b"\x2E\xF1\x91\x01\x02"]
    assert response == b"\xF1\x91"


def test_routine_control_request() -> None:
    transport = FakeTransport(b"\x71\x01\x02\x03\x00")
    client = UdsClient(transport)
    response = client.routine_control(0x01, 0x0203)
    assert transport.sent == [b"\x31\x01\x02\x03"]
    assert response == b"\x02\x03\x00"


def test_negative_response_raises_with_nrc() -> None:
    transport = FakeTransport(b"\x7F\x22\x31")
    client = UdsClient(transport)
    with pytest.raises(NegativeResponse) as exc_info:
        client.read_data_by_identifier(0xFFFF)
    assert exc_info.value.service == 0x22
    assert exc_info.value.nrc == 0x31


def test_unexpected_sid_raises() -> None:
    transport = FakeTransport(b"\x62\x01\x02")
    client = UdsClient(transport)
    with pytest.raises(UnexpectedResponse):
        client.change_session(0x03)
