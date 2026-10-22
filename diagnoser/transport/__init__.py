"""CAN bus and ISO-TP transport layers."""

from __future__ import annotations

import ctypes.util
import os
from pathlib import Path

import can

from diagnoser.config import NetworkConfig
from diagnoser.transport.isotp import IsoTpTransport

__all__ = ["IsoTpTransport", "create_bus", "prepare_usb_backend", "resolve_channel"]

# candleLight/gs_usb adapters are USB devices selected by numeric index.
USB_INDEX_INTERFACES = frozenset({"gs_usb"})


def resolve_channel(config: NetworkConfig) -> object:
    """Return the channel value the configured python-can interface expects.

    Network configs store every channel as a string, while candleLight
    (gs_usb) adapters are selected by numeric index. Numeric channels are
    converted back to int for those interfaces; a non-numeric channel is kept
    as-is so it can still match a device name.
    """

    channel: object = config.channel
    if config.interface in USB_INDEX_INTERFACES and isinstance(channel, str):
        stripped = channel.strip()
        if stripped.isdigit():
            return int(stripped)
    return channel


def _bundled_libusb_path() -> Path | None:
    """Return the libusb library shipped with the ``libusb`` wheel, if present."""

    try:
        from libusb._platform import DLL_PATH
    except Exception:
        return None

    path = Path(str(DLL_PATH))
    return path if path.is_file() else None


def prepare_usb_backend() -> None:
    """Point pyusb at the libusb library bundled with the ``libusb`` package.

    pyusb resolves ``libusb-1.0.dll`` through the DLL search path, but the
    ``libusb`` wheel keeps the library inside its own package directory. Without
    this hand-off, candleLight (``gs_usb``) adapters fail with
    ``NoBackendError`` instead of reporting a missing device.
    """

    try:
        import usb.backend.libusb1 as libusb1
    except ImportError:
        return

    library = _bundled_libusb_path()
    if library is None:
        return

    if hasattr(os, "add_dll_directory"):
        try:
            os.add_dll_directory(str(library.parent))
        except OSError:
            pass
    os.environ["PATH"] = f"{library.parent}{os.pathsep}{os.environ.get('PATH', '')}"

    libusb1.get_backend(
        find_library=lambda name: ctypes.util.find_library(name) or str(library)
    )


def create_bus(config: NetworkConfig) -> can.BusABC:
    if config.interface in USB_INDEX_INTERFACES:
        prepare_usb_backend()
    kwargs: dict[str, object] = {
        "interface": config.interface,
        "channel": resolve_channel(config),
        "bitrate": config.baudrate,
    }
    return can.Bus(**kwargs)
