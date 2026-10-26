#include "main.h"
#include "app_param_storage.h"
#include "background_probe.h"
#include "can_driver.h"
#include "can_health.h"
#include "dbc_bench.h"
#include "isotp.h"
#include "isotp_bench.h"
#include "stress_master.h"
#include "uds_ecu.h"
#include "uds_fault_test.h"
#include "watchdog.h"

#define APP_CAN_ID               0x123U
/* ISO-TP diagnostic addresses for this node (UDS server role). */
#ifndef APP_ISOTP_BENCH_TX
#define APP_ISOTP_BENCH_TX       0U
#endif
#ifndef APP_ISOTP_BENCH_RX
#define APP_ISOTP_BENCH_RX       0U
#endif
#if APP_ISOTP_BENCH_TX && APP_ISOTP_BENCH_RX
#error "Select either ISO-TP bench TX or RX, not both"
#endif

#if APP_ISOTP_BENCH_TX
#define APP_ISOTP_RX_ID          0x7E8U
#define APP_ISOTP_TX_ID          0x7E0U
#else
#define APP_ISOTP_RX_ID          0x7E1U
#define APP_ISOTP_TX_ID          0x7E9U
#endif
#define APP_ISOTP_TX_BUFFER_SIZE 300U
#define APP_ISOTP_RX_BUFFER_SIZE 300U
#define APP_ISOTP_BENCH_PERIOD_MS 1000U
#ifndef APP_ISOTP_BENCH_PAYLOAD_SIZE
#define APP_ISOTP_BENCH_PAYLOAD_SIZE 100U
#endif
#ifndef APP_ISOTP_FORCE_TX_BUSY
#define APP_ISOTP_FORCE_TX_BUSY 0U
#endif
/* Bring-up switch: 20 ms cyclic frames so a bus short reaches Bus-Off in under
   a second (32 failures) instead of ~9 s at the normal 500 ms period.
   The shipped value is 0. */
#ifndef APP_CAN_FAST_TX_TEST
#define APP_CAN_FAST_TX_TEST 0U
#endif
#ifndef APP_DBC_BENCH
#define APP_DBC_BENCH 0U
#endif
#ifndef APP_BENCH_TEST
#define APP_BENCH_TEST 0U
#endif
#ifndef APP_STRESS_MASTER
#define APP_STRESS_MASTER 0U
#endif
#if APP_STRESS_MASTER
#define APP_STRESS_REQUEST_ID    0x7E0U
#define APP_STRESS_RESPONSE_ID   0x7E8U
#define APP_STRESS_REQUESTS      1000U
#define APP_STRESS_INTERVAL_US   20000U
#define APP_STRESS_RESPONSE_US   50000U
#define APP_STRESS_BG_PERIOD_US  228U
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
static AppParamStorage_t g_param_storage;

typedef struct
{
  AppParamStorage_t *params;
  CanHealth_t *health;
} AppUdsHooksContext_t;

static AppUdsHooksContext_t g_uds_hooks_context;
static UdsEcuHooks_t g_uds_hooks;
volatile uint32_t g_can_tx_count = 0U;
volatile uint32_t g_can_rx_count = 0U;

/* ISO-TP transport endpoints (UDS server role on 0x7E1/0x7E9). */
#if !APP_ISOTP_BENCH_RX
static uint8_t g_isotp_tx_buffer[APP_ISOTP_TX_BUFFER_SIZE];
#endif
static uint8_t g_isotp_rx_buffer[APP_ISOTP_RX_BUFFER_SIZE];
static IsotpRx_t g_isotp_rx;
static IsotpTx_t g_isotp_tx;
#if APP_BENCH_TEST
BackgroundProbe_t g_background_probe;
UdsFaultTest_t g_uds_fault_test;
#endif
#if APP_STRESS_MASTER
StressMasterStats_t g_stress_master;
volatile uint8_t g_stress_response_seen;
#endif
#if APP_DBC_BENCH
static DbcBench_t g_dbc_bench;
#endif

typedef struct
{
  uint32_t tx_errors;
  uint32_t rx_errors;
  uint32_t tx_timeouts;
  uint32_t rx_timeouts;
  int32_t last_tx_result;
  int32_t last_rx_result;
  int32_t last_route_result;
} IsotpTestStats_t;

volatile IsotpTestStats_t g_isotp_test_stats;
#if !APP_ISOTP_BENCH_TX && !APP_ISOTP_BENCH_RX
static bool g_isotp_serving = false;
#endif

#if APP_ISOTP_BENCH_TX || APP_ISOTP_BENCH_RX
volatile uint32_t g_isotp_bench_tx_started;
volatile uint32_t g_isotp_bench_rx_messages;
volatile uint32_t g_isotp_bench_rx_errors;
volatile uint16_t g_isotp_bench_rx_bad_index;
#if APP_ISOTP_BENCH_TX
volatile uint8_t g_isotp_bench_last_ack;
static bool g_isotp_bench_payload_ready;
static uint32_t g_isotp_bench_last_tx_ms;
#endif
#endif

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
static uint8_t APP_UdsSelfTest(void *context);
static bool APP_UdsEraseParams(void *context);
#if APP_STRESS_MASTER
static void APP_RunStressMaster(void);
#endif
static void APP_ServiceIsotp(uint32_t now);
static bool APP_IsotpSend(void *ctx, uint32_t txid, const uint8_t data[8U]);
static bool APP_IsotpTxPending(void *ctx);
static uint32_t APP_IsotpTick(void *ctx);
#if APP_ISOTP_BENCH_TX || APP_ISOTP_BENCH_RX
static void APP_ServiceIsotpBench(uint32_t now);
#endif

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();

  AppParamStorage_Init(&g_param_storage);

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

  g_uds_hooks_context.params = &g_param_storage;
  g_uds_hooks_context.health = &g_can_health;
  g_uds_hooks.self_test = APP_UdsSelfTest;
  g_uds_hooks.erase_params = APP_UdsEraseParams;
  g_uds_hooks.context = &g_uds_hooks_context;
  UdsEcu_InitWithHooks(&g_uds,
                       UDS_ECU_F103,
                       AppParamStorage_GetUdsStorage(&g_param_storage),
                       &g_uds_hooks);

#if APP_DBC_BENCH
  DbcBench_Init(&g_dbc_bench, DBC_BENCH_F103, 100U);
#endif
#if APP_BENCH_TEST
  BackgroundProbe_Init(&g_background_probe);
  UdsFaultTest_Init(&g_uds_fault_test);
#endif

  /* ISO-TP transport endpoints bound to the CAN driver. */
  {
    IsotpChannel_t isotp_channel = {0};
    IsotpConfig_t isotp_cfg;

    isotp_channel.send = APP_IsotpSend;
    isotp_channel.receive = NULL;
    isotp_channel.tick_ms = APP_IsotpTick;
    isotp_channel.tx_pending = APP_IsotpTxPending;
    isotp_channel.ctx = NULL;

    IsotpConfig_Init(&isotp_cfg, APP_ISOTP_TX_ID, APP_ISOTP_RX_ID,
                     g_isotp_rx_buffer, sizeof(g_isotp_rx_buffer));

    IsotpTx_Init(&g_isotp_tx, &isotp_channel, &isotp_cfg);
    IsotpRx_Init(&g_isotp_rx, &isotp_channel, &isotp_cfg);
  }

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

#if APP_STRESS_MASTER
  APP_RunStressMaster();
#endif

  while (1)
  {
    static uint32_t last_tx_tick = 0U;
    static uint8_t tx_value = 0U;
    uint32_t now = HAL_GetTick();

    CanDriver_ProcessRx(&g_can);
    UdsEcu_Tick(&g_uds, now);
    APP_ServiceIsotp(now);

#if APP_DBC_BENCH
    {
      uint32_t dbc_id;
      uint8_t dbc_data[8];

      if (DbcBench_Poll(&g_dbc_bench, now, &dbc_id, dbc_data))
      {
        (void)CanDriver_SendStd(&g_can, dbc_id, dbc_data, 8U);
      }
    }
#endif

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

#if APP_BENCH_TEST
    HAL_Delay(1U);
#else
    HAL_Delay(10U);
#endif
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
#if APP_STRESS_MASTER
  if (!g_stress_response_seen &&
      (StressMaster_AcceptResponse(&g_stress_master,
                                   id,
                                   data,
                                   (uint8_t)len,
                                   APP_STRESS_RESPONSE_ID)))
  {
    g_stress_response_seen = 1U;
  }
  return;
#endif

#if APP_BENCH_TEST
  if (id == 0x7F0U && len == 8U)
  {
    uint32_t sequence = ((uint32_t)data[0] << 24U) |
                        ((uint32_t)data[1] << 16U) |
                        ((uint32_t)data[2] << 8U) |
                        (uint32_t)data[3];
    BackgroundProbe_OnFrame(&g_background_probe, sequence);
    return;
  }
#endif

  if (id == APP_CAN_ID && len >= 1U)
  {
    g_can_rx_count++;
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13,
                      (data[0] & 0x01U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
  }
  else if (id == APP_ISOTP_RX_ID && len == 8U)
  {
    IsotpResult_t result = Isotp_RouteFrame(&g_isotp_tx, &g_isotp_rx, data);
    g_isotp_test_stats.last_route_result = (int32_t)result;
    if (result != ISOTP_OK)
    {
      g_isotp_test_stats.rx_errors++;
    }
  }
}

static bool APP_IsotpSend(void *ctx, uint32_t txid, const uint8_t data[8U])
{
  (void)ctx;
#if APP_ISOTP_FORCE_TX_BUSY
  (void)txid;
  (void)data;
  return false;
#else
  return CanDriver_SendStd(&g_can, txid, data, 8U) == HAL_OK;
#endif
}

static bool APP_IsotpTxPending(void *ctx)
{
  (void)ctx;
  return CanDriver_IsTxPending(&g_can);
}

static uint32_t APP_IsotpTick(void *ctx)
{
  (void)ctx;
  return HAL_GetTick();
}

static void APP_ServiceIsotp(uint32_t now)
{
  IsotpResult_t result;

  result = IsotpTx_Poll(&g_isotp_tx);
  g_isotp_test_stats.last_tx_result = (int32_t)result;
  if (result == ISOTP_ERR_TIMEOUT)
  {
    g_isotp_test_stats.tx_timeouts++;
    g_isotp_test_stats.tx_errors++;
  }
  else if (result != ISOTP_OK)
  {
    g_isotp_test_stats.tx_errors++;
  }

  result = IsotpRx_Poll(&g_isotp_rx);
  g_isotp_test_stats.last_rx_result = (int32_t)result;
  if (result == ISOTP_ERR_TIMEOUT)
  {
    g_isotp_test_stats.rx_timeouts++;
    g_isotp_test_stats.rx_errors++;
  }
  else if (result != ISOTP_OK)
  {
    g_isotp_test_stats.rx_errors++;
  }

#if APP_ISOTP_BENCH_TX || APP_ISOTP_BENCH_RX
  APP_ServiceIsotpBench(now);
#else
  uint16_t response_length = 0U;

  if (IsotpRx_MessageReady(&g_isotp_rx) && !g_isotp_serving)
  {
    uint8_t request[APP_ISOTP_RX_BUFFER_SIZE];
    uint16_t request_length = 0U;
    UdsProcessResult_t result = UDS_PROCESS_NO_RESPONSE;
    UdsFaultAction_t fault_action = UDS_FAULT_PASS;

    if (IsotpRx_Read(&g_isotp_rx, request, sizeof(request), &request_length) == ISOTP_OK)
    {
#if APP_BENCH_TEST
      fault_action = UdsFaultTest_Process(&g_uds_fault_test,
                                          request,
                                          request_length,
                                          now,
                                          g_isotp_tx_buffer,
                                          sizeof(g_isotp_tx_buffer),
                                          &response_length);
#endif
      if (fault_action == UDS_FAULT_PASS)
      {
        result = UdsEcu_Process(&g_uds, now, request, request_length,
                                g_isotp_tx_buffer, sizeof(g_isotp_tx_buffer),
                                &response_length);
      }
      if ((fault_action == UDS_FAULT_HANDLED) ||
          ((fault_action == UDS_FAULT_PASS) &&
           ((result == UDS_PROCESS_RESPONSE) ||
            (result == UDS_PROCESS_RESPONSE_PENDING))))
      {
        if ((response_length > 0U) &&
            (IsotpTx_Start(&g_isotp_tx,
                           g_isotp_tx_buffer,
                           response_length) == ISOTP_OK))
        {
          g_isotp_serving = true;
        }
      }
    }
  }

  if (!g_isotp_serving)
  {
    UdsProcessResult_t pending_result;

    response_length = 0U;
    pending_result = UdsEcu_PollPending(&g_uds,
                                        now,
                                        g_isotp_tx_buffer,
                                        sizeof(g_isotp_tx_buffer),
                                        &response_length);
    if ((pending_result == UDS_PROCESS_RESPONSE) && (response_length > 0U))
    {
      if (IsotpTx_Start(&g_isotp_tx, g_isotp_tx_buffer, response_length) == ISOTP_OK)
      {
        g_isotp_serving = true;
      }
    }
  }

  if (g_isotp_serving && !IsotpTx_IsBusy(&g_isotp_tx))
  {
    g_isotp_serving = false;
  }
#endif
}

#if APP_ISOTP_BENCH_TX || APP_ISOTP_BENCH_RX
static void APP_ServiceIsotpBench(uint32_t now)
{
  if (IsotpRx_MessageReady(&g_isotp_rx))
  {
    uint8_t message[APP_ISOTP_RX_BUFFER_SIZE];
    uint16_t message_length = 0U;

    if (IsotpRx_Read(&g_isotp_rx, message, sizeof(message), &message_length) == ISOTP_OK)
    {
#if APP_ISOTP_BENCH_RX
      uint16_t bad_index = 0xFFFFU;
      uint8_t status = ISOTP_BENCH_STATUS_FAIL;
      g_isotp_bench_rx_messages++;
      if (IsotpBench_ValidatePayload(message, message_length,
                                     APP_ISOTP_BENCH_PAYLOAD_SIZE, &bad_index))
      {
        status = ISOTP_BENCH_STATUS_OK;
      }
      else
      {
        g_isotp_bench_rx_errors++;
        g_isotp_bench_rx_bad_index = bad_index;
      }
      {
        uint8_t status_frame[8];
        IsotpBench_BuildStatusFrame(status_frame, status);
        (void)CanDriver_SendStd(&g_can, APP_ISOTP_TX_ID, status_frame, 8U);
      }
#elif APP_ISOTP_BENCH_TX
      if (message_length == 1U)
      {
        g_isotp_bench_last_ack = message[0];
      }
#endif
    }
  }

#if APP_ISOTP_BENCH_TX
  if (!IsotpTx_IsBusy(&g_isotp_tx) &&
      ((now - g_isotp_bench_last_tx_ms) >= APP_ISOTP_BENCH_PERIOD_MS))
  {
    if (!g_isotp_bench_payload_ready)
    {
      IsotpBench_BuildPayload(g_isotp_tx_buffer, APP_ISOTP_BENCH_PAYLOAD_SIZE);
      g_isotp_bench_payload_ready = true;
    }
    if (IsotpTx_Start(&g_isotp_tx, g_isotp_tx_buffer,
                      APP_ISOTP_BENCH_PAYLOAD_SIZE) == ISOTP_OK)
    {
      g_isotp_bench_tx_started++;
      g_isotp_bench_last_tx_ms = now;
    }
    else
    {
      g_isotp_bench_rx_errors++;
    }
  }
#endif
}
#endif

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

static uint8_t APP_UdsSelfTest(void *context)
{
  const AppUdsHooksContext_t *hooks = (const AppUdsHooksContext_t *)context;
  uint8_t result = 0U;
  uint8_t watchdog_status;

  if (!AppParamStorage_IsValid(hooks->params))
  {
    result |= UDS_ECU_SELFTEST_PARAM_FAILURE;
  }
  if (CanHealth_GetState(hooks->health) != CAN_HEALTH_STATE_ERROR_ACTIVE)
  {
    result |= UDS_ECU_SELFTEST_CAN_FAILURE;
  }
  watchdog_status = Watchdog_GetStatus();
  if ((watchdog_status &
       (WATCHDOG_STATUS_IWDG_READY | WATCHDOG_STATUS_WWDG_READY)) !=
      (WATCHDOG_STATUS_IWDG_READY | WATCHDOG_STATUS_WWDG_READY))
  {
    result |= UDS_ECU_SELFTEST_WATCHDOG_FAILURE;
  }
  return result;
}

static bool APP_UdsEraseParams(void *context)
{
  const AppUdsHooksContext_t *hooks = (const AppUdsHooksContext_t *)context;
  return AppParamStorage_Clear(hooks->params);
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

#if APP_STRESS_MASTER
static void APP_StressCycleInit(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint32_t APP_StressCycles(void)
{
  return DWT->CYCCNT;
}

static bool APP_StressBefore(uint32_t left, uint32_t right)
{
  return (int32_t)(left - right) < 0;
}

static void APP_StressServiceWatchdog(void)
{
  static uint32_t last_service_ms = 0U;
  uint32_t now = HAL_GetTick();

  if ((uint32_t)(now - last_service_ms) >= 10U)
  {
    last_service_ms = now;
    Watchdog_Kick();
    Watchdog_WwdgService(now);
  }
}

static void APP_StressServiceBackground(uint32_t now,
                                        uint32_t *next_background,
                                        uint32_t *sequence,
                                        uint32_t cycles_per_us)
{
  uint8_t frame[8];
  const uint32_t period = APP_STRESS_BG_PERIOD_US * cycles_per_us;

  if (!APP_StressBefore(now, *next_background))
  {
    uint32_t late = now - *next_background;
    if (late >= period)
    {
      uint32_t skipped = late / period;
      g_stress_master.background_missed += skipped;
      *sequence += skipped;
      *next_background += skipped * period;
    }

    StressMaster_BuildBackground(*sequence, frame);
    (*sequence)++;
    if (CanDriver_SendStd(&g_can, 0x7F0U, frame, 8U) == HAL_OK)
    {
      g_stress_master.background_sent++;
    }
    else
    {
      g_stress_master.background_missed++;
    }
    *next_background += period;
  }
}

static void APP_RunStressMaster(void)
{
  uint8_t request[8];
  uint32_t cycles_per_us = SystemCoreClock / 1000000U;
  uint32_t request_period = APP_STRESS_INTERVAL_US * cycles_per_us;
  uint32_t response_timeout = APP_STRESS_RESPONSE_US * cycles_per_us;
  uint32_t now;
  uint32_t request_due;
  uint32_t next_background;
  uint32_t sequence = 0U;
  uint32_t test_started;

  StressMaster_Init(&g_stress_master, APP_STRESS_REQUESTS);
  APP_StressCycleInit();
  now = APP_StressCycles();
  test_started = now;
  request_due = now;
  next_background = now;
  StressMaster_BuildRequest(request);

  while (g_stress_master.requests_sent < g_stress_master.requests_target)
  {
    uint32_t request_started;
    uint32_t response_deadline;

    while (APP_StressBefore(APP_StressCycles(), request_due))
    {
      now = APP_StressCycles();
      APP_StressServiceBackground(now,
                                  &next_background,
                                  &sequence,
                                  cycles_per_us);
      CanDriver_ProcessRx(&g_can);
      APP_StressServiceWatchdog();
    }

    g_stress_response_seen = 0U;
    request_started = APP_StressCycles();
    StressMaster_RecordRequest(&g_stress_master);
    (void)CanDriver_SendStd(&g_can,
                            APP_STRESS_REQUEST_ID,
                            request,
                            8U);
    response_deadline = request_started + response_timeout;
    request_due += request_period;

    while (!g_stress_response_seen &&
           APP_StressBefore(APP_StressCycles(), response_deadline))
    {
      now = APP_StressCycles();
      APP_StressServiceBackground(now,
                                  &next_background,
                                  &sequence,
                                  cycles_per_us);
      CanDriver_ProcessRx(&g_can);
      APP_StressServiceWatchdog();
    }

    if (g_stress_response_seen)
    {
      StressMaster_RecordLatency(&g_stress_master,
                                 APP_StressCycles() - request_started);
    }
    else
    {
      StressMaster_RecordTimeout(&g_stress_master);
    }

    if (APP_StressBefore(APP_StressCycles(), request_due))
    {
      continue;
    }
    request_due = APP_StressCycles();
  }

  g_stress_master.runtime_cycles = APP_StressCycles() - test_started;
  StressMaster_Finish(&g_stress_master);
  while (1)
  {
    CanDriver_ProcessRx(&g_can);
    APP_StressServiceWatchdog();
  }
}
#endif

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
