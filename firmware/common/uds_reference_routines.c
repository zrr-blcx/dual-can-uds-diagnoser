#include "uds_reference_routines.h"

#include <stddef.h>

void UdsRefRoutine_Init(UdsRefRoutineContext_t *context,
                        uint8_t polls_until_complete,
                        uint8_t result)
{
  if (context == NULL)
  {
    return;
  }

  context->polls = 0U;
  context->polls_until_complete = (polls_until_complete == 0U)
                                      ? 1U
                                      : polls_until_complete;
  context->result = result;
}

UdsRoutineStatus_t UdsRefRoutine_Handler(uint8_t subfunction,
                                         const uint8_t *request_data,
                                         uint16_t request_length,
                                         uint8_t *result,
                                         uint16_t result_capacity,
                                         uint16_t *result_length,
                                         void *context)
{
  UdsRefRoutineContext_t *routine = (UdsRefRoutineContext_t *)context;

  (void)request_data;
  (void)request_length;

  if ((routine == NULL) || (result_length == NULL))
  {
    return UDS_ROUTINE_FAILED;
  }

  *result_length = 0U;

  if (subfunction == UDS_ROUTINE_START)
  {
    routine->polls = 0U;
    return UDS_ROUTINE_RUNNING;
  }

  if (subfunction == UDS_ROUTINE_STOP)
  {
    routine->polls = 0U;
    if (result_capacity > 0U)
    {
      result[0] = routine->result;
      *result_length = 1U;
    }
    return UDS_ROUTINE_IDLE;
  }

  if (subfunction != UDS_ROUTINE_REQUEST_RESULTS)
  {
    return UDS_ROUTINE_FAILED;
  }

  if (routine->polls < routine->polls_until_complete)
  {
    routine->polls++;
  }

  if (routine->polls < routine->polls_until_complete)
  {
    return UDS_ROUTINE_RUNNING;
  }

  if (result_capacity > 0U)
  {
    result[0] = routine->result;
    *result_length = 1U;
  }
  return UDS_ROUTINE_COMPLETE;
}
