from __future__ import annotations

from diagnoser.tools.trace import TraceFrame
from diagnoser.transport.isotp import encode_flow_control, pack_message


def test_summarize_100_byte_transfer_with_two_flow_control_blocks() -> None:
    from diagnoser.tools.isotp_trace import summarize_isotp_transfer

    payload = bytes(range(100))
    can_frames = pack_message(payload)
    frames: list[TraceFrame] = [
        TraceFrame(
            timestamp=0.000,
            arbitration_id=0x7E1,
            dlc=8,
            data=can_frames[0],
        )
    ]
    frames.append(
        TraceFrame(
            timestamp=0.001,
            arbitration_id=0x7E9,
            dlc=8,
            data=encode_flow_control(block_size=8, stmin=20),
        )
    )
    for index, data in enumerate(can_frames[1:9], start=1):
        frames.append(
            TraceFrame(
                timestamp=0.001 + index * 0.020,
                arbitration_id=0x7E1,
                dlc=8,
                data=data,
            )
        )
    frames.append(
        TraceFrame(
            timestamp=0.170,
            arbitration_id=0x7E9,
            dlc=8,
            data=encode_flow_control(block_size=8, stmin=20),
        )
    )
    for index, data in enumerate(can_frames[9:], start=9):
        frames.append(
            TraceFrame(
                timestamp=0.170 + (index - 8) * 0.020,
                arbitration_id=0x7E1,
                dlc=8,
                data=data,
            )
        )

    summary = summarize_isotp_transfer(
        frames,
        target_id=0x7E1,
        flow_control_id=0x7E9,
    )

    assert summary.complete
    assert summary.payload == payload
    assert summary.consecutive_frames == 14
    assert summary.flow_control_count == 2
    assert summary.flow_block_sizes == [8, 8]
    assert summary.flow_stmin_ms == [20, 20]
    assert summary.min_cf_gap_ms is not None
    assert summary.min_cf_gap_ms >= 19.0
