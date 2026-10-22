#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#include "can_health.h"

#if defined(STM32F103xB)
#include "stm32f1xx_hal.h"
#elif defined(STM32F407xx)
#include "stm32f4xx_hal.h"
#else
#error "Unsupported STM32 family for CAN driver"
#endif

typedef void (*CanDriverRxCallback_t)(uint32_t id, const uint8_t *data, uint32_t len);

/* Bring-up trace: the error ISR records every interrupt it sees together with
   the raw HAL error code and the ESR register value, so the caller can compare
   "what the software counted" against "what the controller reported".
   Read `g_can_error_log` over SWD after a test. */
#define CAN_DRIVER_ERROR_LOG_SIZE 32U

typedef struct
{
  uint32_t sequence;
  uint32_t error_code;
  uint32_t esr;
  uint32_t tick;
} CanDriverErrorLogEntry_t;

typedef struct
{
  CanDriverErrorLogEntry_t entries[CAN_DRIVER_ERROR_LOG_SIZE];
  uint32_t next_index;
  uint32_t recorded;
} CanDriverErrorLog_t;

extern volatile CanDriverErrorLog_t g_can_error_log;

typedef struct
{
  CAN_HandleTypeDef *handle;
  CanDriverRxCallback_t rx_callback;
  CanHealth_t *health;
  uint32_t prescaler;
  uint32_t time_seg1;
  uint32_t time_seg2;
  uint32_t filter_bank;
} CanDriver_t;

HAL_StatusTypeDef CanDriver_Init(CanDriver_t *driver,
                                CAN_HandleTypeDef *handle,
                                uint32_t prescaler,
                                uint32_t time_seg1,
                                uint32_t time_seg2,
                                uint32_t filter_bank,
                                CanDriverRxCallback_t rx_callback);

HAL_StatusTypeDef CanDriver_Start(CanDriver_t *driver);

HAL_StatusTypeDef CanDriver_SendStd(CanDriver_t *driver,
                                    uint32_t std_id,
                                    const uint8_t *data,
                                    uint8_t len);

void CanDriver_RxIrqHandler(CanDriver_t *driver);

/* Wire the error/Bus-Off policy core; optional, but then errors are only
   counted by the HAL itself. */
void CanDriver_AttachHealth(CanDriver_t *driver, CanHealth_t *health);

/* Call from HAL_CAN_ErrorCallback: classifies the HAL error code, records the
   event, and reads TEC/REC/BOFF straight from the ESR register. */
void CanDriver_OnErrorIsr(CanDriver_t *driver);

/* Read the hardware error counters; returns false when the driver is unusable. */
bool CanDriver_GetErrorCounters(const CanDriver_t *driver,
                                uint16_t *tec,
                                uint16_t *rec,
                                bool *bus_off_active);

/* Leave Bus-Off: deinit, reinitialize with the stored parameters, and start
   again. Returns HAL_OK when the controller is back up. */
HAL_StatusTypeDef CanDriver_Recover(CanDriver_t *driver);

#endif /* CAN_DRIVER_H */
