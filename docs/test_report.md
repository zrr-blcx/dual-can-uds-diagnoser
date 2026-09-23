# Test Report Template

> Fill in measured results after hardware testing. Values marked Target are not
> yet measured.

## Environment

| Item | Value |
| --- | --- |
| Date | - |
| Tester | - |
| Firmware version | - |
| Diagnostic tool version | - |
| CAN bitrate | 500 kbps |
| Transceivers | SN65HVD230 x2 |
| USB-CAN | CANable slcan |

## Metrics Definition

- Physical frame loss rate: frames sent on bus vs frames observed/ACKed.
- UDS request timeout rate: diagnostic requests without a valid response.
- Response time: request send to positive/negative response receive.
- Bus load: theoretical and measured percentage of bus bit time.

## Baseline Test

| Node | Requests | Success | Timeout | Error | Loss rate | Avg RTT | Max RTT |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| ecu1 | - | - | - | - | - | - | - |
| ecu2 | - | - | - | - | - | - | - |

## 90%+ Load Stress Test

| Node | Requests | Success | Timeout | Error | Loss rate | Avg RTT | Max RTT |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| ecu1 | - | - | - | - | - | - | - |
| ecu2 | - | - | - | - | - | - | - |

## 10,000 Frame Final Test

| Node | Frames | Loss rate target | Result |
| --- | ---: | ---: | --- |
| ecu1 | 5000 | < 0.1% | Target |
| ecu2 | 5000 | < 0.1% | Target |

## NRC Coverage Matrix

| NRC | Scenario | Expected | Result |
| --- | --- | --- | --- |
| 0x11 | Unsupported service | PASS | Target |
| 0x12 | Unsupported subfunction | PASS | Target |
| 0x13 | Wrong message length | PASS | Target |
| 0x22 | Conditions not correct | PASS | Target |
| 0x31 | Request out of range | PASS | Target |
| 0x33 | Security access denied | PASS | Target |
| 0x78 | Response pending | PASS | Target |

## Bus-Off Recovery

| Test | Expected | Result |
| --- | --- | --- |
| Trigger Bus-Off with relay | Node enters Bus-Off | Target |
| Software reinit via HAL_CAN_Start | Node returns to normal mode | Target |
| Recovery time | Measured in ms | Target |
| Recover 3 times consecutively | Stable | Target |

## Virtual Bus-Off Validation

Measured on 2026-09-13 with `python-can` MemoryBus and VirtualEcu:

| Test | Expected | Result |
| --- | --- | --- |
| Inject Bus-Off for 50 ms | Node stops responding | PASS, 3/3 cycles |
| Automatically restore communication | Node responds again | PASS, 3/3 cycles |
| Recover 3 times consecutively | Stable | PASS |
| Observed recovery interval | Report probe latency | 75.6 ms to 89.7 ms |

The observed interval includes CLI polling latency and is not a hardware
acceptance measurement. See `docs/bus_off_recovery_report.md`. Real relay and
STM32 validation remains Target.

## Automated Regression

| Date | Python | Command | Result |
| --- | --- | --- | --- |
| 2026-09-20 | 3.12.13 (`.venv`) | `python -m pytest` | `36 passed`, 1 cache-permission warning |

The `py` launcher was unavailable in the current Windows shell, so the project
virtual environment was used for the complete regression. The warning came from
writing `.pytest_cache`; no test case failed or was skipped.

## Firmware Build Validation

| Date | Target | Toolchain | Artifact | Result |
| --- | --- | --- | --- | --- |
| 2026-09-20 | STM32F407ZGT6 | GNU ARM GCC 14.3.1 | `firmware/ecu-f407/Build/ecu-f407.elf` | PASS, text 10196 B |
| 2026-09-20 | STM32F103C8T6 | GNU ARM GCC 14.3.1 | `firmware/ecu-f103/Build/ecu-f103.elf` | PASS, text 8692 B |

The F103 image was programmed, verified, and reset through ST-Link. The F407
image was also programmed successfully after reconnecting its SWD target.
An initial dual-node CAN exchange was observed. After the intermittent physical
bus connection was corrected, repeated tests confirmed stable bidirectional
traffic.

## Week 1 CAN Baseline

Measured on 2026-09-20 with both programmed nodes connected to the same
500 kbps CAN bus. RAM counters were cleared, both MCUs were reset, and the
counters were read back through ST-Link.

| Node | First read TX/RX | Later read TX/RX | Result |
| --- | --- | --- | --- |
| F407 | 16 / 15 | 33 / 30, 54 / 58, 219 / 222 | PASS |
| F103 | 16 / 17 | 33 / 30, 63 / 59, 224 / 221 | PASS |

Both RX counters increased continuously across repeated reads, which confirms
that frames were acknowledged and received in both directions. F407 reported
`ESR=0`, and F103 reported no active protocol error flags. The common CAN
driver, CubeIDE project metadata, IOC configuration, and filter-bank plan are
complete.

### Week 1 Final Repeat Test

Three consecutive synchronized-reset cycles were run after all Week 1
deliverables were closed.

| Cycle | F407 TX/RX | F103 TX/RX | F407 ESR | F103 ESR | Result |
| ---: | ---: | ---: | ---: | ---: | --- |
| 1 | 6 / 6 | 6 / 7 | 0x00000000 | 0x00000000 | PASS |
| 2 | 6 / 6 | 6 / 6 | 0x00000000 | 0x00000000 | PASS |
| 3 | 6 / 6 | 6 / 6 | 0x00000000 | 0x00000000 | PASS |

The Week 1 hardware and CAN baseline milestone is complete.

## Conclusion

- Physical frame loss below 0.1% at normal load: Target.
- UDS timeout handling verified: Target.
- Bus-Off recovery conforms to ISO 11898: Target.
