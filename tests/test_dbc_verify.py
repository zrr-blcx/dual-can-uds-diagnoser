from __future__ import annotations

from can_test_fakes import QueueBus
from diagnoser.tools.dbc_verify import render_dbc_verify, run_dbc_verify


def test_dbc_verify_decodes_expected_engine_and_vehicle_frames() -> None:
    bus = QueueBus()
    bus.inject(0x180, bytes.fromhex("134A7D0000000000"))
    bus.inject(0x280, bytes.fromhex("9A10010000000000"))

    result = run_dbc_verify(
        bus,
        duration_seconds=0.05,
        minimum_frames=1,
    )

    assert result.passed
    assert result.engine_frames == 1
    assert result.vehicle_frames == 1
    assert result.engine_values["EngineSpeed"] == 1234.5
    assert result.engine_values["CoolantTemp"] == 85.0
    assert result.vehicle_values["VehicleSpeed"] == 42.5
    assert result.vehicle_values["BrakeActive"] == 1
    assert "PASS" in render_dbc_verify(result)
