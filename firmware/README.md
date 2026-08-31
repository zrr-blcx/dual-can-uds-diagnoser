# Firmware

Firmware is developed with STM32CubeIDE and STM32 HAL.

Planned project layout:

```text
firmware/
  ecu-f407/    STM32F407ZGT6 node
  ecu-f103/    STM32F103RCT6 node
  common/      CAN driver, ISO-TP, UDS service layer shared by both nodes
```

Target layer structure:

```text
Application
  -> UDS service layer (0x10/0x22/0x2E/0x27/0x31)
  -> ISO 15765-2 TP layer
  -> CAN driver layer
  -> STM32 HAL
```

Status: under development. See docs/ROADMAP.md for the schedule.
