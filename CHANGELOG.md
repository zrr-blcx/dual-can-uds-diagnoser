# Changelog

## 1.0.0 - 2026-10-27

- Add a duration-based dual-node UDS stability runner with periodic sampling,
  interrupt-safe partial results, and explicit separation between software
  completion and hardware reset/watchdog evidence.
- Add a board-internal load generator, bidirectional peer diagnostics, hardware
  and software RX priority for diagnostic traffic, and fixed-count physical
  frame-loss test modes.
- Complete a 4.046-hour dual-node stability run at about 90% bus load with no
  surprise reset. F407 completed 145596/145673 peer diagnostics and F103
  completed 145650/145650.
- Validate three independent Bus-Off injections with three successful F407
  recoveries, final error-active state, and no reset. Restore both formal
  images with 20/20 read-only requests per node.
- Pass the 10,000-frame normal-load test with 0.04% F407-to-F103 physical loss
  and 0% F103-to-F407 loss.
- Record a separate 2000-frame 90% supplemental run with UDS timeout rates
  distinct from physical frame loss.
- Publish bilingual README, final architecture, wiring, stability, and release
  documentation.

## 0.6.0 - 2026-10-25

- Standardize repository metadata, privacy boundaries, community files, raw
  firmware build reproducibility, and CI coverage.

- Add Python diagnostic tool V1.0 scaffold.
- Add ISO-TP multi-frame transport and UDS core services.
- Add DBC generation and signal struct generation.
- Add dual-node scheduler, bus load, stress, and fault injection tools.
- Route two physical diagnostic clients through one CAN device and send stress
  background traffic on that shared bus.
- Add `dual-node-smoke` and validate 20 concurrent read-only scheduler requests
  per node on the real F407/F103 CAN bus.
- Add `session-conflict` and validate real programming-session exclusion,
  skip-before-execution behavior and post-release recovery.
- Add isolated DBC bench frames and live cantools verification for `0x180` and
  `0x280`, with formal firmware and F407 Sector 11 restored afterward.
- Add per-node configurable stress intervals, low-priority `0x7F0` background
  frames, and separate UDS timeout versus physical gap metrics.
- Add board-internal 90%+ stress modes, background sequence probes, test-only
  DTC/NRC/communication-loss hooks, and bidirectional hardware results.
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
- Complete Week 4 hardware acceptance on F407/F103: all three sessions,
  standard and multi-DID reads, S3 keepalive/expiry, wrong-length and unknown
  DID negative responses, and dual-node request/response addressing.
- Validate `udsoncan` 1.26 against both STM32 nodes through a real CANable and
  the project ISO-TP connection adapter.
- Add `0x27` security access with seed/key state, attempt limiting, and a
  10-second delay after three invalid keys.
- Add session and security gates for `0x2E`, plus writable DID storage and
  per-ECU F407/F103 DID defaults.
- Add `0x31` Start/Stop/RequestResults with `0x78` response-pending support
  and reference self-test/erase routines.
- Transmit firmware `0x78` responses through ISO-TP and poll running routines
  until the automatic final response is ready, with host-test coverage.
- Defer `0x2E` persistence until after `7F 2E 78`, add NRC `0x72`, and update
  RAM only after the storage callback and read-back verify successfully.
- Reserve F407 Flash Sector 11 for CRC-protected append-only DID snapshots,
  with startup-only compaction before the watchdogs are enabled.
- Validate F407 pending writes and parameter persistence on hardware, including
  a full power-cycle read-back and a complete `uds-week5` flow.
- Reflash F103 with the pending-response fix and validate the complete Week 5
  flow plus the shared NRC matrix on both nodes.
- Add F103 persistence using its last 1 KB Flash page.
- Implement hardware-backed `0203` self-test, append-only `FF00` parameter
  clear, and forced-save NRC `0x72` validation on F407.
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
- Record preliminary bidirectional F407/F103 100-byte results at BS=8 and
  STmin=20 ms. Final acceptance was reopened after queue, error-state and
  mailbox-retry hardening.
- Add a portable ISR-to-main CAN RX queue, stricter SF/FF/SN validation,
  BS=0 support, mailbox-busy retries, tick-wrap coverage, and project-default
  Python flow-control parameters.
- Add configurable 100/120-byte bench payloads and target-side ISO-TP test
  counters. Verify wrong-SN recovery, bidirectional SN wrap, and
  `N_As/N_Bs/N_Cr/N_Ar` timeout paths on hardware.
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
- Complete Week 3 final acceptance on the hardened firmware: bidirectional
  100-byte transfers, bidirectional 120-byte SN wrap, wrong-SN recovery,
  `N_As/N_Bs/N_Cr/N_Ar` timeouts, and formal UDS regression.
- Publish pytest failure details as GitHub check annotations and fix the Linux
  direct-`pytest` helper import. The final Python 3.10/3.11/3.12 matrix passes
  in Actions run `40086628980`.
