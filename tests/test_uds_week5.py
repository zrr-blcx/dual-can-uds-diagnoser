"""Week 5 Python-side coverage for 0x27, 0x2E, 0x31, and the NRC matrix."""

from __future__ import annotations

import pytest

from diagnoser.emu.memory_bus import MemoryBus
from diagnoser.emu.virtual_ecu import (
    ROUTINE_ERASE,
    ROUTINE_SELF_TEST,
    VirtualEcu,
)
from diagnoser.tools.uds_week5 import render_uds_week5, run_uds_week5
from diagnoser.transport.isotp import IsoTpTransport
from diagnoser.uds.client import UdsClient
from diagnoser.uds.constants import (
    ROUTINE_CONTROL_REQUEST_RESULTS,
    ROUTINE_CONTROL_START,
    SESSION_DEFAULT,
    SESSION_EXTENDED,
    SESSION_PROGRAMMING,
)
from diagnoser.uds.dids import DID_CUSTOM_DATA, DID_VIN
from diagnoser.uds.errors import NegativeResponse
from diagnoser.uds.security import seed_to_key


def _build_pair(
    channel: str, name: str = "ecu1", request_id: int = 0x7E0, response_id: int = 0x7E8
) -> tuple[VirtualEcu, UdsClient, MemoryBus, MemoryBus]:
    ecu_bus = MemoryBus(channel, node_id=request_id)
    tester_bus = MemoryBus(channel, node_id=response_id)
    ecu = VirtualEcu(name, ecu_bus, request_id, response_id)
    ecu.start()
    transport = IsoTpTransport(tester_bus, txid=request_id, rxid=response_id, timeout=1.0)
    client = UdsClient(transport, timeout=1.0)
    return ecu, client, ecu_bus, tester_bus


def _expect_nrc(client: UdsClient, operation, nrc: int) -> NegativeResponse:
    with pytest.raises(NegativeResponse) as exc_info:
        operation()
    assert exc_info.value.nrc == nrc
    return exc_info.value


def test_seed_key_vector() -> None:
    assert seed_to_key(0x87985AA5) == 0xE174F5DA
    assert seed_to_key(0x87985AA5, 0x50324341) == 0xE174F5DA


def test_week5_flow_passes_on_virtual_ecu() -> None:
    ecu, client, ecu_bus, tester_bus = _build_pair("week5-flow")
    try:
        result = run_uds_week5(client, "ecu1")
        assert result.passed, render_uds_week5(result)
        assert [step.name for step in result.steps] == [
            "session-extended",
            "security-request-seed",
            "security-send-key",
            "write-did",
            "read-back",
            "routine-self-test-start",
            "routine-self-test-results",
            "routine-erase-start",
            "routine-erase-results",
            "session-default",
            "write-denied-in-default",
            "session-extended-again",
            "write-denied-locked",
        ]
    finally:
        ecu.stop()
        ecu_bus.shutdown()
        tester_bus.shutdown()


def test_week5_nrc_matrix() -> None:
    ecu, client, ecu_bus, tester_bus = _build_pair("week5-nrc")
    try:
        # 0x11: unsupported service.
        assert client.send_raw(bytes([0x99])) == bytes([0x7F, 0x99, 0x11])

        # 0x12: unsupported security subfunction.
        _expect_nrc(client, lambda: client.security_access(0x05), 0x12)

        # 0x13: send-key without a four-byte key.
        client.change_session(SESSION_EXTENDED)
        _expect_nrc(client, lambda: client.security_access(0x02), 0x13)

        # 0x33: extended session but still locked.
        _expect_nrc(
            client,
            lambda: client.write_data_by_identifier(DID_CUSTOM_DATA, b"\xAA"),
            0x33,
        )

        client.unlock()

        # 0x31: write to an unknown DID.
        _expect_nrc(
            client,
            lambda: client.write_data_by_identifier(0xFFFF, b"\x00"),
            0x31,
        )

        # 0x22: VIN write requires the programming session.
        _expect_nrc(
            client,
            lambda: client.write_data_by_identifier(DID_VIN, b"X" * 17),
            0x22,
        )

        # 0x78: a running routine answers with response-pending.
        client.routine_control(ROUTINE_CONTROL_START, ROUTINE_SELF_TEST)
        _expect_nrc(
            client,
            lambda: client.routine_control(
                ROUTINE_CONTROL_REQUEST_RESULTS, ROUTINE_SELF_TEST
            ),
            0x78,
        )

        # 0x7E: write in the default session.
        client.change_session(SESSION_DEFAULT)
        _expect_nrc(
            client,
            lambda: client.write_data_by_identifier(DID_CUSTOM_DATA, b"\xAA"),
            0x7E,
        )

        # 0x24: send key before requesting a seed.
        client.change_session(SESSION_EXTENDED)
        _expect_nrc(
            client,
            lambda: client.send_key(bytes(4)),
            0x24,
        )

        # 0x35: wrong key.
        client.request_seed()
        _expect_nrc(client, lambda: client.send_key(bytes(4)), 0x35)

        # 0x36 / 0x37: three wrong keys arm the delay timer.
        for _ in range(2):
            client.request_seed()
            with pytest.raises(NegativeResponse) as exc_info:
                client.send_key(bytes(4))
            assert exc_info.value.nrc in (0x35, 0x36)
        assert exc_info.value.nrc == 0x36
        _expect_nrc(client, client.request_seed, 0x37)
    finally:
        ecu.stop()
        ecu_bus.shutdown()
        tester_bus.shutdown()


def test_security_relocks_on_session_change() -> None:
    ecu, client, ecu_bus, tester_bus = _build_pair("week5-relock")
    try:
        client.change_session(SESSION_EXTENDED)
        client.unlock()
        client.write_data_by_identifier(DID_CUSTOM_DATA, b"\x11\x22")

        client.change_session(SESSION_PROGRAMMING)
        _expect_nrc(
            client,
            lambda: client.write_data_by_identifier(DID_CUSTOM_DATA, b"\x33\x44"),
            0x33,
        )
    finally:
        ecu.stop()
        ecu_bus.shutdown()
        tester_bus.shutdown()


def test_erase_requires_unlock() -> None:
    ecu, client, ecu_bus, tester_bus = _build_pair("week5-erase")
    try:
        client.change_session(SESSION_EXTENDED)
        _expect_nrc(
            client,
            lambda: client.routine_control(ROUTINE_CONTROL_START, ROUTINE_ERASE),
            0x33,
        )
        client.unlock()
        client.routine_control(ROUTINE_CONTROL_START, ROUTINE_ERASE)
        _expect_nrc(
            client,
            lambda: client.routine_control(
                ROUTINE_CONTROL_REQUEST_RESULTS, ROUTINE_ERASE
            ),
            0x78,
        )
        result = client.routine_control(
            ROUTINE_CONTROL_REQUEST_RESULTS, ROUTINE_ERASE
        )
        assert result == bytes([0xFF, 0x00]) + b"\x00"
    finally:
        ecu.stop()
        ecu_bus.shutdown()
        tester_bus.shutdown()


def test_dual_node_virtual_week5_flow() -> None:
    channel = "week5-dual-node"
    node1 = _build_pair(channel, "ecu1", 0x7E0, 0x7E8)
    node2 = _build_pair(channel, "ecu2", 0x7E1, 0x7E9)
    try:
        for name, (ecu, client, _, _) in (("ecu1", node1), ("ecu2", node2)):
            result = run_uds_week5(client, name)
            assert result.passed, render_uds_week5(result)
    finally:
        for ecu, _, ecu_bus, tester_bus in (node1, node2):
            ecu.stop()
            ecu_bus.shutdown()
            tester_bus.shutdown()
