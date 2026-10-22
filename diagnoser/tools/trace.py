"""Raw CAN frame tracing and per-ID inter-frame gap measurement."""

from __future__ import annotations

import time
from collections.abc import Iterable
from dataclasses import dataclass, field

import can

from diagnoser.tools.console import timestamp_text

DEFAULT_TRACE_ID = 0x123

# (timestamp, arbitration_id, dlc, data, is_extended_id)
RawFrame = tuple[float, int, int, bytes, bool]


@dataclass(frozen=True)
class TraceFrame:
    timestamp: float
    arbitration_id: int
    dlc: int
    data: bytes
    is_extended_id: bool = False
    gap_ms: float | None = None

    def as_dict(self) -> dict[str, object]:
        return {
            "timestamp": timestamp_text(self.timestamp),
            "time": self.timestamp,
            "id": f"0x{self.arbitration_id:X}",
            "dlc": self.dlc,
            "data": self.data.hex(" ").upper(),
            "extended": self.is_extended_id,
            "gap_ms": None if self.gap_ms is None else round(self.gap_ms, 3),
        }

    def format_line(self) -> str:
        gap = "-" if self.gap_ms is None else f"{self.gap_ms:.1f} ms"
        frame_type = "EXT" if self.is_extended_id else "STD"
        return (
            f"[{timestamp_text(self.timestamp)}] {frame_type} "
            f"0x{self.arbitration_id:X} DLC {self.dlc} "
            f"{self.data.hex(' ').upper()} | gap {gap}"
        )


@dataclass
class IdStats:
    arbitration_id: int
    frames: int = 0
    first_time: float | None = None
    last_time: float | None = None
    gaps: int = 0
    gap_sum_ms: float = 0.0
    min_gap_ms: float | None = None
    max_gap_ms: float | None = None

    @property
    def average_gap_ms(self) -> float | None:
        if self.gaps == 0:
            return None
        return self.gap_sum_ms / self.gaps


@dataclass
class TraceResult:
    window_s: float = 0.0
    frames: list[TraceFrame] = field(default_factory=list)
    stats: dict[int, IdStats] = field(default_factory=dict)

    @property
    def duration_s(self) -> float:
        if len(self.frames) < 2:
            return 0.0
        return self.frames[-1].timestamp - self.frames[0].timestamp

    @property
    def largest_gap(self) -> TraceFrame | None:
        measured = [frame for frame in self.frames if frame.gap_ms is not None]
        if not measured:
            return None
        return max(measured, key=lambda frame: frame.gap_ms or 0.0)


def summarize_frames(raw_frames: Iterable[RawFrame], *, window_s: float = 0.0) -> TraceResult:
    """Build per-ID gap statistics from raw frame records.

    Gaps are measured between consecutive frames that share the same
    arbitration ID, so unrelated bus traffic cannot shorten a Bus-Off window.
    """

    result = TraceResult(window_s=window_s)
    last_seen: dict[int, float] = {}

    for timestamp, arbitration_id, dlc, data, is_extended in raw_frames:
        previous = last_seen.get(arbitration_id)
        gap_ms = None if previous is None else (timestamp - previous) * 1000.0
        last_seen[arbitration_id] = timestamp

        result.frames.append(
            TraceFrame(
                timestamp=timestamp,
                arbitration_id=arbitration_id,
                dlc=dlc,
                data=bytes(data),
                is_extended_id=bool(is_extended),
                gap_ms=gap_ms,
            )
        )

        stat = result.stats.get(arbitration_id)
        if stat is None:
            stat = IdStats(arbitration_id=arbitration_id)
            result.stats[arbitration_id] = stat
        stat.frames += 1
        if stat.first_time is None:
            stat.first_time = timestamp
        stat.last_time = timestamp
        if gap_ms is not None:
            stat.gaps += 1
            stat.gap_sum_ms += gap_ms
            stat.min_gap_ms = (
                gap_ms if stat.min_gap_ms is None else min(stat.min_gap_ms, gap_ms)
            )
            stat.max_gap_ms = (
                gap_ms if stat.max_gap_ms is None else max(stat.max_gap_ms, gap_ms)
            )

    return result


def run_trace(bus: can.BusABC, *, seconds: float, max_frames: int | None = None) -> TraceResult:
    """Collect raw frames from a bus for a bounded capture window."""

    deadline = time.monotonic() + max(0.0, seconds)
    raw: list[RawFrame] = []

    while True:
        if max_frames is not None and len(raw) >= max_frames:
            break
        remaining = deadline - time.monotonic()
        if remaining <= 0.0:
            break
        message = bus.recv(timeout=min(0.1, remaining))
        if message is None:
            continue
        timestamp = message.timestamp if message.timestamp is not None else time.time()
        raw.append(
            (
                timestamp,
                message.arbitration_id,
                message.dlc,
                bytes(message.data),
                bool(message.is_extended_id),
            )
        )

    return summarize_frames(raw, window_s=seconds)


def _gap_text(value: float | None) -> str:
    return "-" if value is None else f"{value:.1f}"


def render_trace(result: TraceResult, *, focus_id: int | None = None) -> str:
    """Render the trace summary as Markdown."""

    lines = [
        "# CAN Trace Summary",
        "",
        f"- Capture window: {result.window_s:.1f} s",
        f"- Frames observed: {len(result.frames)}",
        f"- Observed span: {result.duration_s:.3f} s",
    ]

    if not result.frames:
        lines.extend(["", "No frames were observed in the capture window.", ""])
        return "\n".join(lines)

    lines.extend(
        [
            "",
            "| ID | Frames | Min gap (ms) | Avg gap (ms) | Max gap (ms) |",
            "| --- | ---: | ---: | ---: | ---: |",
        ]
    )
    for arbitration_id, stat in sorted(result.stats.items()):
        lines.append(
            f"| 0x{arbitration_id:X} | {stat.frames} | "
            f"{_gap_text(stat.min_gap_ms)} | {_gap_text(stat.average_gap_ms)} | "
            f"{_gap_text(stat.max_gap_ms)} |"
        )

    largest = result.largest_gap
    if largest is not None and largest.gap_ms is not None:
        lines.extend(
            [
                "",
                f"- Largest same-ID gap: {largest.gap_ms:.1f} ms on "
                f"0x{largest.arbitration_id:X}",
                "- Use that value as the offline/recovery window when tracing "
                "the cyclic frame.",
            ]
        )

    if focus_id is not None:
        stat = result.stats.get(focus_id)
        if stat is None:
            lines.append(f"- Focus ID 0x{focus_id:X}: no frames observed")
        else:
            lines.append(
                f"- Focus ID 0x{focus_id:X}: {stat.frames} frames, "
                f"max gap {_gap_text(stat.max_gap_ms)} ms"
            )

    lines.append("")
    return "\n".join(lines)
