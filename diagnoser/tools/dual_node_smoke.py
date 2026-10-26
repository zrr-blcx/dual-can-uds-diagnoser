"""Read-only shared-bus smoke workflow for two physical diagnostic clients."""

from __future__ import annotations

from dataclasses import dataclass

from diagnoser.tools.scheduler import QueuedDualNodeScheduler, SchedulerNode
from diagnoser.uds.client import UdsClient
from diagnoser.uds.dids import DID_ECU_NAME


@dataclass(frozen=True)
class NodeSmokeResult:
    name: str
    requests: int
    success: int
    response_id: int | None
    session_restored: bool
    error: str | None = None

    @property
    def passed(self) -> bool:
        return (
            self.success == self.requests
            and self.session_restored
            and self.error is None
        )


@dataclass(frozen=True)
class DualNodeSmokeResult:
    requests_per_node: int
    did: int
    nodes: tuple[NodeSmokeResult, ...]

    @property
    def passed(self) -> bool:
        return bool(self.nodes) and all(node.passed for node in self.nodes)


def run_dual_node_smoke(
    clients: dict[str, UdsClient],
    *,
    requests_per_node: int = 20,
    did: int = DID_ECU_NAME,
) -> DualNodeSmokeResult:
    if requests_per_node <= 0:
        raise ValueError("requests_per_node must be positive")
    if not clients:
        raise ValueError("at least one diagnostic client is required")

    scheduler = QueuedDualNodeScheduler(
        [SchedulerNode(name=name) for name in clients],
        default_retries=0,
    )

    def exercise_node(name: str, client: UdsClient) -> NodeSmokeResult:
        success = 0
        response_ids: set[int] = set()
        session_restored = False
        error: str | None = None
        try:
            client.change_session(0x03)
            for _ in range(requests_per_node):
                payload = client.read_data_by_identifier(did)
                expected_did = did.to_bytes(2, "big")
                if not payload.startswith(expected_did):
                    raise ValueError(
                        f"expected DID {did:04X}, got {payload.hex(' ')}"
                    )
                success += 1
                exchange = client.last_exchange
                if exchange is not None and exchange.rx_id is not None:
                    response_ids.add(exchange.rx_id)
        except Exception as exc:  # noqa: BLE001 - recorded in the report
            error = f"{type(exc).__name__}: {exc}"
        finally:
            try:
                client.change_session(0x01)
                session_restored = True
            except Exception as exc:  # noqa: BLE001 - recorded in the report
                if error is None:
                    error = f"restore default session: {type(exc).__name__}: {exc}"

        expected_response = getattr(client.transport, "rxid", None)
        if expected_response is not None and response_ids != {expected_response}:
            observed = ", ".join(f"0x{item:X}" for item in sorted(response_ids))
            response_error = (
                f"unexpected response IDs: {observed or 'none'}; "
                f"expected 0x{expected_response:X}"
            )
            error = f"{error}; {response_error}" if error else response_error

        response_id = next(iter(response_ids)) if len(response_ids) == 1 else None
        return NodeSmokeResult(
            name=name,
            requests=requests_per_node,
            success=success,
            response_id=response_id,
            session_restored=session_restored,
            error=error,
        )

    futures = {
        name: scheduler.submit(name, lambda name=name, client=client: exercise_node(name, client))
        for name, client in clients.items()
    }
    try:
        nodes = []
        for name, future in futures.items():
            try:
                outcome = future.result(timeout=15.0)
            except Exception as exc:  # noqa: BLE001 - scheduler failure
                nodes.append(
                    NodeSmokeResult(
                        name=name,
                        requests=requests_per_node,
                        success=0,
                        response_id=None,
                        session_restored=False,
                        error=f"{type(exc).__name__}: {exc}",
                    )
                )
                continue

            if outcome.status == "ok" and isinstance(outcome.value, NodeSmokeResult):
                nodes.append(outcome.value)
            else:
                nodes.append(
                    NodeSmokeResult(
                        name=name,
                        requests=requests_per_node,
                        success=0,
                        response_id=None,
                        session_restored=False,
                        error=outcome.error or f"scheduler status {outcome.status}",
                    )
                )
    finally:
        scheduler.close()

    return DualNodeSmokeResult(
        requests_per_node=requests_per_node,
        did=did,
        nodes=tuple(nodes),
    )


def render_dual_node_smoke(result: DualNodeSmokeResult) -> str:
    lines = [
        "# Dual-Node Shared-Bus Smoke Test",
        "",
        f"- DID: 0x{result.did:04X}",
        f"- Requests per node: {result.requests_per_node}",
        f"- Outcome: {'PASS' if result.passed else 'FAIL'}",
        "",
        "| Node | Success | Response ID | Session restored | Error |",
        "| --- | ---: | --- | --- | --- |",
    ]
    for node in result.nodes:
        response_id = (
            f"0x{node.response_id:X}" if node.response_id is not None else "-"
        )
        lines.append(
            f"| {node.name} | {node.success}/{node.requests} | {response_id} | "
            f"{'Yes' if node.session_restored else 'No'} | {node.error or '-'} |"
        )
    return "\n".join(lines) + "\n"
