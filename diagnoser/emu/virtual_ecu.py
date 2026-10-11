"""A software UDS responder for testing without hardware."""

from __future__ import annotations

import threading
import time
from dataclasses import dataclass, field

import can

from diagnoser.tools.can_health import CanBusState, CanHealthMonitor
from diagnoser.transport.isotp import IsoTpTransport
from diagnoser.uds.constants import (
    ROUTINE_CONTROL_REQUEST_RESULTS,
    ROUTINE_CONTROL_START,
    ROUTINE_CONTROL_STOP,
    SECURITY_REQUEST_SEED,
    SECURITY_SEND_KEY,
    SESSION_DEFAULT,
    SESSION_EXTENDED,
    SESSION_PROGRAMMING,
    SID_DIAGNOSTIC_SESSION_CONTROL,
    SID_NEGATIVE_RESPONSE,
    SID_READ_DATA_BY_IDENTIFIER,
    SID_ROUTINE_CONTROL,
    SID_SECURITY_ACCESS,
    SID_TESTER_PRESENT,
    SID_WRITE_DATA_BY_IDENTIFIER,
)
from diagnoser.uds.dids import (
    DID_CAN_HEALTH,
    DID_CUSTOM_DATA,
    DID_DTC_STATUS,
    DID_FAULT_CONTROL,
    DID_VIN,
    default_dids,
)
from diagnoser.uds.security import DEFAULT_SECURITY_SALT, SeedGenerator, seed_to_key

FAULT_CONTROL_DID = DID_FAULT_CONTROL
DTC_STATUS_DID = DID_DTC_STATUS
CAN_HEALTH_DID = DID_CAN_HEALTH

SECURITY_MAX_ATTEMPTS = 3
SECURITY_DELAY_S = 10.0

ROUTINE_SELF_TEST = 0x0203
ROUTINE_COMM_CHECK = 0x0301
ROUTINE_ERASE = 0xFF00


@dataclass(frozen=True)
class DidWriteRule:
    """Session and security conditions for a writable DID."""

    sessions: tuple[int, ...]
    requires_unlock: bool = True
    required_session: int | None = None
    max_length: int | None = None


@dataclass
class VirtualRoutine:
    """A small stateful routine used by the virtual ECU."""

    sessions: tuple[int, ...] = (SESSION_EXTENDED, SESSION_PROGRAMMING)
    requires_unlock: bool = False
    polls_until_complete: int = 1
    result: bytes = b"\x00"
    state: str = "idle"
    polls: int = 0

    def start(self, data: bytes) -> bytes:
        del data
        self.state = "running"
        self.polls = 0
        return b""

    def stop(self) -> bytes:
        self.state = "idle"
        self.polls = 0
        return b"\x00"

    def request_results(self) -> tuple[str, bytes]:
        if self.state == "complete":
            return self.state, self.result
        if self.state != "running":
            return self.state, b""

        self.polls += 1
        if self.polls >= self.polls_until_complete:
            self.state = "complete"
            return self.state, self.result
        return "running", b""


def default_write_rules() -> dict[int, DidWriteRule]:
    return {
        DID_CUSTOM_DATA: DidWriteRule(
            (SESSION_EXTENDED, SESSION_PROGRAMMING), True, None, 16
        ),
        DID_DTC_STATUS: DidWriteRule(
            (SESSION_EXTENDED, SESSION_PROGRAMMING), True, None, 4
        ),
        DID_VIN: DidWriteRule(
            (SESSION_EXTENDED, SESSION_PROGRAMMING), True, SESSION_PROGRAMMING, 17
        ),
        # Virtual-only fault hook, intentionally usable without a session switch.
        DID_FAULT_CONTROL: DidWriteRule(
            (SESSION_DEFAULT, SESSION_EXTENDED, SESSION_PROGRAMMING), False, None, 16
        ),
    }


def default_routines() -> dict[int, VirtualRoutine]:
    return {
        ROUTINE_SELF_TEST: VirtualRoutine(polls_until_complete=2, result=b"\x00"),
        ROUTINE_COMM_CHECK: VirtualRoutine(polls_until_complete=1, result=b"\x00"),
        ROUTINE_ERASE: VirtualRoutine(
            requires_unlock=True, polls_until_complete=2, result=b"\x00"
        ),
    }


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
        writable_dids: dict[int, DidWriteRule] | set[int] | None = None,
        routines: dict[int, VirtualRoutine] | None = None,
        supported_sessions: tuple[int, ...] = (0x01, 0x02, 0x03),
        s3_timeout_s: float = 5.0,
        security_salt: int = DEFAULT_SECURITY_SALT,
    ) -> None:
        if s3_timeout_s <= 0:
            raise ValueError("s3_timeout_s must be positive")

        supported_sessions = tuple(supported_sessions)
        self.name = name
        self.bus = bus
        self.health = CanHealthMonitor()
        self.transport = IsoTpTransport(
            bus, txid=response_id, rxid=request_id, timeout=0.5
        )
        self.supported_sessions = supported_sessions
        self.session = SESSION_DEFAULT
        self.s3_timeout_s = s3_timeout_s
        self.session_timeout_count = 0
        self._s3_deadline: float | None = None
        if dids is None:
            self.dids = default_dids(self.name)
        else:
            self.dids = dict(dids)
        self.dids.setdefault(CAN_HEALTH_DID, bytes(7))
        self.writable_dids = self._coerce_write_rules(writable_dids)
        self.routines = dict(routines) if routines is not None else default_routines()
        self.security_salt = security_salt & 0xFFFFFFFF
        self._seed_generator = SeedGenerator()
        self._pending_seed: int | None = None
        self._security_unlocked = False
        self._security_attempts = 0
        self._security_delay_until = 0.0
        self.fault = EcuFaultState()
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None

    @staticmethod
    def _coerce_write_rules(
        rules: dict[int, DidWriteRule] | set[int] | None,
    ) -> dict[int, DidWriteRule]:
        if rules is None:
            return default_write_rules()
        if isinstance(rules, set):
            return {
                did: DidWriteRule(
                    (SESSION_EXTENDED, SESSION_PROGRAMMING), False, None, None
                )
                for did in rules
            }
        return dict(rules)

    @property
    def security_unlocked(self) -> bool:
        return self._security_unlocked

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
            self._expire_session(time.monotonic())
            try:
                request = self.transport.receive(timeout=0.1)
            except TimeoutError:
                continue
            response = self._handle(request)
            if response is not None:
                self.transport.send(response)

    def _handle(self, request: bytes) -> bytes | None:
        now = time.monotonic()
        self._expire_session(now)
        if now < self.fault.comm_loss_until or now < self.fault.bus_off_until:
            return None
        if self.fault.bus_off_until:
            self.fault.bus_off_until = 0.0
            self.health.mark_recovered()
        if not request:
            return self._negative(0x00, 0x10)

        service = request[0]
        if service in self.fault.nrc_for_service:
            return self._negative(service, self.fault.nrc_for_service[service])

        if service == SID_DIAGNOSTIC_SESSION_CONTROL:
            return self._session_control(request, now)
        if service == SID_TESTER_PRESENT:
            response = self._tester_present(request, now)
        elif service == SID_READ_DATA_BY_IDENTIFIER:
            response = self._read_did(request)
        elif service == SID_SECURITY_ACCESS:
            response = self._security_access(request, now)
        elif service == SID_WRITE_DATA_BY_IDENTIFIER:
            response = self._write_did(request)
        elif service == SID_ROUTINE_CONTROL:
            response = self._routine_control(request)
        else:
            response = self._negative(service, 0x11)

        if (
            response is not None
            and response[0] != SID_NEGATIVE_RESPONSE
            and service != SID_DIAGNOSTIC_SESSION_CONTROL
        ):
            self._keep_session_alive(now)
        return response

    def _session_control(self, request: bytes, now: float) -> bytes | None:
        if len(request) < 2:
            return self._negative(SID_DIAGNOSTIC_SESSION_CONTROL, 0x13)
        subfunction = request[1]
        session = subfunction & 0x7F
        if session not in self.supported_sessions:
            return self._negative(SID_DIAGNOSTIC_SESSION_CONTROL, 0x12)

        if session != self.session:
            self._lock_security()
        self.session = session
        if session == SESSION_DEFAULT:
            self._s3_deadline = None
        else:
            self._s3_deadline = now + self.s3_timeout_s

        if subfunction & 0x80:
            return None
        return bytes([0x50, session, 0x00, 0x32, 0x01, 0xF4])

    def _tester_present(self, request: bytes, now: float) -> bytes | None:
        if len(request) < 2:
            return self._negative(SID_TESTER_PRESENT, 0x13)
        subfunction = request[1]
        if subfunction & 0x7F:
            return self._negative(SID_TESTER_PRESENT, 0x12)
        self._keep_session_alive(now)
        if subfunction & 0x80:
            return None
        return bytes([0x7E, 0x00])

    def _security_access(self, request: bytes, now: float) -> bytes:
        if len(request) < 2:
            return self._negative(SID_SECURITY_ACCESS, 0x13)

        subfunction = request[1] & 0x7F
        suppress = bool(request[1] & 0x80)
        if subfunction not in (SECURITY_REQUEST_SEED, SECURITY_SEND_KEY):
            return self._negative(SID_SECURITY_ACCESS, 0x12)
        if self.session not in (SESSION_EXTENDED, SESSION_PROGRAMMING):
            return self._negative(SID_SECURITY_ACCESS, 0x7E)

        if now < self._security_delay_until:
            return self._negative(SID_SECURITY_ACCESS, 0x37)

        if subfunction == SECURITY_REQUEST_SEED:
            if self._security_unlocked:
                response = bytes([0x67, SECURITY_REQUEST_SEED]) + bytes(4)
            else:
                seed = self._seed_generator.next_seed()
                self._pending_seed = seed
                response = bytes([0x67, SECURITY_REQUEST_SEED]) + seed.to_bytes(4, "big")
            return None if suppress else response

        if len(request) != 6:
            return self._negative(SID_SECURITY_ACCESS, 0x13)
        if self._pending_seed is None:
            return self._negative(SID_SECURITY_ACCESS, 0x24)

        key = int.from_bytes(request[2:6], "big")
        expected = seed_to_key(self._pending_seed, self.security_salt)
        self._pending_seed = None
        if key != expected:
            self._security_attempts += 1
            if self._security_attempts >= SECURITY_MAX_ATTEMPTS:
                self._security_attempts = 0
                self._security_delay_until = now + SECURITY_DELAY_S
                return self._negative(SID_SECURITY_ACCESS, 0x36)
            return self._negative(SID_SECURITY_ACCESS, 0x35)

        self._security_unlocked = True
        self._security_attempts = 0
        response = bytes([0x67, SECURITY_SEND_KEY])
        return None if suppress else response

    def _lock_security(self) -> None:
        self._security_unlocked = False
        self._pending_seed = None
        self._security_attempts = 0

    def _keep_session_alive(self, now: float) -> None:
        if self.session != SESSION_DEFAULT:
            self._s3_deadline = now + self.s3_timeout_s

    def _expire_session(self, now: float) -> None:
        if self._s3_deadline is not None and now >= self._s3_deadline:
            self.session = SESSION_DEFAULT
            self._s3_deadline = None
            self.session_timeout_count += 1
            self._lock_security()

    def _read_did(self, request: bytes) -> bytes:
        if len(request) < 3:
            return self._negative(SID_READ_DATA_BY_IDENTIFIER, 0x13)
        did = int.from_bytes(request[1:3], "big")
        if did not in self.dids:
            return self._negative(SID_READ_DATA_BY_IDENTIFIER, 0x31)
        value = self._health_payload() if did == CAN_HEALTH_DID else self.dids[did]
        return bytes([0x62]) + request[1:3] + value

    def _write_did(self, request: bytes) -> bytes:
        if len(request) < 3:
            return self._negative(SID_WRITE_DATA_BY_IDENTIFIER, 0x13)
        did = int.from_bytes(request[1:3], "big")
        value = request[3:]
        rule = self.writable_dids.get(did)
        if rule is None:
            return self._negative(SID_WRITE_DATA_BY_IDENTIFIER, 0x31)
        if self.session not in rule.sessions:
            return self._negative(SID_WRITE_DATA_BY_IDENTIFIER, 0x7E)
        if rule.requires_unlock and not self._security_unlocked:
            return self._negative(SID_WRITE_DATA_BY_IDENTIFIER, 0x33)
        if rule.required_session is not None and self.session != rule.required_session:
            return self._negative(SID_WRITE_DATA_BY_IDENTIFIER, 0x22)
        if rule.max_length is not None and len(value) > rule.max_length:
            return self._negative(SID_WRITE_DATA_BY_IDENTIFIER, 0x31)

        if did == FAULT_CONTROL_DID:
            self._apply_fault(value)
        self.dids[did] = value
        return bytes([0x6E]) + request[1:3]

    def _routine_control(self, request: bytes) -> bytes:
        if len(request) < 4:
            return self._negative(SID_ROUTINE_CONTROL, 0x13)

        subfunction = request[1] & 0x7F
        suppress = bool(request[1] & 0x80)
        routine_id = int.from_bytes(request[2:4], "big")
        routine = self.routines.get(routine_id)
        if routine is None:
            return self._negative(SID_ROUTINE_CONTROL, 0x31)
        if subfunction not in (
            ROUTINE_CONTROL_START,
            ROUTINE_CONTROL_STOP,
            ROUTINE_CONTROL_REQUEST_RESULTS,
        ):
            return self._negative(SID_ROUTINE_CONTROL, 0x12)
        if self.session not in routine.sessions:
            return self._negative(SID_ROUTINE_CONTROL, 0x7E)
        if routine.requires_unlock and not self._security_unlocked:
            return self._negative(SID_ROUTINE_CONTROL, 0x33)

        if subfunction == ROUTINE_CONTROL_START:
            if routine.state == "running":
                return self._negative(SID_ROUTINE_CONTROL, 0x22)
            payload = routine.start(request[4:])
            response = bytes([0x71, ROUTINE_CONTROL_START]) + request[2:4] + payload
        elif subfunction == ROUTINE_CONTROL_STOP:
            if routine.state != "running":
                return self._negative(SID_ROUTINE_CONTROL, 0x24)
            payload = routine.stop()
            response = bytes([0x71, ROUTINE_CONTROL_STOP]) + request[2:4] + payload
        else:
            if routine.state == "idle":
                return self._negative(SID_ROUTINE_CONTROL, 0x24)
            state, payload = routine.request_results()
            if state == "running":
                return self._negative(SID_ROUTINE_CONTROL, 0x78)
            response = (
                bytes([0x71, ROUTINE_CONTROL_REQUEST_RESULTS])
                + request[2:4]
                + payload
            )

        return None if suppress else response

    def _apply_fault(self, value: bytes) -> None:
        if not value:
            return
        mode = value[0]
        if mode == 0x00:
            if self.health.state == CanBusState.BUS_OFF:
                self.health.mark_recovered()
            self.fault = EcuFaultState()
        elif mode == 0x01 and len(value) >= 5:
            self.dids[DTC_STATUS_DID] = value[1:5]
        elif mode == 0x02 and len(value) >= 2:
            self.fault.comm_loss_until = time.monotonic() + value[1]
        elif mode == 0x03 and len(value) >= 2:
            duration_s = self._fault_duration(value)
            self.fault.bus_off_until = time.monotonic() + duration_s
            self.health.enter_bus_off()
        elif mode == 0x04 and len(value) >= 3:
            self.fault.nrc_for_service[value[1]] = value[2]

    def _negative(self, service: int, nrc: int) -> bytes:
        return bytes([SID_NEGATIVE_RESPONSE, service, nrc])

    def _health_payload(self) -> bytes:
        snapshot = self.health.snapshot()
        state_code = {
            CanBusState.ERROR_ACTIVE: 0,
            CanBusState.ERROR_PASSIVE: 1,
            CanBusState.BUS_OFF: 2,
        }[snapshot.state]
        error_total = min(sum(snapshot.error_counts.values()), 0xFF)
        return bytes(
            [
                state_code,
                min(snapshot.tec, 0xFF),
                min(snapshot.rec, 0xFF),
                min(snapshot.bus_off_events, 0xFF),
                min(snapshot.recovery_successes, 0xFF),
                min(snapshot.recovery_failures, 0xFF),
                error_total,
            ]
        )

    @staticmethod
    def _fault_duration(value: bytes) -> float:
        if len(value) >= 3:
            return int.from_bytes(value[1:3], "big") / 1000.0
        return float(value[1])
