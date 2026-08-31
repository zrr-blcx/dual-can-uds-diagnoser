# Hardware Wiring

## Hardware List

| Item | Model / Spec | Qty |
| --- | --- | --- |
| STM32F407 board | Positive Atom Explorer F407ZGT6 | 1 |
| STM32F103 board | Positive Atom Mini / Wildfire RCT6 | 1 |
| CAN transceiver | SN65HVD230 module | 2 |
| USB-CAN | CANable 2.0 / Pro with slcan firmware | 1 |
| Fault relay | 2-channel 5V opto-isolated | 1 |
| USB-TTL | CH340 | 1 |
| Multimeter | Any | 1 |
| Termination | 120 ohm 0.25W | 2 |
| Power | 5V for relay and modules | 1 |

## MCU CAN Pins

| MCU | CAN peripheral | TX | RX |
| --- | --- | --- | --- |
| STM32F407ZGT6 | CAN1 | PA12 | PA11 |
| STM32F103RCT6 | CAN1 | PA12 | PA11 |

## SN65HVD230 Module

| Module pin | Connection |
| --- | --- |
| VCC | 3.3V |
| GND | GND (common ground with all nodes) |
| TXD | MCU CAN TX |
| RXD | MCU CAN RX |
| CANH | CAN bus high |
| CANL | CAN bus low |
| STB/RS | GND or leave at module default if enabled |

Before wiring external transceivers, disable or bypass any onboard CAN
transceiver on the boards. Two RXD drivers on the same MCU pin can conflict.

## Bus Termination

- Place one 120 ohm resistor at each physical end of the bus.
- Disable onboard termination if the board has a solder bridge/jumper.
- Measure CANH to CANL with power off: approximately 60 ohm.

## Power and Grounding

- F407/F103 boards: USB power.
- SN65HVD230 modules: 3.3V from the respective board.
- Relay: 5V supply.
- CANable: USB power.
- All devices must share a common GND.

## Fault Injection Wiring

- Communication loss: relay in series with CANH or CANL, open the relay.
- Bus-Off injection: short CANH and CANL briefly through the relay, then release.
  Do not keep the bus shorted for a long time.
- DTC and NRC injection: implemented in software, no extra wiring required.
