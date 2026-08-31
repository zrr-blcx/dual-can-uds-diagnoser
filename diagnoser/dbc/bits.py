"""Bit-level signal decode/encode helpers shared by DBC tooling."""

from __future__ import annotations


def decode_signal(
    data: bytes,
    start_bit: int,
    length: int,
    byte_order: str = "big_endian",
) -> int:
    if length <= 0:
        return 0
    if byte_order == "little_endian":
        first_byte = start_bit // 8
        offset = start_bit % 8
        raw = int.from_bytes(data[first_byte:], "little") >> offset
        return raw & ((1 << length) - 1)

    value = 0
    for i in range(length):
        position = start_bit + i
        byte = data[position // 8]
        bit = 7 - (position % 8)
        value = (value << 1) | ((byte >> bit) & 1)
    return value


def encode_signal(
    data: bytearray,
    start_bit: int,
    length: int,
    value: int,
    byte_order: str = "big_endian",
) -> None:
    if length <= 0:
        return
    if byte_order == "little_endian":
        for i in range(length):
            position = start_bit + i
            byte_index = position // 8
            if byte_index >= len(data):
                break
            bit_in_byte = position % 8
            if (value >> i) & 1:
                data[byte_index] |= 1 << bit_in_byte
            else:
                data[byte_index] &= ~(1 << bit_in_byte)
        return

    for i in range(length):
        position = start_bit + i
        byte_index = position // 8
        if byte_index >= len(data):
            break
        bit_in_byte = 7 - (position % 8)
        value_bit = (value >> (length - 1 - i)) & 1
        if value_bit:
            data[byte_index] |= 1 << bit_in_byte
        else:
            data[byte_index] &= ~(1 << bit_in_byte)


def decode_signal_value(
    data: bytes,
    start_bit: int,
    length: int,
    byte_order: str = "big_endian",
    is_signed: bool = False,
    scale: float = 1.0,
    offset: float = 0.0,
) -> float:
    raw = decode_signal(data, start_bit, length, byte_order)
    if is_signed and raw & (1 << (length - 1)):
        raw -= 1 << length
    return raw * scale + offset


def encode_signal_value(
    data: bytearray,
    start_bit: int,
    length: int,
    value: float,
    byte_order: str = "big_endian",
    is_signed: bool = False,
    scale: float = 1.0,
    offset: float = 0.0,
) -> None:
    raw = int(round((value - offset) / scale))
    if is_signed and raw < 0:
        raw &= (1 << length) - 1
    encode_signal(data, start_bit, length, raw, byte_order)
