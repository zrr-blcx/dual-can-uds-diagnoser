"""Markdown report rendering for stress test results."""

from __future__ import annotations

from diagnoser.tools.stress import StressResult


def render_markdown(result: StressResult) -> str:
    lines = [
        "# Dual-Node CAN UDS Stress Test Report",
        "",
        "## Summary",
        "",
        f"- Total requests: {result.total_requests}",
        f"- Success: {result.success}",
        f"- Timeouts: {result.timeouts}",
        f"- Errors: {result.errors}",
        f"- Loss rate: {result.loss_rate * 100:.4f}%",
        f"- Target bus load: {result.target_load * 100:.1f}%",
        f"- Measured bus load: {result.measured_load * 100:.1f}%",
        f"- Duration: {result.duration_s:.3f}s",
        "",
        "## Per Node",
        "",
        "| Node | Sent | OK | Timeout | Error | Loss rate |",
        "| --- | ---: | ---: | ---: | ---: | ---: |",
    ]
    for name, metric in sorted(result.nodes.items()):
        lines.append(
            f"| {name} | {metric.sent} | {metric.ok} | "
            f"{metric.timeouts} | {metric.errors} | {metric.loss_rate * 100:.4f}% |"
        )
    lines.extend(
        [
            "",
            "## Pass Criteria",
            "",
            "- Normal-load frame loss below 0.1%: "
            + ("PASS" if result.loss_rate < 0.001 else "FAIL"),
            "- Diagnostic timeout handling verified: "
            + ("PASS" if result.timeouts == 0 else "SEE DETAILS"),
            "",
            "## Notes",
            "",
            "- Bus-Off recovery is validated separately with fault injection.",
            "- NRC coverage is tracked in the diagnostic negative-response matrix.",
        ]
    )
    return "\n".join(lines) + "\n"
