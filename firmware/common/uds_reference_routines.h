#ifndef UDS_REFERENCE_ROUTINES_H
#define UDS_REFERENCE_ROUTINES_H

#include <stdint.h>

#include "uds_server.h"

typedef struct
{
  uint8_t polls;
  uint8_t polls_until_complete;
  uint8_t result;
} UdsRefRoutineContext_t;

void UdsRefRoutine_Init(UdsRefRoutineContext_t *context,
                        uint8_t polls_until_complete,
                        uint8_t result);

UdsRoutineStatus_t UdsRefRoutine_Handler(uint8_t subfunction,
                                         const uint8_t *request_data,
                                         uint16_t request_length,
                                         uint8_t *result,
                                         uint16_t result_capacity,
                                         uint16_t *result_length,
                                         void *context);

#endif /* UDS_REFERENCE_ROUTINES_H */
