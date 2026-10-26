"""Exercise exclusive programming-session arbitration on real ECUs."""

from __future__ import annotations

import threading
import time
from dataclasses import dataclass

from diagnoser.tools.scheduler import QueuedDualNodeScheduler, SchedulerNode
from diagnoser.uds.client import UdsClient
from diagnoser.uds.dids import DID_ECU_NAME


@dataclass(frozen=True)
class SessionConflictResult:
    first_node: str
    second_node: str
    first_session_entered: bool
    first_restored: bool
    second_skipped: bool
    second_job_executed: bool
    second_recovered: bool
    first_job_status: str
    conflict_status: str
    recovery_status: str
    error: str | None = None

    @property
    def passed(self) -> bool:
        return (
            self.first_session_entered
            and self.first_restored
            and self.second_skipped
            and not self.second_job_executed
            and self.second_recovered
            and self.first_job_status == "ok"
            and self.conflict_status == "skipped"
            and self.recovery_status == "ok"
            and self.error is None
        )


def run_session_conflict(
    clients: dict[str, UdsClient],
    *,
    hold_seconds: float = 0.4,
    wait_timeout_seconds: float = 0.1,
) -> SessionConflictResult:
    if len(clients) < 2:
        raise ValueError("at least two diagnostic clients are required")
    if hold_seconds <= 0:
        raise ValueError("hold_seconds must be positive")
    if wait_timeout_seconds <= 0:
        raise ValueError("wait_timeout_seconds must be positive")
    if wait_timeout_seconds >= hold_seconds:
        raise ValueError("wait_timeout_seconds must be shorter than hold_seconds")

    names = list(clients)
    first_node = names[0]
    second_node = names[1]
    first_client = clients[first_node]
    second_client = clients[second_node]

    first_entered = threading.Event()
    second_executed = threading.Event()
    first_restored = threading.Event()
    scheduler = QueuedDualNodeScheduler(
        [SchedulerNode(name=name) for name in names],
        default_retries=0,
        session_wait_timeout_s=wait_timeout_seconds,
    )

    def hold_programming_session() -> None:
        try:
            first_client.change_session(0x02)
            first_entered.set()
            time.sleep(hold_seconds)
        finally:
            try:
                first_client.change_session(0x01)
                first_client.read_data_by_identifier(DID_ECU_NAME)
                first_restored.set()
            finally:
                if not first_restored.is_set():
                    first_client.change_session(0x01)

    def conflicting_programming_session() -> None:
        second_executed.set()
        second_client.change_session(0x02)
        second_client.change_session(0x01)

    def recovery_read() -> None:
        second_client.change_session(0x01)
        second_client.read_data_by_identifier(DID_ECU_NAME)

    first_future = scheduler.submit(
        first_node,
        hold_programming_session,
        session=0x02,
    )
    error: str | None = None
    try:
        if not first_entered.wait(timeout=5.0):
            error = "first node did not enter programming session"
            second_future = None
        else:
            second_future = scheduler.submit(
                second_node,
                conflicting_programming_session,
                session=0x02,
            )

        first_outcome = first_future.result(timeout=10.0)
        conflict_outcome = (
            second_future.result(timeout=10.0) if second_future is not None else None
        )

        if first_entered.is_set() and conflict_outcome is not None:
            recovery_outcome = scheduler.submit(
                second_node,
                recovery_read,
            ).result(timeout=5.0)
        else:
            recovery_outcome = None
    except Exception as exc:  # noqa: BLE001 - reported in the result
        error = f"{type(exc).__name__}: {exc}"
        first_outcome = None
        conflict_outcome = None
        recovery_outcome = None
    finally:
        scheduler.close()

    return SessionConflictResult(
        first_node=first_node,
        second_node=second_node,
        first_session_entered=first_entered.is_set(),
        first_restored=first_restored.is_set(),
        second_skipped=(
            conflict_outcome is not None and conflict_outcome.status == "skipped"
        ),
        second_job_executed=second_executed.is_set(),
        second_recovered=(
            recovery_outcome is not None and recovery_outcome.status == "ok"
        ),
        first_job_status=first_outcome.status if first_outcome is not None else "error",
        conflict_status=(
            conflict_outcome.status if conflict_outcome is not None else "error"
        ),
        recovery_status=(
            recovery_outcome.status if recovery_outcome is not None else "error"
        ),
        error=error,
    )


def render_session_conflict(result: SessionConflictResult) -> str:
    lines = [
        "# Programming Session Conflict Test",
        "",
        f"- First node: {result.first_node}",
        f"- Waiting node: {result.second_node}",
        f"- Outcome: {'PASS' if result.passed else 'FAIL'}",
        "",
        "| Check | Result |",
        "| --- | --- |",
        f"| {result.first_node} entered `0x02` | "
        f"{'PASS' if result.first_session_entered else 'FAIL'} |",
        f"| {result.second_node} job skipped | "
        f"{'PASS' if result.second_skipped else 'FAIL'} |",
        f"| Conflicting job never executed | "
        f"{'PASS' if not result.second_job_executed else 'FAIL'} |",
        f"| {result.first_node} restored Default | "
        f"{'PASS' if result.first_restored else 'FAIL'} |",
        f"| {result.second_node} recovered | "
        f"{'PASS' if result.second_recovered else 'FAIL'} |",
        "",
        f"- First job status: `{result.first_job_status}`",
        f"- Conflict status: `{result.conflict_status}`",
        f"- Recovery status: `{result.recovery_status}`",
        f"- Error: {result.error or '-'}",
        "",
    ]
    return "\n".join(lines)
