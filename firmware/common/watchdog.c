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

#define WWDG_CFR_CONFIG ((3U << WWDG_CFR_WDGTB_Pos) | WATCHDOG_WWDG_WINDOW)

/* Bounded wait for the prescaler/reload value transfer. */
#define WATCHDOG_SYNC_SPIN_LIMIT 0x000FFFFFU

_Static_assert(WATCHDOG_WWDG_WINDOW > 0x3FU,
               "WWDG window must be above the timeout boundary");
_Static_assert(WATCHDOG_WWDG_WINDOW < WATCHDOG_WWDG_COUNTER_INIT,
               "WWDG window must be below the initial counter");
_Static_assert(WATCHDOG_WWDG_REFRESH_MS > 0U,
               "WWDG refresh interval must be non-zero");

static uint32_t s_wwdg_last_refresh_tick;

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

bool Watchdog_WwdgInit(uint32_t now)
{
  RCC->APB1ENR |= RCC_APB1ENR_WWDGEN;
  (void)RCC->APB1ENR;

  WWDG->CFR = WWDG_CFR_CONFIG;
  if ((WWDG->CFR & (WWDG_CFR_WDGTB_Msk | WWDG_CFR_W_Msk)) != WWDG_CFR_CONFIG)
  {
    return false;
  }

  WWDG->CR = WATCHDOG_WWDG_COUNTER_INIT;
  s_wwdg_last_refresh_tick = now;
  WWDG->CR = WWDG_CR_WDGA | WATCHDOG_WWDG_COUNTER_INIT;
  return true;
}

void Watchdog_WwdgService(uint32_t now)
{
  if ((now - s_wwdg_last_refresh_tick) >= WATCHDOG_WWDG_REFRESH_MS)
  {
    s_wwdg_last_refresh_tick = now;
    Watchdog_WwdgKick();
  }
}

void Watchdog_WwdgKick(void)
{
  WWDG->CR = WATCHDOG_WWDG_COUNTER_INIT;
}
