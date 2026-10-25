#ifndef UDS_ECU_H
#define UDS_ECU_H

#include <stdbool.h>
#include <stdint.h>

#include "uds_did_db.h"
#include "uds_server.h"

#define UDS_ECU_SELFTEST_PARAM_FAILURE 0x01U
#define UDS_ECU_SELFTEST_CAN_FAILURE 0x02U
#define UDS_ECU_SELFTEST_WATCHDOG_FAILURE 0x04U

typedef uint8_t (*UdsEcuSelfTestHandler_t)(void *context);
typedef bool (*UdsEcuEraseParamsHandler_t)(void *context);

typedef struct
{
  UdsEcuSelfTestHandler_t self_test;
  UdsEcuEraseParamsHandler_t erase_params;
  void *context;
} UdsEcuHooks_t;

typedef struct
{
  void *owner;
  uint8_t polls;
  bool executed;
  uint8_t result;
} UdsEcuRoutineContext_t;

typedef struct
{
  UdsServer_t server;
  UdsDidDb_t did_db;
  UdsEcuVariant_t variant;
  UdsDidStorage_t storage;
  bool has_storage;
  UdsEcuHooks_t hooks;
  UdsEcuRoutineContext_t self_test_context;
  UdsEcuRoutineContext_t erase_context;
  UdsRoutine_t routines[2];
} UdsEcu_t;

void UdsEcu_Init(UdsEcu_t *ecu, UdsEcuVariant_t variant);

void UdsEcu_InitWithStorage(UdsEcu_t *ecu,
                            UdsEcuVariant_t variant,
                            const UdsDidStorage_t *storage);

void UdsEcu_InitWithHooks(UdsEcu_t *ecu,
                          UdsEcuVariant_t variant,
                          const UdsDidStorage_t *storage,
                          const UdsEcuHooks_t *hooks);

void UdsEcu_Tick(UdsEcu_t *ecu, uint32_t now_ms);

UdsProcessResult_t UdsEcu_Process(UdsEcu_t *ecu,
                                  uint32_t now_ms,
                                  const uint8_t *request,
                                  uint16_t request_length,
                                  uint8_t *response,
                                  uint16_t response_capacity,
                                  uint16_t *response_length);

UdsProcessResult_t UdsEcu_PollPending(UdsEcu_t *ecu,
                                      uint32_t now_ms,
                                      uint8_t *response,
                                      uint16_t response_capacity,
                                      uint16_t *response_length);

#endif /* UDS_ECU_H */
