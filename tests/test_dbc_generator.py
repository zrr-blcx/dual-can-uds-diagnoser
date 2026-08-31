from diagnoser.dbc.bits import decode_signal, encode_signal
from diagnoser.dbc.generator import (
    CanMessage,
    CanNetwork,
    CanSignal,
    DbcGenerator,
    SignalStructGenerator,
)


def _network() -> CanNetwork:
    return CanNetwork(
        nodes=["ECU_A", "ECU_B"],
        messages=[
            CanMessage(
                arbitration_id=0x180,
                name="EngineData",
                dlc=8,
                sender="ECU_A",
                signals=[
                    CanSignal("EngineSpeed", 0, 16, "big_endian", 0.25, 0.0, 0.0, 16383.75, "rpm"),
                    CanSignal("CoolantTemp", 16, 8, "big_endian", 1.0, -40.0, -40.0, 215.0, "degC"),
                ],
            ),
            CanMessage(
                arbitration_id=0x280,
                name="VehicleStatus",
                dlc=8,
                sender="ECU_B",
                signals=[
                    CanSignal("VehicleSpeed", 0, 16, "little_endian", 0.01, 0.0, 0.0, 655.35, "km/h"),
                    CanSignal("BrakeActive", 16, 1, "little_endian"),
                ],
            ),
        ],
    )


def test_dbc_generation_contains_messages_and_signals() -> None:
    text = DbcGenerator.generate(_network())
    assert "BU_: ECU_A ECU_B" in text
    assert "BO_ 384 EngineData: 8 ECU_A" in text
    assert "SG_ EngineSpeed" in text
    assert "SG_ VehicleSpeed" in text


def test_python_struct_generation() -> None:
    code = SignalStructGenerator.generate_python(_network())
    assert "class EngineData:" in code
    assert "class VehicleStatus:" in code
    assert "decode_signal_value" in code
    assert "encode_signal_value" in code


def test_c_struct_generation() -> None:
    code = SignalStructGenerator.generate_c(_network())
    assert "typedef struct {" in code
    assert "EngineData_t;" in code
    assert "float EngineSpeed;" in code


def test_signal_encode_decode_roundtrip_big_endian() -> None:
    signal = CanSignal("Speed", 0, 16, "big_endian", 0.25, 0.0)
    data = bytearray(8)
    encode_signal(data, signal.start_bit, signal.length, 1000, signal.byte_order)
    assert decode_signal(bytes(data), signal.start_bit, signal.length, signal.byte_order) == 1000


def test_signal_encode_decode_roundtrip_little_endian() -> None:
    signal = CanSignal("Speed", 8, 12, "little_endian")
    data = bytearray(8)
    encode_signal(data, signal.start_bit, signal.length, 0xABC, signal.byte_order)
    assert decode_signal(bytes(data), signal.start_bit, signal.length, signal.byte_order) == 0xABC


