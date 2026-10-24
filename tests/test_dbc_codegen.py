"""DBC parsing and generated C pack/unpack tests."""

from __future__ import annotations

from pathlib import Path

import cantools
import pytest

from diagnoser.dbc.codegen import DbcCodeGenerator
from host_c_build import build_host_binary, run_host_binary

PROJECT_ROOT = Path(__file__).resolve().parents[1]
DBC_PATH = PROJECT_ROOT / "dbc" / "project.dbc"
HEADER_PATH = PROJECT_ROOT / "firmware" / "common" / "project_dbc.h"
SOURCE_PATH = PROJECT_ROOT / "firmware" / "common" / "project_dbc.c"
TEST_SOURCE = PROJECT_ROOT / "tests" / "firmware" / "project_dbc_test.c"


def test_generated_c_matches_committed_files() -> None:
    generated = DbcCodeGenerator.from_file(DBC_PATH)

    assert generated.header == HEADER_PATH.read_text(encoding="utf-8")
    assert generated.source == SOURCE_PATH.read_text(encoding="utf-8")


def test_project_dbc_parses_and_roundtrips_motorola_and_intel() -> None:
    database = cantools.database.load_file(DBC_PATH)
    names = {message.name for message in database.messages}
    assert {
        "DiagnosticRequest_ECU1",
        "DiagnosticResponse_ECU1",
        "DiagnosticRequest_ECU2",
        "DiagnosticResponse_ECU2",
        "EngineData",
        "VehicleStatus",
    } <= names

    engine = database.get_message_by_name("EngineData")
    engine_data = engine.encode({"EngineSpeed": 1234.5, "CoolantTemp": 85})
    assert engine_data == bytes.fromhex("13 4A 7D 00 00 00 00 00")
    assert engine.decode(engine_data) == {
        "EngineSpeed": 1234.5,
        "CoolantTemp": 85,
    }

    vehicle = database.get_message_by_name("VehicleStatus")
    vehicle_data = vehicle.encode({"VehicleSpeed": 42.5, "BrakeActive": 1})
    assert vehicle_data == bytes.fromhex("9A 10 01 00 00 00 00 00")
    decoded_vehicle = vehicle.decode(vehicle_data)
    assert decoded_vehicle["VehicleSpeed"] == pytest.approx(42.5)
    assert decoded_vehicle["BrakeActive"] == 1


@pytest.fixture(scope="module")
def project_dbc_binary() -> Path:
    return build_host_binary(
        "project_dbc_test",
        [SOURCE_PATH],
        TEST_SOURCE,
    )


def test_generated_c_pack_unpack(project_dbc_binary: Path) -> None:
    result = run_host_binary(project_dbc_binary)

    assert result.returncode == 0, f"{result.stdout}\n{result.stderr}"
    assert "project DBC host tests: PASS" in result.stdout
