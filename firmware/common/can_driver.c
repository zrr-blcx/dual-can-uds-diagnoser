#include "can_driver.h"

/* The F1 and F4 HALs share this error bit layout; fail the build if it moves. */
_Static_assert(HAL_CAN_ERROR_STF == CAN_HEALTH_ERROR_FLAG_STUFF, "HAL stuff error bit changed");
_Static_assert(HAL_CAN_ERROR_FOR == CAN_HEALTH_ERROR_FLAG_FORM, "HAL form error bit changed");
_Static_assert(HAL_CAN_ERROR_ACK == CAN_HEALTH_ERROR_FLAG_ACK, "HAL ack error bit changed");
_Static_assert(HAL_CAN_ERROR_BR == CAN_HEALTH_ERROR_FLAG_BIT_RECESSIVE, "HAL bit error bit changed");
_Static_assert(HAL_CAN_ERROR_BD == CAN_HEALTH_ERROR_FLAG_BIT_DOMINANT, "HAL bit error bit changed");
_Static_assert(HAL_CAN_ERROR_CRC == CAN_HEALTH_ERROR_FLAG_CRC, "HAL crc error bit changed");

#define CAN_DRIVER_ERROR_INTERRUPTS                                          \
  (CAN_IT_ERROR | CAN_IT_ERROR_WARNING | CAN_IT_ERROR_PASSIVE |             \
   CAN_IT_BUSOFF | CAN_IT_LAST_ERROR_CODE)

volatile CanDriverErrorLog_t g_can_error_log;

static void CanDriver_LogErrorIsr(uint32_t error_code, uint32_t esr)
{
  uint32_t index = g_can_error_log.next_index;

  g_can_error_log.entries[index].sequence = g_can_error_log.recorded;
  g_can_error_log.entries[index].error_code = error_code;
  g_can_error_log.entries[index].esr = esr;
  g_can_error_log.entries[index].tick = HAL_GetTick();
  g_can_error_log.next_index = (index + 1U) % CAN_DRIVER_ERROR_LOG_SIZE;
  g_can_error_log.recorded++;
}

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
  driver->prescaler = prescaler;
  driver->time_seg1 = time_seg1;
  driver->time_seg2 = time_seg2;
  driver->filter_bank = filter_bank;

  handle->Init.Prescaler = prescaler;
  handle->Init.Mode = CAN_MODE_NORMAL;
  handle->Init.SyncJumpWidth = CAN_SJW_1TQ;
  handle->Init.TimeSeg1 = time_seg1;
  handle->Init.TimeSeg2 = time_seg2;
  handle->Init.TimeTriggeredMode = DISABLE;
  /* Keep the node in Bus-Off until CanDriver_Recover() runs, so the software
     recovery path (and its counter/limit policy) stays observable. */
  handle->Init.AutoBusOff = DISABLE;
  /* Disable automatic retransmission. With NART enabled, a frame that nobody
     acknowledges is retried back-to-back forever: measured 2026-10-15 as a
     ~6.8 kHz ACK-error storm that pins TEC at 128 (error-passive), never reaches
     Bus-Off, and keeps the TX mailbox occupied so the cyclic frame is blocked.
     With NART disabled each attempt costs one error, Bus-Off becomes reachable,
     and the mailbox is released between attempts. */
  handle->Init.AutoRetransmission = DISABLE;
  handle->Init.AutoWakeUp = DISABLE;
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

  return HAL_CAN_ActivateNotification(
      driver->handle, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_DRIVER_ERROR_INTERRUPTS);
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

bool CanDriver_IsTxPending(const CanDriver_t *driver)
{
  if ((driver == NULL) || (driver->handle == NULL))
  {
    return false;
  }

  return HAL_CAN_IsTxMessagePending(
             driver->handle,
             CAN_TX_MAILBOX0 | CAN_TX_MAILBOX1 | CAN_TX_MAILBOX2) != 0U;
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

void CanDriver_AttachHealth(CanDriver_t *driver, CanHealth_t *health)
{
  if (driver != NULL)
  {
    driver->health = health;
  }
}

bool CanDriver_GetErrorCounters(const CanDriver_t *driver,
                                uint16_t *tec,
                                uint16_t *rec,
                                bool *bus_off_active)
{
  uint32_t esr;

  if ((driver == NULL) || (driver->handle == NULL))
  {
    return false;
  }

  esr = driver->handle->Instance->ESR;
  if (tec != NULL)
  {
    *tec = (uint16_t)((esr & CAN_ESR_TEC) >> CAN_ESR_TEC_Pos);
  }
  if (rec != NULL)
  {
    *rec = (uint16_t)((esr & CAN_ESR_REC) >> CAN_ESR_REC_Pos);
  }
  if (bus_off_active != NULL)
  {
    *bus_off_active = ((esr & CAN_ESR_BOFF) != 0U);
  }

  return true;
}

void CanDriver_OnErrorIsr(CanDriver_t *driver)
{
  uint32_t error_code;
  uint16_t tec = 0U;
  uint16_t rec = 0U;
  bool bus_off = false;
  bool transmit;

  if ((driver == NULL) || (driver->handle == NULL) || (driver->health == NULL))
  {
    return;
  }

  error_code = driver->handle->ErrorCode;
  CanDriver_LogErrorIsr(error_code, driver->handle->Instance->ESR);
  if (error_code == HAL_CAN_ERROR_NONE)
  {
    return;
  }

  /* The HAL only accumulates this field, so consume it here. */
  driver->handle->ErrorCode = HAL_CAN_ERROR_NONE;

  if (!CanDriver_GetErrorCounters(driver, &tec, &rec, &bus_off))
  {
    return;
  }

  if (bus_off || ((error_code & HAL_CAN_ERROR_BOF) != 0U))
  {
    CanHealth_SyncHardwareCounters(driver->health, tec, rec, true);
    CanHealth_EnterBusOff(driver->health);
    return;
  }

  if ((error_code & (uint32_t)~(HAL_CAN_ERROR_EWG | HAL_CAN_ERROR_EPV)) == 0U)
  {
    /* Warning/passive status only: refresh the counters, nothing to classify. */
    CanHealth_SyncHardwareCounters(driver->health, tec, rec, false);
    return;
  }

  /* TEC grows on transmit errors and REC on receive errors; fall back to
     transmit because a missing ACK is the common bench failure. */
  if (tec > driver->health->tec)
  {
    transmit = true;
  }
  else if (rec > driver->health->rec)
  {
    transmit = false;
  }
  else
  {
    transmit = true;
  }

  CanHealth_RecordError(driver->health, CanHealth_KindFromErrorFlags(error_code), transmit);
  CanHealth_SyncHardwareCounters(driver->health, tec, rec, false);
}

HAL_StatusTypeDef CanDriver_Recover(CanDriver_t *driver)
{
  if ((driver == NULL) || (driver->handle == NULL))
  {
    return HAL_ERROR;
  }

  (void)HAL_CAN_DeInit(driver->handle);

  if (CanDriver_Init(driver,
                     driver->handle,
                     driver->prescaler,
                     driver->time_seg1,
                     driver->time_seg2,
                     driver->filter_bank,
                     driver->rx_callback) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return CanDriver_Start(driver);
}
