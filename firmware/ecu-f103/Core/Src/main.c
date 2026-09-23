#include "main.h"
#include "can_driver.h"

#define APP_CAN_ID               0x123U
#define APP_CAN_TX_PERIOD_MS     500U

CAN_HandleTypeDef hcan1;
static CanDriver_t g_can;
volatile uint32_t g_can_tx_count = 0U;
volatile uint32_t g_can_rx_count = 0U;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void APP_CAN_Send(uint8_t value);
static void APP_OnCanRx(uint32_t id, const uint8_t *data, uint32_t len);

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

  if (CanDriver_Start(&g_can) != HAL_OK)
  {
    Error_Handler();
  }

  while (1)
  {
    static uint32_t last_tx_tick = 0U;
    static uint8_t tx_value = 0U;
    uint32_t now = HAL_GetTick();

    if ((now - last_tx_tick) >= APP_CAN_TX_PERIOD_MS)
    {
      last_tx_tick = now;
      APP_CAN_Send(tx_value);
      tx_value ^= 1U;
    }

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

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
