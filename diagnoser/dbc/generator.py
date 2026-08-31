"""Generate DBC files and signal structure code from message definitions."""

from __future__ import annotations

import re
from dataclasses import dataclass, field

from diagnoser.dbc.bits import decode_signal_value, encode_signal_value


@dataclass
class CanSignal:
    name: str
    start_bit: int
    length: int
    byte_order: str = "big_endian"
    scale: float = 1.0
    offset: float = 0.0
    min_value: float = 0.0
    max_value: float = 0.0
    unit: str = ""
    is_signed: bool = False

    def decode(self, data: bytes) -> float:
        return decode_signal_value(
            data,
            self.start_bit,
            self.length,
            self.byte_order,
            self.is_signed,
            self.scale,
            self.offset,
        )

    def encode(self, data: bytearray, value: float) -> None:
        encode_signal_value(
            data,
            self.start_bit,
            self.length,
            value,
            self.byte_order,
            self.is_signed,
            self.scale,
            self.offset,
        )


@dataclass
class CanMessage:
    arbitration_id: int
    name: str
    dlc: int
    sender: str
    signals: list[CanSignal] = field(default_factory=list)


@dataclass
class CanNetwork:
    nodes: list[str]
    messages: list[CanMessage]
    version: str = ""


class DbcGenerator:
    @staticmethod
    def generate(network: CanNetwork) -> str:
        lines = [
            f'VERSION "{network.version}"',
            "",
            "NS_ :",
            "    NS_DESC_",
            "    CM_",
            "    BA_DEF_",
            "    BA_",
            "    VAL_",
            "    CAT_DEF_",
            "    CAT_",
            "    FILTER",
            "    BA_DEF_DEF_",
            "    EV_DATA_",
            "    ENVVAR_DATA_",
            "    SGTYPE_",
            "    SGTYPE_VAL_",
            "    BA_DEF_SGTYPE_",
            "    BA_SGTYPE_",
            "    SIG_TYPE_REF_",
            "    VAL_TABLE_",
            "    SIG_GROUP_",
            "    SIG_VALTYPE_",
            "    SIGTYPE_VALTYPE_",
            "    BO_TX_BU_",
            "    BA_DEF_REL_",
            "    BA_REL_",
            "    BA_DEF_DEF_REL_",
            "    BU_SG_REL_",
            "    BU_EV_REL_",
            "    BU_BO_REL_",
            "    SG_MUL_VAL_",
            "",
            "BS_:",
            "",
        ]
        if network.nodes:
            lines.append(f"BU_: {' '.join(network.nodes)}")
        else:
            lines.append("BU_:")
        lines.append("")

        for message in sorted(network.messages, key=lambda m: m.arbitration_id):
            lines.append(
                f"BO_ {message.arbitration_id} {_sanitize_name(message.name)}: "
                f"{message.dlc} {_sanitize_name(message.sender)}"
            )
            for signal in sorted(message.signals, key=lambda s: s.start_bit):
                lines.append(f" {DbcGenerator._signal_line(signal)}")
        lines.append("")
        return "\n".join(lines)

    @staticmethod
    def _signal_line(signal: CanSignal) -> str:
        byte_order = "1" if signal.byte_order == "big_endian" else "0"
        signed = "+" if not signal.is_signed else "-"
        name = _sanitize_name(signal.name)
        return (
            f"SG_ {name} : {signal.start_bit}|{signal.length}@{byte_order}{signed} "
            f"({signal.scale:g},{signal.offset:g}) "
            f"[{signal.min_value:g}|{signal.max_value:g}] "
            f'"{signal.unit}" {_sanitize_name("Vector__XXX")}'
        )


class SignalStructGenerator:
    @staticmethod
    def generate_python(network: CanNetwork) -> str:
        lines = [
            '"""Auto-generated CAN signal structs. Do not edit by hand."""',
            "",
            "from dataclasses import dataclass",
            "",
            "from diagnoser.dbc.bits import decode_signal_value, encode_signal_value",
            "",
        ]
        for message in network.messages:
            struct_name = _camel_case(message.name)
            fields = [
                f"    {_sanitize_name(s.name)}: {_python_type(s)} = {_python_default(s)}"
                for s in message.signals
            ]
            lines.append("@dataclass")
            lines.append(f"class {struct_name}:")
            if fields:
                lines.extend(fields)
            else:
                lines.append("    pass")
            lines.append("")

            lines.append(f"    @classmethod")
            lines.append(f"    def from_bytes(cls, data: bytes) -> '{struct_name}':")
            lines.append(f"        return cls(")
            for signal in message.signals:
                lines.append(
                    f"            {_sanitize_name(signal.name)}="
                    f"{_decode_expr(signal)},"
                )
            lines.append("        )")
            lines.append("")

            lines.append("    def to_bytes(self) -> bytes:")
            lines.append(f"        data = bytearray({message.dlc})")
            for signal in message.signals:
                lines.append(
                    f"        encode_signal_value(data, {signal.start_bit}, "
                    f"{signal.length}, self.{_sanitize_name(signal.name)}, "
                    f"'{signal.byte_order}', {str(signal.is_signed).lower()}, "
                    f"{signal.scale:g}, {signal.offset:g})"
                )
            lines.append("        return bytes(data)")
            lines.append("")
        return "\n".join(lines)

    @staticmethod
    def generate_c(network: CanNetwork) -> str:
        lines = ["#pragma once", "", "#include <stdint.h>", ""]
        for message in network.messages:
            lines.append(f"typedef struct {{")
            for signal in message.signals:
                lines.append(
                    f"    {_c_type(signal)} {_sanitize_name(signal.name)};"
                )
            lines.append(f"}} {_camel_case(message.name)}_t;")
            lines.append("")
        return "\n".join(lines)


def _decode_expr(signal: CanSignal) -> str:
    return (
        f"decode_signal_value(data, {signal.start_bit}, {signal.length}, "
        f"'{signal.byte_order}', {str(signal.is_signed).lower()}, "
        f"{signal.scale:g}, {signal.offset:g})"
    )


def _python_type(signal: CanSignal) -> str:
    if signal.scale == 1.0 and signal.offset == 0.0:
        return "int"
    return "float"


def _python_default(signal: CanSignal) -> str:
    return "0" if _python_type(signal) == "int" else "0.0"


def _c_type(signal: CanSignal) -> str:
    if signal.scale != 1.0 or signal.offset != 0.0:
        return "float"
    if signal.length <= 8:
        return "int8_t" if signal.is_signed else "uint8_t"
    if signal.length <= 16:
        return "int16_t" if signal.is_signed else "uint16_t"
    if signal.length <= 32:
        return "int32_t" if signal.is_signed else "uint32_t"
    return "int64_t" if signal.is_signed else "uint64_t"


def _sanitize_name(name: str) -> str:
    value = re.sub(r"[^0-9a-zA-Z_]", "_", name)
    value = re.sub(r"_+", "_", value).strip("_")
    if not value:
        return "signal"
    if value[0].isdigit():
        value = f"_{value}"
    return value


def _camel_case(name: str) -> str:
    parts = _sanitize_name(name).split("_")
    return "".join(part[:1].upper() + part[1:] for part in parts if part)
