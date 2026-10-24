# Test Report

> Fill in measured results after hardware testing. Values marked Target are not
> yet measured.

## Environment

| Item | Value |
| --- | --- |
| Date | 2026-10-24 |
| Tester | Local bench run |
| Firmware version | Week 3 final formal build, ISO-TP and CAN RX queue |
| Diagnostic tool version | 1.0.0 |
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
| Trigger Bus-Off with relay | Node enters Bus-Off | PASS, CH2 shorted CANH-CANL; `BOFF=1` observed |
| Software reinit via HAL_CAN_Start | Node returns to normal mode | PASS, 3/3 recovery successes |
| Recovery time | Measured in ms | PASS, 3025-4543 ms per Bus-Off episode |
| Recover 3 times consecutively | Stable | PASS, 3 automatic events in one 2 s injection |
| Keep fault until 3 recovery failures | Software reset on third failure | PASS, `SFTRSTF=1`, `IWDGRSTF=0` |

The three hardware recovery events occurred during one approximately 2-second
fault injection. This satisfies the automatic 3/3 recovery criterion, but it is
not the same as three independent manual injection rounds.

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
STM32 validation was completed later in the Week 2 hardware section below.

## Automated Regression

| Date | Python | Command | Result |
| --- | --- | --- | --- |
| 2026-09-20 | 3.12.13 (`.venv`) | `python -m pytest` | `36 passed`, 1 cache-permission warning |
| 2026-09-27 | 3.12.13 (`.venv`) | `python -m pytest` | `39 passed`, 1 cache-permission warning |
| 2026-10-04 | 3.12.13 (`.venv`) | `python -m pytest` | `48 passed`, 1 cache-permission warning |
| 2026-10-11 | 3.12.13 (`.venv`) | `python -m pytest` | `66 passed` |
| 2026-10-22 | 3.14.7 | `py -3.14 -m pytest -p no:cacheprovider` | `78 passed` |
| 2026-10-24 | 3.14.7 | `py -3.14 -m pytest -p no:cacheprovider` | `100 passed` |
| 2026-10-24 | 3.10 / 3.11 / 3.12 | GitHub Actions `pytest` | 3/3 matrix jobs PASS, run `40086628980` |

The 2026-10-24 local regression used the `py -3.14` launcher with the cache
provider disabled. GitHub Actions independently passed the complete suite on
Python 3.10, 3.11, and 3.12. An initial CI failure was limited to a Linux test
import (`ModuleNotFoundError: No module named 'tests'`); the test now imports
the shared helper from the `tests` directory, matching the other host-test
modules. No ISO-TP or firmware assertion changed for that fix.

The 2026-10-04 run includes a C host test that compiles the firmware UDS
service layer (`uds_server.c`, `uds_reference_routines.c`, `uds_did_db.c`,
`uds_ecu.c`) with MSVC and executes the resulting binary. The test covers
session transitions, S3 timeout and refresh, `0x22` reads, `0x27` seed/key
state, `0x2E` write rules, `0x31` Start/Stop/RequestResults, `0x78` response
pending, F407/F103 DID differences, and the full NRC matrix.

The 2026-10-11 run builds that binary in the repository-local `.host_test`
directory instead of the platform temp directory. The new tests cover
`UdsClient` timing/retry behavior, `0x78` waiting, console formatting, JSON
output, and `udsoncan` interoperability.

## Firmware Build Validation

| Date | Target | Toolchain | Artifact | Result |
| --- | --- | --- | --- | --- |
| 2026-09-20 | STM32F407ZGT6 | GNU ARM GCC 14.3.1 | `firmware/ecu-f407/Build/ecu-f407.elf` | PASS, text 10196 B |
| 2026-09-20 | STM32F103C8T6 | GNU ARM GCC 14.3.1 | `firmware/ecu-f103/Build/ecu-f103.elf` | PASS, text 8692 B |
| 2026-09-27 | STM32F407ZGT6 | GNU ARM GCC 14.3.1 | `firmware/ecu-f407/Build/ecu-f407.elf` | PASS, text 10596 B |
| 2026-09-27 | STM32F103C8T6 | GNU ARM GCC 14.3.1 | `firmware/ecu-f103/Build/ecu-f103.elf` | PASS, text 9104 B |
| 2026-10-04 | STM32F407ZGT6 | GNU ARM GCC 14.3.1 | `firmware/ecu-f407/Build/ecu-f407.elf` | PASS, text 12808 B |
| 2026-10-04 | STM32F103C8T6 | GNU ARM GCC 14.3.1 | `firmware/ecu-f103/Build/ecu-f103.elf` | PASS, text 11304 B |
| 2026-10-22 | STM32F407ZGT6 | GNU ARM GCC 14.3.1 | `firmware/ecu-f407/Build/ecu-f407.elf` | PASS, final r22 text 15828 B, bss 3452 B |
| 2026-10-22 | STM32F103C8T6 | GNU ARM GCC 14.3.1 | `firmware/ecu-f103/Build/ecu-f103.elf` | PASS, final r22 text 14304 B, bss 3452 B |

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
STM32 targets. At the time of Week 5 testing, real CANable capture and hardware
acceptance were Target because ISO-TP was pending. The Week 3 hardware section
below records the later closure.

## Week 6 Diagnostic Tool V1.0 Alpha Validation

Measured on 2026-10-11 with the virtual ECU and MemoryBus. No firmware source
changed in Week 6, so the 2026-10-04 firmware build sizes still apply.

| Test | Expected | Result |
| --- | --- | --- |
| `0x78` then final response | Client waits P2* and returns `71 03 ...` | PASS |
| Raw `0x78` observation | `handle_response_pending=False` raises NRC `0x78` | PASS |
| Timeout retry | First receive times out, second succeeds, `retries=1` | PASS |
| Retries exhausted | `TimeoutError` after the configured attempts | PASS |
| Text output | Timestamp, TX/RX IDs, bytes, elapsed ms, NRC name | PASS |
| JSON Lines output | Positive and negative exchanges are valid JSON | PASS |
| udsoncan interop | `change_session` and `read_data_by_identifier(0xF190)` | PASS |
| udsoncan negative response | NRC `0x31` raises `NegativeResponseException` | PASS |
| Bus-Off offline detection | Offline window still observed with retries disabled | PASS |
| Dual-node virtual full flow | `ecu1` and `ecu2` `uds-week5` PASS | PASS |
| CLI JSON smoke | `--json read-did` and negative response | PASS |

At the time of Week 6 Alpha testing, real CANable capture and hardware timing
were Target until the ISO-TP firmware path was available. The later dual-node
hardware section below records physical addressing and timing results.

## Week 2 Bus-Off Hardware Validation

Measured on 2026-10-22 at 500 kbps. F407 initially ran the 20 ms fast-frame
test image and F103 ran the normal 500 ms image. Relay channel CH2 was wired
COM-NO across CANH and CANL; IN2 was pulled to GND for approximately 2 seconds.
Health data was read through ST-Link in `hotplug` mode so that connecting the
probe did not reset the target.

| Test | Expected | Result |
| --- | --- | --- |
| Physical CAN fault enters Bus-Off | `BOFF=1` and TEC crosses 128 | PASS, TEC reached at least 168; `BOFF=1` captured |
| Automatic recovery | Return to `error-active` | PASS, 3/3 events |
| Recovery time | Measured in ms | PASS, 3025-4543 ms per event; approximately 1507 ms per attempt |
| Recovery failure escalation | Reset only after 3 recovery-chain failures | PASS, `RCC_CSR` changed from `SFTRSTF=0` to `SFTRSTF=1`; `IWDGRSTF=0` |
| Final bus state | No permanent Bus-Off | PASS, both nodes final `TEC/REC=0/0`, `BOFF=0` |

Raw evidence is retained locally because it contains diagnostic buffers and
local hardware details. The publishable result is the table above and the
decoded conclusion, not the raw binary logs.

For the sustained-fault reset test, the F407 baseline was
`RCC_CSR=0x0E000002` with `SFTRSTF=0` and `IWDGRSTF=0`. After holding CH2
shorted for approximately 8 seconds, the register became `0x1E000002` with
`SFTRSTF=1` and `IWDGRSTF=0`.

Final formal-firmware regression after restoring both nodes:

| Metric | Result |
| --- | --- |
| Capture | 39 frames / 10 s |
| Average same-ID interval | 253.0 ms |
| Maximum same-ID interval | 261.9 ms |
| F407 health | error-active, TEC/REC=0/0, Bus-Off=0 |
| F103 health | error-active, TEC/REC=0/0, Bus-Off=0 |

The exported health snapshot now synchronizes TEC/REC/BOFF from the controller
ESR on every refresh. Before that fix the hardware ESR was already zero while
the RAM snapshot retained the previous error-passive value.

## WWDG Validation

The common watchdog module now starts the WWDG from PCLK1, uses a /8 prescaler,
programs window `0x68`, and refreshes every 30 ms in the main loop. Compile-time
checks reject an invalid window or a zero refresh interval.

| Test | Software/build result | Hardware result |
| --- | --- | --- |
| Normal WWDG service on F407/F103 | PASS, both builds link WWDG functions | PASS, 35 s run with no WWDG/IWDG reset; Tick and TX/RX kept increasing |
| Stop-refresh self-test | PASS, both builds produce test images | PASS on F407, `WWDGRSTF=1`, `IWDGRSTF=0` |
| Early-refresh self-test | PASS, both builds produce test images | PASS on F103, `WWDGRSTF=1`, `IWDGRSTF=0`, Tick stayed 0 in reset cycle |
| IWDG isolation during WWDG reset | Test modes keep IWDG fed | PASS, `IWDGRSTF=0` in both test modes |

The F407 WWDG is implemented directly through the CMSIS register map, matching
the existing register-level IWDG approach; no CubeF4 package is required.

Final CAN regression after restoring the normal WWDG images:

| Metric | Result |
| --- | --- |
| Capture | 39 frames / 10 s |
| Average same-ID interval | 253.0 ms |
| Maximum same-ID interval | 340.9 ms |
| F407 health | error-active, TEC/REC=0/0, Bus-Off=0 |
| F103 health | error-active, TEC/REC=0/0, Bus-Off=0 |

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

## Week 3 ISO-TP Validation

Measured on 2026-10-24 by compiling the portable C ISO-TP core
(`firmware/common/isotp.c`) plus the host test drivers with MSVC, executing them,
and running bidirectional F407/F103 100-byte transfers plus CANable capture.

| Test | Expected | Result |
| --- | --- | --- |
| Single frame (SF) round trip | 3-byte payload preserved | PASS |
| Invalid frames | Zero-length SF and declared FF length <=7 rejected | PASS |
| Unread message | New SF/FF does not overwrite a complete unread message | PASS |
| Multi-frame 100 B, BS=8, STmin=20 ms | FF + 14 CF reassembled intact | PASS |
| SN wraparound 120 B | 17 CF, SN hits `0x0F` then `0x00` | PASS |
| FC direction | Request `0x7E0` -> FC emitted on response `0x7E8` | PASS |
| Frame dispatch | FC resumes the sender; SF/FF/CF reach the receiver | PASS |
| N_BS timeout | No FC arrives -> `ISOTP_ERR_TIMEOUT` | PASS |
| N_AS timeout | CAN frame never completes transmission -> `ISOTP_ERR_TIMEOUT` | PASS |
| N_CR timeout | Next block FC / next CF never arrives -> `ISOTP_ERR_TIMEOUT` | PASS |
| N_AR timeout | FC never completes transmission -> `ISOTP_ERR_TIMEOUT` | PASS |
| SN out of order | Wrong CF index -> `ISOTP_ERR_INVALID` | PASS |
| Buffer overflow | Declared length > buffer -> `ISOTP_ERR_OVERFLOW` | PASS |
| FC WAIT | Sender keeps waiting, refreshed deadline | PASS |
| FC OVERFLOW | Sender aborts the transfer | PASS |
| STmin spacing | CFs spaced by the peer's STmin, including across BS blocks | PASS |
| BS=0 | Receiver advertises one full-transfer grant | PASS |
| CAN mailbox busy | FF/CF and FC retry within their timeout window | PASS |
| Tick wrap | Timeout arithmetic works across 32-bit wrap | PASS |
| CAN RX queue | ISR producer/main consumer order and overflow policy | PASS |
| Bench payload | Fixed 100-byte sender pattern and receiver validation | PASS |
| Trace summary | 14 CF, 2 FC, BS/STmin and reassembled payload extracted | PASS |

The Python reference transport also gained full BS/STmin flow control
(per-block FC waits on send; block advertisement on receive). The tests verify
send waits at each BS boundary, receive advertises two blocks for 100 B,
OVERFLOW aborts, WAIT is handled before CTS, and WAIT refreshes the FC deadline.

Firmware builds now include the ISO-TP core on both targets:

| Date | Target | Toolchain | Artifact | Result |
| --- | --- | --- | --- | --- |
| 2026-10-24 | STM32F407ZGT6 | GNU ARM GCC 14.3.1 | `firmware/ecu-f407/Build/ecu-f407.elf` | PASS, text 26628 B, bss 8620 B |
| 2026-10-24 | STM32F103C8T6 | GNU ARM GCC 14.3.1 | `firmware/ecu-f103/Build/ecu-f103.elf` | PASS, text 25068 B, bss 8620 B |

Both `main.c` files route `0x7E0/0x7E8` (F407) and `0x7E1/0x7E9` (F103) request
frames into the ISO-TP receiver, feed complete requests to the UDS server, and
send responses back out through the ISO-TP sender. Compile-time bench modes
(`-IsotpBenchTx` / `-IsotpBenchRx`) exchange the fixed 100-byte payload without
using the UDS parser.

### Dual-Node Hardware Result (Final Candidate)

| Test | Result |
| --- | --- |
| F407 -> F103, 100-byte payload | PASS, 14 CF, 2 FC, 5 x status `0x5A` |
| F407 -> F103 CF gap | Minimum `21.845 ms` |
| F407 -> F103 receiver counters | `rx_messages=22`, `rx_errors=0` |
| F103 -> F407, 100-byte payload | PASS, 14 CF, 2 FC, 5 x status `0x5A` |
| F103 -> F407 CF gap | Minimum `21.839 ms` |
| F103 -> F407 receiver counters | `rx_messages=21`, `rx_errors=0` |
| FC settings in both directions | BS=8, STmin=20 ms |
| F407 formal `10 03` / `22 F190` | PASS, `9.5 ms` / `39.9 ms` |
| F103 formal `10 03` / `22 F190` | PASS, `2.0 ms` / `42.1 ms` |

Both bench images and then both formal images were flashed and verified with
STM32CubeProgrammer. These measurements include the CAN RX queue, stricter
receive-state validation, mailbox retry and BS=0 fixes.

### Negative Hardware Validation

| Test | Expected | Result |
| --- | --- | --- |
| Wrong SN then valid transfer | Error once, reset, next transfer succeeds | PASS: `rx_errors=1`, recovery status `0x5A` |
| F407 -> F103, 120 bytes | 17 CF, 3 FC, SN `0xF -> 0x0` | PASS |
| F103 -> F407, 120 bytes | 17 CF, 3 FC, SN `0xF -> 0x0` | PASS |
| `N_Bs` with no FC | Sender TX timeout increases | PASS |
| `N_Cr` sender with no next block FC | Sender TX timeout increases | PASS |
| `N_Cr` receiver after stopping CF | Receiver RX timeout increases | PASS: `rx_timeouts=1` |
| `N_As` with forced transmit-busy | Sender TX timeout increases | PASS: `tx_timeouts=12` |
| `N_Ar` with forced FC transmit-busy | Receiver RX timeout increases | PASS: `rx_timeouts=1` |

The `N_As/N_Ar` cases use a test-image switch that forces the CAN send callback
to report busy. This exercises the real firmware timeout state machines with
`HAL_GetTick()` while avoiding the unstable and indirect "remove ACK" setup.
After the tests, both normal images were restored and passed `10 03` and
multi-frame `22 F190`.

## Conclusion

- Physical frame loss below 0.1% at normal load: Target.
- Week 3 ISO-TP bidirectional 100-byte transfer: PASS.
- UDS timeout handling: PASS in host/virtual tests and final multi-frame UDS
  regression.
- Bus-Off physical detection, automatic recovery, and sustained-fault software
  reset escalation: PASS.
