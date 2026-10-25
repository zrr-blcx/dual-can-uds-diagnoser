"""Compile and execute the portable F407 parameter-store tests."""

from __future__ import annotations

from pathlib import Path

import pytest

from host_c_build import PROJECT_ROOT, build_host_binary, run_host_binary

INCLUDE = PROJECT_ROOT / "firmware" / "common"
SOURCE = INCLUDE / "param_store.c"
TEST_SOURCE = PROJECT_ROOT / "tests" / "firmware" / "param_store_test.c"


@pytest.fixture(scope="module")
def param_store_test_binary() -> Path:
    return build_host_binary("param_store_test", [SOURCE], TEST_SOURCE)


def test_parameter_store_behavior(param_store_test_binary: Path) -> None:
    result = run_host_binary(param_store_test_binary)

    assert result.returncode == 0, f"{result.stdout}\n{result.stderr}"
    assert "parameter store host tests: PASS" in result.stdout
