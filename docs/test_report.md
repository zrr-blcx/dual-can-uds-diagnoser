# Test Report

> Measured results are recorded by subsystem below. Values marked Target are
> not yet measured.

## Environment

| Item | Value |
| --- | --- |
| Date | 2026-10-27 |
| Tester | Local bench run |
| Firmware version | v1.0.0 formal build plus isolated stability/frame-loss modes |
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
| ecu1 (F407) | 1000 | 1000 | 0 | 0 | 0.0000% | 64.37 ms | 91.08 ms |
| ecu2 (F103) | 1000 | 1000 | 0 | 0 | 0.0000% | 66.67 ms | 86.59 ms |

Low-load baseline conditions: 10% background load, 100 ms request interval,
`retries=0`, F407/F103 each 1000 requests. The background sequence probe
observed 45,036 frames at each node with zero gaps or out-of-order frames.

## 90%+ Load Stress Test

| Node | Requests | Success | Timeout | Error | Loss rate | Avg RTT | Max RTT |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| ecu1 (F407 DUT) | 1000 | 517 | 483 | 0 | 3.812% | 1.756 ms | 2.843 ms |
| ecu2 (F103 DUT) | 1000 | 524 | 476 | 0 | 3.476% | 1.969 ms | 2.713 ms |

Stress conditions: board-internal generator at a 228 us background period,
1000 requests per DUT, 20 ms start interval. Both directions measured
94.4% effective bus load. Loss is the physical background frame gap rate;
UDS timeout rates are 48.3% for F407 and 47.6% for F103.

## 10,000 Frame Final Test

| Direction | Sent | Received | Physical loss | Target | Result |
| --- | ---: | ---: | ---: | ---: | --- |
| F407 -> F103 | 5000 | 4998 | 0.0400% | < 0.1% | PASS |
| F103 -> F407 | 5000 | 5000 | 0.0000% | < 0.1% | PASS |

The 50% aggregate-load run used an 888 us frame period and independent sequence
IDs `0x6F0` / `0x6F1`. Both nodes completed 5000 successful transmissions.
UDS traffic was measured separately: F407 368/396 responses with 28 timeouts
(7.07%); F103 373/376 with 3 timeouts (0.80%).

The 90% supplemental run sent 1000 frames per node, 2000 total. F103 received
1000/1000 F407 frames (0%); F407 received 999/1000 F103 frames (0.1%). UDS
timeouts were 27/395 (6.84%) on F407 and 5/375 (1.33%) on F103. The normal-load
0.1% physical gate is not applied to this high-load record.

## Week 9 Four-Hour Stability

Final reproducible image hashes:

| Artifact | SHA-256 |
| --- | --- |
| F407 HEX | `85B8217654D1C022EA2AB020A0713E95AA2C9105EF5842FF70ECE1A6B25DD29E` |
| F103 HEX | `3AF3A9B33C05C9BD03A7F4A8C51D20D2AA96E99C3085BCCDA0711923650B39C8` |

| Metric | F407 | F103 |
| --- | ---: | ---: |
| Continuous uptime | 4.046 h | 4.046 h |
| UDS requests | 145673 | 145650 |
| UDS success | 145596 | 145650 |
| UDS timeouts | 77 (0.053%) | 0 |
| Invalid responses | 0 | 0 |
| Average RTT | 1.32 ms | 2.00 ms |
| Maximum RTT | 3 ms | 2 ms |
| Final CAN state | error-active | error-active |
| IWDG/SFTRST after flash baseline | unchanged | unchanged |

F407 generated 57,971,063 accepted `0x7F0` frames during the run and reached
about 90% bus load. F103 counted 57,925,485 of them, a 0.0786% observed loss
relative to accepted transmissions. Intentional missed scheduling slots are
reported separately from actual transmitted-frame loss.

Three independent 1-second CH2 CANH-CANL injections were then performed under
the fast-frame test image. F407 recorded three Bus-Off events and three
successful recoveries, returned to `error-active/TEC=0`, and did not reset.
F103 also returned to error-active. The formal images were restored and both
nodes passed 20/20 read-only smoke requests.

## NRC Coverage Matrix

| NRC | Scenario | Expected | Result |
| --- | --- | --- | --- |
| 0x11 | Unsupported service | PASS | F407 + F103 hardware PASS |
| 0x12 | Unsupported subfunction | PASS | F407 + F103 hardware PASS |
| 0x13 | Wrong message length | PASS | Host + F407/F103 hardware PASS |
| 0x22 | Conditions not correct | PASS | F407 + F103 hardware PASS |
| 0x24 | Request sequence error | PASS | F407 + F103 hardware PASS |
| 0x31 | Request out of range | PASS | Host + F407/F103 hardware PASS |
| 0x33 | Security access denied | PASS | F407 + F103 hardware PASS |
| 0x35 | Invalid key | PASS | F407 + F103 hardware PASS |
| 0x36 | Exceeded number of attempts | PASS | F407 + F103 hardware PASS |
| 0x37 | Required time delay not expired | PASS | F407 + F103 hardware PASS |
| 0x72 | General programming failure | PASS | F407 hardware PASS |
| 0x78 | Response pending | PASS | F407 + F103 hardware PASS |
| 0x7E | Subfunction not supported in active session | PASS | F407 + F103 hardware PASS |

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
not the same as three independent manual injection rounds. The independent
Week 9 three-round result is recorded above.

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
| 2026-10-24 | 3.14.7 | `py -3.14 -m pytest -p no:cacheprovider` | `102 passed` |
| 2026-10-25 | 3.14.7 | `py -3.14 -m pytest -p no:cacheprovider` | `102 passed` |
| 2026-10-25 | 3.12.13 (`.venv`) | `.venv\Scripts\python.exe -m pytest -p no:cacheprovider` | `107 passed` |
| 2026-10-26 | 3.12.13 (`.venv`) | `.venv\Scripts\python.exe -m pytest -p no:cacheprovider` | `110 passed` |
| 2026-10-26 | 3.12.13 (`.venv`) | `.venv\Scripts\python.exe -m pytest -p no:cacheprovider` | `112 passed` |
| 2026-10-26 | 3.12.13 (`.venv`) | `.venv\Scripts\python.exe -m pytest -p no:cacheprovider` | `114 passed` |
| 2026-10-26 | 3.12.13 (`.venv`) | `.venv\Scripts\python.exe -m pytest -p no:cacheprovider` | `125 passed` |
| 2026-10-26 | 3.14.7 | `py -3.14 -m pytest -p no:cacheprovider tests\test_stability.py tests\test_cli.py` | `18 passed` |
| 2026-10-26 | 3.12.13 (`.venv`) | `.venv\Scripts\python.exe -m pytest -p no:cacheprovider` | `130 passed` |
| 2026-10-27 | 3.12.13 (`.venv`) | `.venv\Scripts\python.exe -m pytest -p no:cacheprovider` | `132 passed` |
| 2026-10-27 | 3.12.13 (`.venv`) | `.venv\Scripts\python.exe -m pytest -p no:cacheprovider` | `132 passed in 110.63s`, final release run |
| 2026-10-27 | 3.12.13 (`.venv`) | `.venv\Scripts\python.exe -m pytest -p no:cacheprovider` | `132 passed in 109.00s`, final code freeze |
| 2026-10-24 | 3.10 / 3.11 / 3.12 | GitHub Actions `pytest` | 3/3 matrix jobs PASS, run `40086628980` |
| 2026-10-25 | 3.10 / 3.11 / 3.12 | GitHub Actions `pytest` | 3/3 matrix jobs PASS, run `40167418774` |

The final 2026-10-26 `125 passed` run includes shared physical-bus routing,
dual-node smoke and session arbitration, DBC capture, per-node stress metrics,
background sequence probes, and portable fault/stress test helpers. It used
the existing Python 3.12 `.venv`; the local
Python 3.14 environment did not have the declared `PyYAML` development
dependency, so targeted Python 3.14 tests were run there but no full Python 3.14
result is claimed for this change.

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
| 2026-10-26 | STM32F407ZGT6 | GNU ARM GCC 14.3.1 | `firmware/ecu-f407/Build/ecu-f407.elf` | PASS, formal text 34160 B, data 44 B, bss 9164 B |
| 2026-10-26 | STM32F103C8T6 | GNU ARM GCC 14.3.1 | `firmware/ecu-f103/Build/ecu-f103.elf` | PASS, formal text 32072 B, data 20 B, bss 9192 B |

The F103 image was programmed, verified, and reset through ST-Link. The F407
image was also programmed successfully after reconnecting its SWD target.
An initial dual-node CAN exchange was observed. After the intermittent physical
bus connection was corrected, repeated tests confirmed stable bidirectional
traffic.

## Week 4 UDS Host and Hardware Validation

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

### F407/F103 Hardware Validation

Measured on 2026-10-24 through a CANable slcan adapter at 500 kbps. The Week 2
fault relay remained installed but de-energized during the test.

| Test | F407 | F103 |
| --- | --- | --- |
| `10 01`, `10 02`, `10 03` | PASS | PASS |
| `22 F190`, `22 F193`, `22 0100` | PASS | PASS |
| Multi-DID `22 F193 0100` | PASS, 34.7 ms | PASS, 37.5 ms |
| `3E 00` keeps Extended beyond the original 5 s deadline | PASS | PASS |
| S3 expiry returns Default | PASS, `7F 2E 7E` | PASS, `7F 2E 7E` |
| Wrong length `22 F1` | PASS, `7F 22 13` | PASS, `7F 22 13` |
| Unknown DID | PASS, `7F 22 31` | PASS, `7F 22 31` |
| Physical request/response IDs | `0x7E0/0x7E8` | `0x7E1/0x7E9` |
| `udsoncan` session and DID reads | PASS | PASS |
| `udsoncan` negative response | PASS, NRC `0x31` | PASS, NRC `0x31` |

Both clients ran in one CANable process for the dual-node check. No response
crossed between the F407 and F103. Full commands and measurements are recorded
in [uds_week4_acceptance.md](uds_week4_acceptance.md).

## Week 5 UDS Security / Write / Routine Validation

Measured on 2026-10-04 with the portable C core, the Python VirtualEcu, and a
dual-node MemoryBus configuration:

| Test | Expected | Result |
| --- | --- | --- |
| Seed/key test vector | `0x87985AA5 -> 0xE174F5DA` | PASS |
| 0x2E before session change | `7F 2E 7E` | PASS |
| 0x2E in Extended without unlock | `7F 2E 33` | PASS |
| 0x2E custom DID after unlock | Write and read-back `DE AD BE EF` | PASS |
| 0x2E persistent write | First request returns `7F 2E 78`, then `6E 01 00` | F407 + F103 hardware PASS |
| 0x2E VIN outside Programming | `7F 2E 22` | PASS |
| First RequestResults while routine runs | `7F 31 78` | PASS |
| Final response after pending | Automatic `71 03 ...` without a repeated RequestResults | F407 + F103 hardware PASS |
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

On 2026-10-24 the firmware application path was corrected to transmit
`UDS_PROCESS_RESPONSE_PENDING` through ISO-TP and to poll
`UdsEcu_PollPending()` until the routine produces its final response. The C
host test now verifies that the final response is generated without repeating
the RequestResults request. Both firmware targets build successfully:
F407 `text=34124, bss=9164`; F103 `text=32036, bss=9200`.

F407 parameter storage uses the reserved last Flash sector. Runtime writes append
an 80-byte CRC-protected snapshot without erasing; startup compacts the log
before either watchdog is enabled. Host tests cover append, reload, CRC
corruption, partial records, write failure, and delayed `0x2E` completion.

### F407 Hardware Persistence and Pending Validation

Measured on 2026-10-24 through the real CANable at 500 kbps. The F407 application
was flashed with sectors 0-10 erased explicitly; Sector 11 was excluded.

| Test | Expected | Result |
| --- | --- | --- |
| Preserve Sector 11 while flashing | Hash unchanged | PASS |
| `22 F190` after flash | Valid VIN | PASS, 44.0 ms |
| `2E 0100 DE AD BE EF` | `7F 2E 78`, then `6E 01 00` | PASS, 44.2 ms, one pending |
| Immediate read-back | `DE AD BE EF` | PASS, 7.4 ms |
| Power-cycle read-back | `DE AD BE EF` remains | PASS, 6.9 ms |
| Parameter record placement | 80 non-FF bytes in Sector 11 | PASS |
| Full F407 `uds-week5` | All steps pass | PASS |
| `31 03 0203` pending | Final `71 03 02 03 00` | PASS, 43.8 ms, one pending |

The F407 ST-Link probe was disconnected after the power-cycle test, so the
post-power-cycle raw Sector 11 hash remains unverified. The DID read-back proves
the persisted application value survived power loss.

The original F103 image did not contain the pending-response fix;
`uds-week5 --node ecu2` stopped at `routine-self-test-results` with an ISO-TP
timeout. After backing up the 64 KB image, F103 was erased and reflashed under
reset. The repeated full flow passed all 13 steps. A single CANable process then
validated the required NRC matrix on both nodes.

### Week 5 Maintenance Routines and F103 Persistence

Measured on 2026-10-25 after flashing the formal images.

| Test | Expected | Result |
| --- | --- | --- |
| F407 `0203` self-test | Bitmask `00` | PASS, 43.8 ms, one pending |
| F103 `0203` self-test | Bitmask `00` | PASS, 43.9 ms, one pending |
| F407 `FF00` clear | Result `00`, DID resets to `00 01` | PASS |
| F103 `FF00` clear | Result `00`, DID resets to `00 01` | PASS |
| F103 write and immediate read-back | `11 22 33 44` | PASS |
| F103 write record | One 80-byte record in the last 1 KB page | PASS |
| F103 power-cycle read-back | `11 22 33 44` survives | PASS, 11.1 ms |
| Forced F407 save failure | `7F 2E 78` then `7F 2E 72` | PASS, one pending |
| Failed write read-back | Value remains unchanged | PASS |
| Formal F407 restore | Self-test returns `00` | PASS |

`0xFF00` uses an append-only empty snapshot at runtime. It logically removes
persisted values and restores DID defaults without risking the running WWDG;
physical sector/page erasure remains confined to startup compaction.

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

## Week 7 DBC Hardware Validation

Measured on 2026-10-26 using isolated `-DbcBench` images. F407 sent
`EngineData` on `0x180`; F103 sent `VehicleStatus` on `0x280`. A CANable
captured the live bus for 3 seconds and cantools decoded every frame.

| Test | Expected | Result |
| --- | --- | --- |
| `0x180` frame count | At least 10 in 3 seconds | PASS, 27 frames |
| `0x180` decoded values | 1234.5 rpm, 85 degC | PASS |
| `0x280` frame count | At least 10 in 3 seconds | PASS, 27 frames |
| `0x280` decoded values | 42.5 km/h, brake active | PASS |
| F407 Sector 11 | Hash unchanged through test image and restore | PASS, `F91D4632...EBFD2F` |
| Formal firmware restore | Both nodes pass dual-node smoke | PASS, 20/20 each |

The formal firmware does not include the DBC bench sender or stress/fault test
hooks.

## Week 7 Shared-Bus Scheduler Smoke

Measured on 2026-10-26 through one CANable slcan device at 500 kbps. The CLI
opened the physical adapter once, ran two node workers through
`QueuedDualNodeScheduler`, and issued read-only DID requests concurrently.

| Test | Expected | Result |
| --- | --- | --- |
| F407 requests | 20/20 successful reads | PASS, response ID `0x7E8` |
| F103 requests | 20/20 successful reads | PASS, response ID `0x7E9` |
| Session restore | Both nodes return to Default | PASS |
| Shared device | One physical CAN adapter for both clients | PASS |
| Cross-node response | No `0x7E8/0x7E9` crossover | PASS |

This validates the shared physical bus and concurrent scheduler request path.
DBC and Week 8 evidence are recorded in the sections below.

## Week 7 Programming-Session Conflict

Measured on 2026-10-26 on the real F407/F103 bus. F407 held Programming session
`0x02` for 0.5 seconds while F103 submitted an exclusive scheduler job with a
0.1 second conflict wait.

| Test | Expected | Result |
| --- | --- | --- |
| F407 enters `0x02` | Session active before F103 submits | PASS |
| F103 conflicting job | Skipped after wait timeout | PASS |
| Skipped job execution | Job function never runs | PASS |
| F407 restore | Returns to Default and remains responsive | PASS |
| F103 recovery | Read succeeds after lock release | PASS |

## Week 8 Stress and Fault Injection

The final 90% stress run used board-internal test images so one MCU generated
low-priority `0x7F0` background frames while issuing single-frame UDS requests
to the other MCU. This avoids the slcan tester becoming the bottleneck and
allows the receiving MCU to count sequence gaps independently.

| DUT | Load | Requests | Success | Timeout | Physical gap loss | Avg RTT | Max RTT |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| F103 | 94.40% | 1000 | 524 | 476 | 3.476% | 1.969 ms | 2.713 ms |
| F407 | 94.41% | 1000 | 517 | 483 | 3.812% | 1.756 ms | 2.843 ms |

Real hardware fault-hook checks on both nodes:

| Injection | F407 | F103 |
| --- | --- | --- |
| DTC status | PASS, `01 02 03 2A` read back | PASS, `04 05 06 2B` read back |
| NRC `0x31` on service `0x22` | PASS | PASS |
| Simulated communication loss | PASS, offline then recovery | PASS, offline then recovery |
| Bus-Off relay | Covered by Week 2 physical 3/3 recovery evidence | Same bus experiment |
| Formal restore | PASS, 20/20 dual-node smoke | PASS, 20/20 dual-node smoke |

The 90% scenario intentionally exposes degradation. Its DUT timeout and gap
rates are not compared with the normal-load `<0.1%` physical frame loss gate.

## Week 9 Stability Runner Validation

Measured on 2026-10-26 with the duration-based runner on a virtual dual-node
bus. This validates timing, sampling, graceful interruption, timeout accounting,
and CLI routing. The final hardware stability result is recorded above.

| Test | Expected | Result |
| --- | --- | --- |
| Full-duration short run | Complete requested runtime | PASS |
| Per-node request counters | Both nodes issue and complete requests | PASS |
| Periodic samples | At least three snapshots over the run | PASS |
| Operator interruption | Keep partial samples, do not mark PASS | PASS |
| UDS timeout accounting | Timeout count separate from errors | PASS |
| CLI route and report | Render Stability Report and return success | PASS |
| Targeted Python 3.14 regression | Stability and CLI tests | PASS, 18 tests |
| Full Python 3.12 regression | Complete suite | PASS, 130 tests |

The four-hour F407/F103 run, independent background-load measurement, CAN
health snapshots, and `RCC_CSR` evidence completed on 2026-10-27.

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

- Week 8 low-load physical frame loss: PASS, zero gaps across 45,036
  background frames at each node.
- Week 8 90%+ stress: PASS as an acceptance exercise, 94.40-94.41% effective
  load with degradation and physical gaps recorded separately.
- Week 7 DBC hardware signals, shared-bus scheduling, and session exclusion:
  PASS.
- Week 3 ISO-TP bidirectional 100-byte transfer: PASS.
- Week 4 UDS sessions, DID reads, S3 timeout, NRC, dual-node routing and
  udsoncan CANable interoperability: PASS.
- Week 5 F407 persistent DID write/read-back and dual-node pending/NRC flow:
  PASS. Dual-node real self-test, parameter clear, F103 persistence and NRC
  `0x72` injection also PASS.
- Week 6 diagnostic CLI against both real ECUs: session, read, write, routine,
  raw, pending wait and the complete `uds-week5` flow PASS.
- Week 8 DTC, NRC, and communication-loss injection: PASS on F407 and F103;
  Bus-Off uses the earlier physical relay evidence.
- Bus-Off physical detection, automatic recovery, and sustained-fault software
  reset escalation: PASS.
