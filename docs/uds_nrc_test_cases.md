# 第 5 周 NRC 测试用例表

本表是第五周交付物之一。`Host PASS` 表示 C 主机测试和 Python 虚拟 ECU
测试均已覆盖；`HW Target` 表示真实 F407/F103 + CANable 验收仍需在 ISO-TP
层完成后执行。

## 必测 NRC

| NRC | 场景 | 请求示例 | 期望响应 | 覆盖位置 | 状态 |
| --- | --- | --- | --- | --- | --- |
| `0x11` | 不支持的 SID | `99` | `7F 99 11` | C `test_negative_responses` / Python `test_week5_nrc_matrix` | Host PASS, HW Target |
| `0x12` | 不支持的子功能 | `27 05` | `7F 27 12` | C / Python | Host PASS, HW Target |
| `0x13` | 请求长度错误 | `10`、`27 02` | `7F <SID> 13` | C / Python | Host PASS, HW Target |
| `0x22` | 条件不满足 | Extended + 已解锁后写 `0xF190` | `7F 2E 22` | C / Python | Host PASS, HW Target |
| `0x31` | DID 或 Routine 超出范围 | `22 FF FF`、`31 01 12 34` | `7F 22 31`、`7F 31 31` | C / Python | Host PASS, HW Target |
| `0x33` | 未解锁写受保护 DID | Extended 下 `2E 01 00 ...` | `7F 2E 33` | C / Python / 双节点虚拟流程 | Host PASS, HW Target |
| `0x78` | 例程仍在运行 | `31 03 02 03` 首次请求 | `7F 31 78` | C / Python / 双节点虚拟流程 | Host PASS, HW Target |
| `0x7E` | 当前会话不支持该子功能 | Default 下 `2E 01 00 ...` | `7F 2E 7E` | C / Python / 双节点虚拟流程 | Host PASS, HW Target |

## 安全访问补充 NRC

| NRC | 场景 | 请求示例 | 期望响应 | 覆盖位置 | 状态 |
| --- | --- | --- | --- | --- | --- |
| `0x24` | 未请求 Seed 就发送 Key | `27 02 00 00 00 00` | `7F 27 24` | C / Python | Host PASS, HW Target |
| `0x24` | Start 前请求例程结果 | `31 03 02 03` | `7F 31 24` | C / Python | Host PASS, HW Target |
| `0x35` | Key 错误 | `27 02 <错误 Key>` | `7F 27 35` | C / Python | Host PASS, HW Target |
| `0x36` | 连续 3 次 Key 错误 | 三次 `27 01 + 27 02` | 第三次 `7F 27 36` | C / Python | Host PASS, HW Target |
| `0x37` | 延时期内再次请求 | `27 01` | `7F 27 37` | C / Python | Host PASS, HW Target |
| `0x22` | Running 例程重复 Start | 连续两次 `31 01 02 03` | 第二次 `7F 31 22` | C / Python | Host PASS, HW Target |

## 执行方式

```powershell
.venv\Scripts\python.exe -m pytest tests\test_uds_week5.py tests\test_firmware_uds_server.py
.venv\Scripts\python.exe -m diagnoser.cli uds-week5 --node ecu1
```

真实硬件执行时，保持相同请求序列，通过 CANable/slcan 抓取响应，并把结果
追加到 `docs/test_report.md`。未实测前不得把本表状态改为 `HW PASS`。
