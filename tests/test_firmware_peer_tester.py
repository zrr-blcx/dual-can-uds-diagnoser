"""Compile and execute the portable peer diagnostic tester tests."""

from __future__ import annotations

from pathlib import Path

import pytest

from host_c_build import PROJECT_ROOT, build_host_binary, run_host_binary

INCLUDE = PROJECT_ROOT / "firmware" / "common"
SOURCES = [INCLUDE / "peer_tester.c"]
TEST_SOURCE = PROJECT_ROOT / "tests" / "firmware" / "peer_tester_test.c"


@pytest.fixture(scope="module")
def peer_tester_binary() -> Path:
    return build_host_binary("peer_tester_test", SOURCES, TEST_SOURCE)


def test_peer_tester_helpers(peer_tester_binary: Path) -> None:
    result = run_host_binary(peer_tester_binary)

    assert result.returncode == 0, f"{result.stdout}\n{result.stderr}"
    assert "peer tester host tests: PASS" in result.stdout
