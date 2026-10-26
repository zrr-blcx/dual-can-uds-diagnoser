from __future__ import annotations

from diagnoser.emu.memory_bus import MemoryBus
from diagnoser.emu.virtual_ecu import VirtualEcu
from diagnoser.tools.session_conflict import (
    render_session_conflict,
    run_session_conflict,
)
from diagnoser.transport.isotp import IsoTpTransport
from diagnoser.uds.client import UdsClient


def _build_pair(
    channel: str,
    name: str,
    request_id: int,
    response_id: int,
) -> tuple[VirtualEcu, UdsClient, MemoryBus, MemoryBus]:
    ecu_bus = MemoryBus(channel, node_id=request_id)
    tester_bus = MemoryBus(channel, node_id=response_id)
    ecu = VirtualEcu(name, ecu_bus, request_id, response_id)
    ecu.start()
    transport = IsoTpTransport(
        tester_bus,
        txid=request_id,
        rxid=response_id,
        timeout=1.0,
    )
    client = UdsClient(transport, timeout=1.0, retries=0)
    return ecu, client, ecu_bus, tester_bus


def test_programming_session_conflict_skips_second_node() -> None:
    channel = "session-conflict"
    node1 = _build_pair(channel, "ecu1", 0x7E0, 0x7E8)
    node2 = _build_pair(channel, "ecu2", 0x7E1, 0x7E9)

    try:
        result = run_session_conflict(
            {"ecu1": node1[1], "ecu2": node2[1]},
            hold_seconds=0.2,
            wait_timeout_seconds=0.05,
        )
    finally:
        for ecu, _, ecu_bus, tester_bus in (node1, node2):
            ecu.stop()
            ecu_bus.shutdown()
            tester_bus.shutdown()

    assert result.passed
    assert result.first_session_entered
    assert result.second_skipped
    assert result.second_job_executed is False
    assert result.first_restored
    assert result.second_recovered
    assert node1[0].session == 0x01
    assert node2[0].session == 0x01
    assert "PASS" in render_session_conflict(result)
