# ISO-TP Protocol Contract

This document is the Week 3 implementation contract for the F407/F103
diagnostic bus. C firmware and the Python reference transport must use the
same addressing and timing assumptions.

## Addressing

| Role | Request ID | Response ID | FC TX ID |
| --- | ---: | ---: | ---: |
| F407 UDS server | `0x7E0` | `0x7E8` | `0x7E8` |
| F103 UDS server | `0x7E1` | `0x7E9` | `0x7E9` |
| Functional request | `0x7DF` | n/a | n/a |

- A server receives SF/FF/CF data on its request ID.
- A server sends FC and response SF/FF/CF on its response ID.
- A tester sends request data on the request ID and response-direction FC on
  the response ID.
- FC must never be transmitted on the request ID.

## CAN Frame Format

- Classical CAN, 11-bit standard identifiers.
- CAN bitrate: 500 kbps.
- Diagnostic frames use DLC 8.
- Unused data bytes are padded with `0xCC`.
- SF payload: 1 to 7 bytes.
- FF payload: 6 bytes, followed by CF frames carrying up to 7 bytes.
- Total payload length: 12-bit field, maximum 4095.
- CF sequence number: `0x0` to `0xF`, wrapping to `0x0`.

## Flow Control

- Default BS: 8.
- Default STmin: 20 ms.
- BS=0 means all remaining CF frames are allowed after one CTS.
- Receiver advertises the configured BS and STmin in every FC.
- Sender honors the peer's BS and STmin, including across BS block boundaries.
- Supported FC statuses: CTS, WAIT, OVERFLOW.
- Reserved FC statuses abort the sender.
- A WAIT refreshes the applicable FC wait deadline.

## Buffers and Configuration

| Item | Contract |
| --- | --- |
| Per-endpoint RX capacity | 300 bytes |
| Diagnostic request/response buffer | 300 bytes |
| CAN RX queue depth | 16 raw frames |
| Queue overflow policy | Drop newest frame and increment `dropped` |
| `IsotpConfig_Init()` | Applies the project defaults above |
| Padding | `0xCC` |

The receive buffer must not be overwritten while an unread complete message is
waiting. A new SF/FF received in that state returns `ISOTP_ERR_BUSY`.

## Timeouts

| Timer | Default | Meaning |
| --- | ---: | --- |
| `N_As` | 1000 ms | Sender frame enqueue/transmit completion |
| `N_Bs` | 1000 ms | Sender waiting for the first FC |
| `N_Cr` | 1000 ms | Sender waiting for the next block FC |
| `N_Cr` | 1000 ms | Receiver waiting for the next CF |
| `N_Ar` | 1000 ms | Receiver FC transmit completion |

All timeout comparisons use unsigned elapsed-time arithmetic so a 32-bit
millisecond tick wrap is supported.

## State and Error Rules

- Only one outgoing transfer may be active per endpoint.
- Only one incoming transfer may be active per endpoint.
- A second FF while a transfer is active aborts the old transfer.
- An out-of-order CF aborts and resets the receive transaction.
- A CF without FF is rejected.
- A declared FF length of 7 or less is invalid.
- A zero-length SF is invalid.
- Payload overflow sends FC.OVERFLOW when possible and aborts reception.
- Timeout aborts the current transfer and leaves the endpoint ready for a new
  FF/SF.
- Clearing a complete message is explicit through `IsotpRx_Read()`.

## Interrupt and Main-Loop Boundary

- `CanDriver_RxIrqHandler()` only reads HAL RX FIFO0 frames into a portable
  ring queue.
- `CanDriver_ProcessRx()` runs in the main loop and invokes the CAN callback.
- ISO-TP parsing, FC transmission, UDS dispatch and response generation run in
  the main loop.
- The application must call RX processing, ISO-TP polling and UDS ticking every
  main-loop iteration.

## API Surface

```c
void IsotpConfig_Init(...);
void IsotpTx_Init(...);
void IsotpRx_Init(...);
IsotpResult_t IsotpTx_Start(...);
IsotpResult_t IsotpTx_Poll(...);
IsotpResult_t IsotpRx_OnFrame(...);
IsotpResult_t Isotp_RouteFrame(...);
IsotpResult_t IsotpRx_Poll(...);
bool IsotpRx_MessageReady(...);
IsotpResult_t IsotpRx_Read(...);
void IsotpTx_Reset(...);
void IsotpRx_Reset(...);
```

## Host Acceptance Matrix

| Area | Required case |
| --- | --- |
| SF | 1, 3 and 7 bytes; zero length rejected |
| Multi-frame | 8, 63, 100, 120, 255 bytes where capacity permits |
| Sequence | Normal wrap `0xF -> 0x0`; wrong SN aborts |
| Flow control | BS=0, BS=8, WAIT, OVERFLOW, reserved status |
| STmin | Zero, 20 ms, and cross-block spacing |
| Timers | `N_As`, `N_Bs`, `N_Cr`, `N_Ar`, tick wrap |
| Buffer | Overflow and unread-message protection |
| Dispatch | FC to sender; SF/FF/CF to receiver |
| Integration | CAN RX queue -> main loop -> UDS -> TP response |

## Hardware Acceptance Gate

The milestone is not complete until:

- F407 to F103 and F103 to F407 each transfer 100 bytes successfully.
- Each direction uses FF + 14 CF and two FC with BS=8.
- Observed CF spacing is not below 20 ms by more than measurement tolerance.
- Receiver-side payload validation reports no errors.
- CANable evidence includes representative FF, FC, CF and final status frames.
- Formal images are restored and pass `10 03` and multi-frame `22 F190`.
