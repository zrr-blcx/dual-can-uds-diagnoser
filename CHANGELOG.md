# Changelog

## Unreleased

- Add Python diagnostic tool V1.0 scaffold.
- Add ISO-TP multi-frame transport and UDS core services.
- Add DBC generation and signal struct generation.
- Add dual-node scheduler, bus load, stress, and fault injection tools.
- Add virtual ECU and memory bus for hardware-free validation.
- Add CAN error-state monitoring, Bus-Off recovery policy, and watchdog supervision.
- Add repeated virtual Bus-Off injection with health reporting and CLI tests.
- Add project documentation, Roadmap, and CI configuration.
- Add STM32F407/F103 HAL firmware projects with 500 kbps CAN1 baseline.
- Add reproducible PowerShell ARM GCC build scripts for both firmware nodes.
- Verify stable bidirectional `0x123` CAN frame exchange and ACK on real hardware.
- Add portable firmware UDS core for `0x10/0x22/0x3E`, DID lookup, S3 timeout,
  and negative responses.
- Add host-side C behavior tests for the firmware UDS core.
- Add a shared DID database and align `0xF190` with VIN.
- Add a single-node `uds-smoke` workflow for virtual or physical CAN.
- Add `0x27` security access with seed/key state, attempt limiting, and a
  10-second delay after three invalid keys.
- Add session and security gates for `0x2E`, plus writable DID storage and
  per-ECU F407/F103 DID defaults.
- Add `0x31` Start/Stop/RequestResults with `0x78` response-pending support
  and reference self-test/erase routines.
- Add the Week 5 NRC test-case table and a dual-node virtual
  `10 03 -> 27 01 -> 27 02 -> 2E -> 31` workflow.
- Add the `uds-week5` and `security` CLI commands and secure-write/routine
  convenience flags.
- Add Week 6 diagnostic tool V1.0 Alpha: P2/P2* timing, `0x78` response-pending
  wait, timeout retries, and per-exchange timing metadata.
- Add timestamped text output and JSON Lines output with NRC names.
- Add `configs/canable.example.json` and `--p2/--p2-star/--retries` options.
- Add a `udsoncan` connection adapter and interoperability tests.
- Keep fault-injection and stress measurements on `retries=0` so retries do not
  hide offline windows or distort loss metrics.
- Declare `python-can[serial]`, `pyusb`, `gs_usb`, and `libusb` so the slcan and
  candleLight (`gs_usb`) CANable backends work without extra manual installs,
  and hand the bundled `libusb-1.0.dll` to pyusb on Windows automatically.
- Report python-can initialization failures as one error line instead of a
  traceback when an adapter is missing or a COM port is wrong.
- Add a candleLight/`gs_usb` network configuration example.
- Add the `trace` command: raw CAN frame capture with per-ID inter-frame gap
  statistics, text or JSON Lines frames, and an optional Markdown summary.
- Add the Week 2 Bus-Off injection wiring diagram covering both ECUs, the fault
  relay, bus termination, the CANable tap, and the ST-Link measurement path.
- Correct the termination documentation: the SN65HVD230 modules carry the 120
  ohm terminators, so no external resistors are added and the CANable
  termination must stay open.
- Add `firmware/common/can_health.c/.h`: a HAL-free CAN error counter and
  Bus-Off recovery policy core with host tests, mirroring the Python model.
- Add hardware-validated CAN health snapshots and SCE error handling to both
  STM32 nodes, including IWDG supervision and a two-phase Bus-Off recovery
  state machine.
- Add the portable firmware ISO-TP core with SF/FF/CF/FC sender/receiver state
  machines, response-ID flow control, BS/STmin handling, and
  transmit-completion-aware `N_As/N_Ar` plus `N_Bs/N_Cr` timeouts.
- Add shared ISO-TP configuration and frame-routing helpers, plus host tests for
  FC direction/dispatch, timeout windows, sequence wrap, and 100-byte payloads.
- Add compile-time `-IsotpBenchTx`/`-IsotpBenchRx` firmware modes and a fixed
  100-byte payload for hardware acceptance.
- Add ISO-TP trace summarization and correct the Python FC.WAIT deadline.
- Verify bidirectional F407/F103 100-byte transfers at BS=8 and STmin=20 ms,
  including cross-block STmin spacing, then restore and regress both formal
  images with single-frame and multi-frame UDS.
- Validate a physical CANH-CANL fault: 3 Bus-Off events and 3/3 automatic
  recoveries, with no recovery-chain reset request.
- Verify the sustained-fault escalation path: three consecutive recovery
  failures produce `SFTRSTF=1` with `IWDGRSTF=0`.
- Synchronize the exported CAN health snapshot from the hardware ESR on every
  refresh so it returns to error-active after the bus is restored.
- Add a register-level window watchdog to both STM32 nodes, with a 30 ms
  refresh service point and build-time stop-refresh / early-refresh tests.
- Wire the policy core into the CAN driver: error interrupts, TEC/REC/BOFF read
  from ESR, error-code classification with host tests, and
  `CanDriver_Recover()`; `AutoBusOff` is disabled so software recovery stays
  observable.
- Assert the HAL error-bit layout with `_Static_assert` so a HAL change cannot
  silently break the classifier, and add `can_health.c` to both build scripts.
- Extract the firmware host-test build wrapper into `tests/host_c_build.py` so
  both portable firmware modules share one compile-and-run helper.
