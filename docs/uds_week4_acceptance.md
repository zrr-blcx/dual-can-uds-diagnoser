# 第 4 周 UDS 实机验收

## 目标

关闭第 4 周 Roadmap 的实机验收缺口：

- F407/F103 的 `0x10` Default、Extended、Programming 会话。
- `0x22` 读取 `0xF190`、`0xF193`、`0x0100` 和多 DID。
- `0x3E` 刷新 S3，以及超时回退 Default。
- `NRC 0x13`、`NRC 0x31` 真实负响应。
- 同一 CANable 通道下的 F407/F103 双节点寻址。
- `python-can + udsoncan` 通过 CANable 完成真实互操作。

## 环境

| 项目 | 配置 |
| --- | --- |
| 日期 | 2026-10-24 |
| 节点 | STM32F407ZGT6 / STM32F103C8T6 |
| CAN | 500 kbps，经典 CAN |
| USB-CAN | CANable，slcan，板载 120Ω 关闭 |
| 固件 | 第 3 周最终候选，含第 4 周 UDS 服务 |
| Python | 3.14.7 |
| python-can | 4.6.1 |
| udsoncan | 1.26.1 |

接线拓扑与第 3 周相同。第二周接入的继电器仍在总线中，但验收期间保持去磁：
CH1 常闭导通，CH2 常开，未执行任何故障注入。

## 基础连通性

命令：

```powershell
py -3.14 -m diagnoser.cli --interface slcan --channel COMx trace --seconds 3 --id 0x123
```

结果：3.0 秒收到 12 帧 `0x123`，最小间隔 `173.6 ms`，平均 `245.9 ms`，
最大 `332.0 ms`。数据 `00/01` 交替，确认两个节点的周期帧均在发送。

## 会话与 DID

`uds-smoke` 在 `retries=0` 下分别对两节点执行，均通过：

| 节点 | 会话 | VIN | 硬件版本 | 自定义 DID | 未知 DID |
| --- | --- | --- | --- | --- | --- |
| F407 | `50 03` | `P2F407TESTVIN0001` | `F407-HW-REV-A` | `00 01` | NRC `0x31` |
| F103 | `50 03` | `P2F103TESTVIN0001` | `F103-HW-REV-A` | `00 01` | NRC `0x31` |

三种会话分别验证：

| 节点 | `10 01` | `10 02` | `10 03` | 响应时间 |
| --- | --- | --- | --- | --- |
| F407 | `50 01` | `50 02` | `50 03` | 4.0 / 7.8 / 11.0 ms |
| F103 | `50 01` | `50 02` | `50 03` | 9.7 / 7.3 / 4.9 ms |

多 DID 请求 `22 F1 93 01 00`：

| 节点 | 响应 | 耗时 |
| --- | --- | --- |
| F407 | `62 F1 93 46 34 30 37 2D 48 57 2D 52 45 56 2D 41 01 00 00 01` | 34.7 ms |
| F103 | `62 F1 93 46 31 30 33 2D 48 57 2D 52 45 56 2D 41 01 00 00 01` | 37.5 ms |

## S3 与负响应

单进程测试流程：

1. `10 03` 进入 Extended。
2. 等待 3 秒，发送 `3E 00`。
3. 等待 3 秒，发送未解锁的 `2E 01 00 AA`。
4. Extended 中预期 `7F 2E 33`；若已回 Default，应得到 `7F 2E 7E`。
5. 再等待 5.3 秒，重复 `2E` 请求，确认已回 Default。

| 节点 | `3E 00` 时刻 | 第 6 秒仍为 Extended | 超时后回 Default |
| --- | --- | --- | --- |
| F407 | 3.024 s | PASS，`7F 2E 33` | PASS，`7F 2E 7E` |
| F103 | 3.008 s | PASS，`7F 2E 33` | PASS，`7F 2E 7E` |

错误长度请求 `22 F1`：

| 节点 | 响应 | 耗时 |
| --- | --- | --- |
| F407 | `7F 22 13` | 2.3 ms |
| F103 | `7F 22 13` | 8.9 ms |

未知 DID 请求 `22 FF FF` 在两节点均返回 `7F 22 31`。

## 双节点寻址

两个客户端在同一 CANable 进程中交替请求：

| 节点 | 请求 ID | 响应 ID | VIN 响应 |
| --- | --- | --- | --- |
| F407 | `0x7E0` | `0x7E8` | `P2F407TESTVIN0001` |
| F103 | `0x7E1` | `0x7E9` | `P2F103TESTVIN0001` |

会话、VIN、硬件版本和自定义 DID 均未出现跨节点响应。

## udsoncan 互操作

`udsoncan.Client` 通过 `UdsOnCanLink` 连接同一 CANable 和仓库 ISO-TP
传输，对两个节点完成：

| 测试 | F407 | F103 |
| --- | --- | --- |
| `change_session(0x03)` | PASS | PASS |
| 读取 `0xF190` | PASS | PASS |
| 读取 `0xF193` | PASS | PASS |
| 读取 `0x0100` | PASS | PASS |
| 未知 DID 抛出 `NegativeResponseException(0x31)` | PASS | PASS |

说明：`udsoncan` 是诊断服务客户端，本身不提供 CAN 驱动；本次使用
`python-can + 项目 ISO-TP + UdsOnCanLink + udsoncan`。这保留了真实
CANable 端到端互操作，同时不引入 Roadmap 第 6 周明确不要求的
`can-isotp` 依赖。

## 结论

第 4 周 Roadmap 的 UDS 分发、三种会话、三个标准 DID、多 DID、S3 回退、
`NRC 0x13/0x31`、双节点基础 UDS 通信和 udsoncan 实机互操作均已通过。
未修改固件源码，现有机架接线和去磁继电器状态无需调整。
