import time

import pytest

from diagnoser.emu.memory_bus import MemoryBus
from diagnoser.emu.virtual_ecu import VirtualEcu
from diagnoser.transport.isotp import IsoTpTransport
from diagnoser.tools.uds_smoke import run_uds_smoke
from diagnoser.uds.client import UdsClient
from diagnoser.uds.dids import (
    DID_CAN_HEALTH,
    DID_CUSTOM_DATA,
    DID_DTC_STATUS,
    DID_FAULT_CONTROL,
    DID_HARDWARE_VERSION,
    DID_VIN,
)
from diagnoser.uds.errors import NegativeResponse


def _build_ecu_and_client(channel: str) -> tuple[VirtualEcu, UdsClient, MemoryBus, MemoryBus]:
    ecu_bus = MemoryBus(channel, node_id=0x7E0)
    tester_bus = MemoryBus(channel, node_id=0x7E8)
    ecu = VirtualEcu("ecu1", ecu_bus, 0x7E0, 0x7E8)
    ecu.start()
    transport = IsoTpTransport(tester_bus, txid=0x7E0, rxid=0x7E8, timeout=1.0)
    client = UdsClient(transport, timeout=1.0, retries=0)
    return ecu, client, ecu_bus, tester_bus


def test_core_uds_services() -> None:
    ecu, client, ecu_bus, tester_bus = _build_ecu_and_client("core-services")
    try:
        response = client.change_session(0x03)
        assert response[0] == 0x03
        assert client.read_data_by_identifier(DID_VIN)[2:] == b"P2F407TESTVIN0001"
        assert (
            client.read_data_by_identifier(DID_HARDWARE_VERSION)[2:]
            == b"F407-HW-REV-A"
        )
        assert client.read_data_by_identifier(DID_CUSTOM_DATA)[2:] == b"\x00\x01"
        assert client.read_data_by_identifier(0xF19A) == b"\xF1\x9A" + b"ecu1"
        client.unlock()
        client.write_data_by_identifier(DID_DTC_STATUS, b"\x12\x34\x56\x78")
        assert client.read_data_by_identifier(DID_DTC_STATUS) == b"\xF1\x91" + b"\x12\x34\x56\x78"
        assert client.routine_control(0x01, 0x0203) == b"\x02\x03"
    finally:
        ecu.stop()
        ecu_bus.shutdown()
        tester_bus.shutdown()


def test_negative_response_for_unknown_did() -> None:
    ecu, client, ecu_bus, tester_bus = _build_ecu_and_client("negative-response")
    try:
        with pytest.raises(NegativeResponse) as exc_info:
            client.read_data_by_identifier(0xFFFF)
        assert exc_info.value.nrc == 0x31
    finally:
        ecu.stop()
        ecu_bus.shutdown()
        tester_bus.shutdown()


def test_comm_loss_and_recovery() -> None:
    ecu, client, ecu_bus, tester_bus = _build_ecu_and_client("comm-loss")
    try:
        client.write_data_by_identifier(DID_FAULT_CONTROL, b"\x02\x01")
        with pytest.raises(TimeoutError):
            client.tester_present()
        time.sleep(1.3)
        client.tester_present()
    finally:
        ecu.stop()
        ecu_bus.shutdown()
        tester_bus.shutdown()


def test_bus_off_and_auto_recovery() -> None:
    ecu, client, ecu_bus, tester_bus = _build_ecu_and_client("bus-off")
    try:
        client.write_data_by_identifier(DID_FAULT_CONTROL, b"\x03\x01")
        with pytest.raises(TimeoutError):
            client.tester_present()
        time.sleep(1.3)
        client.tester_present()
    finally:
        ecu.stop()
        ecu_bus.shutdown()
        tester_bus.shutdown()


def test_bus_off_health_is_reported_after_recovery() -> None:
    ecu, client, ecu_bus, tester_bus = _build_ecu_and_client("bus-off-health")
    try:
        payload = bytes([0x03]) + (50).to_bytes(2, "big")
        client.write_data_by_identifier(DID_FAULT_CONTROL, payload)
        with pytest.raises(TimeoutError):
            client.tester_present()

        time.sleep(0.1)
        client.tester_present()
        health = client.read_data_by_identifier(DID_CAN_HEALTH)

        assert health[2] == 0
        assert health[5] == 1
        assert health[6] == 1
    finally:
        ecu.stop()
        ecu_bus.shutdown()
        tester_bus.shutdown()


def test_session_timeout_returns_to_default() -> None:
    ecu, client, ecu_bus, tester_bus = _build_ecu_and_client("s3-timeout")
    ecu.s3_timeout_s = 0.05
    try:
        client.change_session(0x03)
        assert ecu.session == 0x03
        deadline = time.monotonic() + 0.5
        while ecu.session != 0x01 and time.monotonic() < deadline:
            time.sleep(0.01)
        assert ecu.session == 0x01
        assert ecu.session_timeout_count == 1
    finally:
        ecu.stop()
        ecu_bus.shutdown()
        tester_bus.shutdown()


def test_single_node_uds_smoke_workflow() -> None:
    ecu, client, ecu_bus, tester_bus = _build_ecu_and_client("uds-smoke")
    try:
        result = run_uds_smoke(client, "ecu1")
        assert result.passed
        assert [step.name for step in result.steps] == [
            "session-extended",
            "read-vin",
            "read-hardware-version",
            "read-custom-data",
            "unknown-did-nrc",
        ]
    finally:
        ecu.stop()
        ecu_bus.shutdown()
        tester_bus.shutdown()


