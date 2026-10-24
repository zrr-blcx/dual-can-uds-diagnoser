"""Dual-node diagnostic request scheduler."""

from __future__ import annotations

import threading
import time
import queue
from collections import defaultdict
from concurrent.futures import Future
from dataclasses import dataclass, field
from typing import Callable, TypeVar

T = TypeVar("T")


class SessionConflictError(RuntimeError):
    pass


@dataclass
class SchedulerNode:
    name: str


@dataclass
class SchedulerStats:
    requests: dict[str, int] = field(default_factory=lambda: defaultdict(int))
    timeouts: dict[str, int] = field(default_factory=lambda: defaultdict(int))
    errors: dict[str, int] = field(default_factory=lambda: defaultdict(int))
    started_at: float = field(default_factory=time.perf_counter)
    finished_at: float | None = None

    def as_dict(self) -> dict[str, object]:
        return {
            "requests": dict(self.requests),
            "timeouts": dict(self.timeouts),
            "errors": dict(self.errors),
            "duration_s": (
                self.finished_at - self.started_at if self.finished_at is not None else None
            ),
        }


class DualNodeScheduler:
    """Serialize requests per node and protect exclusive diagnostic sessions."""

    def __init__(
        self,
        nodes: list[SchedulerNode],
        exclusive_sessions: frozenset[int] = frozenset({0x02}),
    ) -> None:
        self.nodes = {node.name: node for node in nodes}
        self.exclusive_sessions = exclusive_sessions
        self._node_locks = {name: threading.Lock() for name in self.nodes}
        self._session_lock = threading.Lock()
        self._active_sessions: dict[str, int] = {
            name: 0x01 for name in self.nodes
        }
        self._stats = SchedulerStats()

    def enter_session(self, node_name: str, session: int) -> None:
        if node_name not in self.nodes:
            raise KeyError(node_name)
        with self._session_lock:
            if session in self.exclusive_sessions:
                conflicts = [
                    name
                    for name, active in self._active_sessions.items()
                    if name != node_name and active in self.exclusive_sessions
                ]
                if conflicts:
                    raise SessionConflictError(
                        f"session 0x{session:02X} already active on {', '.join(conflicts)}"
                    )
            self._active_sessions[node_name] = session

    def leave_session(self, node_name: str) -> None:
        with self._session_lock:
            self._active_sessions[node_name] = 0x01

    def active_session(self, node_name: str) -> int:
        return self._active_sessions[node_name]

    def run(self, node_name: str, fn: Callable[[], T]) -> T:
        if node_name not in self.nodes:
            raise KeyError(node_name)
        with self._node_locks[node_name]:
            self._stats.requests[node_name] += 1
            try:
                return fn()
            except TimeoutError:
                self._stats.timeouts[node_name] += 1
                raise
            except Exception:
                self._stats.errors[node_name] += 1
                raise

    def stats(self) -> dict[str, object]:
        self._stats.finished_at = time.perf_counter()
        return self._stats.as_dict()


@dataclass(frozen=True)
class JobOutcome:
    node: str
    status: str
    attempts: int
    elapsed_s: float
    value: object | None = None
    error: str | None = None


@dataclass
class _QueuedJob:
    fn: Callable[[], object]
    session: int | None
    retries: int
    retry_delay_s: float
    future: Future[JobOutcome]


class QueuedDualNodeScheduler:
    """Per-node work queues with retries, skips, and exclusive sessions."""

    def __init__(
        self,
        nodes: list[SchedulerNode],
        exclusive_sessions: frozenset[int] = frozenset({0x02}),
        default_retries: int = 1,
        retry_delay_s: float = 0.0,
        session_wait_timeout_s: float = 5.0,
    ) -> None:
        if default_retries < 0:
            raise ValueError("default_retries must not be negative")
        if retry_delay_s < 0:
            raise ValueError("retry_delay_s must not be negative")
        if session_wait_timeout_s <= 0:
            raise ValueError("session_wait_timeout_s must be positive")

        self.nodes = {node.name: node for node in nodes}
        self.exclusive_sessions = exclusive_sessions
        self.default_retries = int(default_retries)
        self.retry_delay_s = float(retry_delay_s)
        self.session_wait_timeout_s = float(session_wait_timeout_s)
        self._queues = {name: queue.Queue() for name in self.nodes}
        self._workers = {
            name: threading.Thread(
                target=self._worker_loop,
                args=(name,),
                name=f"uds-scheduler-{name}",
                daemon=True,
            )
            for name in self.nodes
        }
        self._session_condition = threading.Condition()
        self._active_sessions = {name: 0x01 for name in self.nodes}
        self._stats: dict[str, defaultdict[str, int]] = {
            key: defaultdict(int)
            for key in ("requests", "ok", "timeouts", "errors", "skipped")
        }
        self._closed = False
        for worker in self._workers.values():
            worker.start()

    def submit(
        self,
        node_name: str,
        fn: Callable[[], object],
        *,
        session: int | None = None,
        retries: int | None = None,
        retry_delay_s: float | None = None,
    ) -> Future[JobOutcome]:
        if self._closed:
            raise RuntimeError("scheduler is closed")
        if node_name not in self.nodes:
            raise KeyError(node_name)

        attempts = self.default_retries if retries is None else int(retries)
        if attempts < 0:
            raise ValueError("retries must not be negative")
        delay = self.retry_delay_s if retry_delay_s is None else float(retry_delay_s)
        if delay < 0:
            raise ValueError("retry_delay_s must not be negative")

        future: Future[JobOutcome] = Future()
        self._queues[node_name].put(
            _QueuedJob(fn=fn, session=session, retries=attempts, retry_delay_s=delay, future=future)
        )
        return future

    def active_session(self, node_name: str) -> int:
        with self._session_condition:
            return self._active_sessions[node_name]

    def stats(self) -> dict[str, object]:
        return {
            **{key: dict(value) for key, value in self._stats.items()},
            "closed": self._closed,
        }

    def close(self) -> None:
        if self._closed:
            return
        self._closed = True
        for work_queue in self._queues.values():
            work_queue.put(None)
        for worker in self._workers.values():
            worker.join(timeout=2.0)

    def __enter__(self) -> "QueuedDualNodeScheduler":
        return self

    def __exit__(self, *exc_info: object) -> None:
        self.close()

    def _worker_loop(self, node_name: str) -> None:
        while True:
            job = self._queues[node_name].get()
            if job is None:
                return
            self._execute(node_name, job)

    def _execute(self, node_name: str, job: _QueuedJob) -> None:
        started = time.perf_counter()
        self._stats["requests"][node_name] += 1
        acquired = False

        if job.session in self.exclusive_sessions:
            acquired = self._acquire_session(node_name, int(job.session))
            if not acquired:
                self._stats["skipped"][node_name] += 1
                job.future.set_result(
                    JobOutcome(
                        node=node_name,
                        status="skipped",
                        attempts=0,
                        elapsed_s=time.perf_counter() - started,
                        error="session conflict wait timed out",
                    )
                )
                return

        status = "error"
        attempts = 0
        value: object | None = None
        error: str | None = None
        try:
            for attempt in range(1, job.retries + 2):
                attempts = attempt
                try:
                    value = job.fn()
                    status = "ok"
                    error = None
                    break
                except TimeoutError as exc:
                    status = "timeout"
                    error = str(exc)
                    if attempt <= job.retries:
                        if job.retry_delay_s > 0:
                            time.sleep(job.retry_delay_s)
                        continue
                    break
                except Exception as exc:  # noqa: BLE001 - reported in JobOutcome
                    status = "error"
                    error = f"{type(exc).__name__}: {exc}"
                    break
        finally:
            if acquired:
                self._release_session(node_name)

        stat_key = {"ok": "ok", "timeout": "timeouts", "error": "errors"}[status]
        self._stats[stat_key][node_name] += 1
        job.future.set_result(
            JobOutcome(
                node=node_name,
                status=status,
                attempts=attempts,
                elapsed_s=time.perf_counter() - started,
                value=value,
                error=error,
            )
        )

    def _acquire_session(self, node_name: str, session: int) -> bool:
        deadline = time.monotonic() + self.session_wait_timeout_s
        with self._session_condition:
            while any(
                other != node_name and active in self.exclusive_sessions
                for other, active in self._active_sessions.items()
            ):
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return False
                self._session_condition.wait(remaining)
            self._active_sessions[node_name] = session
            self._session_condition.notify_all()
            return True

    def _release_session(self, node_name: str) -> None:
        with self._session_condition:
            self._active_sessions[node_name] = 0x01
            self._session_condition.notify_all()
