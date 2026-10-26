"""Compile and execute the hardware test fault hook behavior."""

from __future__ import annotations

from pathlib import Path

import pytest

from host_c_build import PROJECT_ROOT, build_host_binary, run_host_binary

INCLUDE = PROJECT_ROOT / "firmware" / "common"
SOURCES = [INCLUDE / "uds_fault_test.c"]
TEST_SOURCE = PROJECT_ROOT / "tests" / "firmware" / "uds_fault_test_test.c"


@pytest.fixture(scope="module")
def uds_fault_test_binary() -> Path:
    return build_host_binary("uds_fault_test", SOURCES, TEST_SOURCE)


def test_uds_fault_hook(uds_fault_test_binary: Path) -> None:
    result = run_host_binary(uds_fault_test_binary)

    assert result.returncode == 0, f"{result.stdout}\n{result.stderr}"
    assert "UDS fault host tests: PASS" in result.stdout
