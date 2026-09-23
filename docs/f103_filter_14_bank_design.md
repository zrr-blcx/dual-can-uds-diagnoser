# F103 bxCAN 过滤器 14 组上限设计说明

> 项目：双节点 CAN 网络与 UDS 诊断系统 · 第 1 周交付物
> 适用目标：江协科技 STM32F103C8T6（`STM32F103xB` / medium-density）
> 关联代码：`firmware/common/can_driver.c`、`firmware/ecu-f103/Core/Src/main.c`
> 更新日期：2026-09-23

---

## 1. 目的

说明 F103 节点上 CAN 过滤器（Filter）的硬件资源上限（**14 组**）、当前第 1 周的配置策略、以及后续 UDS 阶段（第 4 周起）的过滤器迁移规划，保证：

1. 第 1 周双节点 `0x123` 单帧互发验收期间**不做过滤**（接收全部标准帧），避免遗漏帧导致计数对不上；
2. 第 4 周 UDS 阶段能按计划切到**精确匹配**（F103 组 0 = `0x7E1`、组 1 = `0x7DF`）；
3. `can_driver.c` 公共驱动在两个芯片（F103/F407）上行为一致，不因过滤器资源差异产生平台分支。

---

## 2. F103 bxCAN 过滤器硬件结构（14 组上限的由来）

STM32F103C8T6 的 bxCAN 外设（单 CAN：CAN1）内含 **28 个过滤器，每个 32 位宽**，以"过滤器组（Filter Bank）"为单位使用：

- **F103（medium-density，C8T6）**：只有 **1 个 CAN（CAN1）**，过滤器可全部配置给 CAN1，共 **14 组**（Bank 0 ~ Bank 13，每组 2 个 32 位寄存器）。
- **F407（high-density，ZGT6）**：有 2 个 CAN（CAN1 + CAN2），共 **28 组**（Bank 0 ~ Bank 27）；由 `CAN_FMR.CAN2SB` 划界，**CAN1 占用 Bank 0~13，CAN2 从 Bank 14 开始**。

> 结论：**F103 的过滤器上限是 14 组**，即 `SlaveStartFilterBank = 14` 在 F103 上恰好等于"可用的全部组数"；F407 上则是"CAN1 与 CAN2 的分界点"。两者在数字上巧合一致，但语义不同，见第 5 节。

每组过滤器支持两种模式：

| 模式 | 用途 | 可配置规则数/组 |
| --- | --- | --- |
| **掩码模式（IDMASK）** | 一段 ID 范围或"全收/全拒" | 1 组 ID + 1 组掩码 |
| **列表模式（IDLIST）** | 精确匹配若干固定 ID | 32 位宽 1 个 / 16 位宽 2 个 |

---

## 3. F407 与 F103 过滤器资源对比

| 项 | F407ZGT6（节点 A） | F103C8T6（节点 B） |
| --- | --- | --- |
| CAN 控制器 | CAN1 + CAN2 | 仅 CAN1 |
| 过滤器总组数 | 28（Bank 0~27） | 14（Bank 0~13） |
| CAN1 可用组 | Bank 0~13 | Bank 0~13（全部） |
| 第 1 周使用 | Bank 0，掩码全收 | Bank 0，掩码全收 |
| 第 4 周规划 | 组 0=`0x7E0`、组 1=`0x7DF` | 组 0=`0x7E1`、组 1=`0x7DF` |
| 剩余可用组 | Bank 2~13（12 组） | Bank 2~13（12 组） |

两芯片第 1 周都只消耗 1 组（Bank 0），剩余 Bank 1~13 全部保留给后续 TP/UDS/诊断功能，规划一致。

---

## 4. 第 1 周现状：掩码模式全收（Bank 0）

### 4.1 为什么第 1 周不过滤

第 1 周验收口径是"双节点 `0x123` 单字节帧双向互发、ACK 位显性、收发计数增长且两侧对应"。

- 总线上只有 `0x123` 一种 ID，**没有过滤需求**；
- 过滤配置一旦有误（ID/掩码算错、RTR/IDE 位没对齐），表现是"收不到帧"而非"收到错误帧"，会与物理层故障（接触不良、终端电阻）混淆，增加排障难度；
- 用掩码全收可先锁定"物理链路 + 驱动 + 中断"正确，再在第 4 周引入过滤并单独验证。

### 4.2 当前配置（can_driver.c）

`CanDriver_ConfigureFilter()` 统一配置为 **32 位掩码模式、全收**：

```c
filter.FilterActivation    = ENABLE;
filter.FilterBank          = filter_bank;      /* F103/F407 第 1 周均传 0 */
filter.FilterMode          = CAN_FILTERMODE_IDMASK;
filter.FilterScale         = CAN_FILTERSCALE_32BIT;
filter.FilterFIFOAssignment = CAN_RX_FIFO0;
filter.FilterIdHigh        = 0x0000U;
filter.FilterIdLow         = 0x0000U;
filter.FilterMaskIdHigh    = 0x0000U;
filter.FilterMaskIdLow     = 0x0000U;          /* 掩码全 0 = 任何位都不要求匹配 */
filter.SlaveStartFilterBank = 14U;
```

掩码全 0 的含义：掩码位为 0 表示"该位不参与匹配"，因此 **所有标准帧（含远程帧）都通过**，等价于不过滤。

---

## 5. `SlaveStartFilterBank = 14` 的跨芯片语义

该字段在 `can_driver.c` 中固定为 `14U`，在两芯片上的含义不同，但**写法统一、行为兼容**：

| 芯片 | 实际效果 | 说明 |
| --- | --- | --- |
| F407ZGT6 | CAN1 用 Bank 0~13，CAN2 从 Bank 14 开始 | `CAN_FMR.CAN2SB = 14`，双 CAN 分界 |
| F103C8T6 | 无 CAN2，该字段写入保留位，**无实际效果** | 单 CAN 器件 CAN2SB 位域不存在/保留，写 0xE 无副作用 |

设计决定：**保持公共驱动单一份代码、不按芯片宏分支**。`14U` 对 F103 是"无害的兼容占位"，对 F407 是"正确的分界值"。若未来 F103 侧想更严谨，可在 F1 HAL 的头文件宏下将该字段置 0，但当前没有功能影响，不做分支以保持驱动层整洁。

> 注：HAL 层对 F1 单 CAN 器件写 `CAN2SB` 的行为由 `stm32f1xx_hal_can.c` 的标准实现保证（写入 `CAN_FMR` 保留位），不属于未定义行为；已在 F103 实机运行验证（ESR=0、收发计数正常）。

---

## 6. 后续 UDS 阶段过滤器规划（第 4 周起）

按 10 周计划，UDS 阶段引入物理寻址与功能寻址，过滤器改为 **32 位列表模式精确匹配**：

### 6.1 目标分配

| 过滤器组 | F407（节点 A） | F103（节点 B） | 匹配对象 |
| --- | --- | --- | --- |
| Bank 0 | `0x7E0`（物理请求→A） | `0x7E1`（物理请求→B） | 本节点 UDS 物理寻址请求 |
| Bank 1 | `0x7DF`（功能寻址） | `0x7DF`（功能寻址） | 广播诊断请求 |
| Bank 2~13 | 保留 | 保留 | 后续 TP 流控帧、自定义诊断帧 |

### 6.2 32 位列表模式寄存器布局（标准帧）

32 位过滤器寄存器（FilterIdHigh/FilterIdLow 拼接）中，标准帧 ID 位于：

```
Bit 31..21 : STID[10:0]
Bit 20     : RTR（数据帧=0）
Bit 19     : IDE（标准帧=0）
Bit 18..3  : EXID / 保留（标准帧填 0）
```

因此精确匹配标准帧 ID 的寄存器值为 **`ID << 5`**（RTR=0、IDE=0、EXID=0）：

| 目标 ID | FilterIdHigh = ID<<5 | FilterIdLow |
| --- | --- | --- |
| `0x7E0` | `0xFC00` | `0x0000` |
| `0x7E1` | `0xFC20` | `0x0000` |
| `0x7DF` | `0xFBE0` | `0x0000` |

### 6.3 迁移时的 HAL 配置示意（F103 组 0 为例）

```c
CAN_FilterTypeDef f = {0};
f.FilterActivation     = ENABLE;
f.FilterBank           = 0U;                     /* 组 0 */
f.FilterMode           = CAN_FILTERMODE_IDLIST;  /* 列表模式 */
f.FilterScale          = CAN_FILTERSCALE_32BIT;
f.FilterFIFOAssignment = CAN_RX_FIFO0;
f.FilterIdHigh         = (0x7E1U << 5);          /* 0xFC20 */
f.FilterIdLow          = 0x0000U;
f.SlaveStartFilterBank = 14U;
HAL_CAN_ConfigFilter(&hcan1, &f);
```

### 6.4 迁移验收判据

- 物理寻址：PC 发 `0x7E1` 帧，F103 收到并应答；发 `0x7E0` 帧，F103 **不接收**（该帧应只被 F407 接收）；
- 功能寻址：发 `0x7DF`，两块板都接收；
- 无关 ID（如 `0x123` 继续保留时）不再进入 F103 的 RX FIFO0，`HAL_CAN_RxFifo0MsgPendingCallback` 只对匹配 ID 触发；
- 两侧 ESR 保持 0，无错误帧增长。

---

## 7. 当前代码与文档的对应关系

| 文件 | 角色 |
| --- | --- |
| `firmware/common/can_driver.c` | 过滤器统一配置入口（当前掩码全收） |
| `firmware/ecu-f103/Core/Src/main.c` | `CanDriver_Init(..., 0U, APP_OnCanRx)` 传 filter_bank=0 |
| `firmware/ecu-f103/ecu-f103.ioc` | CubeMX 工程配置（CAN1=Normal、PA11/PA12、RX0 中断） |
| `firmware/ecu-f407/ecu-f407.ioc` | CubeMX 工程配置（CAN1=Normal、PA11/PA12、RX0 中断） |
| 本文档 | 14 组上限资源说明与迁移规划 |

---

## 8. 验收状态

- [x] 第 1 周掩码全收配置已实机验证：F407/F103 双向 `0x123` 收发计数对应增长、两侧 ESR=0
- [ ] 第 4 周精确匹配迁移（待 UDS 阶段执行）
- [ ] 迁移后无关 ID 过滤验证（待第 4 周执行）
