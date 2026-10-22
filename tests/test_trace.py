import threading
import time

import can
import pytest

from diagnoser.cli import main
from diagnoser.emu.memory_bus import MemoryBus
from diagnoser.tools.trace import render_trace, summarize_frames


def test_summarize_measures_same_id_gaps() -> None:
    raw = [
        (0.000, 0x123, 1, b"\x00", False),
        (0.500, 0x123, 1, b"\x01", False),
        (1.400, 0x123, 1, b"\x00", False),
    ]

    result = summarize_frames(raw, window_s=2.0)

    stat = result.stats[0x123]
    assert stat.frames == 3
    assert stat.gaps == 2
    assert stat.min_gap_ms == pytest.approx(500.0)
    assert stat.max_gap_ms == pytest.approx(900.0)
    assert stat.average_gap_ms == pytest.approx(700.0)
    assert result.duration_s == pytest.approx(1.400)
    assert result.largest_gap is not None
    assert result.largest_gap.gap_ms == pytest.approx(900.0)


def test_summarize_keeps_gaps_per_id() -> None:
    raw = [
        (0.0, 0x123, 1, b"\x00", False),
        (0.1, 0x7E8, 8, bytes(8), False),
        (0.2, 0x123, 1, b"\x01", False),
        (0.3, 0x7E8, 8, bytes(8), False),
    ]

    result = summarize_frames(raw)

    assert result.frames[0].gap_ms is None
    assert result.stats[0x123].max_gap_ms == pytest.approx(200.0)
    assert result.stats[0x7E8].max_gap_ms == pytest.approx(200.0)


def test_render_trace_reports_focus_id_and_largest_gap() -> None:
    raw = [
        (0.0, 0x123, 1, b"\x00", False),
        (0.9, 0x123, 1, b"\x01", False),
    ]

    text = render_trace(summarize_frames(raw, window_s=1.0), focus_id=0x123)

    assert "| 0x123 | 2 |" in text
    assert "Largest same-ID gap: 900.0 ms on 0x123" in text
    assert "Focus ID 0x123: 2 frames" in text


def test_render_trace_without_frames() -> None:
    text = render_trace(summarize_frames([]))

    assert "No frames were observed" in text


def test_frame_line_includes_gap() -> None:
    frame = summarize_frames([(0.0, 0x123, 1, b"\x00", False)]).frames[0]

    assert "0x123" in frame.format_line()
    assert "gap -" in frame.format_line()

    second = summarize_frames(
        [(0.0, 0x123, 1, b"\x00", False), (0.25, 0x123, 1, b"\x01", False)]
    ).frames[1]
    assert "gap 250.0 ms" in second.format_line()


def test_cli_trace_memory_smoke(capsys) -> None:
    channel = "trace-cli-smoke"
    sender = MemoryBus(channel, node_id=0x999)

    def drive() -> None:
        time.sleep(0.2)
        for _ in range(3):
            sender.send(can.Message(arbitration_id=0x123, data=b"\x01"))
            time.sleep(0.05)

    thread = threading.Thread(target=drive, daemon=True)
    thread.start()
    try:
        code = main(
            [
                "--interface",
                "memory",
                "--channel",
                channel,
                "trace",
                "--seconds",
                "0.8",
                "--id",
                "0x123",
                "--quiet",
            ]
        )
    finally:
        thread.join(timeout=2.0)
        sender.shutdown()

    out = capsys.readouterr().out
    assert code == 0
    assert "| 0x123 | 3 |" in out
    assert "Focus ID 0x123: 3 frames" in out
