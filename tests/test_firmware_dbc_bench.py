"""Compile and execute the DBC bench frame-scheduling tests."""

from __future__ import annotations

from pathlib import Path

import pytest

from host_c_build import PROJECT_ROOT, build_host_binary, run_host_binary

INCLUDE = PROJECT_ROOT / "firmware" / "common"
SOURCES = [INCLUDE / "project_dbc.c", INCLUDE / "dbc_bench.c"]
TEST_SOURCE = PROJECT_ROOT / "tests" / "firmware" / "dbc_bench_test.c"


@pytest.fixture(scope="module")
def dbc_bench_binary() -> Path:
    return build_host_binary("dbc_bench_test", SOURCES, TEST_SOURCE)


def test_dbc_bench_frames_and_period(dbc_bench_binary: Path) -> None:
    result = run_host_binary(dbc_bench_binary)

    assert result.returncode == 0, f"{result.stdout}\n{result.stderr}"
    assert "DBC bench host tests: PASS" in result.stdout
