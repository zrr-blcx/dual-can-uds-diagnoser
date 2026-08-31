"""Stress test runner for dual-node diagnostic traffic."""

from __future__ import annotations

import time
from dataclasses import dataclass, field

import can

from diagnoser.tools.bus_load import BusLoadSimulator
from diagnoser.uds.client import UdsClient, UdsError


@dataclass
class NodeMetrics:
    sent: int = 0
    ok: int = 0
    timeouts: int = 0
    errors: int = 0
    rtts: list[float] = field(default_factory=list)

    @property
    def loss_rate(self) -> float:
        if self.sent == 0:
            return 0.0
        return (self.timeouts + self.errors) / self.sent


@dataclass
class StressResult:
    total_requests: int
    success: int
    timeouts: int
    errors: int
    duration_s: float
    measured_load: float
    target_load: float
    nodes: dict[str, NodeMetrics]

    @property
    def loss_rate(self) -> float:
        if self.total_requests == 0:
            return 0.0
        return (self.timeouts + self.errors) / self.total_requests


def run_stress(
    bus: can.BusABC,
    clients: dict[str, UdsClient],
    did: int = 0xF190,
    requests: int = 1000,
    target_load: float = 0.9,
    baudrate: int = 500000,
) -> StressResult:
    load_sim = BusLoadSimulator(
        bus,
        arbitration_id=0x100,
        dlc=8,
        baudrate=baudrate,
        target_load=target_load,
    )
    metrics = {name: NodeMetrics() for name in clients}
    load_sim.start()
    started = time.perf_counter()
    try:
        names = list(clients)
        for index in range(requests):
            node = names[index % len(names)]
            metric = metrics[node]
            metric.sent += 1
            started_request = time.perf_counter()
            try:
                clients[node].read_data_by_identifier(did)
                metric.ok += 1
            except TimeoutError:
                metric.timeouts += 1
            except UdsError:
                metric.errors += 1
            finally:
                metric.rtts.append(time.perf_counter() - started_request)
    finally:
        load_sim.stop()

    duration = time.perf_counter() - started
    total_ok = sum(m.ok for m in metrics.values())
    total_timeouts = sum(m.timeouts for m in metrics.values())
    total_errors = sum(m.errors for m in metrics.values())
    return StressResult(
        total_requests=requests,
        success=total_ok,
        timeouts=total_timeouts,
        errors=total_errors,
        duration_s=duration,
        measured_load=load_sim.measured_load(),
        target_load=target_load,
        nodes=metrics,
    )
