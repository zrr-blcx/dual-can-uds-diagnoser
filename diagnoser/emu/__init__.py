"""Software-emulated CAN nodes for hardware-free validation."""

from diagnoser.emu.memory_bus import MemoryBus
from diagnoser.emu.virtual_ecu import VirtualEcu

__all__ = ["MemoryBus", "VirtualEcu"]
