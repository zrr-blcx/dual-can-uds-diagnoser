from __future__ import annotations

import time

from diagnoser.emu.memory_bus import MemoryBus
from diagnoser.tools.stability import render_stability_report, run_stability


class SuccessfulClient:
    retries = 1

    def __init__(self, delay_s: float = 0.001) -> None:
        self.delay_s = delay_s

    def read_data_by_identifier(self, did: int) -> bytes:
        time.sleep(self.delay_s)
        return b"\x62" + did.to_bytes(2, "big")


def test_stability_runs_for_duration_and_samples_both_nodes() -> None:
    bus = MemoryBus("stability-run")
    clients = {
        "ecu1": SuccessfulClient(),
        "ecu2": SuccessfulClient(),
    }
    try:
        result = run_stability(
            bus,
            clients,
            duration_s=0.24,
            request_interval_ms=8,
            sample_interval_s=0.05,
            target_load=0.0,
        )
    finally:
        bus.shutdown()

    assert result.passed
    assert result.completed
    assert not result.interrupted
    assert not result.worker_errors
    assert result.elapsed_s >= 0.2
    assert result.total_requests >= 20
    assert all(metric.ok == metric.sent for metric in result.nodes.values())
    assert len(result.samples) >= 3
    assert all(set(sample.nodes) == {"ecu1", "ecu2"} for sample in result.samples)
    assert all(client.retries == 1 for client in clients.values())

    report = render_stability_report(result)
    assert "# Dual-Node UDS Stability Report" in report
    assert "Software completion: PASS" in report
    assert "UDS timeouts are recorded separately" in report


def test_stability_interrupt_returns_partial_result() -> None:
    bus = MemoryBus("stability-interrupt")

    def interrupt(sample) -> None:
        del sample
        raise KeyboardInterrupt

    try:
        result = run_stability(
            bus,
            {"ecu1": SuccessfulClient()},
            duration_s=1.0,
            request_interval_ms=5,
            sample_interval_s=0.02,
            target_load=0.0,
            progress=interrupt,
        )
    finally:
        bus.shutdown()

    assert not result.passed
    assert not result.completed
    assert result.interrupted
    assert len(result.samples) == 2
    assert result.total_requests > 0
    assert client_retry_state(result)


def client_retry_state(result) -> bool:
    return result.nodes["ecu1"].sent >= 1


def test_stability_records_timeouts_separately() -> None:
    class TimeoutClient:
        retries = 0

        def read_data_by_identifier(self, did: int) -> bytes:
            del did
            raise TimeoutError

    bus = MemoryBus("stability-timeout")
    try:
        result = run_stability(
            bus,
            {"ecu1": TimeoutClient()},
            duration_s=0.08,
            request_interval_ms=5,
            sample_interval_s=0.04,
            target_load=0.0,
        )
    finally:
        bus.shutdown()

    metric = result.nodes["ecu1"]
    assert result.passed
    assert metric.sent > 0
    assert metric.timeouts == metric.sent
    assert result.errors == 0
    assert metric.uds_timeout_rate == 1.0


def test_stability_validates_parameters() -> None:
    bus = MemoryBus("stability-validation")
    try:
        for kwargs in (
            {"duration_s": 0},
            {"sample_interval_s": 0},
            {"request_interval_ms": -1},
            {"target_load": 1.0},
        ):
            parameters = {
                "duration_s": 1.0,
                "request_interval_ms": 10.0,
                "sample_interval_s": 0.1,
                "target_load": 0.0,
                **kwargs,
            }
            try:
                run_stability(bus, {"ecu1": SuccessfulClient()}, **parameters)
            except ValueError:
                pass
            else:
                raise AssertionError(f"expected ValueError for {parameters}")
    finally:
        bus.shutdown()
