# ISO-TP Representative Frame Examples

These frames are from the final candidate images after CAN RX queue,
error-state and mailbox-retry hardening.

## F407 to F103

Data ID: `0x7E1`  
FC/status ID: `0x7E9`

| Stage | CAN ID | Data | Check |
| --- | ---: | --- | --- |
| FF | `0x7E1` | `10 64 03 0A 11 18 1F 26` | Total length `0x64` = 100 |
| FC 1 | `0x7E9` | `30 08 14 CC CC CC CC CC` | CTS, BS=8, STmin=20 ms |
| CF 1 | `0x7E1` | `21 2D 34 3B 42 49 50 57` | SN=1 |
| CF 8 | `0x7E1` | `28 84 8B 92 99 A0 A7 AE` | SN=8, block boundary |
| FC 2 | `0x7E9` | `30 08 14 CC CC CC CC CC` | Second block granted |
| CF 9 | `0x7E1` | `29 B5 BC C3 CA D1 D8 DF` | SN=9 |
| CF 14 | `0x7E1` | `2E AA B1 B8 CC CC CC CC` | Final 3 payload bytes |
| Status | `0x7E9` | `01 5A CC CC CC CC CC CC` | Receiver validation passed |

## F103 to F407

Data ID: `0x7E0`  
FC/status ID: `0x7E8`

| Stage | CAN ID | Data | Check |
| --- | ---: | --- | --- |
| FF | `0x7E0` | `10 64 03 0A 11 18 1F 26` | Total length 100 |
| FC 1 | `0x7E8` | `30 08 14 CC CC CC CC CC` | CTS, BS=8, STmin=20 ms |
| CF 1 | `0x7E0` | `21 2D 34 3B 42 49 50 57` | SN=1 |
| CF 8 | `0x7E0` | `28 84 8B 92 99 A0 A7 AE` | SN=8 |
| FC 2 | `0x7E8` | `30 08 14 CC CC CC CC CC` | Second block granted |
| CF 9 | `0x7E0` | `29 B5 BC C3 CA D1 D8 DF` | SN=9 |
| CF 14 | `0x7E0` | `2E AA B1 B8 CC CC CC CC` | Final data frame |
| Status | `0x7E8` | `01 5A CC CC CC CC CC CC` | Receiver validation passed |

## Expected Final Capture Checks

- Two FC frames per 100-byte transfer.
- BS=8 and STmin=20 ms in both FC frames.
- Fourteen CF frames with SN `0x1` through `0xE`.
- Observed consecutive CF spacing not materially below 20 ms.
- Final receiver status `0x5A`.
- Both receiver-side counters report zero payload errors.

## Measured Final Candidate Results

| Direction | CF / FC | Min CF-to-CF gap | Receiver counter |
| --- | --- | ---: | --- |
| F407 -> F103 | 14 / 2 | 21.845 ms | `22` success, `0` errors |
| F103 -> F407 | 14 / 2 | 21.839 ms | `21` success, `0` errors |

Both directions returned status SF `0x5A`. Formal images were restored and
passed `10 03` plus the 20-byte `22 F190` response.

## 120-Byte SN Wrap

| Direction | CF / FC | Sequence result | Min CF gap |
| --- | --- | --- | ---: |
| F407 -> F103 | 17 / 3 | `0x1...0xF, 0x0` observed | 21.837 ms |
| F103 -> F407 | 17 / 3 | `0x1...0xF, 0x0` observed | 21.876 ms |

Both directions returned `0x5A` and validated all 120 payload bytes.
