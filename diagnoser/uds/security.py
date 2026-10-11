"""Seed/key helpers shared by the virtual ECU and the diagnostic client.

The Week 5 transform is a deterministic project placeholder. Production
firmware must replace the salt and the transform with an OEM-defined secret;
the wire protocol and the security state machine stay the same.
"""

from __future__ import annotations

SECURITY_LEVEL_1 = 0x01
SECURITY_REQUEST_SEED = 0x01
SECURITY_SEND_KEY = 0x02

SECURITY_SEED_SIZE = 4
SECURITY_KEY_SIZE = 4
DEFAULT_SECURITY_SALT = 0x50324341
DEFAULT_SEED = 0x12345678


def seed_to_key(seed: int, salt: int = DEFAULT_SECURITY_SALT) -> int:
    """Return the Week 5 demo key for a 32-bit seed."""

    value = (seed ^ salt) & 0xFFFFFFFF
    value = ((value << 13) | (value >> 19)) & 0xFFFFFFFF
    value = (value + 0x9E3779B9) & 0xFFFFFFFF
    value ^= value >> 16
    return value & 0xFFFFFFFF


class SeedGenerator:
    """Deterministic xorshift seed source for host tests and the simulator."""

    def __init__(self, initial: int = DEFAULT_SEED) -> None:
        self._state = (initial & 0xFFFFFFFF) or DEFAULT_SEED

    def next_seed(self) -> int:
        value = self._state
        value ^= (value << 13) & 0xFFFFFFFF
        value ^= value >> 17
        value ^= (value << 5) & 0xFFFFFFFF
        value &= 0xFFFFFFFF
        self._state = value or DEFAULT_SEED
        if value in (0, 0xFFFFFFFF):
            return DEFAULT_SEED
        return value
