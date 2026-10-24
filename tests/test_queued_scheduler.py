"""Queued dual-node scheduler behavior and full-chain integration tests."""

from __future__ import annotations

import threading
import time

from diagnoser.emu.memory_bus import MemoryBus
from diagnoser.emu.virtual_ecu import VirtualEcu
from diagnoser.tools.scheduler import (
    QueuedDualNodeScheduler,
    SchedulerNode,
)
from diagnoser.tools.uds_week5 import render_uds_week5, run_uds_week5
from diagnoser.transport.isotp import IsoTpTransport
from diagnoser.uds.client import UdsClient


def _scheduler(node_names: tuple[str, ...] = ("ecu1", "ecu2"), **kwargs):
    return QueuedDualNodeScheduler(
        [SchedulerNode(name=name) for name in node_names],
        **kwargs,
    )


def test_per_node_queues_run_independently() -> None:
    scheduler = _scheduler(default_retries=0)
    first_started = threading.Event()
    second_started = threading.Event()

    def first_job() -> str:
        first_started.set()
        assert second_started.wait(1.0)
        return "first"

    def second_job() -> str:
        second_started.set()
        assert first_started.wait(1.0)
        return "second"

    try:
        first = scheduler.submit("ecu1", first_job)
        second = scheduler.submit("ecu2", second_job)
        first_result = first.result(timeout=2.0)
        second_result = second.result(timeout=2.0)
    finally:
        scheduler.close()

    assert first_result.status == "ok"
    assert first_result.value == "first"
    assert second_result.status == "ok"
    assert second_result.value == "second"


def test_jobs_on_same_node_are_serialized() -> None:
    scheduler = _scheduler(node_names=("ecu1",), default_retries=0)
    lock = threading.Lock()
    active = 0
    max_active = 0

    def job() -> None:
        nonlocal active, max_active
        with lock:
            active += 1
            max_active = max(max_active, active)
        time.sleep(0.02)
        with lock:
            active -= 1

    try:
        futures = [scheduler.submit("ecu1", job) for _ in range(4)]
        outcomes = [future.result(timeout=2.0) for future in futures]
    finally:
        scheduler.close()

    assert all(outcome.status == "ok" for outcome in outcomes)
    assert max_active == 1
    stats = scheduler.stats()
    assert stats["requests"]["ecu1"] == 4
    assert stats["ok"]["ecu1"] == 4


def test_timeout_retry_then_success() -> None:
    scheduler = _scheduler(node_names=("ecu1",), default_retries=0)
    calls = 0

    def flaky() -> str:
        nonlocal calls
        calls += 1
        if calls < 3:
            raise TimeoutError("temporary timeout")
        return "recovered"

    try:
        outcome = scheduler.submit("ecu1", flaky, retries=2).result(timeout=2.0)
    finally:
        scheduler.close()

    assert outcome.status == "ok"
    assert outcome.attempts == 3
    assert outcome.value == "recovered"
    assert calls == 3


def test_timeout_and_error_outcomes_are_reported() -> None:
    scheduler = _scheduler(node_names=("ecu1",), default_retries=0)

    def always_timeout() -> None:
        raise TimeoutError("no response")

    def always_error() -> None:
        raise ValueError("bad request")

    try:
        timeout = scheduler.submit(
            "ecu1", always_timeout, retries=1
        ).result(timeout=2.0)
        error = scheduler.submit(
            "ecu1", always_error, retries=2
        ).result(timeout=2.0)
    finally:
        scheduler.close()

    assert timeout.status == "timeout"
    assert timeout.attempts == 2
    assert "no response" in (timeout.error or "")
    assert error.status == "error"
    assert error.attempts == 1
    assert "ValueError" in (error.error or "")


def test_exclusive_programming_session_skips_after_wait_timeout() -> None:
    scheduler = _scheduler(session_wait_timeout_s=0.02)
    started = threading.Event()

    def long_programming_job() -> str:
        started.set()
        time.sleep(0.15)
        return "programmed"

    try:
        first = scheduler.submit(
            "ecu1", long_programming_job, session=0x02
        )
        assert started.wait(1.0)

        second = scheduler.submit(
            "ecu2", lambda: "should not run", session=0x02
        )
        second_result = second.result(timeout=2.0)
        first_result = first.result(timeout=2.0)

        third_result = scheduler.submit(
            "ecu2", lambda: "after release", session=0x02
        ).result(timeout=2.0)
    finally:
        scheduler.close()

    assert first_result.status == "ok"
    assert first_result.value == "programmed"
    assert second_result.status == "skipped"
    assert second_result.error == "session conflict wait timed out"
    assert third_result.status == "ok"
    assert third_result.value == "after release"


def _build_pair(
    channel: str, name: str, request_id: int, response_id: int
) -> tuple[VirtualEcu, UdsClient, MemoryBus, MemoryBus]:
    ecu_bus = MemoryBus(channel, node_id=request_id)
    tester_bus = MemoryBus(channel, node_id=response_id)
    ecu = VirtualEcu(name, ecu_bus, request_id, response_id)
    ecu.start()
    transport = IsoTpTransport(
        tester_bus,
        txid=request_id,
        rxid=response_id,
        timeout=1.0,
    )
    client = UdsClient(transport, timeout=1.0, retries=0)
    return ecu, client, ecu_bus, tester_bus


def test_full_chain_diagnostic_scheduler_dual_virtual_ecus() -> None:
    channel = "week7-scheduler-dual"
    node1 = _build_pair(channel, "ecu1", 0x7E0, 0x7E8)
    node2 = _build_pair(channel, "ecu2", 0x7E1, 0x7E9)
    scheduler = _scheduler(default_retries=0)

    try:
        first = scheduler.submit(
            "ecu1", lambda: run_uds_week5(node1[1], "ecu1"), session=0x03
        )
        second = scheduler.submit(
            "ecu2", lambda: run_uds_week5(node2[1], "ecu2"), session=0x03
        )
        first_result = first.result(timeout=10.0)
        second_result = second.result(timeout=10.0)
    finally:
        scheduler.close()
        for ecu, _, ecu_bus, tester_bus in (node1, node2):
            ecu.stop()
            ecu_bus.shutdown()
            tester_bus.shutdown()

    assert first_result.status == "ok"
    assert second_result.status == "ok"
    assert first_result.value is not None
    assert second_result.value is not None
    assert first_result.value.passed, render_uds_week5(first_result.value)
    assert second_result.value.passed, render_uds_week5(second_result.value)
