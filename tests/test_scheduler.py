import threading
import time

import pytest

from diagnoser.tools.scheduler import (
    DualNodeScheduler,
    SessionConflictError,
    SchedulerNode,
)


def test_requests_to_same_node_are_serialized() -> None:
    scheduler = DualNodeScheduler([SchedulerNode(name="ecu1")])
    lock = threading.Lock()
    active = 0
    max_active = 0

    def job() -> None:
        nonlocal active, max_active
        with lock:
            active += 1
            max_active = max(max_active, active)
        time.sleep(0.02)
        with lock:
            active -= 1

    threads = [
        threading.Thread(target=lambda: scheduler.run("ecu1", job))
        for _ in range(4)
    ]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()

    assert max_active == 1
    assert scheduler.stats()["requests"]["ecu1"] == 4


def test_exclusive_programming_session_conflict() -> None:
    scheduler = DualNodeScheduler(
        [SchedulerNode(name="ecu1"), SchedulerNode(name="ecu2")]
    )
    scheduler.enter_session("ecu1", 0x02)
    with pytest.raises(SessionConflictError):
        scheduler.enter_session("ecu2", 0x02)
    scheduler.leave_session("ecu1")
    scheduler.enter_session("ecu2", 0x02)
    assert scheduler.active_session("ecu2") == 0x02
