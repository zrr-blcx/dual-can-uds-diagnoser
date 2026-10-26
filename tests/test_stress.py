from __future__ import annotations

from diagnoser.emu.memory_bus import MemoryBus
from diagnoser.emu.virtual_ecu import VirtualEcu
from diagnoser.tools.stress import run_stress
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
        timeout=0.2,
    )
    client = UdsClient(transport, timeout=0.2, retries=0)
    return ecu, client, ecu_bus, tester_bus


def test_stress_counts_requests_per_node_and_uses_interval() -> None:
    node1 = _build_pair("stress-counts", "ecu1", 0x7E0, 0x7E8)
    node2 = _build_pair("stress-counts", "ecu2", 0x7E1, 0x7E9)
    load_bus = MemoryBus("stress-load")

    try:
        result = run_stress(
            load_bus,
            {"ecu1": node1[1], "ecu2": node2[1]},
            requests_per_node=3,
            request_interval_ms=5,
            target_load=0.0,
        )
    finally:
        load_bus.shutdown()
        for ecu, _, ecu_bus, tester_bus in (node1, node2):
            ecu.stop()
            ecu_bus.shutdown()
            tester_bus.shutdown()

    assert result.total_requests == 6
    assert result.nodes["ecu1"].sent == 3
    assert result.nodes["ecu2"].sent == 3
    assert result.nodes["ecu1"].success == 3
    assert result.nodes["ecu2"].success == 3
    assert result.uds_timeout_rate == 0.0
    assert result.request_interval_ms == 5
    assert result.measured_load == 0.0
    assert len(result.nodes["ecu1"].request_intervals_s) == 2
    assert all(
        interval >= 0.003
        for interval in result.nodes["ecu1"].request_intervals_s
    )


def test_stress_records_error_types_and_samples() -> None:
    class FailingClient:
        retries = 0

        def read_data_by_identifier(self, did: int) -> bytes:
            del did
            raise RuntimeError("unexpected response")

    load_bus = MemoryBus("stress-error-types")
    try:
        result = run_stress(
            load_bus,
            {"ecu1": FailingClient()},
            requests_per_node=1,
            request_interval_ms=0,
            target_load=0.0,
        )
    finally:
        load_bus.shutdown()

    metrics = result.nodes["ecu1"]
    assert metrics.errors == 1
    assert metrics.errors_by_type == {"RuntimeError": 1}
    assert metrics.error_samples == ["RuntimeError: unexpected response"]


def test_stress_uses_low_priority_background_frames() -> None:
    class SuccessfulClient:
        retries = 0

        def read_data_by_identifier(self, did: int) -> bytes:
            return did.to_bytes(2, "big")

    from can_test_fakes import QueueBus

    bus = QueueBus()
    try:
        run_stress(
            bus,
            {"ecu1": SuccessfulClient()},
            requests_per_node=1,
            request_interval_ms=0,
            target_load=0.5,
        )
    finally:
        bus.shutdown()

    assert bus.sent
    assert {message.arbitration_id for message in bus.sent} == {0x7F0}
