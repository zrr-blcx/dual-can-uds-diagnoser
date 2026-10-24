# Week 3 ISO-TP Acceptance Matrix

This matrix maps the original Week 3 Roadmap to implementation and evidence.
It does not change the Roadmap status; user acceptance remains required.

## Original Roadmap Requirements

| Requirement | Implementation | Evidence | Result |
| --- | --- | --- | --- |
| Hand-written SF/FF/CF/FC | `firmware/common/isotp.c/.h` | Host PCI tests; final CAN traces | PASS |
| Separate TX and RX state machines | `IsotpTx_t`, `IsotpRx_t` | Host state tests and bidirectional hardware | PASS |
| SN `0x0` to `0xF` wrap | Modulo-16 sender/receiver logic | 120-byte host test | PASS |
| Default BS=8 | `IsotpConfig_Init()` and FC encoding | Host assertions; final FC traces | PASS |
| Default STmin=20 ms | `IsotpConfig_Init()` and sender spacing | Host assertions; final 21.8 ms hardware gaps | PASS |
| `N_As` | CAN enqueue and transmit-completion deadline | Host timeout and mailbox-busy tests | PASS |
| `N_Bs` | First-FC deadline | Host timeout test | PASS |
| `N_Cr` | Sender block-FC and receiver CF deadlines | Host timeout tests | PASS |
| `N_Ar` | FC transmit-completion deadline | Host timeout test | PASS |
| F407 to F103 100 bytes | Benchmark TX/RX modes | Final raw capture and F103 counters | PASS |
| F103 to F407 100 bytes | Benchmark TX/RX modes | Final raw capture and F407 counters | PASS |
| Full receive validation | Fixed 100-byte payload checker | Status `0x5A`, zero receiver errors | PASS |
| CANable frame verification | Python-can capture and trace summarizer | `isotp_frame_examples.md` | PASS |
| C TP source delivery | Firmware common sources | Repository source | PASS |
| Integration record | `isotp_week3.md` and test report | Repository docs | PASS |

## Final Candidate Hardware Values

| Direction | CF / FC | Min CF gap | Receiver result |
| --- | --- | --- | --- |
| F407 -> F103 | 14 / 2 | 21.845 ms | 22 success, 0 errors |
| F103 -> F407 | 14 / 2 | 21.839 ms | 21 success, 0 errors |

Both directions use BS=8 and STmin=20 ms and return status SF `0x5A`.

## Added Engineering Hardening

These items are useful but were not explicit Week 3 Roadmap requirements:

- BS=0 support.
- Reserved/WAIT/OVERFLOW FC handling tests.
- Invalid SF/FF and duplicate FF rejection.
- Immediate abort on out-of-order SN.
- Unread-message overwrite protection.
- CAN RX queue between ISR and main loop.
- CAN mailbox-busy retry within `N_As/N_Ar`.
- 32-bit tick-wrap test.
- Python tester defaults and strict receive validation.
- Formal-image `10 03` and multi-frame `22 F190` regression.

## Final Decision

The original Week 3 Roadmap requirements are implemented and have host plus
final-candidate hardware evidence. The user requested completion of the
remaining negative hardware cases, and all passed. The Week 3 Roadmap milestone
is complete as of 2026-10-24.

Optional stricter hardware cases not required by the original wording:

- Malformed SN and timeout injection on the final firmware: now complete.
- 120-byte SN-wrap transfer on hardware: now complete in both directions.

## Final Negative Hardware Evidence

| Test | Evidence |
| --- | --- |
| Wrong SN then recovery | `rx_errors=1`, subsequent status `0x5A` |
| 120-byte SN wrap, F407 -> F103 | 17 CF, 3 FC, SN `0xF -> 0x0` |
| 120-byte SN wrap, F103 -> F407 | 17 CF, 3 FC, SN `0xF -> 0x0` |
| `N_Bs` | Sender timeout counter increased |
| `N_Cr` sender | Sender timeout counter increased |
| `N_Cr` receiver | `rx_timeouts=1` |
| `N_As` | `tx_timeouts=12` under forced transmit-busy |
| `N_Ar` | `rx_timeouts=1` under forced FC transmit-busy |
