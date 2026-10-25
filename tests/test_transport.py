from diagnoser.config import NetworkConfig
from diagnoser.transport import prepare_usb_backend, resolve_channel


def test_gs_usb_numeric_channel_becomes_index() -> None:
    config = NetworkConfig(interface="gs_usb", channel="0")

    assert resolve_channel(config) == 0


def test_gs_usb_device_name_is_preserved() -> None:
    config = NetworkConfig(interface="gs_usb", channel="canable-1")

    assert resolve_channel(config) == "canable-1"


def test_slcan_serial_port_stays_a_string() -> None:
    config = NetworkConfig(interface="slcan", channel="COM_TEST")

    assert resolve_channel(config) == "COM_TEST"


def test_prepare_usb_backend_is_idempotent() -> None:
    prepare_usb_backend()
    prepare_usb_backend()
