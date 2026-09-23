# CAN Wiring Diagrams

本目录保存 Graphviz 源文件和导出的接线图。

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
```
