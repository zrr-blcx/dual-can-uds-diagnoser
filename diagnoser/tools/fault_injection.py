"""Fault injection helpers for DTC, communication loss, and Bus-Off scenarios."""

from __future__ import annotations

import struct

from diagnoser.uds.client import UdsClient

FAULT_CONTROL_DID = 0xF190


class FaultInjector:
    def __init__(self, clients: dict[str, UdsClient]) -> None:
        self.clients = clients

    def inject_dtc(self, node: str, dtc: int, status: int = 0x2A) -> bytes:
        payload = struct.pack(
            ">B3sB", 0x01, dtc.to_bytes(3, "big"), status & 0xFF
        )
        return self.clients[node].write_data_by_identifier(FAULT_CONTROL_DID, payload)

    def simulate_comm_loss(self, node: str, seconds: float) -> bytes:
        payload = bytes([0x02, int(seconds) & 0xFF])
        return self.clients[node].write_data_by_identifier(FAULT_CONTROL_DID, payload)

    def trigger_bus_off(self, node: str, seconds: float) -> bytes:
        payload = bytes([0x03, int(seconds) & 0xFF])
        return self.clients[node].write_data_by_identifier(FAULT_CONTROL_DID, payload)

    def inject_nrc(self, node: str, service: int, nrc: int) -> bytes:
        payload = bytes([0x04, service & 0xFF, nrc & 0xFF])
        return self.clients[node].write_data_by_identifier(FAULT_CONTROL_DID, payload)

    def clear_faults(self, node: str) -> bytes:
        return self.clients[node].write_data_by_identifier(
            FAULT_CONTROL_DID, bytes([0x00, 0x00])
        )
