# dual-can-uds-diagnoser

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Python](https://img.shields.io/badge/Python-3.10%2B-blue.svg)]()
[![CI](https://github.com/zrr-blcx/dual-can-uds-diagnoser/actions/workflows/ci.yml/badge.svg)](https://github.com/zrr-blcx/dual-can-uds-diagnoser/actions/workflows/ci.yml)

基于 Python 的双节点 CAN + UDS 诊断工具链，用于 STM32F407/F103 双节点 CAN 总线环境下的协议验证与故障分析。

## 核心能力

- 双节点 CAN 仲裁与时序同步验证
- UDS 诊断客户端、NRC 处理与编码
- ISO-TP 传输层封装
- 虚拟 ECU 与内存总线仿真
- DBC 信号生成与位操作
- 总线负载、压力测试、故障注入与报告工具

## 目录结构

| 路径 | 说明 |
| --- | --- |
| `diagnoser/` | 诊断工具链源码 |
| `tests/` | pytest 单元测试 |
| `configs/` | 网络与运行配置 |
| `开发日志/` | 每周开发日志 |
| `docs/` | 项目文档 |

## 快速开始

```bash
pip install -e ".[dev]"
pytest
python -m diagnoser.cli --help
```

## 开发日志

每周日晚自动整理当周开发任务、技术知识点、代码变更和下周计划，生成 `开发日志/YYYY-Www.md` 并提交到本仓库。

## 许可证

MIT License，见 [LICENSE](LICENSE)。
