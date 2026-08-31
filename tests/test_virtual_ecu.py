import time

import pytest

from diagnoser.emu.memory_bus import MemoryBus
from diagnoser.emu.virtual_ecu import VirtualEcu
from diagnoser.transport.isotp import IsoTpTransport
from diagnoser.uds.client import UdsClient
from diagnoser.uds.errors import NegativeResponse


def _build_ecu_and_client(channel: str) -> tuple[VirtualEcu, UdsClient, MemoryBus, MemoryBus]:
    ecu_bus = MemoryBus(channel, node_id=0x7E0)
    tester_bus = MemoryBus(channel, node_id=0x7E8)
    ecu = VirtualEcu("ecu1", ecu_bus, 0x7E0, 0x7E8)
    ecu.start()
    transport = IsoTpTransport(tester_bus, txid=0x7E0, rxid=0x7E8, timeout=1.0)
    client = UdsClient(transport, timeout=1.0)
    return ecu, client, ecu_bus, tester_bus


def test_core_uds_services() -> None:
    ecu, client, ecu_bus, tester_bus = _build_ecu_and_client("core-services")
    try:
        response = client.change_session(0x03)
        assert response[0] == 0x03
        assert client.read_data_by_identifier(0xF19A) == b"ecu1"
        client.write_data_by_identifier(0xF191, b"\x12\x34\x56\x78")
        assert client.read_data_by_identifier(0xF191) == b"\x12\x34\x56\x78"
        assert client.routine_control(0x01, 0x0203) == b"\x02\x03\x01\x00"
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
        client.write_data_by_identifier(0xF190, b"\x02\x01")
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
        client.write_data_by_identifier(0xF190, b"\x03\x01")
        with pytest.raises(TimeoutError):
            client.tester_present()
        time.sleep(1.3)
        client.tester_present()
    finally:
        ecu.stop()
        ecu_bus.shutdown()
        tester_bus.shutdown()
