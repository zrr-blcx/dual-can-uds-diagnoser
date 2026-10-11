#ifndef UDS_ECU_H
#define UDS_ECU_H

#include <stdint.h>

#include "uds_did_db.h"
#include "uds_reference_routines.h"
#include "uds_server.h"

typedef struct
{
  UdsServer_t server;
  UdsDidDb_t did_db;
  UdsRefRoutineContext_t self_test_context;
  UdsRefRoutineContext_t erase_context;
  UdsRoutine_t routines[2];
} UdsEcu_t;

void UdsEcu_Init(UdsEcu_t *ecu, UdsEcuVariant_t variant);

void UdsEcu_Tick(UdsEcu_t *ecu, uint32_t now_ms);

UdsProcessResult_t UdsEcu_Process(UdsEcu_t *ecu,
                                  uint32_t now_ms,
                                  const uint8_t *request,
                                  uint16_t request_length,
                                  uint8_t *response,
                                  uint16_t response_capacity,
                                  uint16_t *response_length);

#endif /* UDS_ECU_H */
