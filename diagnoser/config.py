"""Runtime configuration for the diagnostic toolchain."""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


@dataclass
class EcuConfig:
    name: str
    request_id: int
    response_id: int
    default_session: int = 0x01

    @classmethod
    def from_dict(cls, raw: dict[str, Any]) -> "EcuConfig":
        return cls(
            name=str(raw["name"]),
            request_id=int(raw["request_id"], 0),
            response_id=int(raw["response_id"], 0),
            default_session=int(raw.get("default_session", "0x01"), 0),
        )


@dataclass
class NetworkConfig:
    interface: str = "virtual"
    channel: str = "dual-can-uds"
    baudrate: int = 500000
    timeout: float = 2.0
    padding: int = 0xCC
    ecus: list[EcuConfig] = field(default_factory=list)

    @classmethod
    def from_dict(cls, raw: dict[str, Any]) -> "NetworkConfig":
        return cls(
            interface=str(raw.get("interface", "virtual")),
            channel=str(raw.get("channel", "dual-can-uds")),
            baudrate=int(raw.get("baudrate", 500000)),
            timeout=float(raw.get("timeout", 2.0)),
            padding=int(raw.get("padding", "0xCC"), 0),
            ecus=[EcuConfig.from_dict(item) for item in raw.get("ecus", [])],
        )

    @classmethod
    def load(cls, path: str | Path) -> "NetworkConfig":
        data = json.loads(Path(path).read_text(encoding="utf-8"))
        return cls.from_dict(data)

    def get_ecu(self, name: str) -> EcuConfig:
        for ecu in self.ecus:
            if ecu.name == name:
                return ecu
        raise KeyError(f"ECU {name!r} not found in network config")


def default_network() -> NetworkConfig:
    return NetworkConfig(
        interface="virtual",
        channel="dual-can-uds",
        baudrate=500000,
        timeout=2.0,
        padding=0xCC,
        ecus=[
            EcuConfig(name="ecu1", request_id=0x7E0, response_id=0x7E8),
            EcuConfig(name="ecu2", request_id=0x7E1, response_id=0x7E9),
        ],
    )
