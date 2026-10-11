"""Console formatting for UDS request/response exchanges."""

from __future__ import annotations

import time
from typing import Any

from diagnoser.uds.client import ExchangeInfo


def timestamp_text(rx_time: float) -> str:
    local = time.localtime(rx_time)
    millis = int((rx_time % 1.0) * 1000.0)
    return f"{time.strftime('%H:%M:%S', local)}.{millis:03d}"


def _can_id(value: int | None) -> str:
    return f"{value:03X}" if value is not None else "---"


def format_exchange(info: ExchangeInfo) -> str:
    """Render one exchange as a single timestamped line."""

    parts = [
        f"[{timestamp_text(info.rx_time)}]",
        f"TX {_can_id(info.tx_id)}: {info.request.hex(' ').upper()}",
        f"RX {_can_id(info.rx_id)}: {info.response.hex(' ').upper()}",
        f"{info.elapsed_ms:.1f} ms",
    ]
    if info.retries:
        parts.append(f"retries={info.retries}")
    if info.pending_responses:
        parts.append(f"pending={info.pending_responses}")
    if info.negative and info.nrc is not None:
        parts.append(f"NRC 0x{info.nrc:02X} ({info.nrc_name})")
    return " | ".join(parts)


def exchange_to_dict(info: ExchangeInfo, title: str | None = None) -> dict[str, Any]:
    """Render one exchange as a JSON-serializable object."""

    payload = info.as_dict()
    payload["timestamp"] = timestamp_text(info.rx_time)
    if title is not None:
        payload["title"] = title
    return payload
