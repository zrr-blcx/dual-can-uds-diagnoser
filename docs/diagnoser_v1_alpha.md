# Python 诊断仪 V1.0 Alpha（第 6 周）

## 状态

Alpha 版诊断仪已在主机侧完成验证：虚拟双节点、ISO-TP 传输、UDS 服务、
安全访问、时间戳输出、P2/P2* 等待与超时重试全部可用。

第 3 周 ISO-TP 已完成。2026-10-24 使用真实 CANable 在 F407/F103 上
分别通过完整 `uds-week5`，并完成双节点 NRC 矩阵。2026-10-25
完成真实自检、参数清除和 F103 持久化后，第 5、6 周里程碑均已收口。

## 依赖

- 运行时：`python-can` + 仓库内 ISO-TP 传输（`diagnoser/transport/isotp.py`）。
- `can-isotp` 不是必需依赖。
- `udsoncan` 作为可选互操作库使用，适配层见
  `diagnoser/transport/udsoncan_link.py`。
- `cantools` 用于 DBC 生成工具。
- 真实 CANable：slcan 固件需要 `pyserial`；candleLight（`gs_usb`）固件需要
  `pyusb` + `gs_usb`，Windows 上还需要 `libusb`（提供 `libusb-1.0.dll`）。
  打开 `gs_usb` 总线时 `diagnoser.transport` 会自动把该 DLL 交给 pyusb，
  不需要手动拷贝；上述依赖均已随 `requirements.txt` 与 `pyproject.toml` 声明。

安装：

```powershell
py -3.14 -m pip install -r requirements.txt -i https://pypi.tuna.tsinghua.edu.cn/simple
```

或使用项目开发环境：

```bash
pip install -e ".[dev]"
```

## 配置

| 配置文件 | 用途 |
| --- | --- |
| `configs/network.json` | 默认虚拟双节点配置（`memory` 接口） |
| `configs/canable.example.json` | CANable/slcan 示例；`channel` 是占位符 |
| `configs/canable.gsusb.example.json` | candleLight/`gs_usb` 示例；`channel` 是设备序号 |

物理寻址：

| 节点 | 请求 ID | 响应 ID |
| --- | --- | --- |
| ecu1 (F407) | `0x7E0` | `0x7E8` |
| ecu2 (F103) | `0x7E1` | `0x7E9` |

协议参数：

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `timeout` | 2.0 s | ISO-TP 单次接收超时 |
| `p2_ms` | 50 ms | P2server 标称值 |
| `p2_star_ms` | 5000 ms | P2*server；收到 `0x78` 后等待最终响应 |
| `retries` | 1 | 接收超时后的重发次数 |
| `padding` | `0xCC` | 单帧填充字节 |

CANable 使用前把示例里的 `channel` 改成本机串口，但不要把真实 COM 口编号
提交到仓库。

```powershell
python -m diagnoser.cli --config configs/canable.example.json --channel COMx read-did --node ecu1 0xF190
```

candleLight 固件按设备序号选通道，不需要串口名：

```powershell
python -m diagnoser.cli --config configs/canable.gsusb.example.json read-did --node ecu1 0xF190
```

## 命令

| 命令 | 说明 |
| --- | --- |
| `session <id>` | `0x10` 会话控制 |
| `read-did <did>` | `0x22` 读 DID |
| `write-did <did> <hex>` | `0x2E` 写 DID，可加 `--session/--unlock` |
| `routine <id>` | `0x31` 例行控制，可加 `--subfunction/--session/--unlock` |
| `raw <hex>` | 发送原始 UDS 请求 |
| `security` | 执行 `0x27` Seed/Key |
| `uds-smoke` | 第 4 周单节点冒烟流程 |
| `uds-week5` | `10 03 -> 27 01 -> 27 02 -> 2E -> 31` 全流程 |
| `dual-node-smoke` | 一条共享 CAN 通道并发读取两个节点，并校验响应 ID 与会话恢复 |
| `session-conflict` | 验证一个节点占用 `0x02` 时，另一节点编程任务被跳过并可恢复 |
| `dbc-verify` | 捕获并解码 `0x180/0x280` DBC 测试帧，校验固定信号值 |
| `demo` | 虚拟双节点演示 |
| `stress` | 双节点压力测试 |
| `fault` | DTC/通信丢失/Bus-Off/NRC 故障注入 |
| `bus-off-test` | Bus-Off 恢复测试 |
| `trace` | 抓原始 CAN 帧，统计同 ID 帧间隔（量离线窗口） |
| `dbc-generate` / `structs` | DBC 与信号结构体生成 |

示例：

```powershell
python -m diagnoser.cli session --node ecu1 0x03
python -m diagnoser.cli read-did --node ecu1 0xF190
python -m diagnoser.cli security --node ecu1
python -m diagnoser.cli write-did --node ecu1 --session 0x03 --unlock 0x0100 DEADBEEF
python -m diagnoser.cli routine --node ecu1 --session 0x03 --unlock 0x0203
python -m diagnoser.cli uds-week5 --node ecu1
```

## 抓包与帧间隔

`trace` 只读总线、不解 UDS，用来确认节点是否在发帧、以及总线中断了多久：

```powershell
python -m diagnoser.cli --config configs/canable.example.json --channel COMx trace --seconds 10 --id 0x123
```

- 帧间隔按**同一个 arbitration ID** 的相邻帧计算，其他总线流量不会把间隔算短。
- 输出每个 ID 的帧数与最小/平均/最大间隔，并标出最大间隔落在哪个 ID。
- `--quiet` 只打印汇总，`--report` 写 Markdown；`--json` 是全局选项，要写在
  子命令之前（`--json trace ...`），它按 JSON Lines 打印每帧。
- `--interface memory` 时按 `--id` 监听，只有发给该 ID 的帧可见。

## 时序与重试

- 正常响应在 P2 窗口内返回；P2server 默认 50 ms。
- 收到 `7F xx 78` 时，客户端不当作错误，而是继续等待最多 P2*（默认 5000 ms）
  直到 ECU 发送最终响应。`client.last_exchange.pending_responses` 记录挂起次数。
- 需要在测试里直接观察 `0x78` 时，可设置
  `UdsClient(handle_response_pending=False)`；此时 `0x78` 会作为
  `NegativeResponse` 抛出，并把完整交换信息挂在 `exc.exchange`。
- ISO-TP 接收超时后按 `retries` 重发；`--retries 0` 关闭重发。
- `--stmin-ms` 和 `--block-size` 控制客户端 FC 参数；压力测试可降至 1 ms。
- Bus-Off 与压力测试会临时把 `retries` 置 0，避免重发掩盖离线窗口或污染
  丢帧/超时统计。
- PC `stress` 使用每节点请求数和请求启动间隔；高负载板内测试使用独立
  `-StressMaster` 镜像，避免单只 slcan 适配器接收饱和。
- Extended/Programming 会话由 `0x3E` 或有效请求刷新 S3，超时自动回 Default。

## 输出格式

默认文本输出包含时间戳、CAN ID、请求/响应字节、耗时，以及负响应 NRC 名称：

```text
read DID 0xF190: [10:26:33.561] | TX 7E0: 22 F1 90 | RX 7E8: 62 F1 90 50 32 ... | 0.0 ms
negative response: [10:26:34.311] | TX 7E0: 22 FF FF | RX 7E8: 7F 22 31 | 0.0 ms | NRC 0x31 (requestOutOfRange)
```

`--json` 输出 JSON Lines，每条响应一行，便于脚本消费：

```json
{"tx_id": "0x7E1", "rx_id": "0x7E9", "request": "22 F1 93", "response": "62 F1 93 46 31 30 33 2D 48 57 2D 52 45 56 2D 41", "elapsed_ms": 0.0, "retries": 0, "pending_responses": 0, "negative": false, "service": null, "nrc": null, "nrc_name": null, "timestamp": "10:26:33.921", "title": "read DID 0xF193"}
```

## udsoncan 互操作

`UdsOnCanLink` 把现有 ISO-TP 传输适配成 udsoncan 连接接口，可以直接使用
`udsoncan.Client`：

```python
from udsoncan.client import Client
from diagnoser.transport.udsoncan_link import UdsOnCanLink

with Client(UdsOnCanLink(transport)) as uds:
    uds.change_session(0x03)
    response = uds.read_data_by_identifier(0xF190)
```

`tests/test_udsoncan_interop.py` 覆盖会话控制和 DID 读取，并验证负响应会以
`NegativeResponseException` 抛出。

## 已知限制

- 真实 CANable 已与 F407/F103 完成双节点联调；F407 满足
  `2E`/`31` pending，F103 重烧后同样通过。
- udsoncan 互操作当前覆盖会话与读 DID；安全访问和写 DID 仍走本项目客户端。
- F407 Sector 11 和 F103 最后 1 KB 页均已接入 Flash 参数日志。
  两节点实机写 DID、断电重启回读和 CANable P2* 时序均已通过。
- `raw` 命令不做 `0x78` 等待，直接返回第一帧响应。
