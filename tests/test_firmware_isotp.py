"""Compile and execute the portable firmware ISO-TP tests."""

from __future__ import annotations

from pathlib import Path

import pytest

from host_c_build import PROJECT_ROOT, build_host_binary, run_host_binary

INCLUDE = PROJECT_ROOT / "firmware" / "common"
SOURCES = [INCLUDE / "isotp.c"]
TEST_SOURCE = PROJECT_ROOT / "tests" / "firmware" / "isotp_test.c"


@pytest.fixture(scope="module")
def isotp_test_binary() -> Path:
    return build_host_binary("isotp_test", SOURCES, TEST_SOURCE)


def test_firmware_isotp_behavior(isotp_test_binary: Path) -> None:
    result = run_host_binary(isotp_test_binary)

    assert result.returncode == 0, f"{result.stdout}\n{result.stderr}"
    assert "firmware ISO-TP host tests: PASS" in result.stdout
