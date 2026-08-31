# dual-can-uds-diagnoser

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Python](https://img.shields.io/badge/Python-3.10%2B-blue.svg)]()
[![CI](https://github.com/zrr-blcx/dual-can-uds-diagnoser/actions/workflows/ci.yml/badge.svg)](https://github.com/zrr-blcx/dual-can-uds-diagnoser/actions/workflows/ci.yml)

基于 Python 的双节点 CAN + UDS 诊断工具链，用于 STM32F407/F103 双节点 CAN
总线环境下的协议验证与故障分析。

## Status

- Python diagnostic tool V1.0 scaffold: ready for development and tests.
- STM32 firmware: planned with STM32CubeIDE + HAL.
- Acceptance metrics below are targets until measured on real hardware.

## 核心能力

- 双节点 CAN 仲裁与时序同步验证
- UDS 诊断客户端、NRC 处理与编码，支持 0x10/0x22/0x2E/0x31
- ISO 15765-2 多帧分包/组包与流控
- 虚拟 ECU 与内存总线仿真，无需硬件即可验证协议栈
- DBC 文件生成与 C/Python 信号结构体自动生成
- 总线负载模拟、压力测试、故障注入与报告工具
- 双节点请求调度与编程会话互斥

## 架构

```text
PC Diagnostic Tool
  diagnoser.cli           CLI commands
  diagnoser.uds           UDS services and NRC handling
  diagnoser.transport     python-can + ISO-TP transport
  diagnoser.tools         scheduler, load, stress, fault injection
  diagnoser.dbc           DBC and signal struct generation
  diagnoser.emu           MemoryBus and VirtualEcu for simulation

STM32 ECU (planned, CubeIDE + HAL)
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
py -3.14 -m diagnoser.cli write-did --node ecu1 0xF191 01020304
py -3.14 -m diagnoser.cli routine --node ecu1 0x0203
py -3.14 -m diagnoser.cli stress --frames 1000 --load 0.9
```

真实 CANable/slcan 适配器：

```powershell
py -3.14 -m diagnoser.cli --interface slcan --channel COMx read-did --node ecu1 0xF190
```

通用开发环境：

```bash
pip install -e ".[dev]"
pytest
python -m diagnoser.cli --help
```

## 目录结构

| 路径 | 说明 |
| --- | --- |
| `diagnoser/` | 诊断工具链源码 |
| `tests/` | pytest 单元测试 |
| `configs/` | 网络与运行配置 |
| `docs/` | 架构、接线、测试报告、Roadmap |
| `firmware/` | STM32 CubeIDE 工程（规划中） |
| `开发日志/` | 每周开发日志 |

## Roadmap

10 周修订计划见 [docs/ROADMAP.md](docs/ROADMAP.md)，验收指标未实测前均为 Target。

## 开发日志

每周日晚自动整理当周开发任务、技术知识点、代码变更和下周计划，生成
`开发日志/YYYY-Www.md` 并提交到本仓库。

## 许可证

MIT License，见 [LICENSE](LICENSE)。

