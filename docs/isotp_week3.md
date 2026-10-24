# 第 3 周 ISO-TP 实现与验证

## 目标

在 F407/F103 固件中实现可移植 ISO 15765-2 TP 层，满足：

- SF/FF/CF/FC 编解码和独立收发状态机。
- 12 位多帧总长度、SN `0x0` 到 `0xF` 回绕。
- `BS=8`、`STmin=20 ms` 流控。
- `N_As`、`N_Bs`、`N_Cr`、`N_Ar` 超时。
- F407/F103 双向 100 字节传输和 CANable 抓包验收。

## 实现结构

- `firmware/common/isotp.c/.h`
  - HAL 无关，可独立进行主机编译测试。
  - `IsotpConfig_Init()` 统一项目默认参数。
  - `Isotp_RouteFrame()` 按 PCI 类型把 FC 路由到发送端，把 SF/FF/CF
    路由到接收端。
  - CAN 通道提供发送完成回调，用于真实 `N_As`/`N_Ar` 判定。
- `firmware/common/isotp_bench.c/.h`
  - 生成固定 100 字节图案。
  - 接收端逐字节校验并返回一个 ISO-TP SF 状态帧。
- `firmware/ecu-f407/Core/Src/main.c`、`firmware/ecu-f103/Core/Src/main.c`
  - 普通模式：TP 请求 -> UDS 服务 -> TP 响应。
  - 基准模式：编译期选择 100 字节发送端或接收端。

## 地址与协议参数

| Node | Request | Response | FC when receiving request |
| --- | --- | --- | --- |
| F407 | `0x7E0` | `0x7E8` | `0x7E8` |
| F103 | `0x7E1` | `0x7E9` | `0x7E9` |

| Parameter | Value |
| --- | --- |
| CAN bitrate | 500 kbps |
| Padding | `0xCC` |
| Block size advertised by ECU | 8 |
| STmin advertised by ECU | 20 ms |
| Payload buffer | 300 bytes per endpoint |
| Timeout defaults | `N_As/N_Bs/N_Cr/N_Ar = 1000 ms` |

## 状态机要点

- 发送端在 FF/CF 入队后等待 CAN 邮箱完成；超过 `N_As` 中止传输。
- FF 后等待第一帧 FC 使用 `N_Bs`。
- 每个 BS 块发送完成后等待下一帧 FC 使用 `N_Cr`。
- 接收端等待下一帧 CF 使用 `N_Cr`。
- FC 自身发送完成使用 `N_Ar`。
- `FC.WAIT` 刷新对应等待窗口。
- `FC.OVERFLOW` 或保留状态中止发送。
- 流控帧必须从响应 ID 发出，不能复用请求 ID。
- 块已满但主循环尚未执行下一次 Poll 时，后续 FC 仍必须被接收端状态机接受，
  避免 FC 早到被丢弃。
- 跨 BS 块的 STmin 从上一帧实际 CF 起算，不能因收到下一块 FC 而重置。

## 验证

主机测试：

```powershell
py -3.14 -m pytest tests\test_firmware_isotp.py tests\test_isotp_flowcontrol.py -q
py -3.14 -m pytest -p no:cacheprovider
```

结果：`86 passed`。

固件构建：

```powershell
powershell -ExecutionPolicy Bypass -File firmware\ecu-f407\build.ps1
powershell -ExecutionPolicy Bypass -File firmware\ecu-f103\build.ps1
```

基准镜像：

```powershell
powershell -ExecutionPolicy Bypass -File firmware\ecu-f407\build.ps1 -IsotpBenchTx
powershell -ExecutionPolicy Bypass -File firmware\ecu-f103\build.ps1 -IsotpBenchRx
powershell -ExecutionPolicy Bypass -File firmware\ecu-f103\build.ps1 -IsotpBenchTx
powershell -ExecutionPolicy Bypass -File firmware\ecu-f407\build.ps1 -IsotpBenchRx
```

### 硬件结果

- CANable -> F103：100 字节载荷完整。
- 100 字节对应 `FF + 14 CF`。
- `BS=8` 产生 2 个 FC。
- 两帧 FC 均为 `BS=8`、`STmin=20 ms`。
- 实测最小 CF 间隔 `20.247 ms`。
- F103 返回状态 SF `0x5A`，RAM 计数在三次运行后为
  `rx_messages=3`、`rx_errors=0`。
- F103 正式镜像 `10 03` 响应时间 `2.1 ms`。
- F103 正式镜像读取 `0xF190` 的 20 字节多帧响应成功，耗时 `28.1 ms`。
- F407 -> F103：100 字节一致，14 CF、2 FC、BS=8、STmin=20 ms，
  最小 CF 间隔 `21.811 ms`；F103 接收计数 `19`、错误 `0`。
- F103 -> F407：100 字节一致，14 CF、2 FC、BS=8、STmin=20 ms，
  最小 CF 间隔 `21.845 ms`；F407 接收计数 `28`、错误 `0`。
- 双向接收端均连续返回状态 SF `0x5A`。
- 两节点正式镜像恢复后：
  - F407 `10 03` 为 `2.9 ms`，`22 F190` 为 `27.3 ms`。
  - F103 `10 03` 为 `8.8 ms`，`22 F190` 为 `26.1 ms`。

第 3 周 ISO-TP 里程碑已完成。
