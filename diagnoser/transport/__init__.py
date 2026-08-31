"""CAN bus and ISO-TP transport layers."""

from __future__ import annotations

import can

from diagnoser.config import NetworkConfig
from diagnoser.transport.isotp import IsoTpTransport

__all__ = ["IsoTpTransport", "create_bus"]


def create_bus(config: NetworkConfig) -> can.BusABC:
    kwargs: dict[str, object] = {
        "interface": config.interface,
        "channel": config.channel,
        "bitrate": config.baudrate,
    }
    return can.Bus(**kwargs)
