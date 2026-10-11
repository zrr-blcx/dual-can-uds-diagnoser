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
