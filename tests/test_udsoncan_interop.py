"""Interoperability test: udsoncan drives the in-repo ISO-TP transport."""

from __future__ import annotations

import pytest

udsoncan = pytest.importorskip("udsoncan")

from udsoncan.client import Client as UdsoncanClient  # noqa: E402

from diagnoser.emu.memory_bus import MemoryBus  # noqa: E402
from diagnoser.emu.virtual_ecu import VirtualEcu  # noqa: E402
from diagnoser.transport.isotp import IsoTpTransport  # noqa: E402
from diagnoser.transport.udsoncan_link import UdsOnCanLink  # noqa: E402


def _udsoncan_config() -> dict:
    config = dict(udsoncan.configs.default_client_config)
    config["data_identifiers"] = {
        0xF190: udsoncan.AsciiCodec(17),
        0xFFFF: udsoncan.AsciiCodec(1),
    }
    return config


def test_udsoncan_client_reads_virtual_ecu() -> None:
    channel = "udsoncan-interop"
    ecu_bus = MemoryBus(channel, node_id=0x7E0)
    tester_bus = MemoryBus(channel, node_id=0x7E8)
    ecu = VirtualEcu("ecu1", ecu_bus, 0x7E0, 0x7E8)
    ecu.start()
    transport = IsoTpTransport(tester_bus, txid=0x7E0, rxid=0x7E8, timeout=1.0)
    try:
        with UdsoncanClient(UdsOnCanLink(transport), config=_udsoncan_config()) as uds:
            session = uds.change_session(0x03)
            assert session.positive is True

            response = uds.read_data_by_identifier(0xF190)
            assert response.positive is True
            assert response.service_data.values[0xF190] == "P2F407TESTVIN0001"
    finally:
        ecu.stop()
        ecu_bus.shutdown()
        tester_bus.shutdown()


def test_udsoncan_client_raises_on_negative_response() -> None:
    channel = "udsoncan-interop-nrc"
    ecu_bus = MemoryBus(channel, node_id=0x7E0)
    tester_bus = MemoryBus(channel, node_id=0x7E8)
    ecu = VirtualEcu("ecu1", ecu_bus, 0x7E0, 0x7E8)
    ecu.start()
    transport = IsoTpTransport(tester_bus, txid=0x7E0, rxid=0x7E8, timeout=1.0)
    try:
        with UdsoncanClient(UdsOnCanLink(transport), config=_udsoncan_config()) as uds:
            with pytest.raises(udsoncan.exceptions.NegativeResponseException) as exc_info:
                uds.read_data_by_identifier(0xFFFF)
            assert int(exc_info.value.response.code) == 0x31
    finally:
        ecu.stop()
        ecu_bus.shutdown()
        tester_bus.shutdown()
