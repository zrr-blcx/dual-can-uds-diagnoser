"""Bus-Off injection and recovery test runner for virtual CAN validation."""

from __future__ import annotations

import time
from collections.abc import Callable
from dataclasses import dataclass

from diagnoser.emu.virtual_ecu import CAN_HEALTH_DID
from diagnoser.tools.fault_injection import FaultInjector
from diagnoser.uds.client import UdsClient
from diagnoser.uds.errors import UdsError


@dataclass(frozen=True)
class BusOffCycleResult:
    cycle: int
    offline_observed: bool
    recovered: bool
    recovery_time_s: float | None


@dataclass(frozen=True)
class BusOffTestResult:
    node: str
    cycles_requested: int
    cycles: tuple[BusOffCycleResult, ...]
    health: bytes | None

    @property
    def passed(self) -> bool:
        return (
            len(self.cycles) == self.cycles_requested
            and all(cycle.offline_observed and cycle.recovered for cycle in self.cycles)
        )


def run_bus_off_test(
    client: UdsClient,
    injector: FaultInjector,
    node: str,
    cycles: int = 3,
    seconds: float = 0.2,
    recovery_timeout_s: float = 3.0,
    poll_interval_s: float = 0.02,
    sleep: Callable[[float], None] = time.sleep,
    clock: Callable[[], float] = time.perf_counter,
) -> BusOffTestResult:
    if cycles <= 0:
        raise ValueError("cycles must be positive")
    if seconds < 0:
        raise ValueError("seconds must be non-negative")
    if recovery_timeout_s <= 0:
        raise ValueError("recovery_timeout_s must be positive")

    probe_timeout_s = max(0.02, min(0.2, max(seconds / 2, 0.02)))
    original_timeout = client.timeout
    results: list[BusOffCycleResult] = []
    try:
        client.timeout = probe_timeout_s
        for cycle in range(1, cycles + 1):
            injector.trigger_bus_off(node, seconds)
            started = clock()
            offline_observed = not _probe(client)

            recovered = False
            recovery_time_s: float | None = None
            deadline = clock() + recovery_timeout_s
            while clock() < deadline:
                if _probe(client):
                    recovered = True
                    recovery_time_s = clock() - started
                    break
                sleep(poll_interval_s)

            results.append(
                BusOffCycleResult(
                    cycle=cycle,
                    offline_observed=offline_observed,
                    recovered=recovered,
                    recovery_time_s=recovery_time_s,
                )
            )
            if not recovered:
                break
    finally:
        client.timeout = original_timeout

    health: bytes | None = None
    if results and results[-1].recovered:
        try:
            health = client.read_data_by_identifier(CAN_HEALTH_DID)[2:]
        except (TimeoutError, UdsError):
            pass

    return BusOffTestResult(
        node=node,
        cycles_requested=cycles,
        cycles=tuple(results),
        health=health,
    )


def render_bus_off_report(result: BusOffTestResult) -> str:
    lines = [
        "# Bus-Off Recovery Test (Virtual CAN)",
        "",
        f"- Node: {result.node}",
        f"- Requested cycles: {result.cycles_requested}",
        f"- Completed cycles: {len(result.cycles)}",
        f"- Outcome: {'PASS' if result.passed else 'FAIL'}",
        "",
        "| Cycle | Offline observed | Recovered | Recovery time |",
        "| ---: | --- | --- | ---: |",
    ]
    for cycle in result.cycles:
        recovery = (
            f"{cycle.recovery_time_s * 1000:.1f} ms"
            if cycle.recovery_time_s is not None
            else "-"
        )
        lines.append(
            f"| {cycle.cycle} | {_yes_no(cycle.offline_observed)} | "
            f"{_yes_no(cycle.recovered)} | {recovery} |"
        )

    lines.extend(
        [
            "",
            "## CAN Health At End",
            "",
        ]
    )
    if result.health is None:
        lines.append("- Health snapshot unavailable.")
    else:
        state_names = {0: "error-active", 1: "error-passive", 2: "bus-off"}
        lines.extend(
            [
                f"- State: {state_names.get(result.health[0], 'unknown')}",
                f"- Bus-Off events: {result.health[3]}",
                f"- Recovery successes: {result.health[4]}",
                f"- Recovery failures: {result.health[5]}",
                f"- Recorded CAN errors: {result.health[6]}",
            ]
        )

    lines.extend(
        [
            "",
            "> This is a VirtualBus validation result. Real relay-triggered",
            "> Bus-Off recovery remains a hardware acceptance item.",
            "",
        ]
    )
    return "\n".join(lines)


def _probe(client: UdsClient) -> bool:
    try:
        client.tester_present()
        return True
    except TimeoutError:
        return False


def _yes_no(value: bool) -> str:
    return "Yes" if value else "No"
