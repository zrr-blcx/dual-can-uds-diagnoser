# 第 5 周 UDS 服务 v1.0（0x27 + 0x2E + 0x31）

## 范围与状态

本周在现有可移植 C UDS 核心上补齐安全访问、写 DID 和例行控制，并在
F407/F103 两套工程中启用同一服务层与差异化 DID 数据库。Python 虚拟 ECU
实现了同样的状态机，用于双节点无硬件联调。

当前状态：

- 服务层 v1.0：`0x10/0x22/0x27/0x2E/0x31/0x3E` 已完成主机验证。
- F407 与 F103：已编译 `uds_server.c`、`uds_reference_routines.c`、
  `uds_did_db.c`、`uds_ecu.c`，应用循环调用 `UdsEcu_Tick`。
- 双节点联调：使用 MemoryBus + 两个 VirtualEcu 完成
  `10 03 -> 27 01 -> 27 02 -> 2E -> 31` 全流程。
- ISO-TP 路径已完成双向 100 字节验收；F407/F103 的 `10 03` 和
  `22 F190` 已通过实机回归。
- F407 已完成 `27 -> 2E -> 31`、`0x2E/0x31` pending、写入后立即回读和
  断电重启回读；参数记录落在保留的 Sector 11。
- F103 已使用最后一个 1 KB Flash 页保存参数，完成写入、断电重启回读和
  完整 `27 -> 2E -> 31`。
- 两节点实机 NRC 矩阵已覆盖 `0x11/12/13/22/24/31/33/35/36/37/78/7E`。
- `0x0203` 检查参数区、CAN 错误状态和 IWDG/WWDG；两节点实机结果均为
  `00`。
- `0xFF00` 仅逻辑清除参数快照并恢复 DID 默认值；物理扇区擦除延后到启动
  压缩。两节点实机均为 `pending=1`、最终结果 `00`。
- F407 强制保存失败镜像返回 `7F 2E 72`，恢复正式镜像后自检和读写正常。
- 第 5 周里程碑完成。

## 0x27 安全访问

会话要求为 Extended 或 Programming，其他会话返回 NRC `0x7E`。

| 请求 | 条件 | 响应 |
| --- | --- | --- |
| `27 01` | 未解锁 | `67 01 <seed[4]>` |
| `27 01` | 已解锁 | `67 01 00 00 00 00` |
| `27 02 <key[4]>` | Key 正确 | `67 02` |
| `27 02 <key[4]>` | Key 错误 | NRC `0x35` |
| 连续 3 次 Key 错误 | - | NRC `0x36`，启动 10 s 延时 |
| 延时期内再次访问 | - | NRC `0x37` |
| `27 02` 未先请求 Seed | - | NRC `0x24` |

默认 Seed 源为 32 位 xorshift，Key 变换为：

```text
key = mix(seed XOR salt)
salt = 0x50324341
```

固定测试向量：

```text
seed = 0x87985AA5
key  = 0xE174F5DA
```

该变换只是项目联调用占位实现，量产时必须替换为 OEM 定义的 Seed/Key 算法和
每 ECU 独立密钥。切换到 Default Session、S3 超时或重新进入其他会话时，
安全状态自动锁定。

## 0x2E 写 DID

写 DID 的检查顺序：

1. 请求长度：不足 SID + DID 返回 NRC `0x13`。
2. DID 是否存在且可写：否则 NRC `0x31`。
3. 当前会话是否允许写：否则 NRC `0x7E`。
4. 是否需要解锁：未解锁返回 NRC `0x33`。
5. 是否要求特定会话：不满足返回 NRC `0x22`。
6. 数据是否超过 DID 容量：超过返回 NRC `0x31`。

本项目把“当前会话不允许写”统一映射为 NRC `0x7E`，以便和第五周用例表
保持一致；量产实现可以按 OEM 规范改成 `0x7F`。

| DID | 内容 | F407 默认值 | F103 默认值 | 写会话 | 额外条件 |
| --- | --- | --- | --- | --- | --- |
| `0xF190` | VIN | `P2F407TESTVIN0001` | `P2F103TESTVIN0001` | Extended/Programming | 需解锁，仅 Programming 可写 |
| `0xF193` | 硬件版本 | `F407-HW-REV-A` | `F103-HW-REV-A` | - | 只读 |
| `0x0100` | 自定义数据 | `00 01` | `00 01` | Extended/Programming | 需解锁，最多 16 字节 |
| `0xF191` | DTC 状态 | `00 00 00 00` | `00 00 00 00` | Extended/Programming | 需解锁，4 字节 |
| `0xF19A` | ECU 名称 | `ecu1` | `ecu2` | - | 只读 |
| `0xF19B` | 软件版本 | `V1.0` | `V1.0` | - | 只读 |

`0x2E` 首次请求只把写入内容挂起并返回 `7F 2E 78`，下一轮主循环执行存储
回调，成功返回 `6E <DID>`，失败返回标准 NRC `0x72`。RAM 缓冲区只在持久化
成功后更新，避免写失败时读到未落盘的数据。

F407 使用保留的 Flash Sector 11 保存 `0xF190/0xF191/0x0100`：

| 项 | 配置 |
| --- | --- |
| 参数区 | `0x080E0000–0x080FFFFF`，128 KB |
| 应用区 | `0x08000000–0x080DFFFF`，896 KB |
| 运行时写入 | 80 字节快照追加到空白槽，不擦除 |
| 容量 | 最多 1638 个快照 |
| 压缩时机 | 下次启动、IWDG/WWDG 启动前 |
| 完整性 | Magic、版本、记录数、CRC32、写后回读 |

F103 使用最后一个 1 KB 页 `0x0800FC00–0x0800FFFF`，应用区限制为
63 KB。它复用相同的追加日志：最多 12 条 80 字节快照，运行时返回
`0x72` 而不擦除，下次启动自动压缩。

`0xFF00` 不执行运行中物理扇区擦除，而是追加一条空快照作为逻辑清除，
立即把 DID 恢复默认值。物理擦除仍只在启动压缩、看门狗启动前发生。

## 0x31 例行控制

子功能支持：

- `01` Start
- `02` Stop
- `03` RequestResults

例程表：

| Routine ID | 名称 | 会话 | 解锁要求 | 行为 |
| --- | --- | --- | --- | --- |
| `0x0203` | 自检 | Extended/Programming | 否 | Start 后进入 Running；RequestResults 时先回 NRC `0x78`，随后 ECU 主动发送最终正响应 |
| `0xFF00` | 参数清除 | Extended/Programming | 是 | 追加空快照恢复 DID 默认值；使用 `0x78` + 最终响应时序 |

处理规则：

- Running 状态下重复 Start：NRC `0x22`。
- 非 Running 状态下 Stop：NRC `0x24`。
- Start 之前 RequestResults：NRC `0x24`。
- RequestResults 期间 ECU 先回 `7F 31 78`，再按 P2* 发送最终正响应；
  Python 诊断仪自动等待。两个固件主循环会把
  `UDS_PROCESS_RESPONSE_PENDING` 交给 ISO-TP 发回，并通过
  `UdsEcu_PollPending()` 继续推进例程；例程完成后自动发送最终正响应，
  不要求诊断仪重复请求。
- 未定义的 Routine ID：NRC `0x31`。
- 未支持子功能：NRC `0x12`。
- 会话不允许：NRC `0x7E`。
- 未解锁且例程要求安全访问：NRC `0x33`。
- `0x0203` 返回 1 字节故障位掩码：bit0 参数区、bit1 CAN 状态、
  bit2 看门狗；`00` 表示全部通过。
- `0xFF00` 成功时返回 `00`，逻辑清除失败时返回 `01`。

例程通过 `UdsRoutineHandler_t` 回调实现。`UdsEcu_InitWithHooks()` 接收
应用自检与参数清除回调；`uds_reference_routines.c` 仅保留为无硬件参考
实现。真实节点使用 `0x0203` 状态检查和 `0xFF00` 空快照清除。

## 主机验证

```powershell
.venv\Scripts\python.exe -m pytest
.venv\Scripts\python.exe -m diagnoser.cli uds-week5 --node ecu1
```

`uds-week5` 覆盖：

```text
10 03 -> 27 01 -> 27 02 -> 2E 01 00 -> 22 01 00
      -> 31 01 02 03 -> 31 03 02 03
      -> 31 01 FF 00 -> 31 03 FF 00
```

未完成项保留在第五周目标中：完整 `27/2E/31` 双节点实测、真实 Flash
擦除和实机 NRC 时序记录。

2026-10-24 修正了固件挂起响应路径：F407/F103 原先只发送
`UDS_PROCESS_RESPONSE`，会丢弃首次 `7F 31 78`。现在 C 主机测试覆盖
“pending -> 自动最终响应 -> 不重复发送”，完整回归 `102 passed`。
F407/F103 的真实 CANable pending 时序均已通过。
