"""Compile and execute the background frame sequence probe tests."""

from __future__ import annotations

from pathlib import Path

import pytest

from host_c_build import PROJECT_ROOT, build_host_binary, run_host_binary

INCLUDE = PROJECT_ROOT / "firmware" / "common"
SOURCES = [INCLUDE / "background_probe.c"]
TEST_SOURCE = PROJECT_ROOT / "tests" / "firmware" / "background_probe_test.c"


@pytest.fixture(scope="module")
def background_probe_binary() -> Path:
    return build_host_binary("background_probe_test", SOURCES, TEST_SOURCE)


def test_background_probe_counts_gaps_and_wraparound(
    background_probe_binary: Path,
) -> None:
    result = run_host_binary(background_probe_binary)

    assert result.returncode == 0, f"{result.stdout}\n{result.stderr}"
    assert "background probe host tests: PASS" in result.stdout
