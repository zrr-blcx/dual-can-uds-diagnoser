"""Compile and execute the portable stress-master helpers."""

from __future__ import annotations

from pathlib import Path

import pytest

from host_c_build import PROJECT_ROOT, build_host_binary, run_host_binary

INCLUDE = PROJECT_ROOT / "firmware" / "common"
SOURCES = [INCLUDE / "stress_master.c"]
TEST_SOURCE = PROJECT_ROOT / "tests" / "firmware" / "stress_master_test.c"


@pytest.fixture(scope="module")
def stress_master_binary() -> Path:
    return build_host_binary("stress_master_test", SOURCES, TEST_SOURCE)


def test_stress_master_helpers(stress_master_binary: Path) -> None:
    result = run_host_binary(stress_master_binary)

    assert result.returncode == 0, f"{result.stdout}\n{result.stderr}"
    assert "stress master host tests: PASS" in result.stdout
