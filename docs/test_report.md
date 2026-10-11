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
| 0x11 | Unsupported service | PASS | Host PASS, HW Target |
| 0x12 | Unsupported subfunction | PASS | Host PASS, HW Target |
| 0x13 | Wrong message length | PASS | Host PASS, HW Target |
| 0x22 | Conditions not correct | PASS | Host PASS, HW Target |
| 0x24 | Request sequence error | PASS | Host PASS, HW Target |
| 0x31 | Request out of range | PASS | Host PASS, HW Target |
| 0x33 | Security access denied | PASS | Host PASS, HW Target |
| 0x35 | Invalid key | PASS | Host PASS, HW Target |
| 0x36 | Exceeded number of attempts | PASS | Host PASS, HW Target |
| 0x37 | Required time delay not expired | PASS | Host PASS, HW Target |
| 0x78 | Response pending | PASS | Host PASS, HW Target |
| 0x7E | Subfunction not supported in active session | PASS | Host PASS, HW Target |

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
| 2026-09-27 | 3.12.13 (`.venv`) | `python -m pytest` | `39 passed`, 1 cache-permission warning |
| 2026-10-04 | 3.12.13 (`.venv`) | `python -m pytest` | `48 passed`, 1 cache-permission warning |

The `py` launcher was unavailable in the current Windows shell, so the project
virtual environment was used for the complete regression. The warning came from
writing `.pytest_cache`; no test case failed or was skipped.

The 2026-10-04 run includes a C host test that compiles the firmware UDS
service layer (`uds_server.c`, `uds_reference_routines.c`, `uds_did_db.c`,
`uds_ecu.c`) with MSVC and executes the resulting binary. The test covers
session transitions, S3 timeout and refresh, `0x22` reads, `0x27` seed/key
state, `0x2E` write rules, `0x31` Start/Stop/RequestResults, `0x78` response
pending, F407/F103 DID differences, and the full NRC matrix.

## Firmware Build Validation

| Date | Target | Toolchain | Artifact | Result |
| --- | --- | --- | --- | --- |
| 2026-09-20 | STM32F407ZGT6 | GNU ARM GCC 14.3.1 | `firmware/ecu-f407/Build/ecu-f407.elf` | PASS, text 10196 B |
| 2026-09-20 | STM32F103C8T6 | GNU ARM GCC 14.3.1 | `firmware/ecu-f103/Build/ecu-f103.elf` | PASS, text 8692 B |
| 2026-09-27 | STM32F407ZGT6 | GNU ARM GCC 14.3.1 | `firmware/ecu-f407/Build/ecu-f407.elf` | PASS, text 10596 B |
| 2026-09-27 | STM32F103C8T6 | GNU ARM GCC 14.3.1 | `firmware/ecu-f103/Build/ecu-f103.elf` | PASS, text 9104 B |
| 2026-10-04 | STM32F407ZGT6 | GNU ARM GCC 14.3.1 | `firmware/ecu-f407/Build/ecu-f407.elf` | PASS, text 12808 B |
| 2026-10-04 | STM32F103C8T6 | GNU ARM GCC 14.3.1 | `firmware/ecu-f103/Build/ecu-f103.elf` | PASS, text 11304 B |

The F103 image was programmed, verified, and reset through ST-Link. The F407
image was also programmed successfully after reconnecting its SWD target.
An initial dual-node CAN exchange was observed. After the intermittent physical
bus connection was corrected, repeated tests confirmed stable bidirectional
traffic.

## Week 4 UDS Host Validation

Measured on 2026-09-27 with the portable C UDS core:

| Test | Expected | Result |
| --- | --- | --- |
| Extended session response | `50 03 00 32 01 F4` | PASS |
| S3server timeout | Return to Default after 5000 ms | PASS |
| Tester Present refresh | Reset the S3 deadline | PASS |
| Read DID `0xF190/0xF193/0x0100` | Positive response with configured data | PASS |
| Read unknown DID | `7F 22 31` | PASS |
| Invalid request length | `7F <SID> 13` | PASS |
| Unsupported service | `7F <SID> 11` | PASS |

The single-node memory-bus smoke workflow also passed:

```text
python -m diagnoser.cli uds-smoke --node ecu1
```

The workflow verified `10 03`, three standard DID reads, and unknown-DID NRC
`0x31`. This is a host-side result; it does not replace F407/F103 CANable
acceptance.

## Week 5 UDS Security / Write / Routine Validation

Measured on 2026-10-04 with the portable C core, the Python VirtualEcu, and a
dual-node MemoryBus configuration:

| Test | Expected | Result |
| --- | --- | --- |
| Seed/key test vector | `0x87985AA5 -> 0xE174F5DA` | PASS |
| 0x2E before session change | `7F 2E 7E` | PASS |
| 0x2E in Extended without unlock | `7F 2E 33` | PASS |
| 0x2E custom DID after unlock | Write and read-back `DE AD BE EF` | PASS |
| 0x2E VIN outside Programming | `7F 2E 22` | PASS |
| First RequestResults while routine runs | `7F 31 78` | PASS |
| Self-test completion | `71 03 02 03 00` | PASS |
| Erase routine without unlock | `7F 31 33` | PASS |
| Erase routine after unlock | `71 03 FF 00 00` | PASS |
| F407/F103 VIN and hardware DID | ECU-specific values | PASS |
| Dual-node virtual full flow | Both nodes PASS | PASS |

Run the workflow directly:

```powershell
.venv\Scripts\python.exe -m diagnoser.cli uds-week5 --node ecu1
```

The C host test also confirms that the full service layer compiles into both
STM32 targets. Real CANable capture and F407/F103 hardware acceptance remain
Target because the ISO-TP firmware path is still pending.

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
