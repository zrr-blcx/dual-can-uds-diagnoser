# Architecture

## System Layers

```text
PC Diagnostic Tool
==================
CLI layer        diagnoser.cli
UDS layer        diagnoser.uds (0x10/0x22/0x27/0x2E/0x31/0x3E, NRC)
ISO-TP layer     diagnoser.transport.isotp
CAN layer        python-can bus adapter + diagnoser.tools.can_health

STM32 ECU
==================
Application layer
UDS service layer      firmware/common/uds_server.c (Week 5 service v1.0)
ISO 15765-2 TP layer
CAN driver layer
STM32 HAL
```

## CAN ID Allocation

| Purpose | CAN ID | Node |
| --- | --- | --- |
| Physical request ecu1 | 0x7E0 | Tester -> F407 |
| Physical response ecu1 | 0x7E8 | F407 -> Tester |
| Physical request ecu2 | 0x7E1 | Tester -> F103 |
| Physical response ecu2 | 0x7E9 | F103 -> Tester |
| Functional request | 0x7DF | Broadcast |

## ISO 15765-2 Transport

- Single frame: PCI type 0, max 7 data bytes.
- First frame: PCI type 1, 12-bit total length, max 6 data bytes.
- Consecutive frame: PCI type 2, sequence number modulo 16.
- Flow control: PCI type 3, CTS/Wait/Overflow, BS and STmin.
- Default flow control: BS=8, STmin=20ms for firmware bring-up.
- Stress test can lower STmin to 1-5ms after stability is proven.

## UDS Services

| SID | Service | Notes |
| --- | --- | --- |
| 0x10 | Diagnostic session control | Default/Extended/Programming |
| 0x22 | Read data by identifier | DID table, single or multi-frame |
| 0x2E | Write data by identifier | Session/security checks and DID capacity |
| 0x27 | Security access | Seed/Key, attempts, delay, session lock |
| 0x31 | Routine control | Start/Stop/RequestResults, `0x78` pending |
| 0x3E | Tester present | Keep session alive |

### DID Database

| DID | Contents | Notes |
| --- | --- | --- |
| 0x0100 | Custom data | Application-defined |
| 0xF190 | VIN | 17-character identification |
| 0xF191 | DTC status | Virtual test value |
| 0xF192 | CAN health | Seven-byte dynamic snapshot |
| 0xF193 | Hardware version | ECU-specific |
| 0xF19A | ECU name | ECU-specific |
| 0xF19B | Software version | ECU-specific |
| 0xF1A0 | Fault control | VirtualBus injection hook only |

The portable UDS core in `firmware/common/uds_server.c` implements request
dispatch, `0x10`, `0x22`, `0x27`, `0x2E`, `0x31`, `0x3E`, S3 timeout, DID
lookup, security state, routine state, and negative responses.
`firmware/common/uds_did_db.c` provides the F407/F103 DID differences, and
`firmware/common/uds_ecu.c` wires the DID database and reference routines into
both applications. The layer is host-tested and compiled for both STM32
targets. Hardware integration still depends on the ISO-TP layer.

## Scheduling and Fault Injection

- Per-node request locks serialize access to each ECU.
- Programming session is exclusive across nodes to avoid session conflicts.
- Bus load simulator targets 90%+ with periodic background frames.
- Fault injector covers DTC, communication loss, NRC injection, and Bus-Off.
- Stress metrics distinguish physical CAN frame loss from UDS request timeouts.

## CAN Health and Bus-Off Recovery

- `CanHealthMonitor` models TEC/REC, error-active/error-passive/Bus-Off state,
  classified CAN errors, recovery attempts, and reset requests.
- Error-passive starts at 128; transmit errors reaching 256 enter Bus-Off.
- Three consecutive recovery failures request a controller or system reset.
- `WatchdogSupervisor` tracks named task deadlines as a testable policy for the
  firmware IWDG/WWDG implementation.
- VirtualEcu exposes a seven-byte health snapshot through DID `0xF192`.
- `bus-off-test` injects repeated Bus-Off events and emits a Markdown report.

The software monitor validates policy and integration only. The STM32 CAN
controller still owns the ISO 11898 error counters and 128-recessive-bit
recovery sequence.
