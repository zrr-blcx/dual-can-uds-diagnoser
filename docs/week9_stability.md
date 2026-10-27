# 第 9 周稳定性执行器

## 状态

4 小时稳定性执行器、板内负载源、双向 Peer 互测和实机验收已完成。
最终运行连续 `4.046 h`，两端没有意外复位，Roadmap 第 9 周已完成。

## 目的

`stability` 命令在固定时长内持续向两个节点发送只读 UDS 请求，按分钟累计：

- 每节点请求数、成功数、UDS 超时数和异常数；
- 每节点平均与最大响应时间；
- 主机背景帧发生器记录的实际负载；
- 中途中断时的部分结果。

工具不会把 UDS 超时解释为物理帧丢帧，也不会根据持续运行结果推断看门狗或
控制器没有复位。发布报告必须另附健康快照、`RCC_CSR` 和总线负载证据。

## 运行方式

主机生成 90% 背景负载：

```powershell
.venv\Scripts\python.exe -m diagnoser.cli stability `
  --hours 4 `
  --interval-ms 100 `
  --sample-seconds 60 `
  --load 0.9 `
  --report outputs/stability-4h.md
```

使用板内 `-PeerStability` 测试镜像时，关闭主机发生器。两台 MCU 每 100 ms
互为诊断端读取 `0xF19A`，因此单只 slcan 不需要承受 90% 接收流：

```powershell
powershell -ExecutionPolicy Bypass -File firmware\ecu-f407\build.ps1 `
  -LoadGenerator -LoadPeriodUs 250 -PeerStability
powershell -ExecutionPolicy Bypass -File firmware\ecu-f103\build.ps1 `
  -PeerStability
```

F407 生成低优先级背景帧，F103 统计序号缺口；两端同时继续处理对方请求。
CAN RX 使用 FIFO0/FIFO1 硬件分流，诊断和控制在软件队列中也先于背景帧。
负载源只在三个 TX 邮箱全部空闲时发送背景帧，避免抢走 UDS 响应邮箱。

短时虚拟冒烟：

```powershell
.venv\Scripts\python.exe -m diagnoser.cli stability `
  --hours 0.001 `
  --interval-ms 10 `
  --sample-seconds 1 `
  --load 0
```

## 4 小时验收证据

| 证据 | 来源 |
| --- | --- |
| 请求数、成功数、UDS 超时、RTT | 两端 `g_peer_tester` |
| 实际总线负载 | 板内探针、CAN 分析仪或独立测量记录 |
| TEC/REC/BOFF、恢复次数 | 两节点健康快照 |
| IWDG/WWDG/软件复位 | `RCC_CSR` 与复位前后基线 |
| 连续运行时长 | 两端 `uwTick`，至少 4 小时且无重启 |

第 9 周只有在双节点并发诊断连续运行 4 小时、无不可恢复 Bus-Off、无意外
看门狗复位且证据归档后才能更新为完成。

## 最终结果

| Metric | F407 | F103 |
| --- | ---: | ---: |
| Continuous uptime | 4.046 h | 4.046 h |
| Requests | 145673 | 145650 |
| Successful responses | 145596 | 145650 |
| Timeouts | 77 | 0 |
| Final state | error-active | error-active |

F407 generated 57,971,063 accepted background frames. F103 observed
57,925,485, an actual transmitted-frame loss of 0.0786%. Three independent
1-second CH2 fault injections produced F407 Bus-Off 3, recovery success 3,
final `TEC=0`, no reset. Formal images were restored with 20/20 smoke on each
node.
