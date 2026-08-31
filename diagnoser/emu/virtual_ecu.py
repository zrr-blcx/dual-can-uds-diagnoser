"""A software UDS responder for testing without hardware."""

from __future__ import annotations

import threading
import time
from dataclasses import dataclass, field

import can

from diagnoser.transport.isotp import IsoTpTransport
from diagnoser.uds.constants import (
    SID_DIAGNOSTIC_SESSION_CONTROL,
    SID_NEGATIVE_RESPONSE,
    SID_READ_DATA_BY_IDENTIFIER,
    SID_ROUTINE_CONTROL,
    SID_TESTER_PRESENT,
    SID_WRITE_DATA_BY_IDENTIFIER,
)

FAULT_CONTROL_DID = 0xF190
DTC_STATUS_DID = 0xF191


@dataclass
class EcuFaultState:
    comm_loss_until: float = 0.0
    bus_off_until: float = 0.0
    nrc_for_service: dict[int, int] = field(default_factory=dict)


class VirtualEcu:
    def __init__(
        self,
        name: str,
        bus: can.BusABC,
        request_id: int,
        response_id: int,
        dids: dict[int, bytes] | None = None,
        writable_dids: set[int] | None = None,
        routines: dict[int, object] | None = None,
        supported_sessions: tuple[int, ...] = (0x01, 0x02, 0x03),
    ) -> None:
        self.name = name
        self.bus = bus
        self.transport = IsoTpTransport(
            bus, txid=response_id, rxid=request_id, timeout=0.5
        )
        self.supported_sessions = supported_sessions
        self.session = 0x01
        self.dids = dids or {
            FAULT_CONTROL_DID: bytes([0x00, 0x00]),
            DTC_STATUS_DID: bytes([0x00, 0x00, 0x00, 0x00]),
            0xF19A: self.name.encode("ascii"),
            0xF19B: b"V1.0",
        }
        self.writable_dids = writable_dids or {
            FAULT_CONTROL_DID,
            DTC_STATUS_DID,
        }
        self.routines = routines or {
            0x0203: self._run_self_test,
            0x0301: self._run_comm_check,
        }
        self.fault = EcuFaultState()
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None

    def start(self) -> None:
        if self._thread is not None:
            return
        self._stop.clear()
        self._thread = threading.Thread(target=self._loop, name=self.name, daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=2.0)
            self._thread = None

    def _loop(self) -> None:
        while not self._stop.is_set():
            try:
                request = self.transport.receive(timeout=0.1)
            except TimeoutError:
                continue
            response = self._handle(request)
            if response is not None:
                self.transport.send(response)

    def _handle(self, request: bytes) -> bytes | None:
        now = time.monotonic()
        if now < self.fault.comm_loss_until or now < self.fault.bus_off_until:
            return None
        if self.fault.bus_off_until:
            self.fault.bus_off_until = 0.0
        if not request:
            return self._negative(0x00, 0x10)

        service = request[0]
        if service in self.fault.nrc_for_service:
            return self._negative(service, self.fault.nrc_for_service[service])

        if service == SID_DIAGNOSTIC_SESSION_CONTROL:
            return self._session_control(request)
        if service == SID_TESTER_PRESENT:
            return bytes([0x7E, 0x00])
        if service == SID_READ_DATA_BY_IDENTIFIER:
            return self._read_did(request)
        if service == SID_WRITE_DATA_BY_IDENTIFIER:
            return self._write_did(request)
        if service == SID_ROUTINE_CONTROL:
            return self._routine_control(request)
        return self._negative(service, 0x11)

    def _session_control(self, request: bytes) -> bytes:
        if len(request) < 2:
            return self._negative(SID_DIAGNOSTIC_SESSION_CONTROL, 0x13)
        session = request[1]
        if session not in self.supported_sessions:
            return self._negative(SID_DIAGNOSTIC_SESSION_CONTROL, 0x12)
        self.session = session
        return bytes([0x50, session, 0x00, 0x32, 0x00, 0x00])

    def _read_did(self, request: bytes) -> bytes:
        if len(request) < 3:
            return self._negative(SID_READ_DATA_BY_IDENTIFIER, 0x13)
        did = int.from_bytes(request[1:3], "big")
        if did not in self.dids:
            return self._negative(SID_READ_DATA_BY_IDENTIFIER, 0x31)
        return bytes([0x62]) + request[1:3] + self.dids[did]

    def _write_did(self, request: bytes) -> bytes:
        if len(request) < 3:
            return self._negative(SID_WRITE_DATA_BY_IDENTIFIER, 0x13)
        did = int.from_bytes(request[1:3], "big")
        value = request[3:]
        if did == FAULT_CONTROL_DID:
            self._apply_fault(value)
        elif did in self.writable_dids:
            self.dids[did] = value
        else:
            return self._negative(SID_WRITE_DATA_BY_IDENTIFIER, 0x31)
        return bytes([0x6E]) + request[1:3]

    def _routine_control(self, request: bytes) -> bytes:
        if len(request) < 4:
            return self._negative(SID_ROUTINE_CONTROL, 0x13)
        subfunction = request[1]
        routine_id = int.from_bytes(request[2:4], "big")
        data = request[4:]
        routine = self.routines.get(routine_id)
        if routine is None:
            return self._negative(SID_ROUTINE_CONTROL, 0x31)
        try:
            result = routine(subfunction, data)
        except TypeError:
            result = routine()
        return bytes([0x71, subfunction]) + routine_id.to_bytes(2, "big") + result

    def _apply_fault(self, value: bytes) -> None:
        if not value:
            return
        mode = value[0]
        if mode == 0x00:
            self.fault = EcuFaultState()
        elif mode == 0x01 and len(value) >= 5:
            self.dids[DTC_STATUS_DID] = value[1:5]
        elif mode == 0x02 and len(value) >= 2:
            self.fault.comm_loss_until = time.monotonic() + value[1]
        elif mode == 0x03 and len(value) >= 2:
            self.fault.bus_off_until = time.monotonic() + value[1]
        elif mode == 0x04 and len(value) >= 3:
            self.fault.nrc_for_service[value[1]] = value[2]

    def _run_self_test(self, subfunction: int, data: bytes) -> bytes:
        if subfunction == 0x01:
            return bytes([0x01, 0x00])
        if subfunction == 0x03:
            return bytes([0x01, 0x00, 0x00])
        return b""

    def _run_comm_check(self, subfunction: int, data: bytes) -> bytes:
        return bytes([0x00])

    def _negative(self, service: int, nrc: int) -> bytes:
        return bytes([SID_NEGATIVE_RESPONSE, service, nrc])
