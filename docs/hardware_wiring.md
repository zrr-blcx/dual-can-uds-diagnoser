# Hardware Wiring

## Hardware List

| Item | Model / Spec | Qty |
| --- | --- | --- |
| STM32F407 board | Positive Atom Explorer F407ZGT6 | 1 |
| STM32F103 board | Jiangkeda STM32F103C8T6 minimum board | 1 |
| CAN transceiver | SN65HVD230 module | 2 |
| USB-CAN | CANable 2.0 / Pro with slcan firmware | 1 |
| Fault relay | 2-channel 5V opto-isolated | 1 |
| USB-TTL | CH340 | 1 |
| Multimeter | Any | 1 |
| Termination | SN65HVD230 modules carry 120 ohm onboard | 0 extra |
| Power | 5V for relay and modules | 1 |

## MCU CAN Pins

| MCU | CAN peripheral | TX | RX |
| --- | --- | --- | --- |
| STM32F407ZGT6 | CAN1 | PA12 | PA11 |
| STM32F103C8T6 | CAN1 | PA12 | PA11 |

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

The current F407 Explorer and Jiangkeda F103C8T6 boards do not include an
onboard CAN transceiver. Their MCUs contain only the CAN controller, so the
external SN65HVD230 modules are required for CANH/CANL. If a different board
with an onboard transceiver is used later, disable or bypass that transceiver
before connecting an external module.

## Bus Termination

- Both SN65HVD230 modules in this project carry a 120 ohm termination across
  CANH/CANL (some boards route it through a jumper or solder bridge). With one
  module at each physical end of the bus, those two onboard resistors already
  give the required 60 ohm: do **not** add external 120 ohm resistors on top.
- Check each module on its own, unplugged from the bus and with power off:
  120 ohm means its onboard termination is connected, an open reading means it
  is not (bridge/jumper the module if so).
- Keep the CANable's switchable 120 ohm termination **off** (the onboard
  switch). In this topology the CANable taps the middle of the bus, so a third
  120 ohm would drop the bus to about 40 ohm. Use the switch as a quick check:
  toggling it must move the reading between 60 ohm and 40 ohm.
- Measure CANH to CANL with power off and everything connected: approximately
  60 ohm. 30 ohm means two extra resistors are paralleled in; 40 ohm means
  three terminations are present.

## Power and Grounding

- F407/F103 boards: USB power.
- SN65HVD230 modules: 3.3V from the respective board.
- Relay: 5V supply.
- CANable: USB power.
- All devices must share a common GND.

## Fault Injection Wiring

- Communication loss: CH1 normally-closed contact in series with CANH; opening
  it simulates loss of communication. On this hardware it only reached
  error-passive and is not a valid Bus-Off trigger.
- Bus-Off injection: use CH2 normally-open contact across CANH and CANL.
  The validated r11 run used approximately 2 seconds and observed three
  Bus-Off/recovery events. W2-10 used the 20 ms test image for approximately
  8 seconds to verify the three-failure reset branch. Use only a controlled
  bench setup and release the relay immediately after the acceptance event.
- DTC and NRC injection: implemented in software, no extra wiring required.
- **Bench status (2026-10-22): the relay is wired and the CH2 path is verified.**
  Phase 2/3 did not require the relay. A listen-only CANable experiment was
  retained as a diagnostic step, but it did not reach Bus-Off because TEC froze
  at 128. Final acceptance therefore uses the physical CH2 short.
- Full Week 2 test topology (relay, termination, CANable tap, ST-Link):
  `docs/diagrams/week2_bus_off_injection_wiring.svg`
  (source: `week2_bus_off_injection_wiring.dot`).

## Debug, Flashing and Bus-Off Isolation (Week 2 Phase 3)

- ST-Link connects to the target SWD header (3.3V / GND / PA13 SWDIO / PA14
  SWCLK). Both probes may remain connected; select the intended one with its
  probe serial number and flash with
  `STM32_Programmer_CLI -c port=SWD sn=<serial> -w <hex> -v -rst`.
- A listen-only CANable can remove ACK for diagnostics, but on this hardware it
  only produced ACK errors and error-passive operation. It is not the final
  Bus-Off injection method.
- Health snapshot read-out: `STM32_Programmer_CLI -c port=SWD -u <address> 4 file.bin`.
- Wiring and step order: `docs/diagrams/week2_stlink_debug_isolate_wiring.svg`
  (source: `week2_stlink_debug_isolate_wiring.dot`).

## Week 3 ISO-TP Diagnostic Topology

- The physical CAN topology is unchanged from the Week 2 dual-node bench:
  F407 and F103 each use one external SN65HVD230, both modules provide the
  two end terminations, and CANable taps the middle with its 120 ohm switch
  off.
- The relay and ST-Link are optional for Week 3. Keep the relay de-energized
  (`CH1` normally closed, `CH2` normally open).
- CANable may run as a passive capture node or as the active diagnostic
  tester. Label the intended mode explicitly; "capture" alone does not imply
  listen-only.
- Diagnostic IDs are `0x7E0/0x7E8` for F407 and `0x7E1/0x7E9` for F103.
- Diagram: `docs/项目二-第3周-ISOTP双节点诊断接线.svg`
  (source: `docs/项目二-第3周-ISOTP双节点诊断接线.dot`).
