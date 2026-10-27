"""Duration-based dual-node diagnostic stability runner."""

from __future__ import annotations

import threading
import time
from collections.abc import Callable
from dataclasses import dataclass, field

import can

from diagnoser.tools.bus_load import BusLoadSimulator
from diagnoser.tools.stress import BACKGROUND_FRAME_ID, NodeMetrics, record_error
from diagnoser.uds.client import UdsClient
from diagnoser.uds.errors import UdsError


@dataclass(frozen=True)
class StabilityNodeSnapshot:
    sent: int
    ok: int
    timeouts: int
    errors: int
    avg_rtt_s: float
    max_rtt_s: float

    @property
    def uds_timeout_rate(self) -> float:
        return self.timeouts / self.sent if self.sent else 0.0


@dataclass(frozen=True)
class StabilitySample:
    elapsed_s: float
    generator_load: float
    nodes: dict[str, StabilityNodeSnapshot]


@dataclass
class StabilityResult:
    requested_duration_s: float
    elapsed_s: float
    request_interval_ms: float
    sample_interval_s: float
    target_load: float
    generator_load: float
    completed: bool
    interrupted: bool
    worker_errors: list[str]
    samples: list[StabilitySample] = field(default_factory=list)
    nodes: dict[str, NodeMetrics] = field(default_factory=dict)

    @property
    def total_requests(self) -> int:
        return sum(metric.sent for metric in self.nodes.values())

    @property
    def success(self) -> int:
        return sum(metric.ok for metric in self.nodes.values())

    @property
    def timeouts(self) -> int:
        return sum(metric.timeouts for metric in self.nodes.values())

    @property
    def errors(self) -> int:
        return sum(metric.errors for metric in self.nodes.values())

    @property
    def passed(self) -> bool:
        return (
            self.completed
            and not self.interrupted
            and not self.worker_errors
            and all(metric.sent > 0 for metric in self.nodes.values())
        )


def run_stability(
    bus: can.BusABC,
    clients: dict[str, UdsClient],
    *,
    duration_s: float = 4 * 60 * 60,
    request_interval_ms: float = 100.0,
    sample_interval_s: float = 60.0,
    did: int = 0xF190,
    target_load: float = 0.9,
    baudrate: int = 500000,
    progress: Callable[[StabilitySample], None] | None = None,
) -> StabilityResult:
    """Run read-only UDS traffic for a fixed duration.

    ``target_load`` controls only the host-side background generator. Use zero
    when an STM32 stress image provides the load, then attach the independently
    measured load and reset/watchdog evidence to the final report.
    """

    if duration_s <= 0:
        raise ValueError("duration_s must be positive")
    if request_interval_ms < 0:
        raise ValueError("request_interval_ms must not be negative")
    if sample_interval_s <= 0:
        raise ValueError("sample_interval_s must be positive")
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
    metrics_lock = threading.Lock()
    stop_event = threading.Event()
    worker_errors: list[str] = []
    worker_errors_lock = threading.Lock()
    started = time.perf_counter()
    deadline = started + duration_s
    interrupted = False

    if load_sim is not None:
        load_sim.start()

    def run_node(name: str, client: UdsClient) -> None:
        metric = metrics[name]
        original_retries = client.retries
        client.retries = 0
        interval_s = request_interval_ms / 1000.0
        next_request = time.perf_counter()
        last_started: float | None = None
        try:
            while not stop_event.is_set():
                now = time.perf_counter()
                if now >= deadline:
                    break
                wake_at = min(next_request, deadline)
                delay = wake_at - now
                if delay > 0 and stop_event.wait(delay):
                    break

                request_started = time.perf_counter()
                if request_started >= deadline:
                    break
                with metrics_lock:
                    if last_started is not None:
                        metric.request_intervals_s.append(
                            request_started - last_started
                        )
                    last_started = request_started
                    metric.sent += 1

                next_request = max(
                    request_started + interval_s,
                    next_request + interval_s,
                )
                exchange_started = time.perf_counter()
                try:
                    client.read_data_by_identifier(did)
                    outcome = "ok"
                    error: Exception | None = None
                except TimeoutError:
                    outcome = "timeout"
                    error = None
                except UdsError as exc:
                    outcome = "error"
                    error = exc
                except Exception as exc:  # noqa: BLE001 - reported in the result
                    outcome = "error"
                    error = exc

                rtt = time.perf_counter() - exchange_started
                with metrics_lock:
                    if outcome == "ok":
                        metric.ok += 1
                    elif outcome == "timeout":
                        metric.timeouts += 1
                    else:
                        metric.errors += 1
                        if error is not None:
                            record_error(metric, error)
                    metric.rtts.append(rtt)
        except Exception as exc:  # noqa: BLE001 - keep the soak report usable
            with worker_errors_lock:
                worker_errors.append(f"{name}: {type(exc).__name__}: {exc}")
        finally:
            client.retries = original_retries

    workers = {
        name: threading.Thread(
            target=run_node,
            args=(name, client),
            name=f"stability-{name}",
            daemon=True,
        )
        for name, client in clients.items()
    }
    samples: list[StabilitySample] = []

    def take_sample() -> StabilitySample:
        with metrics_lock:
            snapshots = {
                name: _snapshot(metric) for name, metric in metrics.items()
            }
        return StabilitySample(
            elapsed_s=max(0.0, time.perf_counter() - started),
            generator_load=(
                load_sim.measured_load() if load_sim is not None else 0.0
            ),
            nodes=snapshots,
        )

    def emit_sample(
        sample: StabilitySample,
        *,
        notify: bool = True,
    ) -> None:
        samples.append(sample)
        if notify and progress is not None:
            progress(sample)

    try:
        for worker in workers.values():
            worker.start()

        next_sample = started + sample_interval_s
        while not stop_event.is_set():
            now = time.perf_counter()
            if now >= deadline:
                break
            wake_at = min(next_sample, deadline)
            delay = wake_at - now
            if delay > 0 and stop_event.wait(delay):
                break
            if time.perf_counter() >= deadline:
                break
            emit_sample(take_sample())
            next_sample += sample_interval_s
            if next_sample <= time.perf_counter():
                next_sample = time.perf_counter() + sample_interval_s
    except KeyboardInterrupt:
        interrupted = True
        stop_event.set()
    finally:
        stop_event.set()
        for worker in workers.values():
            worker.join(timeout=5.0)
        if load_sim is not None:
            load_sim.stop()

    unfinished = [name for name, worker in workers.items() if worker.is_alive()]
    if unfinished:
        worker_errors.append(
            "workers did not stop: " + ", ".join(sorted(unfinished))
        )
    emit_sample(take_sample(), notify=not interrupted)
    elapsed = max(0.0, time.perf_counter() - started)
    completed = not interrupted
    return StabilityResult(
        requested_duration_s=duration_s,
        elapsed_s=elapsed,
        request_interval_ms=request_interval_ms,
        sample_interval_s=sample_interval_s,
        target_load=target_load,
        generator_load=(
            load_sim.measured_load() if load_sim is not None else 0.0
        ),
        completed=completed,
        interrupted=interrupted,
        worker_errors=worker_errors,
        samples=samples,
        nodes=metrics,
    )


def _snapshot(metric: NodeMetrics) -> StabilityNodeSnapshot:
    return StabilityNodeSnapshot(
        sent=metric.sent,
        ok=metric.ok,
        timeouts=metric.timeouts,
        errors=metric.errors,
        avg_rtt_s=metric.avg_rtt_s,
        max_rtt_s=metric.max_rtt_s,
    )


def render_stability_report(result: StabilityResult) -> str:
    status = "PASS" if result.passed else "FAIL"
    lines = [
        "# Dual-Node UDS Stability Report",
        "",
        "## Summary",
        "",
        f"- Software completion: {status}",
        f"- Requested duration: {result.requested_duration_s / 3600:.3f} h",
        f"- Elapsed duration: {result.elapsed_s / 3600:.3f} h",
        f"- Request interval: {result.request_interval_ms:.1f} ms",
        f"- Sample interval: {result.sample_interval_s:.1f} s",
        f"- Host generator target load: {result.target_load * 100:.1f}%",
        f"- Host generator measured load: {result.generator_load * 100:.1f}%",
        f"- Interrupted by operator: {'Yes' if result.interrupted else 'No'}",
        f"- Total requests: {result.total_requests}",
        f"- Success: {result.success}",
        f"- Timeouts: {result.timeouts}",
        f"- Errors: {result.errors}",
        "",
        "## Per Node",
        "",
        "| Node | Sent | OK | Timeout | Error | Timeout rate | Avg RTT | Max RTT |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for name, metric in sorted(result.nodes.items()):
        lines.append(
            f"| {name} | {metric.sent} | {metric.ok} | "
            f"{metric.timeouts} | {metric.errors} | "
            f"{metric.uds_timeout_rate * 100:.4f}% | "
            f"{metric.avg_rtt_s * 1000:.2f} ms | "
            f"{metric.max_rtt_s * 1000:.2f} ms |"
        )

    node_names = sorted(result.nodes)
    node_headers = [f"{name} sent/ok/timeout/error" for name in node_names]
    lines.extend(
        [
            "",
            "## Samples",
            "",
            "| Elapsed h | Generator load | " + " | ".join(node_headers) + " |",
            "| ---: | ---: | " + " | ".join("---:" for _ in node_names) + " |",
        ]
    )
    for sample in result.samples:
        node_values = []
        for name in node_names:
            node = sample.nodes[name]
            node_values.append(
                f"{node.sent}/{node.ok}/{node.timeouts}/{node.errors}"
            )
        lines.append(
            f"| {sample.elapsed_s / 3600:.3f} | "
            f"{sample.generator_load * 100:.1f}% | "
            + " | ".join(node_values)
            + " |"
        )

    lines.extend(
        [
            "",
            "## Acceptance Boundary",
            "",
            "- PASS means the software ran for the requested duration and both "
            "node workers produced requests without an unhandled worker error.",
            "- UDS timeouts are recorded separately and are not physical CAN "
            "frame loss.",
            "- Controller resets, watchdog resets, and Bus-Off state require an "
            "independent health snapshot or ST-Link record.",
            "- With a board-internal stress image, use `--load 0` and attach "
            "the independently measured bus load to this report.",
        ]
    )
    if result.worker_errors:
        lines.extend(["", "## Worker Errors", ""])
        lines.extend(f"- {error}" for error in result.worker_errors)
    return "\n".join(lines) + "\n"
