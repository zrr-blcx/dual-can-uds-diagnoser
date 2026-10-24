# 第 7 周：DBC 代码生成与双节点调度

## 状态

第 7 周可在主机侧完成的部分已经落地：

- 项目 DBC：`dbc/project.dbc`，可用 cantools 解析。
- DBC -> C 生成器：`diagnoser/dbc/codegen.py`。
- 生成代码：`firmware/common/project_dbc.h` / `.c`，含结构体、pack/unpack。
- 双节点队列调度器：`diagnoser/tools/scheduler.py` 中的
  `QueuedDualNodeScheduler`。
- 主机测试：DBC round-trip、Motorola/Intel 生成代码、每节点串行、
  超时重试、会话跳过、诊断仪 -> 调度器 -> 双 VirtualEcu 全链路。

真实 F407/F103 的最终 DBC 联调仍为 Target；当前结论只覆盖主机侧和虚拟总线。

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

## 运行验证

```powershell
.venv\Scripts\python.exe -m pytest tests\test_dbc_generator.py tests\test_dbc_codegen.py tests\test_scheduler.py tests\test_queued_scheduler.py
```

未完成项：

- 真实 F407/F103 上的 DBC 信号联调与总线时序复验。
- 真实硬件上调度器与 CANable 的并发/会话竞争验证。
- 本阶段不修改 Roadmap 第 7 周状态，继续保留 Target。
