"""Week 5 end-to-end 0x27/0x2E/0x31 workflow for virtual or physical CAN."""

from __future__ import annotations

from dataclasses import dataclass

from diagnoser.uds.client import UdsClient
from diagnoser.uds.constants import (
    ROUTINE_CONTROL_REQUEST_RESULTS,
    ROUTINE_CONTROL_START,
    SESSION_DEFAULT,
    SESSION_EXTENDED,
)
from diagnoser.uds.dids import DID_CUSTOM_DATA
from diagnoser.uds.errors import NegativeResponse, UdsError
from diagnoser.uds.security import DEFAULT_SECURITY_SALT, seed_to_key

ROUTINE_SELF_TEST = 0x0203
ROUTINE_ERASE = 0xFF00
DEFAULT_WRITE_VALUE = b"\xDE\xAD\xBE\xEF"


@dataclass(frozen=True)
class UdsWeek5Step:
    name: str
    passed: bool
    detail: str


@dataclass(frozen=True)
class UdsWeek5Result:
    node: str
    steps: tuple[UdsWeek5Step, ...]

    @property
    def passed(self) -> bool:
        return all(step.passed for step in self.steps)


def _request_routine_results(
    client: UdsClient, routine_id: int, *, attempts: int = 4
) -> bytes:
    for _ in range(attempts):
        try:
            return client.routine_control(
                ROUTINE_CONTROL_REQUEST_RESULTS, routine_id
            )
        except NegativeResponse as exc:
            if exc.nrc != 0x78:
                raise
    raise TimeoutError(
        f"routine 0x{routine_id:04X} still pending after {attempts} polls"
    )


def run_uds_week5(
    client: UdsClient,
    node: str,
    *,
    write_value: bytes = DEFAULT_WRITE_VALUE,
) -> UdsWeek5Result:
    steps: list[UdsWeek5Step] = []
    seed_value: bytes | None = None

    def record(name: str, exchange) -> bool:
        try:
            detail = exchange()
            steps.append(UdsWeek5Step(name, True, detail))
            return True
        except (TimeoutError, UdsError, ValueError) as exc:
            steps.append(UdsWeek5Step(name, False, str(exc)))
            return False

    def session_extended() -> str:
        response = client.change_session(SESSION_EXTENDED)
        return response.hex(" ").upper()

    if not record("session-extended", session_extended):
        return UdsWeek5Result(node=node, steps=tuple(steps))

    def request_seed() -> str:
        nonlocal seed_value
        seed_value = client.request_seed()
        if len(seed_value) != 4 or seed_value == bytes(4):
            raise ValueError(f"invalid seed {seed_value.hex(' ').upper()}")
        return f"seed {seed_value.hex(' ').upper()}"

    if not record("security-request-seed", request_seed):
        return UdsWeek5Result(node=node, steps=tuple(steps))

    def send_key() -> str:
        assert seed_value is not None
        key = seed_to_key(int.from_bytes(seed_value, "big"), DEFAULT_SECURITY_SALT)
        response = client.send_key(key.to_bytes(4, "big"))
        return response.hex(" ").upper()

    if not record("security-send-key", send_key):
        return UdsWeek5Result(node=node, steps=tuple(steps))

    def write_did() -> str:
        response = client.write_data_by_identifier(DID_CUSTOM_DATA, write_value)
        return response.hex(" ").upper()

    if not record("write-did", write_did):
        return UdsWeek5Result(node=node, steps=tuple(steps))

    def read_back() -> str:
        response = client.read_data_by_identifier(DID_CUSTOM_DATA)
        if response[2:] != write_value:
            raise ValueError(
                f"read-back mismatch: {response[2:].hex(' ').upper()}"
            )
        return response.hex(" ").upper()

    if not record("read-back", read_back):
        return UdsWeek5Result(node=node, steps=tuple(steps))

    def self_test_start() -> str:
        response = client.routine_control(ROUTINE_CONTROL_START, ROUTINE_SELF_TEST)
        return response.hex(" ").upper()

    if not record("routine-self-test-start", self_test_start):
        return UdsWeek5Result(node=node, steps=tuple(steps))

    def self_test_results() -> str:
        response = _request_routine_results(client, ROUTINE_SELF_TEST)
        return response.hex(" ").upper()

    if not record("routine-self-test-results", self_test_results):
        return UdsWeek5Result(node=node, steps=tuple(steps))

    def erase_start() -> str:
        response = client.routine_control(ROUTINE_CONTROL_START, ROUTINE_ERASE)
        return response.hex(" ").upper()

    if not record("routine-erase-start", erase_start):
        return UdsWeek5Result(node=node, steps=tuple(steps))

    def erase_results() -> str:
        response = _request_routine_results(client, ROUTINE_ERASE)
        return response.hex(" ").upper()

    if not record("routine-erase-results", erase_results):
        return UdsWeek5Result(node=node, steps=tuple(steps))

    def default_session() -> str:
        response = client.change_session(SESSION_DEFAULT)
        return response.hex(" ").upper()

    if not record("session-default", default_session):
        return UdsWeek5Result(node=node, steps=tuple(steps))

    def denied_in_default() -> str:
        try:
            client.write_data_by_identifier(DID_CUSTOM_DATA, write_value)
        except NegativeResponse as exc:
            if exc.nrc != 0x7E:
                raise
            return f"NRC 0x{exc.nrc:02X}"
        raise ValueError("write was accepted in default session")

    record("write-denied-in-default", denied_in_default)

    def session_extended_again() -> str:
        response = client.change_session(SESSION_EXTENDED)
        return response.hex(" ").upper()

    if not record("session-extended-again", session_extended_again):
        return UdsWeek5Result(node=node, steps=tuple(steps))

    def denied_locked() -> str:
        try:
            client.write_data_by_identifier(DID_CUSTOM_DATA, write_value)
        except NegativeResponse as exc:
            if exc.nrc != 0x33:
                raise
            return f"NRC 0x{exc.nrc:02X}"
        raise ValueError("write was accepted while locked")

    record("write-denied-locked", denied_locked)
    return UdsWeek5Result(node=node, steps=tuple(steps))


def render_uds_week5(result: UdsWeek5Result) -> str:
    lines = [
        "# Week 5 UDS Security / Write / Routine Flow",
        "",
        f"- Node: {result.node}",
        f"- Outcome: {'PASS' if result.passed else 'FAIL'}",
        "",
        "| Step | Result | Response / Error |",
        "| --- | --- | --- |",
    ]
    lines.extend(
        f"| {step.name} | {'PASS' if step.passed else 'FAIL'} | {step.detail} |"
        for step in result.steps
    )
    return "\n".join(lines)
