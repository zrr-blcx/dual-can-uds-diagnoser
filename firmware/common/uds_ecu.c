#include "uds_ecu.h"

#include <stddef.h>

#define UDS_ROUTINE_ID_SELF_TEST 0x0203U
#define UDS_ROUTINE_ID_ERASE 0xFF00U
#define UDS_ECU_ROUTINE_COUNT 2U
#define UDS_REF_SELF_TEST_POLLS 2U
#define UDS_REF_ERASE_POLLS 2U

void UdsEcu_Init(UdsEcu_t *ecu, UdsEcuVariant_t variant)
{
  UdsServerConfig_t config;
  UdsRoutine_t self_test;
  UdsRoutine_t erase;

  if (ecu == NULL)
  {
    return;
  }

  UdsDidDb_Init(&ecu->did_db, variant);
  UdsRefRoutine_Init(&ecu->self_test_context, UDS_REF_SELF_TEST_POLLS, 0x00U);
  UdsRefRoutine_Init(&ecu->erase_context, UDS_REF_ERASE_POLLS, 0x00U);

  self_test.routine_id = UDS_ROUTINE_ID_SELF_TEST;
  self_test.sessions_mask = UDS_EXTENDED_PROGRAMMING_MASK;
  self_test.requires_unlock = false;
  self_test.handler = UdsRefRoutine_Handler;
  self_test.context = &ecu->self_test_context;
  ecu->routines[0] = self_test;

  erase.routine_id = UDS_ROUTINE_ID_ERASE;
  erase.sessions_mask = UDS_EXTENDED_PROGRAMMING_MASK;
  erase.requires_unlock = true;
  erase.handler = UdsRefRoutine_Handler;
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
