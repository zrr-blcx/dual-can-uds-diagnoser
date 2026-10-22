#include "main.h"
#include "can_driver.h"
#include "can_health.h"
#include "uds_ecu.h"
#include "watchdog.h"

#define APP_CAN_ID               0x123U
/* Bring-up switch: 20 ms cyclic frames so a bus short reaches Bus-Off in under
   a second (32 failures) instead of ~9 s at the normal 500 ms period.
   The shipped value is 0. */
#ifndef APP_CAN_FAST_TX_TEST
#define APP_CAN_FAST_TX_TEST 0U
#endif
#if APP_CAN_FAST_TX_TEST
#define APP_CAN_TX_PERIOD_MS     20U
#else
#define APP_CAN_TX_PERIOD_MS     500U
#endif
#define APP_CAN_RECOVERY_PERIOD_MS 1000U
/* Must exceed the normal 500 ms cyclic TX period (see the F407 comment). */
#define APP_CAN_RECOVERY_SETTLE_MS 1500U
#define APP_HEALTH_SUMMARY_BYTES   4U
#define APP_HEALTH_LOG_SIZE        24U
#define APP_LOG_ATTEMPT            1U
#define APP_LOG_SUCCESS            2U
#define APP_LOG_FAILURE            3U
#define APP_LOG_RESET              4U

/* Bring-up switch: set to 0 to keep the node running while diagnosing Bus-Off
   counters (a reset would wipe the RAM logs). The shipped value is 1. */
#ifndef APP_CAN_ESCALATION_ENABLE
#define APP_CAN_ESCALATION_ENABLE 1U
#endif

/* WWDG bring-up tests. Normal builds service the WWDG every 30 ms. */
#ifndef APP_WWDG_SELFTEST
#define APP_WWDG_SELFTEST 0U
#endif
#ifndef APP_WWDG_EARLY_FEED_TEST
#define APP_WWDG_EARLY_FEED_TEST 0U
#endif
#if APP_WWDG_SELFTEST && APP_WWDG_EARLY_FEED_TEST
#error "Select only one WWDG self-test mode"
#endif

CAN_HandleTypeDef hcan1;
static CanDriver_t g_can;
static UdsEcu_t g_uds;
static CanHealth_t g_can_health;
volatile uint32_t g_can_tx_count = 0U;
volatile uint32_t g_can_rx_count = 0U;

/* Readable over ST-Link: full counters plus the 4-byte summary reserved for the
   0xF192 diagnostic DID (state / bus-off events / recovery successes /
   consecutive recovery failures). */
CanHealthSnapshot_t g_can_health_full;
uint8_t g_can_health_summary[APP_HEALTH_SUMMARY_BYTES];

typedef struct
{
  uint32_t tick;
  uint8_t kind;
  uint8_t arg;
  uint16_t reserved;
} AppHealthLogEntry_t;

typedef struct
{
  AppHealthLogEntry_t entries[APP_HEALTH_LOG_SIZE];
  uint32_t next_index;
  uint32_t recorded;
} AppHealthLog_t;

/* Readable over ST-Link: the recovery service records every attempt, verdict
   and reset request so they can be lined up with g_can_error_log. */
AppHealthLog_t g_app_health_log;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void APP_CAN_Send(uint8_t value);
static void APP_OnCanRx(uint32_t id, const uint8_t *data, uint32_t len);
static void APP_ServiceCanHealth(uint32_t now);
static void APP_UpdateHealthSummary(void);
static void APP_HealthLog(uint8_t kind, uint8_t arg);

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();

  hcan1.Instance = CAN1;
  if (CanDriver_Init(&g_can, &hcan1, 4U, CAN_BS1_15TQ, CAN_BS2_2TQ, 0U, APP_OnCanRx) != HAL_OK)
  {
    Error_Handler();
  }

  CanHealth_Init(&g_can_health, NULL);
  CanDriver_AttachHealth(&g_can, &g_can_health);

  if (CanDriver_Start(&g_can) != HAL_OK)
  {
    Error_Handler();
  }

  UdsEcu_Init(&g_uds, UDS_ECU_F103);

  /* Starts the IWDG: a stalled loop now resets the MCU. When debugging, add
     __HAL_DBGMCU_FREEZE_IWDG() or breakpoints will reset the target. */
  if (!Watchdog_Init(WATCHDOG_DEFAULT_TIMEOUT_MS))
  {
    Error_Handler();
  }

#ifdef DEBUG
  __HAL_DBGMCU_FREEZE_WWDG();
#endif
  if (!Watchdog_WwdgInit(HAL_GetTick()))
  {
    Error_Handler();
  }

  while (1)
  {
    static uint32_t last_tx_tick = 0U;
    static uint8_t tx_value = 0U;
    uint32_t now = HAL_GetTick();

    UdsEcu_Tick(&g_uds, now);

    if ((now - last_tx_tick) >= APP_CAN_TX_PERIOD_MS)
    {
      last_tx_tick = now;
      APP_CAN_Send(tx_value);
      tx_value ^= 1U;
    }

    APP_ServiceCanHealth(now);
    APP_UpdateHealthSummary();

    Watchdog_Kick();

#if APP_WWDG_SELFTEST
    /* self-test: keep IWDG fed, but intentionally stop refreshing the WWDG */
#elif APP_WWDG_EARLY_FEED_TEST
    /* self-test: refresh before the window opens, which must reset the MCU */
    Watchdog_WwdgKick();
#else
    Watchdog_WwdgService(now);
#endif

    HAL_Delay(10U);
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

static void APP_CAN_Send(uint8_t value)
{
  uint8_t data[1] = { value & 0x01U };

  if (CanDriver_SendStd(&g_can, APP_CAN_ID, data, 1U) == HAL_OK)
  {
    g_can_tx_count++;
  }
}

static void APP_OnCanRx(uint32_t id, const uint8_t *data, uint32_t len)
{
  if ((id == APP_CAN_ID) && (len >= 1U))
  {
    g_can_rx_count++;
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13,
                      (data[0] & 0x01U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
  }
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  if ((hcan != NULL) && (hcan->Instance == CAN1))
  {
    CanDriver_RxIrqHandler(&g_can);
  }
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
  if ((hcan != NULL) && (hcan->Instance == CAN1))
  {
    CanDriver_OnErrorIsr(&g_can);
  }
}

/* Bus-Off recovery.
 *
 * Phase 1: while bus-off, retry the driver recovery chain every
 *          APP_CAN_RECOVERY_PERIOD_MS.
 * Phase 2: APP_CAN_RECOVERY_SETTLE_MS later, judge the attempt from the
 *          controller itself: success requires a cleared BOFF flag AND no new
 *          Bus-Off event during the settle window. The settle delay matters -
 *          right after leaving Bus-Off the controller keeps reporting BOFF until
 *          it has seen 11 recessive bits, so judging immediately would turn a
 *          healthy recovery into a failure.
 *
 * Counting a re-entry into Bus-Off as a failure is what makes the
 * "three consecutive failures -> MCU reset" escalation reachable: a bus that
 * stays broken keeps pushing the node back into Bus-Off, so three failures
 * accumulate and the node restarts. The retry cadence is deliberately slower
 * (1 s) than the settle window so a persistent fault produces a visible reset
 * every few seconds instead of a tight recovery loop.
 * Three consecutive failures request an MCU reset; the counters are visible in
 * g_can_health_summary / g_can_health_full over ST-Link. */
static void APP_ServiceCanHealth(uint32_t now)
{
  static uint32_t last_recovery_tick = 0U;
  static uint32_t attempt_started_tick = 0U;
  static uint32_t bus_off_events_at_attempt = 0U;
  static bool attempt_pending = false;
  uint16_t tec = 0U;
  uint16_t rec = 0U;
  bool bus_off_active = true;
  bool recovered;

  if (attempt_pending)
  {
    if ((now - attempt_started_tick) < APP_CAN_RECOVERY_SETTLE_MS)
    {
      return;
    }
    attempt_pending = false;
    recovered = CanDriver_GetErrorCounters(&g_can, &tec, &rec, &bus_off_active) &&
                !bus_off_active &&
                (g_can_health.bus_off_events == bus_off_events_at_attempt);
    if (CanHealth_AttemptRecovery(&g_can_health, recovered))
    {
      APP_HealthLog(APP_LOG_SUCCESS, 0U);
    }
    else
    {
      APP_HealthLog(APP_LOG_FAILURE, g_can_health.consecutive_recovery_failures);
      if (CanHealth_ResetRequested(&g_can_health))
      {
        APP_HealthLog(APP_LOG_RESET, g_can_health.consecutive_recovery_failures);
#if APP_CAN_ESCALATION_ENABLE
        NVIC_SystemReset();
#endif
      }
    }
    return;
  }

  if (CanHealth_GetState(&g_can_health) != CAN_HEALTH_STATE_BUS_OFF)
  {
    return;
  }

  if ((now - last_recovery_tick) < APP_CAN_RECOVERY_PERIOD_MS)
  {
    return;
  }
  last_recovery_tick = now;

  if (CanDriver_Recover(&g_can) == HAL_OK)
  {
    attempt_pending = true;
    attempt_started_tick = now;
    bus_off_events_at_attempt = g_can_health.bus_off_events;
    APP_HealthLog(APP_LOG_ATTEMPT, 0U);
    return;
  }

  if (!CanHealth_AttemptRecovery(&g_can_health, false))
  {
    APP_HealthLog(APP_LOG_FAILURE, g_can_health.consecutive_recovery_failures);
    if (CanHealth_ResetRequested(&g_can_health))
    {
      APP_HealthLog(APP_LOG_RESET, g_can_health.consecutive_recovery_failures);
#if APP_CAN_ESCALATION_ENABLE
      NVIC_SystemReset();
#endif
    }
  }
}

static void APP_HealthLog(uint8_t kind, uint8_t arg)
{
  uint32_t index = g_app_health_log.next_index;

  g_app_health_log.entries[index].tick = HAL_GetTick();
  g_app_health_log.entries[index].kind = kind;
  g_app_health_log.entries[index].arg = arg;
  g_app_health_log.entries[index].reserved = 0U;
  g_app_health_log.next_index = (index + 1U) % APP_HEALTH_LOG_SIZE;
  g_app_health_log.recorded++;
}

static void APP_UpdateHealthSummary(void)
{
  uint16_t tec = 0U;
  uint16_t rec = 0U;
  bool bus_off_active = false;

  if (CanDriver_GetErrorCounters(&g_can, &tec, &rec, &bus_off_active))
  {
    CanHealth_SyncHardwareCounters(&g_can_health, tec, rec, bus_off_active);
  }

  CanHealth_GetSnapshot(&g_can_health, &g_can_health_full);

  g_can_health_summary[0] = (uint8_t)g_can_health_full.state;
  g_can_health_summary[1] = (g_can_health_full.bus_off_events > 255U)
                              ? 255U
                              : (uint8_t)g_can_health_full.bus_off_events;
  g_can_health_summary[2] = (g_can_health_full.recovery_successes > 255U)
                              ? 255U
                              : (uint8_t)g_can_health_full.recovery_successes;
  g_can_health_summary[3] = g_can_health_full.consecutive_recovery_failures;
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
