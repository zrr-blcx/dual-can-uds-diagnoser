"""CAN error-state supervision and watchdog liveness tracking."""

from __future__ import annotations

import time
from collections.abc import Callable
from dataclasses import dataclass
from enum import Enum


class CanErrorKind(str, Enum):
    BIT = "bit"
    STUFF = "stuff"
    ACK = "ack"
    CRC = "crc"
    FORM = "form"
    OTHER = "other"


class CanBusState(str, Enum):
    ERROR_ACTIVE = "error-active"
    ERROR_PASSIVE = "error-passive"
    BUS_OFF = "bus-off"


@dataclass(frozen=True)
class CanHealthSnapshot:
    state: CanBusState
    tec: int
    rec: int
    error_counts: dict[str, int]
    bus_off_events: int
    recovery_attempts: int
    recovery_successes: int
    recovery_failures: int
    consecutive_recovery_failures: int
    reset_requests: int


@dataclass
class _WatchdogTask:
    timeout_s: float
    last_kick_s: float
    expired: bool = False


class CanHealthMonitor:
    """Software model of the CAN error counters and Bus-Off recovery policy.

    CAN controllers maintain TEC/REC in hardware. This model mirrors that
    policy for virtual-bus validation and gives the firmware implementation a
    directly testable behavior contract.
    """

    def __init__(
        self,
        passive_threshold: int = 128,
        bus_off_threshold: int = 256,
        max_recovery_failures: int = 3,
    ) -> None:
        if passive_threshold <= 0:
            raise ValueError("passive_threshold must be positive")
        if bus_off_threshold <= passive_threshold:
            raise ValueError("bus_off_threshold must exceed passive_threshold")
        if max_recovery_failures <= 0:
            raise ValueError("max_recovery_failures must be positive")

        self.passive_threshold = passive_threshold
        self.bus_off_threshold = bus_off_threshold
        self.max_recovery_failures = max_recovery_failures
        self.tec = 0
        self.rec = 0
        self.state = CanBusState.ERROR_ACTIVE
        self.error_counts = {kind.value: 0 for kind in CanErrorKind}
        self.bus_off_events = 0
        self.recovery_attempts = 0
        self.recovery_successes = 0
        self.recovery_failures = 0
        self.consecutive_recovery_failures = 0
        self.reset_requests = 0

    def record_error(self, kind: CanErrorKind, tx: bool = True) -> None:
        self.error_counts[kind.value] += 1
        if tx:
            self.tec += 8
        else:
            self.rec += 8
        self._update_error_state()

    def record_success(self, tx: bool = True) -> None:
        if self.state == CanBusState.BUS_OFF:
            return
        if tx and self.tec > 0:
            self.tec -= 1
        elif not tx and self.rec > 0:
            self.rec -= 1
        self._update_error_state()

    def enter_bus_off(self) -> None:
        if self.state != CanBusState.BUS_OFF:
            self.bus_off_events += 1
        self.state = CanBusState.BUS_OFF
        self.tec = max(self.tec, self.bus_off_threshold)

    def mark_recovered(self) -> None:
        self.state = CanBusState.ERROR_ACTIVE
        self.tec = 0
        self.rec = 0
        self.recovery_successes += 1
        self.consecutive_recovery_failures = 0

    def attempt_recovery(
        self,
        reinitialize: Callable[[], bool],
        request_reset: Callable[[], None] | None = None,
    ) -> bool:
        self.recovery_attempts += 1
        try:
            recovered = bool(reinitialize())
        except Exception:
            recovered = False

        if recovered:
            self.mark_recovered()
            return True

        self.recovery_failures += 1
        self.consecutive_recovery_failures += 1
        if self.consecutive_recovery_failures >= self.max_recovery_failures:
            self.reset_requests += 1
            if request_reset is not None:
                request_reset()
        return False

    def snapshot(self) -> CanHealthSnapshot:
        return CanHealthSnapshot(
            state=self.state,
            tec=self.tec,
            rec=self.rec,
            error_counts=dict(self.error_counts),
            bus_off_events=self.bus_off_events,
            recovery_attempts=self.recovery_attempts,
            recovery_successes=self.recovery_successes,
            recovery_failures=self.recovery_failures,
            consecutive_recovery_failures=self.consecutive_recovery_failures,
            reset_requests=self.reset_requests,
        )

    def _update_error_state(self) -> None:
        if self.state == CanBusState.BUS_OFF:
            return
        if self.tec >= self.bus_off_threshold:
            self.enter_bus_off()
        elif max(self.tec, self.rec) >= self.passive_threshold:
            self.state = CanBusState.ERROR_PASSIVE
        else:
            self.state = CanBusState.ERROR_ACTIVE


class WatchdogSupervisor:
    """Tracks task liveness so IWDG/WWDG reset policy can be tested."""

    def __init__(
        self,
        clock: Callable[[], float] = time.monotonic,
        on_timeout: Callable[[str], None] | None = None,
    ) -> None:
        self._clock = clock
        self._on_timeout = on_timeout
        self._tasks: dict[str, _WatchdogTask] = {}

    def register(self, name: str, timeout_s: float) -> None:
        if timeout_s <= 0:
            raise ValueError("timeout_s must be positive")
        self._tasks[name] = _WatchdogTask(
            timeout_s=timeout_s,
            last_kick_s=self._clock(),
        )

    def kick(self, name: str) -> None:
        task = self._tasks.get(name)
        if task is None:
            raise KeyError(f"watchdog task is not registered: {name}")
        task.last_kick_s = self._clock()
        task.expired = False

    def check(self) -> list[str]:
        now = self._clock()
        expired: list[str] = []
        for name, task in self._tasks.items():
            if not task.expired and now - task.last_kick_s >= task.timeout_s:
                task.expired = True
                expired.append(name)
                if self._on_timeout is not None:
                    self._on_timeout(name)
        return expired
