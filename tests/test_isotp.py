import pytest

from diagnoser.transport.isotp import (
    encode_first_frame,
    pack_message,
    unpack_frames,
)


@pytest.mark.parametrize("length", [1, 7, 8, 63, 100, 255, 4095])
def test_pack_unpack_roundtrip(length: int) -> None:
    data = bytes((i * 7 + 3) & 0xFF for i in range(length))
    frames = pack_message(data)
    assert unpack_frames(frames) == data


def test_single_frame_pci() -> None:
    frame = pack_message(b"\x10\x03")[0]
    assert frame[0] == 0x02
    assert frame[1:3] == b"\x10\x03"
    assert len(frame) == 8


def test_multi_frame_pci() -> None:
    frames = pack_message(bytes(range(8)))
    assert (frames[0][0] >> 4) == 0x1
    total_length = ((frames[0][0] & 0x0F) << 8) | frames[0][1]
    assert total_length == 8
    assert (frames[1][0] >> 4) == 0x2
    assert frames[1][0] & 0x0F == 0x1


def test_first_frame_rejects_oversized_payload() -> None:
    with pytest.raises(ValueError):
        encode_first_frame(4096, b"")
