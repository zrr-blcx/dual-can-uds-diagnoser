"""Single-node UDS smoke workflow for virtual or physical CAN."""

from __future__ import annotations

from dataclasses import dataclass

from diagnoser.uds.client import UdsClient
from diagnoser.uds.constants import SESSION_EXTENDED
from diagnoser.uds.dids import DID_CUSTOM_DATA, DID_HARDWARE_VERSION, DID_VIN
from diagnoser.uds.errors import NegativeResponse, UdsError


@dataclass(frozen=True)
class UdsSmokeStep:
    name: str
    passed: bool
    detail: str


@dataclass(frozen=True)
class UdsSmokeResult:
    node: str
    steps: tuple[UdsSmokeStep, ...]

    @property
    def passed(self) -> bool:
        return all(step.passed for step in self.steps)


def run_uds_smoke(client: UdsClient, node: str) -> UdsSmokeResult:
    steps: list[UdsSmokeStep] = []

    try:
        response = client.change_session(SESSION_EXTENDED)
        steps.append(
            UdsSmokeStep(
                "session-extended",
                response[:1] == bytes([SESSION_EXTENDED]),
                response.hex(" ").upper(),
            )
        )
    except (TimeoutError, UdsError) as exc:
        steps.append(UdsSmokeStep("session-extended", False, str(exc)))

    for name, did in (
        ("read-vin", DID_VIN),
        ("read-hardware-version", DID_HARDWARE_VERSION),
        ("read-custom-data", DID_CUSTOM_DATA),
    ):
        try:
            response = client.read_data_by_identifier(did)
            passed = response[:2] == did.to_bytes(2, "big")
            steps.append(
                UdsSmokeStep(
                    name,
                    passed,
                    response.hex(" ").upper(),
                )
            )
        except (TimeoutError, UdsError) as exc:
            steps.append(UdsSmokeStep(name, False, str(exc)))

    try:
        client.read_data_by_identifier(0xFFFF)
        steps.append(UdsSmokeStep("unknown-did-nrc", False, "no negative response"))
    except NegativeResponse as exc:
        steps.append(
            UdsSmokeStep(
                "unknown-did-nrc",
                exc.service == 0x22 and exc.nrc == 0x31,
                f"NRC 0x{exc.nrc:02X}",
            )
        )
    except (TimeoutError, UdsError) as exc:
        steps.append(UdsSmokeStep("unknown-did-nrc", False, str(exc)))

    return UdsSmokeResult(node=node, steps=tuple(steps))


def render_uds_smoke(result: UdsSmokeResult) -> str:
    lines = [
        "# Single-Node UDS Smoke Test",
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
