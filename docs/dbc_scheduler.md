# 第 7 周：DBC 代码生成与双节点调度

## 状态

第 7 周主机侧与真实硬件验收均已完成：

- 项目 DBC：`dbc/project.dbc`，可用 cantools 解析。
- DBC -> C 生成器：`diagnoser/dbc/codegen.py`。
- 生成代码：`firmware/common/project_dbc.h` / `.c`，含结构体、pack/unpack。
- 双节点队列调度器：`diagnoser/tools/scheduler.py` 中的
  `QueuedDualNodeScheduler`。
- 多节点 CLI 共享一个物理 CAN 设备：`diagnoser/transport/shared.py`
  按响应 ID 将接收帧分发到独立队列。
- 2026-10-26 实机共享通道 `dual-node-smoke`：F407/F103 各 20 次并发读取，
  均通过 `0x7E8/0x7E9` 独立返回并恢复 Default 会话。
- 2026-10-26 实机 `session-conflict`：F407 占用 `0x02` 时 F103 任务被跳过，
  锁释放后 F103 恢复读取，两节点最终回到 Default。
- 2026-10-26 DBC 测试镜像：`0x180` 与 `0x280` 各捕获 27 帧，
  Motorola/Intel 信号解码值与预期一致。
- 主机测试：DBC round-trip、Motorola/Intel 生成代码、每节点串行、
  超时重试、会话跳过、诊断仪 -> 调度器 -> 双 VirtualEcu 全链路。

真实共享总线、并发调度、编程会话互斥和 DBC 信号联调均已验证。

## 项目 DBC

| 报文 | CAN ID | 发送节点 | 说明 |
| --- | ---: | --- | --- |
| `DiagnosticRequest_ECU1` | `0x7E0` | Tester | F407 物理请求，8 个 raw byte |
| `DiagnosticResponse_ECU1` | `0x7E8` | ECU_F407 | F407 物理响应 |
| `DiagnosticRequest_ECU2` | `0x7E1` | Tester | F103 物理请求 |
| `DiagnosticResponse_ECU2` | `0x7E9` | ECU_F103 | F103 物理响应 |
| `EngineData` | `0x180` | ECU_F407 | Motorola 示例 |
| `VehicleStatus` | `0x280` | ECU_F103 | Intel 示例 |

`EngineData`：

| 信号 | 起始位 | 长度 | 字节序 | 缩放/偏移 |
| --- | ---: | ---: | --- | --- |
| `EngineSpeed` | 7 | 16 | Motorola (`@0`) | ×0.25，单位 rpm |
| `CoolantTemp` | 23 | 8 | Motorola (`@0`) | ×1，偏移 -40，单位 degC |

`VehicleStatus`：

| 信号 | 起始位 | 长度 | 字节序 | 缩放/偏移 |
| --- | ---: | ---: | --- | --- |
| `VehicleSpeed` | 0 | 16 | Intel (`@1`) | ×0.01，单位 km/h |
| `BrakeActive` | 16 | 1 | Intel (`@1`) | 1 bit |

DBC 的 `@0` 表示 Motorola，`@1` 表示 Intel。Motorola 的起始位是
“锯齿编号”的 MSB 位置；cantools 会把它转换成网络位号（bit 0 = byte 0 的 MSB）。

验证向量：

| 报文 | 输入 | 期望字节 |
| --- | --- | --- |
| `EngineData` | speed=1234.5 rpm，temp=85 degC | `13 4A 7D 00 00 00 00 00` |
| `VehicleStatus` | speed=42.5 km/h，brake=1 | `9A 10 01 00 00 00 00 00` |
| `DiagnosticRequest_ECU1` | `10 03` | `10 03 00 00 00 00 00 00` |

## C 代码生成

```powershell
.venv\Scripts\python.exe -m diagnoser.dbc.codegen `
  --dbc dbc/project.dbc `
  --header firmware/common/project_dbc.h `
  --source firmware/common/project_dbc.c
```

生成器用 cantools 解析 DBC，并为每条报文生成：

```c
typedef struct { ... } EngineData_t;
void EngineData_pack(const EngineData_t *message, uint8_t data[8]);
void EngineData_unpack(const uint8_t data[8], EngineData_t *message);
```

实现要点：

- Motorola 信号按网络位号定位，跨字节时不会错位。
- 同一报文可以混合 Motorola/Intel；生成器分别维护 big-endian 与
  little-endian 两个 64-bit 字，最后按字节 OR。
- 缩放/偏移在生成代码里完成；有符号值做符号扩展。
- 生成代码不依赖 cantools，可在固件或主机 C 测试中直接编译。
- `tests/test_dbc_codegen.py` 会重新生成并和已提交文件逐字比较，
  防止 DBC 与 C 代码漂移。

## 双节点队列调度

`QueuedDualNodeScheduler` 为每个节点启动一个队列和一个工作线程：

- 同一节点的任务严格按提交顺序串行执行。
- 不同节点可以并行执行。
- `submit(..., retries=N)` 只在 `TimeoutError` 时重试；其他异常立即记为 error。
- 每次任务返回 `JobOutcome(status=ok|timeout|error|skipped, attempts, elapsed_s, ...)`。
- 编程会话（默认 `0x02`）使用全局互斥条件：其他节点的编程任务会等待；
  等待超过 `session_wait_timeout_s` 时标记为 `skipped`，不阻塞队列。
- 调度器只负责排队和会话互斥；实际 `0x10` 会话切换仍由任务函数调用
  `UdsClient` 完成。

示意：

```python
from diagnoser.tools.scheduler import QueuedDualNodeScheduler, SchedulerNode

scheduler = QueuedDualNodeScheduler(
    [SchedulerNode("ecu1"), SchedulerNode("ecu2")],
    default_retries=1,
)
future = scheduler.submit("ecu1", lambda: client.read_data_by_identifier(0xF190))
outcome = future.result(timeout=5)
scheduler.close()
```

## 全链路验证

`tests/test_queued_scheduler.py` 覆盖：

- 每节点独立队列：两个节点同时等待对方事件，证明可以并行。
- 同节点 4 个任务最大并发为 1。
- 超时重试 3 次后成功；重试耗尽返回 `timeout`；普通异常返回 `error`。
- 两个编程会话任务互斥；等待超时的任务标记 `skipped`。
- `run_uds_week5` 通过调度器分别驱动两个 VirtualEcu，双方 13 步全流程 PASS。

## 物理共享总线

真实 CANable 通常不能被多个 `UdsClient` 重复打开。多节点 CLI 现在复用一条
`python-can` 总线：

- `SharedCanBus` 只有一个底层接收线程。
- `0x7E8` 和 `0x7E9` 分别进入独立队列，其他节点不会消费或丢弃彼此的响应。
- 发送操作共用一把锁，诊断请求和压力测试背景帧可以共用同一物理适配器。
- `stress` 在非内存接口下使用同一个共享总线产生背景负载，不再另建
  `MemoryBus`。

`tests/test_shared_can_bus.py` 覆盖按 ID 分发和两个 ISO-TP 客户端共用底层
总线；`tests/test_cli.py` 覆盖多节点只调用一次总线工厂。

## DBC 实机验证

F407 和 F103 使用隔离的 `-DbcBench` 镜像分别发送：

| Message | CAN ID | Signals | Frames in 3 s | Result |
| --- | --- | --- | ---: | --- |
| EngineData | `0x180` | 1234.5 rpm / 85 degC | 27 | PASS |
| VehicleStatus | `0x280` | 42.5 km/h / brake active | 27 | PASS |

抓包由 cantools 直接解码。测试后恢复正式固件，并再次通过双节点 20/20
读取；F407 Sector 11 哈希未变化。

## 实机共享通道冒烟

`dual-node-smoke` 通过 `QueuedDualNodeScheduler` 同时驱动两个客户端：

- 每个节点进入 Extended 会话。
- 每节点读取指定 DID 的请求次数一致。
- 校验响应 ID 只能是本节点的 `0x7E8` 或 `0x7E9`。
- 完成后恢复 Default 会话。

2026-10-26 使用真实 CANable 在 500 kbps 下测得：

| Node | Requests | Success | Response ID | Session restored |
| --- | ---: | ---: | --- | --- |
| F407 | 20 | 20 | `0x7E8` | PASS |
| F103 | 20 | 20 | `0x7E9` | PASS |

## 实机编程会话冲突

`session-conflict` 让第一个节点在 Extended 之后进入 Programming 并保持
`hold-seconds`，第二个节点的 `0x02` 任务等待 `wait-timeout` 后由调度器跳过。
若冲突任务仍被调度，测试会单独记录该错误。

2026-10-26 真实硬件结果：

| Check | Result |
| --- | --- |
| F407 进入 `0x02` | PASS |
| F103 冲突任务跳过 | PASS |
| 冲突任务未执行 | PASS |
| F407 恢复 Default | PASS |
| F103 恢复读取 | PASS |

## 运行验证

```powershell
.venv\Scripts\python.exe -m pytest tests\test_dbc_generator.py tests\test_dbc_codegen.py tests\test_scheduler.py tests\test_queued_scheduler.py
.venv\Scripts\python.exe -m pytest tests\test_shared_can_bus.py tests\test_cli.py
.venv\Scripts\python.exe -m diagnoser.cli --interface slcan --channel COMx --retries 0 dual-node-smoke --requests-per-node 20
.venv\Scripts\python.exe -m diagnoser.cli --interface slcan --channel COMx --retries 0 session-conflict
```

未完成项：

- 无。第 7 周里程碑已完成，Roadmap 状态已更新。
