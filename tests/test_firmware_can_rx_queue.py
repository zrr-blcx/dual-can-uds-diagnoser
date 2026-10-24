"""Compile and execute the portable CAN receive queue tests."""

from __future__ import annotations

from pathlib import Path

import pytest

from host_c_build import PROJECT_ROOT, build_host_binary, run_host_binary

INCLUDE = PROJECT_ROOT / "firmware" / "common"
SOURCES = [INCLUDE / "can_rx_queue.c"]
TEST_SOURCE = PROJECT_ROOT / "tests" / "firmware" / "can_rx_queue_test.c"


@pytest.fixture(scope="module")
def can_rx_queue_test_binary() -> Path:
    return build_host_binary("can_rx_queue_test", SOURCES, TEST_SOURCE)


def test_firmware_can_rx_queue(can_rx_queue_test_binary: Path) -> None:
    result = run_host_binary(can_rx_queue_test_binary)

    assert result.returncode == 0, f"{result.stdout}\n{result.stderr}"
    assert "firmware CAN RX queue tests: PASS" in result.stdout
