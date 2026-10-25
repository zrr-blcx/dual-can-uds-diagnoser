"""Compile and execute the append-only parameter log tests."""

from __future__ import annotations

from pathlib import Path

import pytest

from host_c_build import PROJECT_ROOT, build_host_binary, run_host_binary

INCLUDE = PROJECT_ROOT / "firmware" / "common"
SOURCES = [INCLUDE / "param_store.c", INCLUDE / "param_log.c"]
TEST_SOURCE = PROJECT_ROOT / "tests" / "firmware" / "param_log_test.c"


@pytest.fixture(scope="module")
def param_log_test_binary() -> Path:
    return build_host_binary("param_log_test", SOURCES, TEST_SOURCE)


def test_parameter_log_behavior(param_log_test_binary: Path) -> None:
    result = run_host_binary(param_log_test_binary)

    assert result.returncode == 0, f"{result.stdout}\n{result.stderr}"
    assert "parameter log host tests: PASS" in result.stdout
