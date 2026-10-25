#include "uds_ecu.h"

#include <stddef.h>

#define UDS_ROUTINE_ID_SELF_TEST 0x0203U
#define UDS_ROUTINE_ID_ERASE 0xFF00U
#define UDS_ECU_ROUTINE_COUNT 2U

void UdsEcu_Init(UdsEcu_t *ecu, UdsEcuVariant_t variant)
{
  UdsEcu_InitWithHooks(ecu, variant, NULL, NULL);
}

void UdsEcu_InitWithStorage(UdsEcu_t *ecu,
                            UdsEcuVariant_t variant,
                            const UdsDidStorage_t *storage)
{
  UdsEcu_InitWithHooks(ecu, variant, storage, NULL);
}

static void UdsEcu_CopyDidsToServer(UdsEcu_t *ecu)
{
  uint16_t index;

  ecu->server.did_count = ecu->did_db.did_count;
  for (index = 0U; index < ecu->did_db.did_count; index++)
  {
    ecu->server.dids[index] = ecu->did_db.dids[index];
  }
}

static void UdsEcu_ResetDids(UdsEcu_t *ecu)
{
  UdsDidDb_Init(&ecu->did_db, ecu->variant);
  if (ecu->has_storage)
  {
    UdsDidDb_AttachStorage(&ecu->did_db, &ecu->storage);
  }
  UdsEcu_CopyDidsToServer(ecu);
}

static UdsRoutineStatus_t UdsEcu_RoutineHandler(uint8_t subfunction,
                                                const uint8_t *request_data,
                                                uint16_t request_length,
                                                uint8_t *result,
                                                uint16_t result_capacity,
                                                uint16_t *result_length,
                                                void *context)
{
  UdsEcuRoutineContext_t *routine = (UdsEcuRoutineContext_t *)context;
  UdsEcu_t *ecu;

  (void)request_data;
  (void)request_length;
  if ((routine == NULL) || (result_length == NULL) ||
      (routine->owner == NULL))
  {
    return UDS_ROUTINE_FAILED;
  }
  ecu = (UdsEcu_t *)routine->owner;
  *result_length = 0U;

  if (subfunction == UDS_ROUTINE_START)
  {
    routine->polls = 0U;
    routine->executed = false;
    return UDS_ROUTINE_RUNNING;
  }

  if (subfunction == UDS_ROUTINE_STOP)
  {
    routine->polls = 0U;
    routine->executed = false;
    return UDS_ROUTINE_IDLE;
  }

  if (subfunction != UDS_ROUTINE_REQUEST_RESULTS)
  {
    return UDS_ROUTINE_FAILED;
  }

  if (!routine->executed)
  {
    if (routine->polls == 0U)
    {
      routine->polls = 1U;
      return UDS_ROUTINE_RUNNING;
    }

    if (routine == &ecu->self_test_context)
    {
      routine->result = (ecu->hooks.self_test == NULL)
                            ? 0U
                            : ecu->hooks.self_test(ecu->hooks.context);
    }
    else
    {
      bool erased = (ecu->hooks.erase_params == NULL)
                        ? true
                        : ecu->hooks.erase_params(ecu->hooks.context);
      routine->result = erased ? 0U : 1U;
      if (erased)
      {
        UdsEcu_ResetDids(ecu);
      }
    }
    routine->executed = true;
  }

  if (result_capacity > 0U)
  {
    result[0] = routine->result;
    *result_length = 1U;
  }
  return UDS_ROUTINE_COMPLETE;
}

void UdsEcu_InitWithHooks(UdsEcu_t *ecu,
                          UdsEcuVariant_t variant,
                          const UdsDidStorage_t *storage,
                          const UdsEcuHooks_t *hooks)
{
  UdsServerConfig_t config;
  UdsRoutine_t self_test;
  UdsRoutine_t erase;

  if (ecu == NULL)
  {
    return;
  }

  ecu->variant = variant;
  ecu->has_storage = (storage != NULL);
  if (storage != NULL)
  {
    ecu->storage = *storage;
  }
  if (hooks != NULL)
  {
    ecu->hooks = *hooks;
  }
  else
  {
    ecu->hooks.self_test = NULL;
    ecu->hooks.erase_params = NULL;
    ecu->hooks.context = NULL;
  }

  UdsDidDb_Init(&ecu->did_db, variant);
  UdsDidDb_AttachStorage(&ecu->did_db, storage);
  ecu->self_test_context.owner = ecu;
  ecu->self_test_context.polls = 0U;
  ecu->self_test_context.executed = false;
  ecu->self_test_context.result = 0U;
  ecu->erase_context.owner = ecu;
  ecu->erase_context.polls = 0U;
  ecu->erase_context.executed = false;
  ecu->erase_context.result = 0U;

  self_test.routine_id = UDS_ROUTINE_ID_SELF_TEST;
  self_test.sessions_mask = UDS_EXTENDED_PROGRAMMING_MASK;
  self_test.requires_unlock = false;
  self_test.handler = UdsEcu_RoutineHandler;
  self_test.context = &ecu->self_test_context;
  ecu->routines[0] = self_test;

  erase.routine_id = UDS_ROUTINE_ID_ERASE;
  erase.sessions_mask = UDS_EXTENDED_PROGRAMMING_MASK;
  erase.requires_unlock = true;
  erase.handler = UdsEcu_RoutineHandler;
  erase.context = &ecu->erase_context;
  ecu->routines[1] = erase;

  config.dids = ecu->did_db.dids;
  config.did_count = ecu->did_db.did_count;
  config.routines = ecu->routines;
  config.routine_count = UDS_ECU_ROUTINE_COUNT;
  config.p2_server_ms = UDS_DEFAULT_P2_SERVER_MS;
  config.p2_star_server_ms = UDS_DEFAULT_P2_STAR_SERVER_MS;
  config.s3_server_ms = UDS_DEFAULT_S3_SERVER_MS;
  config.supported_sessions_mask = UDS_DEFAULT_SESSION_MASK;
  config.security_salt = UDS_DEFAULT_SECURITY_SALT;

  (void)UdsServer_Init(&ecu->server, &config);
}

void UdsEcu_Tick(UdsEcu_t *ecu, uint32_t now_ms)
{
  if (ecu != NULL)
  {
    UdsServer_Tick(&ecu->server, now_ms);
  }
}

UdsProcessResult_t UdsEcu_Process(UdsEcu_t *ecu,
                                  uint32_t now_ms,
                                  const uint8_t *request,
                                  uint16_t request_length,
                                  uint8_t *response,
                                  uint16_t response_capacity,
                                  uint16_t *response_length)
{
  if (ecu == NULL)
  {
    return UDS_PROCESS_INVALID_ARGUMENT;
  }

  return UdsServer_Process(&ecu->server,
                           now_ms,
                           request,
                           request_length,
                           response,
                           response_capacity,
                           response_length);
}

UdsProcessResult_t UdsEcu_PollPending(UdsEcu_t *ecu,
                                      uint32_t now_ms,
                                      uint8_t *response,
                                      uint16_t response_capacity,
                                      uint16_t *response_length)
{
  if (ecu == NULL)
  {
    return UDS_PROCESS_INVALID_ARGUMENT;
  }

  return UdsServer_PollPending(&ecu->server,
                               now_ms,
                               response,
                               response_capacity,
                               response_length);
}
