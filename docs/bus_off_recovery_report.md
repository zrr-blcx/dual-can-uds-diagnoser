# Bus-Off Recovery Test (Virtual CAN)

## Environment

| Item | Value |
| --- | --- |
| Date | 2026-09-13 |
| Interface | python-can MemoryBus / VirtualEcu |
| Node | ecu1 |
| Injection duration | 50 ms per cycle |
| Recovery probe timeout | 25 ms |
| Command | `python -m diagnoser.cli --timeout 0.2 bus-off-test --cycles 3 --seconds 0.05` |

## Result

- Node: ecu1
- Requested cycles: 3
- Completed cycles: 3
- Outcome: PASS

| Cycle | Offline observed | Recovered | Recovery time |
| ---: | --- | --- | ---: |
| 1 | Yes | Yes | 82.5 ms |
| 2 | Yes | Yes | 89.7 ms |
| 3 | Yes | Yes | 75.6 ms |

## CAN Health At End

- State: error-active
- Bus-Off events: 3
- Recovery successes: 3
- Recovery failures: 0
- Recorded CAN errors: 0

> This is a VirtualBus validation result. Real relay-triggered
> Bus-Off recovery remains a hardware acceptance item.

The recovery times include test-loop polling latency. They are not a measurement
of the ISO 11898 128-recessive-bit hardware recovery interval.
