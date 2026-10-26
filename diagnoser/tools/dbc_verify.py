"""Capture and decode the project DBC bench frames."""

from __future__ import annotations

import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

import can
import cantools

PROJECT_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_DBC = PROJECT_ROOT / "dbc" / "project.dbc"


@dataclass
class DbcVerifyResult:
    duration_seconds: float
    minimum_frames: int
    engine_frames: int = 0
    vehicle_frames: int = 0
    engine_values: dict[str, Any] = field(default_factory=dict)
    vehicle_values: dict[str, Any] = field(default_factory=dict)
    errors: list[str] = field(default_factory=list)

    @property
    def passed(self) -> bool:
        return (
            self.engine_frames >= self.minimum_frames
            and self.vehicle_frames >= self.minimum_frames
            and not self.errors
        )


def run_dbc_verify(
    bus: can.BusABC,
    *,
    duration_seconds: float = 3.0,
    minimum_frames: int = 3,
    dbc_path: str | Path = DEFAULT_DBC,
) -> DbcVerifyResult:
    if duration_seconds <= 0:
        raise ValueError("duration_seconds must be positive")
    if minimum_frames <= 0:
        raise ValueError("minimum_frames must be positive")

    database = cantools.database.load_file(str(dbc_path), strict=False)
    messages = {
        0x180: database.get_message_by_frame_id(0x180),
        0x280: database.get_message_by_frame_id(0x280),
    }
    expected = {
        0x180: {"EngineSpeed": 1234.5, "CoolantTemp": 85.0},
        0x280: {"VehicleSpeed": 42.5, "BrakeActive": 1},
    }
    result = DbcVerifyResult(
        duration_seconds=duration_seconds,
        minimum_frames=minimum_frames,
    )
    deadline = time.monotonic() + duration_seconds
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break
        message = bus.recv(timeout=remaining)
        if message is None or message.arbitration_id not in messages:
            continue
        frame_id = message.arbitration_id
        try:
            decoded = messages[frame_id].decode(
                bytes(message.data),
                decode_choices=False,
            )
        except Exception as exc:  # noqa: BLE001 - recorded in report
            result.errors.append(
                f"0x{frame_id:X} decode failed: {type(exc).__name__}: {exc}"
            )
            continue

        if frame_id == 0x180:
            result.engine_frames += 1
            result.engine_values = decoded
        else:
            result.vehicle_frames += 1
            result.vehicle_values = decoded

        for signal, expected_value in expected[frame_id].items():
            actual = decoded.get(signal)
            if isinstance(expected_value, float):
                matches = (
                    isinstance(actual, (int, float))
                    and abs(float(actual) - expected_value) < 0.000001
                )
            else:
                matches = actual == expected_value
            if not matches:
                result.errors.append(
                    f"0x{frame_id:X} {signal}: expected {expected_value}, got {actual}"
                )
    return result


def render_dbc_verify(result: DbcVerifyResult) -> str:
    lines = [
        "# DBC Bench Verification",
        "",
        f"- Duration: {result.duration_seconds:.3f}s",
        f"- Minimum frames per message: {result.minimum_frames}",
        f"- Outcome: {'PASS' if result.passed else 'FAIL'}",
        "",
        "| Message | Frames | Last decoded values |",
        "| --- | ---: | --- |",
        f"| EngineData | {result.engine_frames} | {result.engine_values or '-'} |",
        f"| VehicleStatus | {result.vehicle_frames} | {result.vehicle_values or '-'} |",
        "",
        "## Errors",
        "",
    ]
    if result.errors:
        lines.extend(f"- {error}" for error in result.errors)
    else:
        lines.append("- None")
    return "\n".join(lines) + "\n"
