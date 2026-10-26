from __future__ import annotations

from diagnoser.emu.memory_bus import MemoryBus
from diagnoser.emu.virtual_ecu import VirtualEcu
from diagnoser.tools.dual_node_smoke import (
    render_dual_node_smoke,
    run_dual_node_smoke,
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


def test_dual_node_smoke_reads_both_nodes_concurrently() -> None:
    channel = "dual-node-smoke"
    node1 = _build_pair(channel, "ecu1", 0x7E0, 0x7E8)
    node2 = _build_pair(channel, "ecu2", 0x7E1, 0x7E9)

    try:
        result = run_dual_node_smoke(
            {"ecu1": node1[1], "ecu2": node2[1]},
            requests_per_node=3,
        )
    finally:
        for ecu, _, ecu_bus, tester_bus in (node1, node2):
            ecu.stop()
            ecu_bus.shutdown()
            tester_bus.shutdown()

    assert result.passed
    assert {node.name: node.success for node in result.nodes} == {
        "ecu1": 3,
        "ecu2": 3,
    }
    assert {node.name: node.response_id for node in result.nodes} == {
        "ecu1": 0x7E8,
        "ecu2": 0x7E9,
    }
    assert "PASS" in render_dual_node_smoke(result)
