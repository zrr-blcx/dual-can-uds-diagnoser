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


class ScriptedTransport:
    """Return queued responses or raise queued exceptions."""

    def __init__(
        self,
        responses: list[bytes | Exception],
        *,
        txid: int = 0x7E0,
        rxid: int = 0x7E8,
    ) -> None:
        self.responses = list(responses)
        self.sent: list[bytes] = []
        self.txid = txid
        self.rxid = rxid

    def send(self, payload: bytes) -> None:
        self.sent.append(bytes(payload))

    def receive(self, timeout: float | None = None) -> bytes:
        item = self.responses.pop(0)
        if isinstance(item, Exception):
            raise item
        return item


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


def test_exchange_records_timing_and_can_ids() -> None:
    transport = ScriptedTransport([b"\x50\x03\x00\x32\x01\xF4"])
    client = UdsClient(transport, timeout=0.5, retries=0)
    client.change_session(0x03)

    info = client.last_exchange
    assert info is not None
    assert info.tx_id == 0x7E0
    assert info.rx_id == 0x7E8
    assert info.request == b"\x10\x03"
    assert info.response == b"\x50\x03\x00\x32\x01\xF4"
    assert info.elapsed_ms >= 0.0
    assert info.negative is False
    assert info.nrc is None


def test_response_pending_is_consumed_without_resend() -> None:
    transport = ScriptedTransport([b"\x7F\x31\x78", b"\x71\x03\x02\x03\x00"])
    client = UdsClient(transport, timeout=0.5, p2_star_ms=2000, retries=0)
    response = client.routine_control(0x03, 0x0203)

    assert response == b"\x02\x03\x00"
    assert transport.sent == [b"\x31\x03\x02\x03"]
    info = client.last_exchange
    assert info is not None
    assert info.pending_responses == 1


def test_response_pending_disabled_raises() -> None:
    transport = ScriptedTransport([b"\x7F\x31\x78"])
    client = UdsClient(transport, timeout=0.5, handle_response_pending=False)
    with pytest.raises(NegativeResponse) as exc_info:
        client.routine_control(0x03, 0x0203)

    assert exc_info.value.nrc == 0x78
    info = exc_info.value.exchange
    assert info is not None
    assert info.pending_responses == 0


def test_timeout_is_retried_then_succeeds() -> None:
    transport = ScriptedTransport([TimeoutError("no response"), b"\x62\xF1\x90\x00"])
    client = UdsClient(transport, timeout=0.5, retries=1)
    response = client.read_data_by_identifier(0xF190)

    assert response == b"\xF1\x90\x00"
    assert len(transport.sent) == 2
    info = client.last_exchange
    assert info is not None
    assert info.retries == 1


def test_timeout_raises_after_retries_exhausted() -> None:
    transport = ScriptedTransport([TimeoutError("no response"), TimeoutError("no response")])
    client = UdsClient(transport, timeout=0.5, retries=1)
    with pytest.raises(TimeoutError):
        client.read_data_by_identifier(0xF190)

    assert len(transport.sent) == 2


def test_invalid_timing_configuration_is_rejected() -> None:
    with pytest.raises(ValueError):
        UdsClient(ScriptedTransport([]), p2_ms=0)
    with pytest.raises(ValueError):
        UdsClient(ScriptedTransport([]), p2_star_ms=0)
    with pytest.raises(ValueError):
        UdsClient(ScriptedTransport([]), retries=-1)
