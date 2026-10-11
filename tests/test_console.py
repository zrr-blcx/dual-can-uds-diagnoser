"""Console formatting tests for the V1.0 diagnostic tool."""

from __future__ import annotations

import re

from diagnoser.tools.console import exchange_to_dict, format_exchange, timestamp_text
from diagnoser.uds.client import ExchangeInfo


def _info(
    response: bytes = b"\x62\xF1\x90",
    *,
    pending: int = 0,
    retries: int = 0,
) -> ExchangeInfo:
    return ExchangeInfo(
        request=b"\x22\xF1\x90",
        response=response,
        tx_id=0x7E0,
        rx_id=0x7E8,
        tx_time=1000.0,
        rx_time=1000.0123,
        elapsed_ms=12.3,
        retries=retries,
        pending_responses=pending,
    )


def test_timestamp_text_has_millisecond_precision() -> None:
    assert re.fullmatch(r"\d{2}:\d{2}:\d{2}\.\d{3}", timestamp_text(1000.0123))


def test_format_positive_exchange() -> None:
    text = format_exchange(_info())

    assert "TX 7E0: 22 F1 90" in text
    assert "RX 7E8: 62 F1 90" in text
    assert "12.3 ms" in text
    assert "NRC" not in text


def test_format_negative_exchange_includes_nrc_name() -> None:
    text = format_exchange(_info(b"\x7F\x2E\x33"))

    assert "NRC 0x33 (securityAccessDenied)" in text


def test_format_marks_pending_and_retries() -> None:
    text = format_exchange(_info(pending=2, retries=1))

    assert "pending=2" in text
    assert "retries=1" in text


def test_exchange_to_dict_is_json_ready() -> None:
    payload = exchange_to_dict(_info(b"\x7F\x22\x31"), title="read DID")

    assert payload["title"] == "read DID"
    assert payload["tx_id"] == "0x7E0"
    assert payload["rx_id"] == "0x7E8"
    assert payload["response"] == "7F 22 31"
    assert payload["negative"] is True
    assert payload["service"] == "0x22"
    assert payload["nrc"] == "0x31"
    assert payload["nrc_name"] == "requestOutOfRange"
