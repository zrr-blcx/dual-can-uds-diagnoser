"""Generate C structs and pack/unpack functions from a DBC database."""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import cantools


@dataclass(frozen=True)
class GeneratedCode:
    header: str
    source: str


class DbcCodeGenerator:
    """Turn cantools message/signal definitions into dependency-free C code."""

    @staticmethod
    def from_file(path: str | Path) -> GeneratedCode:
        database = cantools.database.load_file(str(path))
        return DbcCodeGenerator.generate(database)

    @staticmethod
    def generate(database: Any, *, header_name: str = "project_dbc.h") -> GeneratedCode:
        guard = _guard_name(header_name)
        header = [
            "/* Auto-generated from a DBC file. Do not edit by hand. */",
            f"#ifndef {guard}",
            f"#define {guard}",
            "",
            "#include <stdint.h>",
            "",
        ]
        source = [
            "/* Auto-generated from a DBC file. Do not edit by hand. */",
            f'#include "{Path(header_name).name}"',
            "",
            "static int64_t project_dbc_round(double value)",
            "{",
            "  return (int64_t)((value >= 0.0) ? (value + 0.5) : (value - 0.5));",
            "}",
            "",
        ]

        for message in database.messages:
            message_name = _c_identifier(message.name)
            struct_name = f"{message_name}_t"
            header.append("typedef struct")
            header.append("{")
            for signal in message.signals:
                header.append(
                    f"  {_field_type(signal)} {_c_identifier(signal.name)};"
                )
            header.append(f"}} {struct_name};")
            header.append("")
            header.append(
                f"void {message_name}_pack(const {struct_name} *message, "
                "uint8_t data[8]);"
            )
            header.append(
                f"void {message_name}_unpack(const uint8_t data[8], "
                f"{struct_name} *message);"
            )
            header.append("")
            source.extend(_message_source(message, message_name, struct_name))
            source.append("")

        header.extend([f"#endif /* {guard} */", ""])
        return GeneratedCode(header="\n".join(header), source="\n".join(source))


def write_generated_files(code: GeneratedCode, header: str | Path, source: str | Path) -> None:
    header_path = Path(header)
    source_path = Path(source)
    header_path.parent.mkdir(parents=True, exist_ok=True)
    source_path.parent.mkdir(parents=True, exist_ok=True)
    header_path.write_text(code.header, encoding="utf-8", newline="\n")
    source_path.write_text(code.source, encoding="utf-8", newline="\n")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Generate C structs and pack/unpack functions from a DBC file"
    )
    parser.add_argument("--dbc", default="dbc/project.dbc")
    parser.add_argument("--header", default="firmware/common/project_dbc.h")
    parser.add_argument("--source", default="firmware/common/project_dbc.c")
    args = parser.parse_args(argv)

    header_name = Path(args.header).name
    database = cantools.database.load_file(args.dbc)
    code = DbcCodeGenerator.generate(database, header_name=header_name)
    write_generated_files(code, args.header, args.source)
    print(f"generated {args.header} and {args.source}")
    return 0


def _message_source(message: Any, message_name: str, struct_name: str) -> list[str]:
    lines = [
        f"void {message_name}_pack(const {struct_name} *message, uint8_t data[8])",
        "{",
        "  uint64_t be_word = 0U;",
        "  uint64_t le_word = 0U;",
        "",
    ]
    for signal in message.signals:
        field = _c_identifier(signal.name)
        shift = _signal_shift(signal)
        mask = _mask_literal(signal.length)
        scale = _number_literal(signal.scale)
        offset = _number_literal(signal.offset)
        neg_offset = _number_literal(-float(signal.offset))
        raw_expr = (
            f"project_dbc_round((((double)message->{field}) + ({neg_offset})) "
            f"/ ({scale}))"
        )
        if signal.is_signed:
            raw_expr = f"(uint64_t)(int64_t)({raw_expr})"
        else:
            raw_expr = f"(uint64_t)({raw_expr})"
        word = "be_word" if signal.byte_order == "big_endian" else "le_word"
        lines.append(
            f"  {word} |= (({raw_expr}) & {mask}) << {shift}U;"
        )

    lines.extend(
        [
            "",
            f"  for (uint32_t index = 0U; index < {message.length}U; index++)",
            "  {",
            "    const uint8_t be_byte =",
            "        (uint8_t)((be_word >> (8U * (7U - index))) & 0xFFU);",
            "    const uint8_t le_byte =",
            "        (uint8_t)((le_word >> (8U * index)) & 0xFFU);",
            "    data[index] = (uint8_t)(be_byte | le_byte);",
            "  }",
            "}",
            "",
            f"void {message_name}_unpack(const uint8_t data[8], {struct_name} *message)",
            "{",
            "  uint64_t be_word = 0U;",
            "  uint64_t le_word = 0U;",
            f"  for (uint32_t index = 0U; index < {message.length}U; index++)",
            "  {",
            "    be_word = (be_word << 8U) | data[index];",
            "    le_word |= ((uint64_t)data[index]) << (8U * index);",
            "  }",
            "",
        ]
    )
    for signal in message.signals:
        field = _c_identifier(signal.name)
        shift = _signal_shift(signal)
        mask = _mask_literal(signal.length)
        scale = _number_literal(signal.scale)
        offset = _number_literal(signal.offset)
        word = "be_word" if signal.byte_order == "big_endian" else "le_word"
        lines.append(f"  {{")
        lines.append(f"    uint64_t raw = ({word} >> {shift}U) & {mask};")
        if signal.is_signed and signal.length < 64:
            sign = _sign_literal(signal.length)
            extension = _extension_literal(signal.length)
            lines.append(
                f"    if ((raw & {sign}) != 0U) {{ raw |= {extension}; }}"
            )
        if signal.is_signed:
            lines.append(
                f"    message->{field} = ({_field_type(signal)})"
                f"((double)(int64_t)raw * {scale} + {offset});"
            )
        else:
            lines.append(
                f"    message->{field} = ({_field_type(signal)})"
                f"((double)raw * {scale} + {offset});"
            )
        lines.append("  }")
    lines.append("}")
    return lines


def _signal_shift(signal: Any) -> int:
    if signal.byte_order == "big_endian":
        network_start = 8 * (signal.start // 8) + (7 - (signal.start % 8))
        shift = 64 - (network_start + signal.length)
    else:
        shift = signal.start

    if shift < 0 or shift + signal.length > 64:
        raise ValueError(
            f"signal {signal.name!r} does not fit in one 64-bit CAN payload"
        )
    return shift


def _field_type(signal: Any) -> str:
    if signal.scale == 1 and signal.offset == 0:
        if signal.is_signed:
            return "int8_t" if signal.length <= 8 else (
                "int16_t" if signal.length <= 16 else (
                    "int32_t" if signal.length <= 32 else "int64_t"
                )
            )
        return "uint8_t" if signal.length <= 8 else (
            "uint16_t" if signal.length <= 16 else (
                "uint32_t" if signal.length <= 32 else "uint64_t"
            )
        )
    return "double"


def _mask_literal(length: int) -> str:
    if length >= 64:
        return "0xFFFFFFFFFFFFFFFFULL"
    return f"0x{(1 << length) - 1:0{(length + 3) // 4}X}ULL"


def _sign_literal(length: int) -> str:
    return f"0x{1 << (length - 1):X}ULL"


def _extension_literal(length: int) -> str:
    mask = (1 << length) - 1
    value = ((1 << 64) - 1) & ~mask
    return f"0x{value:016X}ULL"


def _number_literal(value: float | int) -> str:
    number = float(value)
    if number == int(number):
        return f"{int(number)}.0"
    return repr(number)


def _guard_name(header_name: str) -> str:
    stem = Path(header_name).stem
    return re.sub(r"[^0-9A-Za-z]+", "_", stem).upper() + "_H"


def _c_identifier(name: str) -> str:
    value = re.sub(r"[^0-9A-Za-z_]", "_", name)
    value = re.sub(r"_+", "_", value).strip("_") or "signal"
    if value[0].isdigit():
        value = f"_{value}"
    return value


if __name__ == "__main__":
    raise SystemExit(main())
