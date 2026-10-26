# dual-can-uds-diagnoser

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Python](https://img.shields.io/badge/Python-3.10%2B-blue.svg)]()
[![CI](https://github.com/zrr-blcx/dual-can-uds-diagnoser/actions/workflows/ci.yml/badge.svg)](https://github.com/zrr-blcx/dual-can-uds-diagnoser/actions/workflows/ci.yml)

基于 Python 的双节点 CAN + UDS 诊断工具链，用于 STM32F407/F103 双节点 CAN
总线环境下的协议验证与故障分析。

项目边界、模块组成与已验证范围见
[docs/项目说明.md](docs/项目说明.md)。

## Status

- Project progress: Weeks 1-8 are complete; Weeks 9-10 remain on the roadmap.
- Week 1: F407/F103 CAN1 at 500 kbps, dual-node frame exchange and hardware
  baseline complete.
- Week 2 CAN health and Bus-Off recovery: SCE error handling, IWDG,
  hardware-synchronized health snapshots, physical Bus-Off recovery and WWDG
  validation complete.
- Week 3 ISO-TP: portable SF/FF/CF/FC state machines, BS/STmin flow control,
  transmit-completion-aware timeouts, UDS request/response routing, and
  100-byte bench modes. Host and bidirectional hardware acceptance complete.
- Week 4: `0x10/0x22/0x3E`, DID database, S3 timeout, multi-DID reads,
  dual-node routing and `udsoncan` CANable interoperability complete.
- Week 5: `0x27/0x2E/0x31`, F407/F103 Flash persistence, hardware self-test,
  parameter clear, pending responses and the dual-node NRC matrix complete.
- Week 6: diagnostic tool V1.0 Alpha with P2/P2* timing, `0x78` handling,
  retries, timestamped/JSON output and full F407/F103 CANable validation.
- Week 7 shared-bus scheduler smoke: 20 read-only requests per node through
  one CANable, with independent `0x7E8/0x7E9` responses and session restore.
- Week 7 programming-session exclusion: the waiting node is skipped without
  executing a conflicting `0x02` request, then recovers after lock release.
- Week 7 DBC hardware validation: `0x180` and `0x280` decoded on the real bus;
  formal firmware and F407 parameter storage restored intact.
- Week 8 low-load baseline: 2000/2000 requests, zero physical background gaps.
- Week 8 stress/fault injection: 94.4% load in both directions with degradation
  recorded separately; DTC, NRC, communication loss and Bus-Off coverage.
- CI: Python 3.10-3.14 plus F407/F103 ARM firmware builds.
- Week 9-10 acceptance metrics remain marked `Target` in the roadmap.

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

Windows 下推荐使用 Python 3.14；最低支持 Python 3.10：

```powershell
py -3.14 -m pip install -e ".[dev]" -i https://pypi.tuna.tsinghua.edu.cn/simple
py -3.14 -m diagnoser.cli demo
py -3.14 -m diagnoser.cli session --node ecu1 0x03
py -3.14 -m diagnoser.cli read-did --node ecu1 0xF190
py -3.14 -m diagnoser.cli security --node ecu1
py -3.14 -m diagnoser.cli write-did --node ecu1 --session 0x03 --unlock 0xF191 01020304
py -3.14 -m diagnoser.cli routine --node ecu1 --session 0x03 --unlock 0x0203
py -3.14 -m diagnoser.cli stress --requests-per-node 1000 --interval-ms 100 --load 0.1
py -3.14 -m diagnoser.cli bus-off-test --cycles 3 --seconds 0.2
py -3.14 -m diagnoser.cli uds-smoke --node ecu1
py -3.14 -m diagnoser.cli uds-week5 --node ecu1
py -3.14 -m diagnoser.cli dual-node-smoke --requests-per-node 20
py -3.14 -m diagnoser.cli session-conflict
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

第 5 周安全访问、持久化、自检与参数清除见
[docs/uds_week5.md](docs/uds_week5.md)。

第 8 周基线、90%+ 板内压力测试和故障注入结果见
[docs/week8_stress_fault.md](docs/week8_stress_fault.md)。

全量测试结果与实机证据见
[docs/test_report.md](docs/test_report.md)。

第 3 周 ISO-TP 设计、测试和当前硬件结论见
[docs/isotp_week3.md](docs/isotp_week3.md)。

第 4 周 UDS 会话、DID、S3、NRC、双节点与 udsoncan 实机结果见
[docs/uds_week4_acceptance.md](docs/uds_week4_acceptance.md)。

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

固件构建会自动从 `ARM_GCC_BIN`、`PATH` 或常见 STM32CubeIDE 安装目录查找
`arm-none-eabi-gcc`：

```powershell
$env:ARM_GCC_BIN = 'C:\path\to\arm-none-eabi\bin' # 仅在自动发现失败时需要
.\firmware\ecu-f407\build.ps1
.\firmware\ecu-f103\build.ps1
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

每周整理当周技术进展、代码变更、测试结果和下周计划，生成
`开发日志/YYYY-Www.md` 并提交到本仓库。

## 贡献与安全

开发流程和提交要求见 [CONTRIBUTING.md](CONTRIBUTING.md)。安全问题请按
[SECURITY.md](SECURITY.md) 私下报告，不要公开提交设备标识或敏感信息。

## 许可证

MIT License，见 [LICENSE](LICENSE)。

