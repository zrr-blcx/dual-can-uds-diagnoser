# Architecture

## System Layers

```text
PC Diagnostic Tool
==================
CLI layer        diagnoser.cli
UDS layer        diagnoser.uds (0x10/0x22/0x2E/0x31, NRC)
ISO-TP layer     diagnoser.transport.isotp
CAN layer        python-can bus adapter

STM32 ECU (planned)
==================
Application layer
UDS service layer
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
| 0x2E | Write data by identifier | Requires security access |
| 0x27 | Security access | Seed/Key before 0x2E |
| 0x31 | Routine control | Start/Stop/RequestResults |
| 0x3E | Tester present | Keep session alive |

## Scheduling and Fault Injection

- Per-node request locks serialize access to each ECU.
- Programming session is exclusive across nodes to avoid session conflicts.
- Bus load simulator targets 90%+ with periodic background frames.
- Fault injector covers DTC, communication loss, NRC injection, and Bus-Off.
- Stress metrics distinguish physical CAN frame loss from UDS request timeouts.
