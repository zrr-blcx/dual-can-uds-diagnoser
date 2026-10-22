"""Compile and execute the portable firmware UDS server tests."""

from __future__ import annotations

from pathlib import Path

import pytest

from host_c_build import PROJECT_ROOT, build_host_binary, run_host_binary

INCLUDE = PROJECT_ROOT / "firmware" / "common"
SOURCES = [
    INCLUDE / "uds_server.c",
    INCLUDE / "uds_reference_routines.c",
    INCLUDE / "uds_did_db.c",
    INCLUDE / "uds_ecu.c",
]
TEST_SOURCE = PROJECT_ROOT / "tests" / "firmware" / "uds_server_test.c"


@pytest.fixture(scope="module")
def uds_server_test_binary() -> Path:
    return build_host_binary("uds_server_test", SOURCES, TEST_SOURCE)


def test_firmware_uds_server_behavior(uds_server_test_binary: Path) -> None:
    result = run_host_binary(uds_server_test_binary)

    assert result.returncode == 0, f"{result.stdout}\n{result.stderr}"
    assert "firmware UDS host tests: PASS" in result.stdout
