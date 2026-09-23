#include "can_driver.h"

static HAL_StatusTypeDef CanDriver_ConfigureFilter(CanDriver_t *driver, uint32_t filter_bank)
{
  CAN_FilterTypeDef filter = {0};

  filter.FilterActivation = ENABLE;
  filter.FilterBank = filter_bank;
  filter.FilterMode = CAN_FILTERMODE_IDMASK;
  filter.FilterScale = CAN_FILTERSCALE_32BIT;
  filter.FilterFIFOAssignment = CAN_RX_FIFO0;
  filter.FilterIdHigh = 0x0000U;
  filter.FilterIdLow = 0x0000U;
  filter.FilterMaskIdHigh = 0x0000U;
  filter.FilterMaskIdLow = 0x0000U;
  filter.SlaveStartFilterBank = 14U;

  return HAL_CAN_ConfigFilter(driver->handle, &filter);
}

HAL_StatusTypeDef CanDriver_Init(CanDriver_t *driver,
                                CAN_HandleTypeDef *handle,
                                uint32_t prescaler,
                                uint32_t time_seg1,
                                uint32_t time_seg2,
                                uint32_t filter_bank,
                                CanDriverRxCallback_t rx_callback)
{
  if ((driver == NULL) || (handle == NULL))
  {
    return HAL_ERROR;
  }

  driver->handle = handle;
  driver->rx_callback = rx_callback;

  handle->Init.Prescaler = prescaler;
  handle->Init.Mode = CAN_MODE_NORMAL;
  handle->Init.SyncJumpWidth = CAN_SJW_1TQ;
  handle->Init.TimeSeg1 = time_seg1;
  handle->Init.TimeSeg2 = time_seg2;
  handle->Init.TimeTriggeredMode = DISABLE;
  handle->Init.AutoBusOff = ENABLE;
  handle->Init.AutoWakeUp = DISABLE;
  handle->Init.AutoRetransmission = ENABLE;
  handle->Init.ReceiveFifoLocked = DISABLE;
  handle->Init.TransmitFifoPriority = DISABLE;

  if (HAL_CAN_Init(handle) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return CanDriver_ConfigureFilter(driver, filter_bank);
}

HAL_StatusTypeDef CanDriver_Start(CanDriver_t *driver)
{
  if (driver == NULL)
  {
    return HAL_ERROR;
  }

  if (HAL_CAN_Start(driver->handle) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_CAN_ActivateNotification(driver->handle, CAN_IT_RX_FIFO0_MSG_PENDING);
}

HAL_StatusTypeDef CanDriver_SendStd(CanDriver_t *driver,
                                    uint32_t std_id,
                                    const uint8_t *data,
                                    uint8_t len)
{
  CAN_TxHeaderTypeDef tx_header = {0};
  uint32_t mailbox = 0U;
  uint8_t payload[8] = {0};

  if ((driver == NULL) || (data == NULL) || (len == 0U) || (len > 8U))
  {
    return HAL_ERROR;
  }

  if (HAL_CAN_GetTxMailboxesFreeLevel(driver->handle) == 0U)
  {
    return HAL_BUSY;
  }

  for (uint8_t i = 0U; i < len; i++)
  {
    payload[i] = data[i];
  }

  tx_header.StdId = std_id;
  tx_header.IDE = CAN_ID_STD;
  tx_header.RTR = CAN_RTR_DATA;
  tx_header.DLC = len;
  tx_header.TransmitGlobalTime = DISABLE;

  return HAL_CAN_AddTxMessage(driver->handle, &tx_header, payload, &mailbox);
}

void CanDriver_RxIrqHandler(CanDriver_t *driver)
{
  CAN_RxHeaderTypeDef rx_header = {0};
  uint8_t data[8] = {0};
  uint32_t id;

  if ((driver == NULL) || (driver->handle == NULL))
  {
    return;
  }

  if (HAL_CAN_GetRxMessage(driver->handle, CAN_RX_FIFO0, &rx_header, data) != HAL_OK)
  {
    return;
  }

  id = (rx_header.IDE == CAN_ID_EXT) ? rx_header.ExtId : rx_header.StdId;

  if (driver->rx_callback != NULL)
  {
    driver->rx_callback(id, data, rx_header.DLC);
  }
}
