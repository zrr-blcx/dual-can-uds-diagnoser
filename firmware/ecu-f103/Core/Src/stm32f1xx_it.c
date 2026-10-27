#include "main.h"
#include "stm32f1xx_it.h"

extern CAN_HandleTypeDef hcan1;

void NMI_Handler(void)
{
  while (1)
  {
  }
}

void HardFault_Handler(void)
{
  while (1)
  {
  }
}

void SysTick_Handler(void)
{
  HAL_IncTick();
}

void CAN1_RX0_IRQHandler(void)
{
  HAL_CAN_IRQHandler(&hcan1);
}

void CAN1_RX1_IRQHandler(void)
{
  HAL_CAN_IRQHandler(&hcan1);
}

/* Error interrupts are enabled in CanDriver_Start(); HAL_CAN_IRQHandler ends up
   in HAL_CAN_ErrorCallback() -> CanDriver_OnErrorIsr(). */
void CAN1_SCE_IRQHandler(void)
{
  HAL_CAN_IRQHandler(&hcan1);
}
