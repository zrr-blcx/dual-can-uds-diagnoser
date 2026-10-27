# 第 10 周终测流程

## 状态

双节点 5000 帧测试模式、真实 10,000 帧结果和 90% 负载 2000 帧补充结果
均已完成。第 10 周与 `v1.0.0` 发布门禁通过。

## 测试镜像

50% 总线负载、双节点各 5000 帧：

```powershell
powershell -ExecutionPolicy Bypass -File firmware\ecu-f407\build.ps1 `
  -LoadGenerator -LoadPeriodUs 888 `
  -PeerStability -FrameLossBench -FrameLossFrames 5000

powershell -ExecutionPolicy Bypass -File firmware\ecu-f103\build.ps1 `
  -LoadGenerator -LoadPeriodUs 888 `
  -PeerStability -FrameLossBench -FrameLossFrames 5000
```

90% 负载、双节点各 1000 帧，共 2000 帧：

```powershell
powershell -ExecutionPolicy Bypass -File firmware\ecu-f407\build.ps1 `
  -LoadGenerator -LoadPeriodUs 250 `
  -PeerStability -FrameLossBench -FrameLossFrames 1000

powershell -ExecutionPolicy Bypass -File firmware\ecu-f103\build.ps1 `
  -LoadGenerator -LoadPeriodUs 250 `
  -PeerStability -FrameLossBench -FrameLossFrames 1000
```

## 帧计数

- F407 发送 `0x6F0`，F103 发送 `0x6F1`。
- 每帧前 4 字节为大端序号，从 `0` 开始。
- 两端在收到第一次 Peer UDS 响应后同时开始发送序号帧。
- F103 的 `g_background_probe` 统计 F407 帧，F407 的同名探针统计 F103 帧。
- `g_load_generator.completed=1` 表示本节点达到目标发送帧数。

物理帧观测率：

```text
observed rate = 1 - gaps / intended frames
```

正常负载门禁为物理缺口率 `< 0.1%`。90% 负载结果只记录和评估稳定性，
不自动套用正常负载门槛。

## UDS 指标

两台节点在发送序号帧时继续执行 `0x22 F19A` 双向互测：

- `g_peer_tester.requests_sent`
- `g_peer_tester.responses_ok`
- `g_peer_tester.timeouts`
- `g_peer_tester.tx_failures`
- `g_peer_tester.invalid_responses`
- `g_peer_tester.min_rtt_ms` / `max_rtt_ms` / `rtt_total_ms`

UDS 超时率必须与物理帧缺口率分开报告。

## 终测证据

每次正式终测必须记录：

1. 两个 ELF/HEX 的 SHA-256；
2. 烧写前 `RCC_CSR` 与 `uwTick` 基线；
3. 测试开始和结束时间；
4. 两端 `g_load_generator`、`g_peer_tester`、`g_background_probe`；
5. 两端 56 字节健康快照与最终 `RCC_CSR`；
6. 实得总线负载、帧缺口率和 UDS 超时率；
7. 测试后恢复正式固件并重新完成双节点冒烟。

原始 ST-Link 内存文件和个人硬件标识只保存在 `本地资料/`，公开报告只保留
清洗后的计数、结果和结论。

## 最终实测

| Direction | Sent | Received | Physical loss | Result |
| --- | ---: | ---: | ---: | --- |
| F407 -> F103 | 5000 | 4998 | 0.04% | PASS |
| F103 -> F407 | 5000 | 5000 | 0% | PASS |

90% supplemental:

| Direction | Sent | Received | Physical loss |
| --- | ---: | ---: | ---: |
| F407 -> F103 | 1000 | 1000 | 0% |
| F103 -> F407 | 1000 | 999 | 0.1% |

At 50% load, UDS timeout rates were 7.07% on F407 and 0.80% on F103. At 90%
load they were 6.84% and 1.33%. These are diagnostic timeout rates, not
physical frame loss. Both nodes finished the normal-load run with `< 0.1%`
physical loss and the formal images passed final 20/20 smoke requests.
