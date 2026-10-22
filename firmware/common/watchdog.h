#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Independent watchdog helper.
 *
 * The F407 project ships a trimmed HAL without the IWDG driver, and no
 * STM32Cube F4 firmware package is available on this machine to copy it from,
 * so this helper drives the IWDG registers directly, following the same
 * sequence as ST's HAL_IWDG_Init(). The IWDG register map is identical on
 * STM32F1 and STM32F4, so both nodes share one implementation.
 *
 * Watchdog_Init() starts the watchdog, and it cannot be stopped afterwards: a
 * stalled main loop resets the MCU. While debugging, freeze it from the
 * debugger side (__HAL_DBGMCU_FREEZE_IWDG()) or a breakpoint will reset the MCU.
 *
 * Timeout resolution: LSI is about 40 kHz on STM32F1 and about 32 kHz on
 * STM32F4, with a wide tolerance, so the real timeout is roughly ±40 % of the
 * requested value. Feed the watchdog from the main loop only.
 */

#define WATCHDOG_DEFAULT_TIMEOUT_MS 1000U

/* Timeout is rounded down to whole reload ticks. Returns false when the
   request cannot be represented, or when the registers did not settle. */
bool Watchdog_Init(uint32_t timeout_ms);

/* Feed the watchdog. Call after a successful Watchdog_Init(). */
void Watchdog_Kick(void);

#endif /* WATCHDOG_H */
