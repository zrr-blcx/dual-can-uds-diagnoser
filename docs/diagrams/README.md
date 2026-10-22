# CAN Wiring Diagrams

本目录保存 Graphviz 源文件和导出的接线图。

> 注意：本项目使用的 SN65HVD230 模块**自带 120Ω 终接**。图中画成独立 120Ω
> 电阻的位置，实际由模块板载电阻提供：两个模块分别挂在总线两端，就得到
> 约 60Ω，**不要再并外接电阻**。CANable 挂在总线中间，它自己的 120Ω
> 开关/跳线必须保持关闭（否则变成三个终接，读数约 40Ω）。
>
> 断电实测对照：60Ω = 正确；30Ω = 多并了外接电阻；40Ω = CANable 终接没断开；
> 120Ω = 只有一个模块在位；∞ = 两个模块的终接都没接通。

## 1. F103 单节点

- 源文件：`f103_sn65hvd230_wiring.dot`
- 矢量图：`f103_sn65hvd230_wiring.svg`
- 位图：`f103_sn65hvd230_wiring.png`

内容：江科大 STM32F103 最小系统板的 4 针 SWD 下载口
（3.3V、GND、PA13/SWDIO、PA14/SWCLK）与 CAN1 信号的区别。
SWD 四针只接 ST-Link；SN65HVD230 必须接 PA11/PA12，或使用
CAN1 remap 后的 PB8/PB9。图中同时标出 RS 高速模式、单节点 120Ω
终接、第二个活动节点和 ACK 注意事项。

## 2. F407 单节点

- 源文件：`f407_sn65hvd230_wiring.dot`
- 矢量图：`f407_sn65hvd230_wiring.svg`
- 位图：`f407_sn65hvd230_wiring.png`

内容：STM32F407ZGT6 的 CAN1、SN65HVD230、3.3V、共地、RS 高速模式、
单节点 120Ω 终接、板载收发器冲突和单节点 ACK 注意事项。

## 3. F407 + F103 双节点

- 源文件：`f407_f103_can_bus.dot`
- 矢量图：`f407_f103_can_bus.svg`
- 位图：`f407_f103_can_bus.png`

内容：F407 与 F103 两套 MCU + SN65HVD230、CANH/CANL 双绞线、公共 GND、
两端各一个 120Ω、500 kbps 参数、CANable 抓包节点和双向 LED 验证。

## 4. 面包板双节点布局

- 源文件：`breadboard_f407_f103_can_wiring.dot`
- 矢量图：`breadboard_f407_f103_can_wiring.svg`
- 位图：`breadboard_f407_f103_can_wiring.png`

内容：面包板逻辑侧分区、独立 3.3V 节点、公共 GND 桥接、
CANH/CANL 双绞线、两端 120Ω 终接，以及单节点和双节点测试区别。

## 5. 第 2 周 Bus-Off 验证接线

- 源文件：`week2_bus_off_injection_wiring.dot`
- 矢量图：`week2_bus_off_injection_wiring.svg`
- 位图：`week2_bus_off_injection_wiring.png`

内容：第 2 周错误统计与 Bus-Off 恢复的完整测试拓扑。F407 与 F103 各接一个
SN65HVD230，CANable 作为第二个 ACK 节点与抓包点；终接由两个模块自带的 120Ω
提供（断电测 CANH-CANL 约 60Ω，不需要外接电阻，CANable 的 120Ω 跳线必须
断开）；故障继电器 CH1 常闭触点可模拟通信丢失，但实测只到 error-passive，
不能作为本硬件的 Bus-Off 判据；CH2 跨接 CANH-CANL 是已上板验证的
Bus-Off 注入路径；ST-Link 读健康快照与
`RCC_CSR` 复位标志，PC 侧用 `trace` 统计 `0x123` 的同 ID 最大间隔作为
离线/恢复窗口；全部模块必须共地。

注：**继电器接线属于 Phase 4**（物理故障注入）；Phase 2/3 不需要它。
Phase 3 的 listen-only 实验只证明错误中断通路，最终 Bus-Off 验收改由 CH2
物理短接完成（见第 6 节）。

## 6. 第 2 周 Phase 3：ST-Link 烧写与 Bus-Off 隔离注入

- 源文件：`week2_stlink_debug_isolate_wiring.dot`
- 矢量图：`week2_stlink_debug_isolate_wiring.svg`
- 位图：`week2_stlink_debug_isolate_wiring.png`

内容：Phase 3 的接线与调试顺序，同时保留 listen-only 注入的失败复盘。ST-Link
接 F407 的 SWD（3.3V / GND / PA13 / PA14）烧写并读取快照；双探针同时连接时用
序列号选择目标。
CANable 挂在总线中间（板载 120Ω 开关必须关闭）：正常模式用于抓 `0x123` 门禁，
listen-only 模式曾用于单方面停止回 ACK。该实验只产生 ACK 错误，TEC 冻结在
128，没有进入 Bus-Off；最终验收改用 CH2 短接 CANH-CANL。健康快照用
`STM32_Programmer_CLI -u <地址> 4 snapshot.bin` 读取。图中另行提示：IWDG 已开启
（约 1 s），不要用断点长时间暂停。

## 重新生成

```powershell
dot -Tsvg docs\diagrams\f407_sn65hvd230_wiring.dot -o docs\diagrams\f407_sn65hvd230_wiring.svg
dot -Tpng -Gdpi=160 docs\diagrams\f407_sn65hvd230_wiring.dot -o docs\diagrams\f407_sn65hvd230_wiring.png

dot -Tsvg docs\diagrams\f103_sn65hvd230_wiring.dot -o docs\diagrams\f103_sn65hvd230_wiring.svg
dot -Tpng -Gdpi=160 docs\diagrams\f103_sn65hvd230_wiring.dot -o docs\diagrams\f103_sn65hvd230_wiring.png

dot -Tsvg docs\diagrams\f407_f103_can_bus.dot -o docs\diagrams\f407_f103_can_bus.svg
dot -Tpng -Gdpi=160 docs\diagrams\f407_f103_can_bus.dot -o docs\diagrams\f407_f103_can_bus.png

dot -Tsvg docs\diagrams\breadboard_f407_f103_can_wiring.dot -o docs\diagrams\breadboard_f407_f103_can_wiring.svg
dot -Tpng -Gdpi=160 docs\diagrams\breadboard_f407_f103_can_wiring.dot -o docs\diagrams\breadboard_f407_f103_can_wiring.png

dot -Tsvg docs\diagrams\week2_bus_off_injection_wiring.dot -o docs\diagrams\week2_bus_off_injection_wiring.svg
dot -Tpng -Gdpi=160 docs\diagrams\week2_bus_off_injection_wiring.dot -o docs\diagrams\week2_bus_off_injection_wiring.png

dot -Tsvg docs\diagrams\week2_stlink_debug_isolate_wiring.dot -o docs\diagrams\week2_stlink_debug_isolate_wiring.svg
dot -Tpng -Gdpi=160 docs\diagrams\week2_stlink_debug_isolate_wiring.dot -o docs\diagrams\week2_stlink_debug_isolate_wiring.png
```
