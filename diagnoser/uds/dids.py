"""DID database shared by the diagnostic client, emulator, and integration code."""

from __future__ import annotations

DID_CUSTOM_DATA = 0x0100
DID_VIN = 0xF190
DID_DTC_STATUS = 0xF191
DID_CAN_HEALTH = 0xF192
DID_HARDWARE_VERSION = 0xF193
DID_ECU_NAME = 0xF19A
DID_SOFTWARE_VERSION = 0xF19B
DID_FAULT_CONTROL = 0xF1A0

DEFAULT_VIN = b"P2TESTVIN00000001"
DEFAULT_HARDWARE_VERSION = b"PROJECT2-HW-A"
DEFAULT_CUSTOM_DATA = b"\x00\x01"

VIN_BY_ECU = {
    "ecu1": b"P2F407TESTVIN0001",
    "ecu2": b"P2F103TESTVIN0001",
}

HARDWARE_VERSION_BY_ECU = {
    "ecu1": b"F407-HW-REV-A",
    "ecu2": b"F103-HW-REV-A",
}


def default_dids(ecu_name: str) -> dict[int, bytes]:
    return {
        DID_VIN: VIN_BY_ECU.get(ecu_name, DEFAULT_VIN),
        DID_HARDWARE_VERSION: HARDWARE_VERSION_BY_ECU.get(
            ecu_name,
            DEFAULT_HARDWARE_VERSION,
        ),
        DID_CUSTOM_DATA: DEFAULT_CUSTOM_DATA,
        DID_DTC_STATUS: bytes(4),
        DID_FAULT_CONTROL: bytes(2),
        DID_ECU_NAME: ecu_name.encode("ascii"),
        DID_SOFTWARE_VERSION: b"V1.0",
    }
