# 双节点 CAN 网络与 UDS 诊断系统 | 10 周修订计划

> 本计划整合了硬件选型、STM32CubeIDE + HAL、Python 依赖、Bus-Off 恢复、
> 丢帧率口径、压力测试参数等全部修订点。

## 每周任务

### 第 1 周：硬件确认与 CAN 基础通信

- 确认 F407/F103 板载收发器型号；准备 2 个 SN65HVD230 模块和 1 个万用表。
- 若使用外接收发器，先禁用/绕过板载收发器，避免 RXD 和总线并联冲突。
- 使用 STM32CubeIDE + HAL 建立 F407、F103 工程。
- CubeMX 配置 CAN1：500Kbps、FIFO0 RX 中断、掩码模式接收全部标准帧。
- F103 过滤器按 14 组上限设计，F407 兼容使用前 14 组。
- 实现双节点单帧互发，F407 发 `0x123` 控制 F103 LED，反向验证。
- 万用表确认 CANH-CANL 终端电阻约 60Ω，可用逻辑分析仪抓波形。

交付物：可复用的 `CAN_Init / CAN_SendFrame / CAN_RxCallback` 驱动层。

### 第 2 周：错误处理与 Bus-Off 恢复

- 学习 TEC/REC 错误状态机：主动错误、被动错误、Bus-Off。
- 开启错误中断，统计位错误、填充错误、ACK 错误、CRC 错误。
- 实现 Bus-Off 检测：在 `HAL_CAN_ErrorCallback` 中检测 BOFF。
- Bus-Off 恢复：重新调用 `HAL_CAN_Start()` 恢复正常模式，并记录恢复次数；
  连续恢复失败 3 次触发系统复位。
- 注意：ISO 11898 要求的 128 个 11 位隐性位恢复时序由 CAN 控制器硬件完成，
  软件不需要手动生成这段波形。
- 加入 IWDG 和 WWDG，防止诊断任务卡死或超时。
- 用继电器断开/短接 CAN 线触发 Bus-Off，验证自动恢复。

交付物：带错误统计与 Bus-Off 恢复的 CAN 驱动层 v2。

### 第 3 周：ISO 15765-2 TP 层

- 手写 SF/FF/CF/FC 四种帧类型，发送/接收各一个状态机。
- 实现 SN 序列号 0x0-0xF 轮转、BS=8、STmin=20ms。
- 使用 `HAL_GetTick()` 差值实现 N_As/N_Bs/N_Cr/N_Ar 超时。
- F407 向 F103 发送 100 字节测试数据，TP 层完整接收并校验，反向测试。
- 使用 CANable + BUSMASTER 或 python-can 抓包核对帧格式。

交付物：C 语言 ISO-TP 协议栈与联调测试记录。

### 第 4 周：UDS 框架与 0x10/0x22

- 搭建 UDS 服务分发框架：请求解析、SID 分发、服务执行、响应组包。
- 实现 0x10 会话控制：Default/Extended/Programming，S3server=5s 超时回退。
- 实现 0x22 读 DID：0xF190 VIN、0xF193 硬件版本、0x0100 自定义数据。
- 统一负响应处理：`0x7F + SID + NRC`，先实现 0x31、0x13。
- PC 端安装 Python 依赖：
  `py -3.14 -m pip install python-can udsoncan cantools pytest -i https://pypi.tuna.tsinghua.edu.cn/simple`
- 用 python-can + udsoncan 自带 ISO-TP 传输连接 CANable，验证单节点再扩展双节点。

交付物：F407/F103 双节点基础 UDS 通信。

### 第 5 周：0x27 安全访问 + 0x2E + 0x31

- 实现 0x27 安全访问：请求 Seed、发送 Key、解锁状态机。
- 0x2E 写 DID 前检查扩展会话与安全状态，未解锁返回 NRC 0x33。
- 实现 0x31 例行控制：Start/Stop/RequestResults，例程 0xFF00 擦除、0x0203 自检。
- 覆盖主流 NRC：0x11、0x12、0x13、0x22、0x31、0x33、0x78、0x7E。
- 将完整服务移植到 F103，DID 数据库差异化。
- 双节点联调：`10 03 -> 27 01 -> 27 02 -> 2E -> 31` 全流程通过。

交付物：UDS 服务层 v1.0 与 NRC 测试用例表。

### 第 6 周：Python 诊断仪 V1.0

- 使用 `python-can + udsoncan`，can-isotp 不作为必需依赖。
- CLI 支持：`session`、`read-did`、`write-did`、`routine`、`raw`。
- 配置 CANable slcan 接口，波特率 500K，物理寻址 `0x7E0/0x7E8`、`0x7E1/0x7E9`。
- 设置 P2=50ms、P2*=5000ms，格式化输出响应、时间戳与 NRC。
- 实现超时处理与重试机制。
- 与真实 F407/F103 联调，修复时序问题。

交付物：Python 诊断仪 V1.0 Alpha。

### 第 7 周：DBC 生成 + 双节点调度

- 创建项目 DBC，定义双节点诊断报文与信号。
- 使用 cantools 解析 DBC，自动生成 C 结构体及 pack/unpack 函数，支持 Motorola/Intel。
- 实现双节点调度器：每节点独立队列，单线程顺序执行，超时重试与跳过。
- 会话管理：每个节点独立维护会话，编程会话设为互斥，避免会话冲突。
- 全链路联调：诊断仪 -> 调度器 -> 双节点，验证无冲突。

交付物：DBC 生成脚本、调度器与联调测试。

### 第 8 周：压力测试与故障注入

- 实现总线负载率计算：负载率 = 报文位时间 / 总时间。
- 实现 90%+ 背景帧发生器。
- 压力测试参数：先以 10-20ms 间隔发诊断请求，再与 100ms 基线对比；
  多帧 STmin 调低到 1-5ms，避免吞吐不足。
- 实现故障注入：通信丢失、DTC 状态注入、NRC 场景注入、继电器触发 Bus-Off。
- 低负载基线：双节点各 1000 帧，记录响应时间与丢帧率。

交付物：压力测试框架、故障注入脚本、基线测试报告。

### 第 9 周：Bus-Off 验证与稳定性

- 注入 Bus-Off，测量恢复时间，连续恢复 3 次验证稳定性。
- 90% 负载 + 双节点并发诊断，连续运行 4 小时。
- 修复 TP 层边界、UDS 会话竞争、Python 诊断仪问题。
- 代码重构：HAL 层 -> CAN 驱动层 -> TP 层 -> UDS 层 -> 应用层。

交付物：Bus-Off 恢复测试报告、4 小时稳定性报告、重构后的 v1.0 代码。

### 第 10 周：10,000 帧终测与发布

- 双节点各 5000 帧，共 10,000 帧；50% 负载基线，90% 负载再跑 2000 帧。
- 分别统计物理帧丢帧率与 UDS 诊断超时率，记录平均/最大响应时间。
- 编写 README（中英双语）、架构文档、硬件接线图、使用说明。
- 整理 GitHub 仓库，打 Tag `v1.0.0`，发布 Release。

交付物：终测报告、完整文档、GitHub Release。

## 最终验收标准

- 双节点 10,000 帧，正常负载物理帧丢帧率 < 0.1%。
- Bus-Off 恢复符合 ISO 11898，恢复时间可测，连续恢复稳定。
- Python 诊断仪支持 0x10/0x22/0x2E/0x31。
- DBC 可自动生成 C 信号结构体。
- 覆盖主流 NRC 场景，故障注入支持通信丢失、DTC、NRC、Bus-Off。
- 代码开源并发布 GitHub Release。

## 全局执行原则

- 每周以目标开始、以验证结束，未通过验证不进入下一阶段。
- 前 3 周是地基：TP 层未调通前不进入 UDS 阶段。
- 硬件未到货时，先用 python-can VirtualBus 和虚拟 ECU 开发 PC 端。
- 固件统一使用 STM32CubeIDE + HAL 库。
- Python 依赖使用清华镜像安装，避免官方源超时。
