"""Dual-node diagnostic request scheduler."""

from __future__ import annotations

import threading
import time
from collections import defaultdict
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
