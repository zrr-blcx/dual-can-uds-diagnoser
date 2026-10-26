"""Stress test runner for dual-node diagnostic traffic."""

from __future__ import annotations

import threading
import time
from collections import defaultdict
from dataclasses import dataclass, field

import can

from diagnoser.tools.bus_load import BusLoadSimulator
from diagnoser.uds.client import UdsClient
from diagnoser.uds.errors import UdsError

BACKGROUND_FRAME_ID = 0x7F0


@dataclass
class NodeMetrics:
    sent: int = 0
    ok: int = 0
    timeouts: int = 0
    errors: int = 0
    rtts: list[float] = field(default_factory=list)
    request_intervals_s: list[float] = field(default_factory=list)
    errors_by_type: dict[str, int] = field(
        default_factory=lambda: defaultdict(int)
    )
    error_samples: list[str] = field(default_factory=list)

    @property
    def success(self) -> int:
        return self.ok

    @property
    def uds_timeout_rate(self) -> float:
        if self.sent == 0:
            return 0.0
        return self.timeouts / self.sent

    @property
    def failure_rate(self) -> float:
        if self.sent == 0:
            return 0.0
        return (self.timeouts + self.errors) / self.sent

    @property
    def avg_rtt_s(self) -> float:
        return sum(self.rtts) / len(self.rtts) if self.rtts else 0.0

    @property
    def max_rtt_s(self) -> float:
        return max(self.rtts, default=0.0)


@dataclass
class StressResult:
    total_requests: int
    success: int
    timeouts: int
    errors: int
    duration_s: float
    measured_load: float
    target_load: float
    requests_per_node: int
    request_interval_ms: float
    nodes: dict[str, NodeMetrics]

    @property
    def uds_timeout_rate(self) -> float:
        if self.total_requests == 0:
            return 0.0
        return self.timeouts / self.total_requests

    @property
    def failure_rate(self) -> float:
        if self.total_requests == 0:
            return 0.0
        return (self.timeouts + self.errors) / self.total_requests


def run_stress(
    bus: can.BusABC,
    clients: dict[str, UdsClient],
    did: int = 0xF190,
    requests_per_node: int = 1000,
    request_interval_ms: float = 100.0,
    target_load: float = 0.9,
    baudrate: int = 500000,
) -> StressResult:
    if requests_per_node <= 0:
        raise ValueError("requests_per_node must be positive")
    if request_interval_ms < 0:
        raise ValueError("request_interval_ms must not be negative")
    if not clients:
        raise ValueError("at least one diagnostic client is required")
    if not 0.0 <= target_load < 1.0:
        raise ValueError("target_load must be in [0, 1)")

    load_sim = (
        BusLoadSimulator(
            bus,
            arbitration_id=BACKGROUND_FRAME_ID,
            dlc=8,
            baudrate=baudrate,
            target_load=target_load,
            sequence_prefix=True,
        )
        if target_load > 0.0
        else None
    )
    metrics = {name: NodeMetrics() for name in clients}
    if load_sim is not None:
        load_sim.start()
    started = time.perf_counter()

    def run_node(name: str, client: UdsClient) -> None:
        metric = metrics[name]
        original_retries = client.retries
        client.retries = 0
        interval_s = request_interval_ms / 1000.0
        next_request = time.perf_counter()
        last_started: float | None = None
        try:
            for _ in range(requests_per_node):
                now = time.perf_counter()
                if now < next_request:
                    time.sleep(next_request - now)
                request_started = time.perf_counter()
                if last_started is not None:
                    metric.request_intervals_s.append(request_started - last_started)
                last_started = request_started
                next_request += interval_s
                metric.sent += 1

                started_request = time.perf_counter()
                try:
                    client.read_data_by_identifier(did)
                    metric.ok += 1
                except TimeoutError:
                    metric.timeouts += 1
                except UdsError as exc:
                    metric.errors += 1
                    record_error(metric, exc)
                except Exception as exc:  # noqa: BLE001 - counted as a failed request
                    metric.errors += 1
                    record_error(metric, exc)
                finally:
                    metric.rtts.append(time.perf_counter() - started_request)
        finally:
            client.retries = original_retries

    workers = {
        name: threading.Thread(
            target=run_node,
            args=(name, client),
            name=f"stress-{name}",
        )
        for name, client in clients.items()
    }
    try:
        for worker in workers.values():
            worker.start()
        for worker in workers.values():
            worker.join()
    finally:
        if load_sim is not None:
            load_sim.stop()

    duration = time.perf_counter() - started
    total_ok = sum(m.ok for m in metrics.values())
    total_timeouts = sum(m.timeouts for m in metrics.values())
    total_errors = sum(m.errors for m in metrics.values())
    return StressResult(
        total_requests=requests_per_node * len(clients),
        success=total_ok,
        timeouts=total_timeouts,
        errors=total_errors,
        duration_s=duration,
        measured_load=load_sim.measured_load() if load_sim is not None else 0.0,
        target_load=target_load,
        requests_per_node=requests_per_node,
        request_interval_ms=request_interval_ms,
        nodes=metrics,
    )


def record_error(metric: NodeMetrics, exc: Exception) -> None:
    error_type = type(exc).__name__
    metric.errors_by_type[error_type] += 1
    if len(metric.error_samples) < 5:
        metric.error_samples.append(f"{error_type}: {exc}")
