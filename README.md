# dual-can-uds-diagnoser

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Python](https://img.shields.io/badge/Python-3.10%2B-blue.svg)]()
[![CI](https://github.com/zrr-blcx/dual-can-uds-diagnoser/actions/workflows/ci.yml/badge.svg)](https://github.com/zrr-blcx/dual-can-uds-diagnoser/actions/workflows/ci.yml)

基于 Python 的双节点 CAN + UDS 诊断工具链，用于 STM32F407/F103 双节点 CAN
总线环境下的协议验证与故障分析。

## Status

- Python diagnostic tool V1.0 scaffold: ready for development and tests.
- STM32 firmware: F407/F103 Week 1 CAN 500 kbps baseline and deliverables complete.
- Week 4 portable UDS core: `0x10/0x22/0x3E`, DID database, and S3 timeout
  implemented and host-tested.
- Week 5 UDS service v1.0: `0x27/0x2E/0x31`, security state machine, write
  rules, routine control, NRC coverage, and dual-node virtual flow.
- Week 6 diagnostic tool V1.0 Alpha: P2/P2* timing, `0x78` wait, timeout
  retries, timestamped/JSON output, CANable config, and udsoncan interop.
- Week 2 CAN health and Bus-Off recovery: SCE error handling, IWDG,
  hardware-synchronized health snapshots, 3/3 automatic recovery events,
  verified software-reset escalation after three recovery failures, and a
  register-level WWDG with stop-refresh and early-refresh self-tests.
- Week 3 ISO-TP: portable SF/FF/CF/FC state machines, BS/STmin flow control,
  transmit-completion-aware timeouts, UDS request/response routing, and
  100-byte bench modes. The software core, host tests, final bidirectional
  hardware revalidation, negative fault tests and formal UDS regression pass.
  The Week 3 milestone is complete.
- CI matrix: Python 3.10, 3.11, and 3.12 pass in Actions run `40086628980`.
- Acceptance metrics below are targets until measured on real hardware.

## 核心能力

- 双节点 CAN 仲裁与时序同步验证
- UDS 诊断客户端、NRC 处理与编码，支持 0x10/0x22/0x27/0x2E/0x31/0x3E
- ISO 15765-2 多帧分包/组包与流控
- 虚拟 ECU 与内存总线仿真，无需硬件即可验证协议栈
- CAN 错误状态、Bus-Off 恢复策略与 IWDG/WWDG 监督
- DBC 文件生成与 C/Python 信号结构体自动生成
- 总线负载模拟、压力测试、故障注入与报告工具
- 双节点请求调度与编程会话互斥
- 诊断仪 Alpha：P2/P2* 时序、`0x78` 响应挂起等待、超时重试、时间戳与
  JSON Lines 输出、udsoncan 互操作适配
- 原始 CAN 帧抓取与同 ID 帧间隔统计（`trace`），用于测量 Bus-Off 离线窗口

## 架构

```text
PC Diagnostic Tool
  diagnoser.cli           CLI commands
  diagnoser.uds           UDS services and NRC handling
  diagnoser.transport     python-can + ISO-TP transport
  diagnoser.tools         scheduler, load, stress, fault injection
  diagnoser.dbc           DBC and signal struct generation
  diagnoser.emu           MemoryBus and VirtualEcu for simulation

STM32 ECU (CubeIDE + HAL)
  App layer -> UDS service layer -> ISO-TP layer -> CAN driver -> HAL
```

物理寻址：

| Node | Request ID | Response ID |
| --- | --- | --- |
| ecu1 (F407) | 0x7E0 | 0x7E8 |
| ecu2 (F103) | 0x7E1 | 0x7E9 |

## 快速开始

Windows 下使用 Python 3.14：

```powershell
py -3.14 -m pip install -e ".[dev]" -i https://pypi.tuna.tsinghua.edu.cn/simple
py -3.14 -m diagnoser.cli demo
py -3.14 -m diagnoser.cli session --node ecu1 0x03
py -3.14 -m diagnoser.cli read-did --node ecu1 0xF190
py -3.14 -m diagnoser.cli security --node ecu1
py -3.14 -m diagnoser.cli write-did --node ecu1 --session 0x03 --unlock 0xF191 01020304
py -3.14 -m diagnoser.cli routine --node ecu1 --session 0x03 --unlock 0x0203
py -3.14 -m diagnoser.cli stress --frames 1000 --load 0.9
py -3.14 -m diagnoser.cli bus-off-test --cycles 3 --seconds 0.2
py -3.14 -m diagnoser.cli uds-smoke --node ecu1
py -3.14 -m diagnoser.cli uds-week5 --node ecu1
py -3.14 -m diagnoser.cli --json read-did --node ecu1 0xF190
py -3.14 -m diagnoser.cli --retries 2 --p2-star 5000 read-did --node ecu1 0xF190
```

真实 CANable/slcan 适配器：

```powershell
py -3.14 -m diagnoser.cli --config configs/canable.example.json --channel COMx read-did --node ecu1 0xF190
```

candleLight（`gs_usb`）固件的 CANable：

```powershell
py -3.14 -m diagnoser.cli --config configs/canable.gsusb.example.json read-did --node ecu1 0xF190
```

抓原始 CAN 帧并统计同 ID 帧间隔（量 Bus-Off 离线窗口用）：

```powershell
py -3.14 -m diagnoser.cli --config configs/canable.example.json --channel COMx trace --seconds 10 --id 0x123
py -3.14 -m diagnoser.cli --config configs/canable.gsusb.example.json trace --seconds 10 --id 0x123 --report outputs/trace.md
```

诊断仪 Alpha 说明、命令列表与输出格式见
[docs/diagnoser_v1_alpha.md](docs/diagnoser_v1_alpha.md)。

第 3 周 ISO-TP 设计、测试和当前硬件结论见
[docs/isotp_week3.md](docs/isotp_week3.md)。

协议参数与验收契约见
[docs/isotp_protocol_contract.md](docs/isotp_protocol_contract.md)。
代表性 ISO-TP 帧样例见
[docs/isotp_frame_examples.md](docs/isotp_frame_examples.md)。
原始计划与证据对照见
[docs/isotp_acceptance_matrix.md](docs/isotp_acceptance_matrix.md)。

通用开发环境：

```bash
pip install -e ".[dev]"
pytest
python -m diagnoser.cli uds-week5 --node ecu1
```

## 目录结构

| 路径 | 说明 |
| --- | --- |
| `diagnoser/` | 诊断工具链源码 |
| `tests/` | pytest 单元测试 |
| `configs/` | 网络与运行配置 |
| `docs/` | 架构、接线、测试报告、Roadmap |
| `firmware/` | STM32F407/F103 HAL 工程与构建脚本 |
| `开发日志/` | 每周开发日志 |

## Roadmap

10 周修订计划见 [docs/ROADMAP.md](docs/ROADMAP.md)，验收指标未实测前均为 Target。

## 开发日志

每周日晚自动整理当周开发任务、技术知识点、代码变更和下周计划，生成
`开发日志/YYYY-Www.md` 并提交到本仓库。

## 许可证

MIT License，见 [LICENSE](LICENSE)。

