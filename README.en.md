# dual-can-uds-diagnoser

[中文](README.md) | English

A Python diagnostic toolchain and STM32F407/F103 firmware lab for validating a
dual-node CAN and UDS system over a real 500 kbps bus.

## Status

- Weeks 1-10 are complete with host and hardware evidence.
- Week 9 completed a 4.046-hour dual-node stability run with no surprise reset,
  plus three independent Bus-Off recovery rounds.
- Week 10 passed the 10,000-frame final test with 0.04% / 0% physical loss and
  completed the 2000-frame 90% supplemental run.
- Release: `v1.0.0`.

## System

```text
PC diagnostic tool
  -> CANable / python-can
  -> 500 kbps CAN bus
  -> F407: 0x7E0 / 0x7E8
  -> F103: 0x7E1 / 0x7E9
```

The PC tool supports real CAN adapters, a deterministic memory bus, and virtual
ECUs. Firmware uses STM32CubeIDE, STM32 HAL, a portable ISO-TP core, a shared
UDS service layer, parameter persistence, and IWDG/WWDG supervision.

## Capabilities

- ISO 15765-2 SF/FF/CF/FC transport with BS/STmin and four timeout classes.
- UDS `0x10`, `0x22`, `0x27`, `0x2E`, `0x31`, and `0x3E`, including pending
  responses and the tested NRC matrix.
- F407/F103 Flash-backed DID persistence and hardware self-test.
- DBC parsing, C signal codec generation, dual-node scheduling, and exclusive
  programming sessions.
- Low-load and 90%+ stress tools, sequence-gap probes, and test-only DTC, NRC,
  and communication-loss injection.
- A duration-based stability runner with periodic samples and interrupt-safe
  partial results.
- Board-internal peer diagnostics and configurable physical frame-loss tests.

## Quick Start

```powershell
py -3.14 -m pip install -e ".[dev]"
py -3.14 -m diagnoser.cli demo
py -3.14 -m diagnoser.cli read-did --node ecu1 0xF190
py -3.14 -m diagnoser.cli dual-node-smoke --requests-per-node 20
```

Real CANable using SLCAN:

```powershell
py -3.14 -m diagnoser.cli `
  --config configs/canable.example.json `
  --channel COMx `
  read-did --node ecu1 0xF190
```

Stability run:

```powershell
py -3.14 -m diagnoser.cli stability `
  --hours 4 `
  --interval-ms 100 `
  --sample-seconds 60 `
  --load 0 `
  --report outputs/stability-4h.md
```

Firmware builds:

```powershell
powershell -ExecutionPolicy Bypass -File firmware\ecu-f407\build.ps1
powershell -ExecutionPolicy Bypass -File firmware\ecu-f103\build.ps1
```

Hardware test images are compiled with explicit switches and must be followed
by a formal-firmware rebuild and reflash. See
[firmware/README.md](firmware/README.md).

## Documentation

- [Architecture](docs/architecture.md)
- [Hardware wiring](docs/hardware_wiring.md)
- [Test report](docs/test_report.md)
- [Week 8 stress and fault injection](docs/week8_stress_fault.md)
- [Week 9 stability runner](docs/week9_stability.md)
- [Week 10 final test procedure](docs/week10_final_test.md)
- [Release checklist](docs/release_checklist.md)
- [Roadmap](docs/ROADMAP.md)

## Repository Layout

| Path | Purpose |
| --- | --- |
| `diagnoser/` | PC transport, UDS client, scheduler, stress, and report tools |
| `firmware/` | Shared firmware cores and F407/F103 CubeIDE projects |
| `tests/` | Python, portable C, and integration tests |
| `dbc/` | Project DBC |
| `configs/` | Network and CANable examples |
| `docs/` | Architecture, acceptance evidence, and operating guidance |
| `开发日志/` | Weekly technical logs |

## Contributing and Security

See [CONTRIBUTING.md](CONTRIBUTING.md) and [SECURITY.md](SECURITY.md). Do not
publish device serial numbers, local COM ports, personal paths, credentials, or
unredacted hardware logs.

## License

MIT License, see [LICENSE](LICENSE).
