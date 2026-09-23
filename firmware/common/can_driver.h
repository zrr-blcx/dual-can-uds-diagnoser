#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

#include <stdint.h>

#if defined(STM32F103xB)
#include "stm32f1xx_hal.h"
#elif defined(STM32F407xx)
#include "stm32f4xx_hal.h"
#else
#error "Unsupported STM32 family for CAN driver"
#endif

typedef void (*CanDriverRxCallback_t)(uint32_t id, const uint8_t *data, uint32_t len);

typedef struct
{
  CAN_HandleTypeDef *handle;
  CanDriverRxCallback_t rx_callback;
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

#endif /* CAN_DRIVER_H */
