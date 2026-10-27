"""Compile and execute the portable board-internal load generator tests."""

from __future__ import annotations

from pathlib import Path

import pytest

from host_c_build import PROJECT_ROOT, build_host_binary, run_host_binary

INCLUDE = PROJECT_ROOT / "firmware" / "common"
SOURCES = [INCLUDE / "load_generator.c"]
TEST_SOURCE = PROJECT_ROOT / "tests" / "firmware" / "load_generator_test.c"


@pytest.fixture(scope="module")
def load_generator_binary() -> Path:
    return build_host_binary("load_generator_test", SOURCES, TEST_SOURCE)


def test_load_generator_counters(load_generator_binary: Path) -> None:
    result = run_host_binary(load_generator_binary)

    assert result.returncode == 0, f"{result.stdout}\n{result.stderr}"
    assert "load generator host tests: PASS" in result.stdout
