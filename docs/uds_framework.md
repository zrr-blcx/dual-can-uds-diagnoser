# UDS 服务核心（第 4 周基础 + 第 5 周服务）

## 范围

第 4 周实现不依赖 STM32 HAL 和 CAN 传输的 UDS 服务核心。第 5 周在同一
核心上补齐安全访问、写 DID 和例行控制。ISO-TP 完成后，F407/F103 已通过
三种会话、标准 DID、多 DID、S3 超时、NRC 和 `udsoncan` 的 CANable 实机
验收；完整 `27/2E/31`、自检、参数清除和双节点 NRC 矩阵均已完成实机验收。

## 分层边界

```text
ISO-TP receive
    |
    v
UdsServer_Process(request, response)
    |
    v
ISO-TP transmit
```

`firmware/common/uds_server.c` 只处理完整 UDS 请求和响应字节，不调用 HAL，
也不直接访问 CAN。后续 TP 层负责：

1. 将收到的完整 UDS 请求交给 `UdsServer_Process`。
2. 将长度非 0 的响应交给 ISO-TP 发送函数。
3. 在主循环或定时任务中调用 `UdsServer_Tick(HAL_GetTick())`。

## 服务分发

| SID | 服务 | 当前行为 |
| --- | --- | --- |
| `0x10` | Diagnostic Session Control | Default、Extended、Programming；支持抑制正响应位 |
| `0x22` | Read Data By Identifier | 支持单个或多个 DID，响应超出缓冲区时拒绝 |
| `0x27` | Security Access | Seed/Key、解锁状态、错误次数、10 s 延时、会话/超时自动锁定 |
| `0x2E` | Write Data By Identifier | 会话/解锁/DID 容量检查；先回 `0x78`，持久化成功后再更新 RAM |
| `0x31` | Routine Control | `0203` 自检、`FF00` 参数清除，均支持 `0x78` 响应挂起 |
| `0x3E` | Tester Present | 支持 `0x00/0x80` 子功能并刷新 S3 计时 |

UDS 核心通过 `UdsServerConfig_t` 接收 DID 表、例程表、P2、P2*、S3 和允许
的会话掩码。DID 数据指针和例程回调必须在服务器生命周期内保持有效。

安全访问、写 DID 和例行控制的详细行为见
[`docs/uds_week5.md`](uds_week5.md)，NRC 用例见
[`docs/uds_nrc_test_cases.md`](uds_nrc_test_cases.md)。

## 会话与 S3

- 默认 S3server 为 5000 ms。
- Extended 和 Programming 会话激活 S3 定时器。
- 收到有效 `0x22` 或 `0x3E` 请求时刷新 S3。
- S3 到期后自动回到 Default Session，并增加超时计数。
- Default Session 请求会停止 S3 定时器。
- P2server 默认编码为 50 ms，P2*server 默认编码为 5000 ms（10 ms 单位）。

## DID 数据库

| DID | 内容 | 模拟节点值 | 写规则 |
| --- | --- | --- | --- |
| `0xF190` | VIN | `P2F407TESTVIN0001` / `P2F103TESTVIN0001` | Extended/Programming + 解锁，仅 Programming 可写 |
| `0xF193` | 硬件版本 | `F407-HW-REV-A` / `F103-HW-REV-A` | 只读 |
| `0x0100` | 自定义数据 | `00 01` | Extended/Programming + 解锁，最多 16 字节 |
| `0xF191` | DTC 状态 | `00 00 00 00` | Extended/Programming + 解锁，4 字节 |
| `0xF192` | CAN 健康快照 | 7 字节动态数据 | 仅虚拟 ECU |
| `0xF1A0` | 故障控制 | 测试钩子 | 仅虚拟 ECU 和隔离 `BenchTest` 镜像，正式固件不包含 |

Python DID 常量集中在 `diagnoser/uds/dids.py`，避免客户端、虚拟 ECU 和测试
各自维护不同编号。

## 负响应

| NRC | 场景 |
| --- | --- |
| `0x11` | 不支持的 SID |
| `0x12` | 不支持的会话或子功能 |
| `0x13` | 请求长度错误 |
| `0x22` | 条件不满足（例如 VIN 未在 Programming 会话写入） |
| `0x24` | 请求顺序错误（Key 前无 Seed、例程未 Start） |
| `0x31` | DID/Routine 不存在或超出范围 |
| `0x33` | 安全访问未解锁 |
| `0x35` | Key 错误 |
| `0x36` | 连续 Key 错误超过次数 |
| `0x37` | 安全延时未结束 |
| `0x72` | 持久化写入或回读校验失败 |
| `0x78` | 例程执行中，响应挂起 |
| `0x7E` | 当前会话不支持该子功能 |

负响应格式固定为：

```text
7F <request SID> <NRC>
```

## 主机验证

编译并执行可移植 C 核心测试：

```powershell
python -m pytest tests\test_firmware_uds_server.py
```

运行单节点虚拟 UDS 联调：

```powershell
python -m diagnoser.cli uds-smoke --node ecu1
python -m diagnoser.cli uds-week5 --node ecu1
```

`uds-week5` 会执行 `10 03 -> 27 01 -> 27 02 -> 2E -> 31` 全流程，并验证
Default 会话拒绝和重新进入会话后自动锁定。

真实 CANable 使用相同流程：

```powershell
python -m diagnoser.cli --interface slcan --channel COMx uds-smoke --node ecu1
```

真实通道验证前必须确认 CANable 接口、波特率、物理总线和节点寻址均正确。
第 4 周完整实机测量见
[`uds_week4_acceptance.md`](uds_week4_acceptance.md)。
