# Contributing

## Before You Start

- Read `README.md`, `docs/ROADMAP.md`, and `docs/PUBLISHING_POLICY.md`.
- Do not add COM-port names, probe serial numbers, local paths, hardware
  credentials, private keys, or personal notes.
- Keep changes scoped to one roadmap stage and update the matching development
  log when behavior or acceptance evidence changes.

## Development Setup

```bash
python -m pip install -e ".[dev]"
pytest
```

Windows can use the documented Python 3.14 launcher:

```powershell
py -3.14 -m pip install -e ".[dev]"
py -3.14 -m pytest
```

## Firmware Builds

Set `ARM_GCC_BIN` when `arm-none-eabi-gcc` is not on `PATH`:

```powershell
$env:ARM_GCC_BIN = 'C:\path\to\arm-none-eabi\bin'
.\firmware\ecu-f407\build.ps1
.\firmware\ecu-f103\build.ps1
```

`ARM_GCC_BIN` is optional when the compiler is on `PATH` or under a standard
STM32CubeIDE installation. CI also builds both firmware targets on Linux.

## Pull Requests

- Explain the problem and the chosen behavior.
- List the exact verification commands and results.
- Call out untested hardware paths explicitly.
- Do not commit generated `Build/`, `.host_test/`, cache, report, or local
  configuration files.
- Keep public test data free of local hardware identifiers.

## Commit Messages

Use a concise conventional prefix such as `feat:`, `fix:`, `test:`, `docs:`,
or `chore:`.
