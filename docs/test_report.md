# Test Report Template

> Fill in measured results after hardware testing. Values marked Target are not
> yet measured.

## Environment

| Item | Value |
| --- | --- |
| Date | - |
| Tester | - |
| Firmware version | - |
| Diagnostic tool version | - |
| CAN bitrate | 500 kbps |
| Transceivers | SN65HVD230 x2 |
| USB-CAN | CANable slcan |

## Metrics Definition

- Physical frame loss rate: frames sent on bus vs frames observed/ACKed.
- UDS request timeout rate: diagnostic requests without a valid response.
- Response time: request send to positive/negative response receive.
- Bus load: theoretical and measured percentage of bus bit time.

## Baseline Test

| Node | Requests | Success | Timeout | Error | Loss rate | Avg RTT | Max RTT |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| ecu1 | - | - | - | - | - | - | - |
| ecu2 | - | - | - | - | - | - | - |

## 90%+ Load Stress Test

| Node | Requests | Success | Timeout | Error | Loss rate | Avg RTT | Max RTT |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| ecu1 | - | - | - | - | - | - | - |
| ecu2 | - | - | - | - | - | - | - |

## 10,000 Frame Final Test

| Node | Frames | Loss rate target | Result |
| --- | ---: | ---: | --- |
| ecu1 | 5000 | < 0.1% | Target |
| ecu2 | 5000 | < 0.1% | Target |

## NRC Coverage Matrix

| NRC | Scenario | Expected | Result |
| --- | --- | --- | --- |
| 0x11 | Unsupported service | PASS | Target |
| 0x12 | Unsupported subfunction | PASS | Target |
| 0x13 | Wrong message length | PASS | Target |
| 0x22 | Conditions not correct | PASS | Target |
| 0x31 | Request out of range | PASS | Target |
| 0x33 | Security access denied | PASS | Target |
| 0x78 | Response pending | PASS | Target |

## Bus-Off Recovery

| Test | Expected | Result |
| --- | --- | --- |
| Trigger Bus-Off with relay | Node enters Bus-Off | Target |
| Software reinit via HAL_CAN_Start | Node returns to normal mode | Target |
| Recovery time | Measured in ms | Target |
| Recover 3 times consecutively | Stable | Target |

## Conclusion

- Physical frame loss below 0.1% at normal load: Target.
- UDS timeout handling verified: Target.
- Bus-Off recovery conforms to ISO 11898: Target.
