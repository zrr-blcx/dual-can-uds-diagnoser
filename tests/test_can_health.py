from diagnoser.tools.can_health import (
    CanBusState,
    CanErrorKind,
    CanHealthMonitor,
    WatchdogSupervisor,
)


def test_error_counters_drive_can_state() -> None:
    monitor = CanHealthMonitor()

    for _ in range(15):
        monitor.record_error(CanErrorKind.ACK, tx=True)
    assert monitor.snapshot().state == CanBusState.ERROR_ACTIVE

    monitor.record_error(CanErrorKind.ACK, tx=True)
    assert monitor.snapshot().state == CanBusState.ERROR_PASSIVE

    for _ in range(16):
        monitor.record_error(CanErrorKind.CRC, tx=True)

    snapshot = monitor.snapshot()
    assert snapshot.state == CanBusState.BUS_OFF
    assert snapshot.tec == 256
    assert snapshot.error_counts["ack"] == 16
    assert snapshot.error_counts["crc"] == 16
    assert snapshot.bus_off_events == 1


def test_recovery_failures_request_reset_after_three_attempts() -> None:
    resets: list[str] = []
    monitor = CanHealthMonitor(max_recovery_failures=3)
    monitor.enter_bus_off()

    for _ in range(3):
        assert monitor.attempt_recovery(
            reinitialize=lambda: False,
            request_reset=lambda: resets.append("reset"),
        ) is False

    snapshot = monitor.snapshot()
    assert snapshot.recovery_attempts == 3
    assert snapshot.recovery_failures == 3
    assert snapshot.reset_requests == 1
    assert resets == ["reset"]


def test_successful_recovery_clears_error_state() -> None:
    monitor = CanHealthMonitor()
    monitor.enter_bus_off()

    assert monitor.attempt_recovery(reinitialize=lambda: True) is True

    snapshot = monitor.snapshot()
    assert snapshot.state == CanBusState.ERROR_ACTIVE
    assert snapshot.tec == 0
    assert snapshot.rec == 0
    assert snapshot.recovery_successes == 1


def test_watchdog_reports_each_expired_task_once() -> None:
    now = [0.0]
    expired: list[str] = []
    watchdog = WatchdogSupervisor(clock=lambda: now[0], on_timeout=expired.append)
    watchdog.register("diagnostic-loop", timeout_s=1.0)

    now[0] = 1.1
    assert watchdog.check() == ["diagnostic-loop"]
    assert watchdog.check() == []
    assert expired == ["diagnostic-loop"]

    watchdog.kick("diagnostic-loop")
    now[0] = 2.2
    assert watchdog.check() == ["diagnostic-loop"]
