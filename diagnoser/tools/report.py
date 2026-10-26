"""Markdown report rendering for stress test results."""

from __future__ import annotations

from diagnoser.tools.stress import StressResult


def render_markdown(result: StressResult) -> str:
    lines = [
        "# Dual-Node CAN UDS Stress Test Report",
        "",
        "## Summary",
        "",
        f"- Requests per node: {result.requests_per_node}",
        f"- Total requests: {result.total_requests}",
        f"- Request interval: {result.request_interval_ms:.1f} ms",
        f"- Success: {result.success}",
        f"- Timeouts: {result.timeouts}",
        f"- Errors: {result.errors}",
        f"- UDS timeout rate: {result.uds_timeout_rate * 100:.4f}%",
        f"- UDS failure rate: {result.failure_rate * 100:.4f}%",
        f"- Target bus load: {result.target_load * 100:.1f}%",
        f"- Generator load (sender-side): {result.measured_load * 100:.1f}%",
        f"- Duration: {result.duration_s:.3f}s",
        "",
        "## Per Node",
        "",
        "| Node | Sent | OK | Timeout | Error | Avg RTT | Max RTT |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for name, metric in sorted(result.nodes.items()):
        lines.append(
            f"| {name} | {metric.sent} | {metric.ok} | "
            f"{metric.timeouts} | {metric.errors} | "
            f"{metric.avg_rtt_s * 1000:.2f} ms | {metric.max_rtt_s * 1000:.2f} ms |"
        )
    lines.extend(["", "## Error Samples", ""])
    samples = [
        f"- {name}: {sample}"
        for name, metric in sorted(result.nodes.items())
        for sample in metric.error_samples
    ]
    lines.extend(samples or ["- None"])
    lines.extend(
        [
            "",
            "## Pass Criteria",
            "",
            "- UDS requests completed without timeout: "
            + ("PASS" if result.timeouts == 0 else "SEE DETAILS"),
            "- Physical CAN frame loss: separate probe/capture required.",
            "",
            "## Notes",
            "",
            "- Generator load is calculated from frames submitted by the load thread.",
            "- UDS failure rate must not be reported as physical CAN frame loss.",
            "- Bus-Off recovery is validated separately with fault injection.",
            "- NRC coverage is tracked in the diagnostic negative-response matrix.",
        ]
    )
    return "\n".join(lines) + "\n"
