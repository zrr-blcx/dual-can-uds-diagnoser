# 项目二

基于 Python 的双节点 CAN + UDS 诊断工具链，用于 STM32F407/F103 验证。

## 目录

- `diagnoser/`：双节点 CAN + UDS 诊断工具链源码
- `tests/`：工具链单元测试
- `configs/`：网络与运行配置
- `开发日志/`：每周项目开发进展与技术知识点整理，按 ISO 周命名

## 每周开发日志

每周末自动整理当周开发任务、技术知识点、代码变更和下周计划，生成 `开发日志/YYYY-Www.md`，并提交到本仓库。

## 常用命令

```bash
pytest
python -m diagnoser.cli --help
```
