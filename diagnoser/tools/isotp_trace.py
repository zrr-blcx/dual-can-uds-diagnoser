"""Summarize an observed ISO-TP transfer from raw CAN trace frames."""

from __future__ import annotations

from dataclasses import dataclass, field
from collections.abc import Iterable

from diagnoser.tools.trace import TraceFrame
from diagnoser.transport.isotp import PCI_CF, PCI_FC, PCI_FF, unpack_frames


@dataclass
class IsoTpTransferSummary:
    target_id: int
    flow_control_id: int
    complete: bool
    payload: bytes
    consecutive_frames: int
    flow_control_count: int
    flow_block_sizes: list[int] = field(default_factory=list)
    flow_stmin_ms: list[int] = field(default_factory=list)
    cf_gaps_ms: list[float] = field(default_factory=list)
    error: str | None = None

    @property
    def min_cf_gap_ms(self) -> float | None:
        return min(self.cf_gaps_ms) if self.cf_gaps_ms else None


def summarize_isotp_transfer(
    frames: Iterable[TraceFrame],
    *,
    target_id: int,
    flow_control_id: int,
) -> IsoTpTransferSummary:
    """Summarize the first FF/CF transfer on ``target_id``.

    Frames seen before the first FF are ignored, so a capture may begin in the
    middle of an earlier transfer.
    """

    observed = list(frames)
    first_index: int | None = None
    for index, frame in enumerate(observed):
        if (
            frame.arbitration_id == target_id
            and frame.data
            and ((frame.data[0] >> 4) & 0x0F) == PCI_FF
        ):
            first_index = index
            break

    if first_index is None:
        return IsoTpTransferSummary(
            target_id=target_id,
            flow_control_id=flow_control_id,
            complete=False,
            payload=b"",
            consecutive_frames=0,
            flow_control_count=0,
            error="no first frame observed",
        )

    first = observed[first_index]
    total_length = ((first.data[0] & 0x0F) << 8) | first.data[1]
    cf_frames: list[bytes] = []
    cf_ticks: list[float] = []
    block_sizes: list[int] = []
    stmin_values: list[int] = []
    error: str | None = None

    for frame in observed[first_index + 1 :]:
        if frame.arbitration_id == flow_control_id and frame.data:
            pci = (frame.data[0] >> 4) & 0x0F
            if pci == PCI_FC and len(frame.data) >= 3:
                block_sizes.append(frame.data[1])
                stmin_values.append(frame.data[2])
            continue

        if frame.arbitration_id != target_id or not frame.data:
            continue
        pci = (frame.data[0] >> 4) & 0x0F
        if pci != PCI_CF:
            continue
        cf_frames.append(frame.data)
        cf_ticks.append(frame.timestamp)
        if total_length <= 6 + sum(len(item) - 1 for item in cf_frames):
            break

    gaps_ms = [
        (current - previous) * 1000.0
        for previous, current in zip(cf_ticks, cf_ticks[1:])
    ]
    payload = b""
    try:
        payload = unpack_frames([first.data, *cf_frames])
    except ValueError as exc:
        error = str(exc)

    return IsoTpTransferSummary(
        target_id=target_id,
        flow_control_id=flow_control_id,
        complete=len(payload) == total_length,
        payload=payload,
        consecutive_frames=len(cf_frames),
        flow_control_count=len(block_sizes),
        flow_block_sizes=block_sizes,
        flow_stmin_ms=stmin_values,
        cf_gaps_ms=gaps_ms,
        error=error,
    )
