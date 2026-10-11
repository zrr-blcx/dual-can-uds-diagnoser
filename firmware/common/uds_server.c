#include "uds_server.h"

#include <stddef.h>

static bool UdsServer_TimeReached(uint32_t now_ms, uint32_t deadline_ms)
{
  return (uint32_t)(now_ms - deadline_ms) < 0x80000000U;
}

static UdsDid_t *UdsServer_FindDid(UdsServer_t *server, uint16_t did)
{
  uint16_t index;

  for (index = 0U; index < server->did_count; index++)
  {
    if (server->dids[index].did == did)
    {
      return &server->dids[index];
    }
  }

  return NULL;
}

static uint8_t UdsServer_FindRoutineIndex(const UdsServer_t *server, uint16_t routine_id)
{
  uint16_t index;

  for (index = 0U; index < server->routine_count; index++)
  {
    if (server->routines[index].routine_id == routine_id)
    {
      return (uint8_t)index;
    }
  }

  return UDS_MAX_ROUTINES;
}

static UdsProcessResult_t UdsServer_Negative(uint8_t *response,
                                             uint16_t response_capacity,
                                             uint16_t *response_length,
                                             uint8_t service,
                                             uint8_t nrc)
{
  if ((response == NULL) || (response_length == NULL))
  {
    return UDS_PROCESS_INVALID_ARGUMENT;
  }

  if (response_capacity < 3U)
  {
    *response_length = 0U;
    return UDS_PROCESS_BUFFER_TOO_SMALL;
  }

  response[0] = UDS_SID_NEGATIVE_RESPONSE;
  response[1] = service;
  response[2] = nrc;
  *response_length = 3U;
  return UDS_PROCESS_RESPONSE;
}

static UdsProcessResult_t UdsServer_ResponsePending(uint8_t *response,
                                                    uint16_t response_capacity,
                                                    uint16_t *response_length,
                                                    uint8_t service)
{
  UdsProcessResult_t result;

  result = UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              service,
                              UDS_NRC_RESPONSE_PENDING);
  if (result == UDS_PROCESS_RESPONSE)
  {
    return UDS_PROCESS_RESPONSE_PENDING;
  }

  return result;
}

static bool UdsServer_SessionAllowed(uint8_t sessions_mask, uint8_t session)
{
  if (sessions_mask == 0U)
  {
    return true;
  }

  if ((session == 0U) || (session > 7U))
  {
    return false;
  }

  return (sessions_mask & (uint8_t)(1U << session)) != 0U;
}

static void UdsServer_LockSecurityState(UdsServer_t *server)
{
  server->security_unlocked = false;
  server->seed_pending = false;
  server->pending_seed = 0U;
  server->security_attempts = 0U;
}

static bool UdsServer_SecurityDelayActive(UdsServer_t *server, uint32_t now_ms)
{
  if (server->security_delay_until_ms == 0U)
  {
    return false;
  }

  if (UdsServer_TimeReached(now_ms, server->security_delay_until_ms))
  {
    server->security_delay_until_ms = 0U;
    server->security_attempts = 0U;
    return false;
  }

  return true;
}

static void UdsServer_SetSession(UdsServer_t *server,
                                 uint8_t session,
                                 uint32_t now_ms)
{
  if (session != server->session)
  {
    UdsServer_LockSecurityState(server);
  }

  server->session = session;

  if (session == UDS_SESSION_DEFAULT)
  {
    server->s3_active = false;
    server->s3_deadline_ms = 0U;
    return;
  }

  server->s3_active = true;
  server->s3_deadline_ms = now_ms + server->s3_server_ms;
}

static void UdsServer_KeepSessionAlive(UdsServer_t *server, uint32_t now_ms)
{
  if (server->s3_active)
  {
    server->s3_deadline_ms = now_ms + server->s3_server_ms;
  }
}

static UdsProcessResult_t UdsServer_SessionControl(UdsServer_t *server,
                                                   uint32_t now_ms,
                                                   const uint8_t *request,
                                                   uint16_t request_length,
                                                   uint8_t *response,
                                                   uint16_t response_capacity,
                                                   uint16_t *response_length)
{
  uint8_t subfunction;
  uint8_t requested_session;
  bool suppress_positive_response;

  if (request_length != 2U)
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_DIAGNOSTIC_SESSION_CONTROL,
                              UDS_NRC_INCORRECT_LENGTH);
  }

  subfunction = request[1];
  suppress_positive_response = (subfunction & UDS_SUPPRESS_POSITIVE_RESPONSE) != 0U;
  requested_session = subfunction & UDS_SUBFUNCTION_MASK;

  if ((requested_session < UDS_SESSION_DEFAULT) ||
      (requested_session > UDS_SESSION_EXTENDED) ||
      (!UdsServer_SessionAllowed(server->supported_sessions_mask, requested_session)))
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_DIAGNOSTIC_SESSION_CONTROL,
                              UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
  }

  UdsServer_SetSession(server, requested_session, now_ms);

  if (suppress_positive_response)
  {
    *response_length = 0U;
    return UDS_PROCESS_NO_RESPONSE;
  }

  if (response_capacity < 6U)
  {
    *response_length = 0U;
    return UDS_PROCESS_BUFFER_TOO_SMALL;
  }

  response[0] = (uint8_t)(UDS_SID_DIAGNOSTIC_SESSION_CONTROL + 0x40U);
  response[1] = requested_session;
  response[2] = (uint8_t)(server->p2_server_ms >> 8U);
  response[3] = (uint8_t)(server->p2_server_ms & 0x00FFU);
  response[4] = (uint8_t)((server->p2_star_server_ms / 10U) >> 8U);
  response[5] = (uint8_t)((server->p2_star_server_ms / 10U) & 0x00FFU);
  *response_length = 6U;
  return UDS_PROCESS_RESPONSE;
}

static UdsProcessResult_t UdsServer_ReadDid(UdsServer_t *server,
                                            const uint8_t *request,
                                            uint16_t request_length,
                                            uint8_t *response,
                                            uint16_t response_capacity,
                                            uint16_t *response_length)
{
  uint16_t request_index;
  uint16_t response_index = 1U;
  uint32_t required_length = 1U;

  if ((request_length < 3U) || (((request_length - 1U) & 1U) != 0U))
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_READ_DATA_BY_IDENTIFIER,
                              UDS_NRC_INCORRECT_LENGTH);
  }

  for (request_index = 1U; request_index < request_length; request_index += 2U)
  {
    uint16_t did = (uint16_t)(((uint16_t)request[request_index] << 8U) |
                              request[request_index + 1U]);
    UdsDid_t *entry = UdsServer_FindDid(server, did);

    if ((entry == NULL) ||
        (!UdsServer_SessionAllowed(entry->read_sessions_mask, server->session)))
    {
      return UdsServer_Negative(response,
                                response_capacity,
                                response_length,
                                UDS_SID_READ_DATA_BY_IDENTIFIER,
                                (entry == NULL) ? UDS_NRC_REQUEST_OUT_OF_RANGE
                                                : UDS_NRC_SUBFUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION);
    }

    required_length += 3U + entry->length;
  }

  if (required_length > response_capacity)
  {
    *response_length = 0U;
    return UDS_PROCESS_BUFFER_TOO_SMALL;
  }

  response[0] = (uint8_t)(UDS_SID_READ_DATA_BY_IDENTIFIER + 0x40U);

  for (request_index = 1U; request_index < request_length; request_index += 2U)
  {
    uint16_t did = (uint16_t)(((uint16_t)request[request_index] << 8U) |
                              request[request_index + 1U]);
    UdsDid_t *entry = UdsServer_FindDid(server, did);
    uint16_t data_index;

    response[response_index] = request[request_index];
    response[response_index + 1U] = request[request_index + 1U];
    response_index += 2U;

    for (data_index = 0U; data_index < entry->length; data_index++)
    {
      response[response_index] = entry->data[data_index];
      response_index++;
    }
  }

  *response_length = response_index;
  return UDS_PROCESS_RESPONSE;
}

static UdsProcessResult_t UdsServer_SecurityAccess(UdsServer_t *server,
                                                   uint32_t now_ms,
                                                   const uint8_t *request,
                                                   uint16_t request_length,
                                                   uint8_t *response,
                                                   uint16_t response_capacity,
                                                   uint16_t *response_length)
{
  uint8_t subfunction;
  bool suppress_positive_response;
  uint32_t key;
  uint32_t expected_key;
  uint16_t index;

  if (request_length < 2U)
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_SECURITY_ACCESS,
                              UDS_NRC_INCORRECT_LENGTH);
  }

  subfunction = request[1] & UDS_SUBFUNCTION_MASK;
  suppress_positive_response = (request[1] & UDS_SUPPRESS_POSITIVE_RESPONSE) != 0U;

  if ((subfunction != UDS_SECURITY_REQUEST_SEED) &&
      (subfunction != UDS_SECURITY_SEND_KEY))
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_SECURITY_ACCESS,
                              UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
  }

  if ((server->session != UDS_SESSION_EXTENDED) &&
      (server->session != UDS_SESSION_PROGRAMMING))
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_SECURITY_ACCESS,
                              UDS_NRC_SUBFUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION);
  }

  if (UdsServer_SecurityDelayActive(server, now_ms))
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_SECURITY_ACCESS,
                              UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED);
  }

  if (subfunction == UDS_SECURITY_REQUEST_SEED)
  {
    uint32_t seed;

    if (server->security_unlocked)
    {
      seed = 0U;
    }
    else
    {
      seed = UdsSecurity_NextSeed(&server->seed_state);
      server->pending_seed = seed;
      server->seed_pending = true;
    }

    if (suppress_positive_response)
    {
      *response_length = 0U;
      return UDS_PROCESS_NO_RESPONSE;
    }

    if (response_capacity < (2U + UDS_SECURITY_SEED_SIZE))
    {
      *response_length = 0U;
      return UDS_PROCESS_BUFFER_TOO_SMALL;
    }

    response[0] = (uint8_t)(UDS_SID_SECURITY_ACCESS + 0x40U);
    response[1] = UDS_SECURITY_REQUEST_SEED;

    for (index = 0U; index < UDS_SECURITY_SEED_SIZE; index++)
    {
      response[2U + index] =
          (uint8_t)(seed >> (8U * (uint8_t)(UDS_SECURITY_SEED_SIZE - 1U - index)));
    }

    *response_length = (uint16_t)(2U + UDS_SECURITY_SEED_SIZE);
    return UDS_PROCESS_RESPONSE;
  }

  if (request_length != 6U)
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_SECURITY_ACCESS,
                              UDS_NRC_INCORRECT_LENGTH);
  }

  if (!server->seed_pending)
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_SECURITY_ACCESS,
                              UDS_NRC_REQUEST_SEQUENCE_ERROR);
  }

  key = ((uint32_t)request[2] << 24U) |
        ((uint32_t)request[3] << 16U) |
        ((uint32_t)request[4] << 8U) |
        (uint32_t)request[5];
  expected_key = UdsSecurity_ComputeKey(server->pending_seed, server->security_salt);
  server->seed_pending = false;
  server->pending_seed = 0U;

  if (key != expected_key)
  {
    server->security_attempts++;
    if (server->security_attempts >= UDS_SECURITY_MAX_ATTEMPTS)
    {
      server->security_attempts = 0U;
      server->security_delay_until_ms = now_ms + UDS_SECURITY_DELAY_MS;
      return UdsServer_Negative(response,
                                response_capacity,
                                response_length,
                                UDS_SID_SECURITY_ACCESS,
                                UDS_NRC_EXCEED_NUMBER_OF_ATTEMPTS);
    }

    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_SECURITY_ACCESS,
                              UDS_NRC_INVALID_KEY);
  }

  server->security_unlocked = true;
  server->security_attempts = 0U;
  server->security_delay_until_ms = 0U;

  if (suppress_positive_response)
  {
    *response_length = 0U;
    return UDS_PROCESS_NO_RESPONSE;
  }

  if (response_capacity < 2U)
  {
    *response_length = 0U;
    return UDS_PROCESS_BUFFER_TOO_SMALL;
  }

  response[0] = (uint8_t)(UDS_SID_SECURITY_ACCESS + 0x40U);
  response[1] = UDS_SECURITY_SEND_KEY;
  *response_length = 2U;
  return UDS_PROCESS_RESPONSE;
}

static UdsProcessResult_t UdsServer_WriteDid(UdsServer_t *server,
                                             const uint8_t *request,
                                             uint16_t request_length,
                                             uint8_t *response,
                                             uint16_t response_capacity,
                                             uint16_t *response_length)
{
  uint16_t did;
  uint16_t data_length;
  uint16_t index;
  UdsDid_t *entry;

  if (request_length < 3U)
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_WRITE_DATA_BY_IDENTIFIER,
                              UDS_NRC_INCORRECT_LENGTH);
  }

  did = (uint16_t)(((uint16_t)request[1] << 8U) | request[2]);
  entry = UdsServer_FindDid(server, did);

  if ((entry == NULL) || (entry->storage == NULL) || (entry->write_sessions_mask == 0U))
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_WRITE_DATA_BY_IDENTIFIER,
                              UDS_NRC_REQUEST_OUT_OF_RANGE);
  }

  if (!UdsServer_SessionAllowed(entry->write_sessions_mask, server->session))
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_WRITE_DATA_BY_IDENTIFIER,
                              UDS_NRC_SUBFUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION);
  }

  if (entry->write_requires_unlock && (!server->security_unlocked))
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_WRITE_DATA_BY_IDENTIFIER,
                              UDS_NRC_SECURITY_ACCESS_DENIED);
  }

  if ((entry->write_required_session != 0U) &&
      (server->session != entry->write_required_session))
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_WRITE_DATA_BY_IDENTIFIER,
                              UDS_NRC_CONDITIONS_NOT_CORRECT);
  }

  data_length = (uint16_t)(request_length - 3U);
  if (data_length > entry->storage_capacity)
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_WRITE_DATA_BY_IDENTIFIER,
                              UDS_NRC_REQUEST_OUT_OF_RANGE);
  }

  for (index = 0U; index < data_length; index++)
  {
    entry->storage[index] = request[3U + index];
  }
  entry->data = entry->storage;
  entry->length = data_length;

  if (response_capacity < 3U)
  {
    *response_length = 0U;
    return UDS_PROCESS_BUFFER_TOO_SMALL;
  }

  response[0] = (uint8_t)(UDS_SID_WRITE_DATA_BY_IDENTIFIER + 0x40U);
  response[1] = (uint8_t)(did >> 8U);
  response[2] = (uint8_t)(did & 0x00FFU);
  *response_length = 3U;
  return UDS_PROCESS_RESPONSE;
}

static void UdsServer_StoreRoutineResult(UdsServer_t *server,
                                         uint8_t routine_index,
                                         const uint8_t *result,
                                         uint16_t result_length)
{
  uint16_t index;

  if (result_length > UDS_ROUTINE_RESULT_MAX)
  {
    result_length = UDS_ROUTINE_RESULT_MAX;
  }

  for (index = 0U; index < result_length; index++)
  {
    server->routine_result[routine_index][index] = result[index];
  }
  server->routine_result_length[routine_index] = result_length;
}

static UdsProcessResult_t UdsServer_RoutinePositive(uint8_t subfunction,
                                                    uint16_t routine_id,
                                                    const uint8_t *result,
                                                    uint16_t result_length,
                                                    bool suppress_positive_response,
                                                    uint8_t *response,
                                                    uint16_t response_capacity,
                                                    uint16_t *response_length)
{
  uint16_t index;

  if (suppress_positive_response)
  {
    *response_length = 0U;
    return UDS_PROCESS_NO_RESPONSE;
  }

  if ((uint32_t)4U + (uint32_t)result_length > (uint32_t)response_capacity)
  {
    *response_length = 0U;
    return UDS_PROCESS_BUFFER_TOO_SMALL;
  }

  response[0] = (uint8_t)(UDS_SID_ROUTINE_CONTROL + 0x40U);
  response[1] = subfunction;
  response[2] = (uint8_t)(routine_id >> 8U);
  response[3] = (uint8_t)(routine_id & 0x00FFU);
  for (index = 0U; index < result_length; index++)
  {
    response[4U + index] = result[index];
  }

  *response_length = (uint16_t)(4U + result_length);
  return UDS_PROCESS_RESPONSE;
}

static UdsProcessResult_t UdsServer_RoutineControl(UdsServer_t *server,
                                                   const uint8_t *request,
                                                   uint16_t request_length,
                                                   uint8_t *response,
                                                   uint16_t response_capacity,
                                                   uint16_t *response_length)
{
  uint8_t subfunction;
  bool suppress_positive_response;
  uint16_t routine_id;
  uint8_t routine_index;
  UdsRoutineStatus_t status;
  uint8_t result[UDS_ROUTINE_RESULT_MAX];
  uint16_t result_length = 0U;

  if (request_length < 4U)
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_ROUTINE_CONTROL,
                              UDS_NRC_INCORRECT_LENGTH);
  }

  subfunction = request[1] & UDS_SUBFUNCTION_MASK;
  suppress_positive_response = (request[1] & UDS_SUPPRESS_POSITIVE_RESPONSE) != 0U;
  routine_id = (uint16_t)(((uint16_t)request[2] << 8U) | request[3]);
  routine_index = UdsServer_FindRoutineIndex(server, routine_id);

  if (routine_index >= UDS_MAX_ROUTINES)
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_ROUTINE_CONTROL,
                              UDS_NRC_REQUEST_OUT_OF_RANGE);
  }

  if ((subfunction != UDS_ROUTINE_START) &&
      (subfunction != UDS_ROUTINE_STOP) &&
      (subfunction != UDS_ROUTINE_REQUEST_RESULTS))
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_ROUTINE_CONTROL,
                              UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
  }

  if (!UdsServer_SessionAllowed(server->routines[routine_index].sessions_mask,
                                server->session))
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_ROUTINE_CONTROL,
                              UDS_NRC_SUBFUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION);
  }

  if (server->routines[routine_index].requires_unlock &&
      (!server->security_unlocked))
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_ROUTINE_CONTROL,
                              UDS_NRC_SECURITY_ACCESS_DENIED);
  }

  if (server->routines[routine_index].handler == NULL)
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_ROUTINE_CONTROL,
                              UDS_NRC_REQUEST_OUT_OF_RANGE);
  }

  if (subfunction == UDS_ROUTINE_START)
  {
    if (server->routine_state[routine_index] == (uint8_t)UDS_ROUTINE_RUNNING)
    {
      return UdsServer_Negative(response,
                                response_capacity,
                                response_length,
                                UDS_SID_ROUTINE_CONTROL,
                                UDS_NRC_CONDITIONS_NOT_CORRECT);
    }

    status = server->routines[routine_index].handler(
        UDS_ROUTINE_START,
        &request[4],
        (uint16_t)(request_length - 4U),
        result,
        (uint16_t)sizeof(result),
        &result_length,
        server->routines[routine_index].context);
    server->routine_state[routine_index] = (uint8_t)status;
    UdsServer_StoreRoutineResult(server, routine_index, result, result_length);
    return UdsServer_RoutinePositive(subfunction,
                                     routine_id,
                                     result,
                                     result_length,
                                     suppress_positive_response,
                                     response,
                                     response_capacity,
                                     response_length);
  }

  if (subfunction == UDS_ROUTINE_STOP)
  {
    if (server->routine_state[routine_index] != (uint8_t)UDS_ROUTINE_RUNNING)
    {
      return UdsServer_Negative(response,
                                response_capacity,
                                response_length,
                                UDS_SID_ROUTINE_CONTROL,
                                UDS_NRC_REQUEST_SEQUENCE_ERROR);
    }

    status = server->routines[routine_index].handler(
        UDS_ROUTINE_STOP,
        &request[4],
        (uint16_t)(request_length - 4U),
        result,
        (uint16_t)sizeof(result),
        &result_length,
        server->routines[routine_index].context);
    server->routine_state[routine_index] = (uint8_t)status;
    UdsServer_StoreRoutineResult(server, routine_index, result, result_length);
    return UdsServer_RoutinePositive(subfunction,
                                     routine_id,
                                     result,
                                     result_length,
                                     suppress_positive_response,
                                     response,
                                     response_capacity,
                                     response_length);
  }

  if (server->routine_state[routine_index] == (uint8_t)UDS_ROUTINE_IDLE)
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_ROUTINE_CONTROL,
                              UDS_NRC_REQUEST_SEQUENCE_ERROR);
  }

  if ((server->routine_state[routine_index] == (uint8_t)UDS_ROUTINE_COMPLETE) ||
      (server->routine_state[routine_index] == (uint8_t)UDS_ROUTINE_FAILED))
  {
    return UdsServer_RoutinePositive(
        subfunction,
        routine_id,
        server->routine_result[routine_index],
        server->routine_result_length[routine_index],
        suppress_positive_response,
        response,
        response_capacity,
        response_length);
  }

  status = server->routines[routine_index].handler(
      UDS_ROUTINE_REQUEST_RESULTS,
      &request[4],
      (uint16_t)(request_length - 4U),
      result,
      (uint16_t)sizeof(result),
      &result_length,
      server->routines[routine_index].context);
  server->routine_state[routine_index] = (uint8_t)status;
  UdsServer_StoreRoutineResult(server, routine_index, result, result_length);

  if (status == UDS_ROUTINE_RUNNING)
  {
    return UdsServer_ResponsePending(response,
                                     response_capacity,
                                     response_length,
                                     UDS_SID_ROUTINE_CONTROL);
  }

  return UdsServer_RoutinePositive(subfunction,
                                   routine_id,
                                   result,
                                   result_length,
                                   suppress_positive_response,
                                   response,
                                   response_capacity,
                                   response_length);
}

static UdsProcessResult_t UdsServer_TesterPresent(UdsServer_t *server,
                                                  uint32_t now_ms,
                                                  const uint8_t *request,
                                                  uint16_t request_length,
                                                  uint8_t *response,
                                                  uint16_t response_capacity,
                                                  uint16_t *response_length)
{
  uint8_t subfunction;

  if (request_length != 2U)
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_TESTER_PRESENT,
                              UDS_NRC_INCORRECT_LENGTH);
  }

  subfunction = request[1];
  if ((subfunction & UDS_SUBFUNCTION_MASK) != 0U)
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              UDS_SID_TESTER_PRESENT,
                              UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
  }

  UdsServer_KeepSessionAlive(server, now_ms);

  if ((subfunction & UDS_SUPPRESS_POSITIVE_RESPONSE) != 0U)
  {
    *response_length = 0U;
    return UDS_PROCESS_NO_RESPONSE;
  }

  if (response_capacity < 2U)
  {
    *response_length = 0U;
    return UDS_PROCESS_BUFFER_TOO_SMALL;
  }

  response[0] = (uint8_t)(UDS_SID_TESTER_PRESENT + 0x40U);
  response[1] = 0x00U;
  *response_length = 2U;
  return UDS_PROCESS_RESPONSE;
}

uint32_t UdsSecurity_ComputeKey(uint32_t seed, uint32_t salt)
{
  uint32_t key = seed ^ salt;

  key = (key << 13U) | (key >> 19U);
  key = key + 0x9E3779B9U;
  key = key ^ (key >> 16U);
  return key;
}

uint32_t UdsSecurity_NextSeed(uint32_t *state)
{
  uint32_t value;

  if (state == NULL)
  {
    return UDS_DEFAULT_SEED;
  }

  value = *state;
  if (value == 0U)
  {
    value = UDS_DEFAULT_SEED;
  }

  value ^= value << 13U;
  value ^= value >> 17U;
  value ^= value << 5U;
  *state = value;

  if ((value == 0U) || (value == 0xFFFFFFFFU))
  {
    return UDS_DEFAULT_SEED;
  }

  return value;
}

bool UdsServer_Init(UdsServer_t *server, const UdsServerConfig_t *config)
{
  uint16_t index;

  if ((server == NULL) || (config == NULL))
  {
    return false;
  }

  if ((config->did_count > UDS_MAX_DIDS) ||
      (config->routine_count > UDS_MAX_ROUTINES))
  {
    return false;
  }

  if ((config->did_count > 0U) && (config->dids == NULL))
  {
    return false;
  }

  if ((config->routine_count > 0U) && (config->routines == NULL))
  {
    return false;
  }

  for (index = 0U; index < config->did_count; index++)
  {
    uint16_t previous;

    if ((config->dids[index].length > 0U) && (config->dids[index].data == NULL))
    {
      return false;
    }

    if ((config->dids[index].storage_capacity > 0U) &&
        (config->dids[index].storage == NULL))
    {
      return false;
    }

    for (previous = 0U; previous < index; previous++)
    {
      if (config->dids[previous].did == config->dids[index].did)
      {
        return false;
      }
    }

    server->dids[index] = config->dids[index];
  }

  for (index = 0U; index < config->routine_count; index++)
  {
    uint16_t previous;

    if (config->routines[index].handler == NULL)
    {
      return false;
    }

    for (previous = 0U; previous < index; previous++)
    {
      if (config->routines[previous].routine_id == config->routines[index].routine_id)
      {
        return false;
      }
    }

    server->routines[index] = config->routines[index];
  }

  server->did_count = config->did_count;
  server->routine_count = config->routine_count;
  server->p2_server_ms =
      (config->p2_server_ms == 0U) ? UDS_DEFAULT_P2_SERVER_MS : config->p2_server_ms;
  server->p2_star_server_ms = (config->p2_star_server_ms == 0U)
                                  ? UDS_DEFAULT_P2_STAR_SERVER_MS
                                  : config->p2_star_server_ms;
  server->s3_server_ms =
      (config->s3_server_ms == 0U) ? UDS_DEFAULT_S3_SERVER_MS : config->s3_server_ms;
  server->supported_sessions_mask = (config->supported_sessions_mask == 0U)
                                        ? UDS_DEFAULT_SESSION_MASK
                                        : config->supported_sessions_mask;
  server->security_salt =
      (config->security_salt == 0U) ? UDS_DEFAULT_SECURITY_SALT : config->security_salt;
  server->session = UDS_SESSION_DEFAULT;
  server->s3_active = false;
  server->s3_deadline_ms = 0U;
  server->session_timeout_count = 0U;
  server->seed_state = UDS_DEFAULT_SEED;
  server->pending_seed = 0U;
  server->seed_pending = false;
  server->security_unlocked = false;
  server->security_attempts = 0U;
  server->security_delay_until_ms = 0U;

  for (index = 0U; index < UDS_MAX_ROUTINES; index++)
  {
    server->routine_state[index] = (uint8_t)UDS_ROUTINE_IDLE;
    server->routine_result_length[index] = 0U;
  }

  return true;
}

void UdsServer_Tick(UdsServer_t *server, uint32_t now_ms)
{
  if (server == NULL)
  {
    return;
  }

  if (server->s3_active &&
      UdsServer_TimeReached(now_ms, server->s3_deadline_ms))
  {
    server->session = UDS_SESSION_DEFAULT;
    server->s3_active = false;
    server->s3_deadline_ms = 0U;
    server->session_timeout_count++;
    UdsServer_LockSecurityState(server);
  }
}

UdsProcessResult_t UdsServer_Process(UdsServer_t *server,
                                     uint32_t now_ms,
                                     const uint8_t *request,
                                     uint16_t request_length,
                                     uint8_t *response,
                                     uint16_t response_capacity,
                                     uint16_t *response_length)
{
  UdsProcessResult_t result;
  uint8_t service;

  if ((server == NULL) || (response == NULL) || (response_length == NULL) ||
      ((request == NULL) && (request_length > 0U)))
  {
    return UDS_PROCESS_INVALID_ARGUMENT;
  }

  *response_length = 0U;
  UdsServer_Tick(server, now_ms);

  if (request_length == 0U)
  {
    return UdsServer_Negative(response,
                              response_capacity,
                              response_length,
                              0U,
                              UDS_NRC_INCORRECT_LENGTH);
  }

  service = request[0];
  switch (service)
  {
    case UDS_SID_DIAGNOSTIC_SESSION_CONTROL:
      return UdsServer_SessionControl(server,
                                      now_ms,
                                      request,
                                      request_length,
                                      response,
                                      response_capacity,
                                      response_length);

    case UDS_SID_READ_DATA_BY_IDENTIFIER:
      result = UdsServer_ReadDid(server,
                                 request,
                                 request_length,
                                 response,
                                 response_capacity,
                                 response_length);
      break;

    case UDS_SID_SECURITY_ACCESS:
      result = UdsServer_SecurityAccess(server,
                                        now_ms,
                                        request,
                                        request_length,
                                        response,
                                        response_capacity,
                                        response_length);
      break;

    case UDS_SID_WRITE_DATA_BY_IDENTIFIER:
      result = UdsServer_WriteDid(server,
                                  request,
                                  request_length,
                                  response,
                                  response_capacity,
                                  response_length);
      break;

    case UDS_SID_ROUTINE_CONTROL:
      result = UdsServer_RoutineControl(server,
                                        request,
                                        request_length,
                                        response,
                                        response_capacity,
                                        response_length);
      break;

    case UDS_SID_TESTER_PRESENT:
      return UdsServer_TesterPresent(server,
                                     now_ms,
                                     request,
                                     request_length,
                                     response,
                                     response_capacity,
                                     response_length);

    default:
      return UdsServer_Negative(response,
                                response_capacity,
                                response_length,
                                service,
                                UDS_NRC_SERVICE_NOT_SUPPORTED);
  }

  if ((result == UDS_PROCESS_RESPONSE_PENDING) ||
      ((result == UDS_PROCESS_RESPONSE) && (*response_length > 0U) &&
       (response[0] != UDS_SID_NEGATIVE_RESPONSE)))
  {
    UdsServer_KeepSessionAlive(server, now_ms);
  }

  return result;
}

uint8_t UdsServer_GetSession(const UdsServer_t *server)
{
  return (server == NULL) ? UDS_SESSION_DEFAULT : server->session;
}

uint32_t UdsServer_GetSessionTimeoutCount(const UdsServer_t *server)
{
  return (server == NULL) ? 0U : server->session_timeout_count;
}

bool UdsServer_IsSecurityUnlocked(const UdsServer_t *server)
{
  return (server == NULL) ? false : server->security_unlocked;
}

uint8_t UdsServer_GetSecurityAttempts(const UdsServer_t *server)
{
  return (server == NULL) ? 0U : server->security_attempts;
}

uint8_t UdsServer_GetRoutineState(const UdsServer_t *server, uint16_t routine_id)
{
  uint8_t index;

  if (server == NULL)
  {
    return (uint8_t)UDS_ROUTINE_IDLE;
  }

  index = UdsServer_FindRoutineIndex(server, routine_id);
  if (index >= UDS_MAX_ROUTINES)
  {
    return (uint8_t)UDS_ROUTINE_IDLE;
  }

  return server->routine_state[index];
}

void UdsServer_LockSecurity(UdsServer_t *server)
{
  if (server != NULL)
  {
    UdsServer_LockSecurityState(server);
  }
}
