#include "watchdog.h"

#if defined(STM32F103xB)
#include "stm32f1xx_hal.h"
#define WATCHDOG_LSI_HZ 40000U
#elif defined(STM32F407xx)
#include "stm32f4xx_hal.h"
#define WATCHDOG_LSI_HZ 32000U
#else
#error "Unsupported STM32 family for the watchdog helper"
#endif

/* PR = 0b100 -> LSI / 64; same encoding on STM32F1 and STM32F4. */
#define WATCHDOG_PRESCALER_DIVIDER 64U
#define WATCHDOG_PRESCALER_BITS IWDG_PR_PR_2
#define WATCHDOG_RELOAD_MAX 0x0FFFU

#define IWDG_KEY_START 0x0000CCCCU
#define IWDG_KEY_ENABLE_WRITE 0x00005555U
#define IWDG_KEY_RELOAD 0x0000AAAAU

/* Bounded wait for the prescaler/reload value transfer. */
#define WATCHDOG_SYNC_SPIN_LIMIT 0x000FFFFFU

bool Watchdog_Init(uint32_t timeout_ms)
{
  uint32_t reload;
  uint32_t spin = WATCHDOG_SYNC_SPIN_LIMIT;

  if (timeout_ms == 0U)
  {
    return false;
  }

  reload = (timeout_ms * WATCHDOG_LSI_HZ) / (WATCHDOG_PRESCALER_DIVIDER * 1000U);
  if ((reload == 0U) || (reload > WATCHDOG_RELOAD_MAX))
  {
    return false;
  }

  /* Enable the watchdog; the LSI oscillator is started automatically. */
  IWDG->KR = IWDG_KEY_START;
  IWDG->KR = IWDG_KEY_ENABLE_WRITE;
  IWDG->PR = WATCHDOG_PRESCALER_BITS;
  IWDG->RLR = reload;

  while ((IWDG->SR != 0U) && (spin > 0U))
  {
    spin--;
  }
  if (IWDG->SR != 0U)
  {
    return false;
  }

  IWDG->KR = IWDG_KEY_RELOAD;
  return true;
}

void Watchdog_Kick(void)
{
  IWDG->KR = IWDG_KEY_RELOAD;
}
