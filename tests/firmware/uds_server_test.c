#include <stdint.h>
#include <stdio.h>

#include "uds_ecu.h"
#include "uds_server.h"

static const uint8_t vin[] = "P2TESTVIN00000001";
static const uint8_t hardware_version[] = "F407-HW-REV-A";
static const uint8_t custom_data[] = {0xDE, 0xAD, 0xBE, 0xEF};

static UdsDid_t dids[] = {
    {0xF190U, vin, (uint16_t)(sizeof(vin) - 1U), NULL, 0U, 0U, 0U, 0U, false},
    {0xF193U, hardware_version, (uint16_t)(sizeof(hardware_version) - 1U), NULL, 0U, 0U, 0U, 0U, false},
    {0x0100U, custom_data, (uint16_t)sizeof(custom_data), NULL, 0U, 0U, 0U, 0U, false},
};

static int failures = 0;

#define CHECK(condition)                                                          \
  do                                                                              \
  {                                                                               \
    if (!(condition))                                                             \
    {                                                                             \
      (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
      failures++;                                                                 \
    }                                                                             \
  } while (0)

static void expect_bytes(const uint8_t *actual,
                         uint16_t actual_length,
                         const uint8_t *expected,
                         uint16_t expected_length)
{
  uint16_t index;

  CHECK(actual_length == expected_length);
  if (actual_length != expected_length)
  {
    (void)fprintf(stderr,
                  "  byte length actual=%u expected=%u\n",
                  (unsigned)actual_length,
                  (unsigned)expected_length);
    return;
  }

  for (index = 0U; index < actual_length; index++)
  {
    CHECK(actual[index] == expected[index]);
  }
}

static UdsProcessResult_t run(UdsServer_t *server,
                              uint32_t now_ms,
                              const uint8_t *request,
                              uint16_t request_length,
                              uint8_t *response,
                              uint16_t response_capacity,
                              uint16_t *response_length)
{
  return UdsServer_Process(server,
                           now_ms,
                           request,
                           request_length,
                           response,
                           response_capacity,
                           response_length);
}

static void expect_negative(UdsServer_t *server,
                            uint32_t now_ms,
                            const uint8_t *request,
                            uint16_t request_length,
                            uint8_t service,
                            uint8_t nrc)
{
  uint8_t response[16] = {0};
  uint16_t response_length = 0U;

  CHECK(run(server,
            now_ms,
            request,
            request_length,
            response,
            (uint16_t)sizeof(response),
            &response_length) == UDS_PROCESS_RESPONSE);
  CHECK(response_length == 3U);
  CHECK(response[0] == UDS_SID_NEGATIVE_RESPONSE);
  CHECK(response[1] == service);
  CHECK(response[2] == nrc);
  if ((response_length != 3U) ||
      (response[0] != UDS_SID_NEGATIVE_RESPONSE) ||
      (response[1] != service) ||
      (response[2] != nrc))
  {
    (void)fprintf(stderr,
                  "  negative actual len=%u bytes=%02X %02X %02X expected=%02X %02X %02X\n",
                  (unsigned)response_length,
                  (unsigned)response[0],
                  (unsigned)response[1],
                  (unsigned)response[2],
                  (unsigned)UDS_SID_NEGATIVE_RESPONSE,
                  (unsigned)service,
                  (unsigned)nrc);
  }
}

static void test_session_control(void)
{
  UdsServer_t server;
  UdsServerConfig_t config = {0};
  const uint8_t request[] = {0x10U, 0x03U};
  const uint8_t expected[] = {0x50U, 0x03U, 0x00U, 0x32U, 0x01U, 0xF4U};
  uint8_t response[16] = {0};
  uint16_t response_length = 0U;

  config.dids = dids;
  config.did_count = (uint16_t)(sizeof(dids) / sizeof(dids[0]));
  CHECK(UdsServer_Init(&server, &config));

  CHECK(run(&server,
            100U,
            request,
            (uint16_t)sizeof(request),
            response,
            (uint16_t)sizeof(response),
            &response_length) == UDS_PROCESS_RESPONSE);
  expect_bytes(response, response_length, expected, (uint16_t)sizeof(expected));
  CHECK(UdsServer_GetSession(&server) == UDS_SESSION_EXTENDED);

  UdsServer_Tick(&server, 5099U);
  CHECK(UdsServer_GetSession(&server) == UDS_SESSION_EXTENDED);
  UdsServer_Tick(&server, 5100U);
  CHECK(UdsServer_GetSession(&server) == UDS_SESSION_DEFAULT);
  CHECK(UdsServer_GetSessionTimeoutCount(&server) == 1U);
}

static void test_suppressed_session_response(void)
{
  UdsServer_t server;
  UdsServerConfig_t config = {0};
  const uint8_t request[] = {0x10U, 0x83U};
  uint8_t response[8] = {0};
  uint16_t response_length = 0xFFFFU;

  config.dids = dids;
  config.did_count = (uint16_t)(sizeof(dids) / sizeof(dids[0]));
  CHECK(UdsServer_Init(&server, &config));

  CHECK(run(&server,
            10U,
            request,
            (uint16_t)sizeof(request),
            response,
            (uint16_t)sizeof(response),
            &response_length) == UDS_PROCESS_NO_RESPONSE);
  CHECK(response_length == 0U);
  CHECK(UdsServer_GetSession(&server) == UDS_SESSION_EXTENDED);
}

static void test_read_dids(void)
{
  UdsServer_t server;
  UdsServerConfig_t config = {0};
  const uint8_t request[] = {0x22U, 0xF1U, 0x90U};
  const uint8_t expected[] = {0x62U, 0xF1U, 0x90U,
                              'P',   '2',   'T',   'E', 'S', 'T',
                              'V',   'I',   'N',   '0', '0', '0',
                              '0',   '0',   '0',   '0', '1'};
  const uint8_t multiple_request[] = {0x22U, 0xF1U, 0x93U, 0x01U, 0x00U};
  const uint8_t multiple_expected[] = {
      0x62U, 0xF1U, 0x93U, 'F', '4', '0', '7', '-', 'H', 'W', '-',
      'R',   'E',   'V',   '-', 'A', 0x01U, 0x00U, 0xDEU, 0xADU,
      0xBEU, 0xEFU};
  uint8_t response[64] = {0};
  uint16_t response_length = 0U;

  config.dids = dids;
  config.did_count = (uint16_t)(sizeof(dids) / sizeof(dids[0]));
  CHECK(UdsServer_Init(&server, &config));

  CHECK(run(&server,
            0U,
            request,
            (uint16_t)sizeof(request),
            response,
            (uint16_t)sizeof(response),
            &response_length) == UDS_PROCESS_RESPONSE);
  expect_bytes(response, response_length, expected, (uint16_t)sizeof(expected));

  CHECK(run(&server,
            0U,
            multiple_request,
            (uint16_t)sizeof(multiple_request),
            response,
            (uint16_t)sizeof(response),
            &response_length) == UDS_PROCESS_RESPONSE);
  expect_bytes(response,
               response_length,
               multiple_expected,
               (uint16_t)sizeof(multiple_expected));
}

static void test_negative_responses(void)
{
  UdsServer_t server;
  UdsServerConfig_t config = {0};
  const uint8_t unsupported_service[] = {0x99U};
  const uint8_t wrong_session_length[] = {0x10U};
  const uint8_t unsupported_session[] = {0x10U, 0x05U};
  const uint8_t malformed_read[] = {0x22U, 0xF1U};
  const uint8_t unknown_did[] = {0x22U, 0xFFU, 0xFFU};
  const uint8_t bad_security_subfunction[] = {0x27U, 0x05U};
  const uint8_t send_key_too_short[] = {0x27U, 0x02U};

  config.dids = dids;
  config.did_count = (uint16_t)(sizeof(dids) / sizeof(dids[0]));
  CHECK(UdsServer_Init(&server, &config));

  expect_negative(&server, 0U, unsupported_service, (uint16_t)sizeof(unsupported_service), 0x99U, UDS_NRC_SERVICE_NOT_SUPPORTED);
  expect_negative(&server, 0U, wrong_session_length, (uint16_t)sizeof(wrong_session_length), 0x10U, UDS_NRC_INCORRECT_LENGTH);
  expect_negative(&server, 0U, unsupported_session, (uint16_t)sizeof(unsupported_session), 0x10U, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
  expect_negative(&server, 0U, malformed_read, (uint16_t)sizeof(malformed_read), 0x22U, UDS_NRC_INCORRECT_LENGTH);
  expect_negative(&server, 0U, unknown_did, (uint16_t)sizeof(unknown_did), 0x22U, UDS_NRC_REQUEST_OUT_OF_RANGE);
  expect_negative(&server, 0U, bad_security_subfunction, (uint16_t)sizeof(bad_security_subfunction), 0x27U, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);

  {
    const uint8_t extended_session[] = {0x10U, 0x03U};
    uint8_t response[8] = {0};
    uint16_t response_length = 0U;
    CHECK(run(&server, 0U, extended_session, (uint16_t)sizeof(extended_session), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  }
  expect_negative(&server, 0U, send_key_too_short, (uint16_t)sizeof(send_key_too_short), 0x27U, UDS_NRC_INCORRECT_LENGTH);
}

static void test_buffer_limits_and_validation(void)
{
  UdsServer_t server;
  UdsServerConfig_t config = {0};
  UdsDid_t duplicate_dids[] = {
      {0xF190U, vin, (uint16_t)(sizeof(vin) - 1U), NULL, 0U, 0U, 0U, 0U, false},
      {0xF190U, custom_data, (uint16_t)sizeof(custom_data), NULL, 0U, 0U, 0U, 0U, false},
  };
  const uint8_t request[] = {0x22U, 0xF1U, 0x90U};
  uint8_t response[2] = {0};
  uint16_t response_length = 0U;

  config.dids = dids;
  config.did_count = (uint16_t)(sizeof(dids) / sizeof(dids[0]));
  CHECK(UdsServer_Init(&server, &config));

  CHECK(run(&server,
            0U,
            request,
            (uint16_t)sizeof(request),
            response,
            (uint16_t)sizeof(response),
            &response_length) == UDS_PROCESS_BUFFER_TOO_SMALL);
  CHECK(response_length == 0U);

  config.dids = duplicate_dids;
  config.did_count = (uint16_t)(sizeof(duplicate_dids) / sizeof(duplicate_dids[0]));
  CHECK(!UdsServer_Init(&server, &config));
}

static void test_tester_present_keeps_session_alive(void)
{
  UdsServer_t server;
  UdsServerConfig_t config = {0};
  const uint8_t session_request[] = {0x10U, 0x03U};
  const uint8_t tester_present[] = {0x3EU, 0x00U};
  const uint8_t bad_subfunction[] = {0x3EU, 0x01U};
  const uint8_t expected[] = {0x7EU, 0x00U};
  uint8_t response[8] = {0};
  uint16_t response_length = 0U;

  CHECK(UdsServer_Init(&server, &config));
  CHECK(run(&server, 100U, session_request, (uint16_t)sizeof(session_request), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  CHECK(run(&server, 5000U, tester_present, (uint16_t)sizeof(tester_present), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  expect_bytes(response, response_length, expected, (uint16_t)sizeof(expected));
  UdsServer_Tick(&server, 9999U);
  CHECK(UdsServer_GetSession(&server) == UDS_SESSION_EXTENDED);
  UdsServer_Tick(&server, 10000U);
  CHECK(UdsServer_GetSession(&server) == UDS_SESSION_DEFAULT);

  expect_negative(&server, 10100U, bad_subfunction, (uint16_t)sizeof(bad_subfunction), 0x3EU, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
}

static void test_ecu_did_variants(void)
{
  UdsEcu_t f407;
  UdsEcu_t f103;
  const uint8_t read_vin[] = {0x22U, 0xF1U, 0x90U};
  const uint8_t read_hw[] = {0x22U, 0xF1U, 0x93U};
  uint8_t response[64] = {0};
  uint16_t response_length = 0U;
  const uint8_t expected_vin_f407[] = {0x62U, 0xF1U, 0x90U, 'P', '2', 'F', '4',
                                       '0', '7', 'T', 'E', 'S', 'T', 'V', 'I',
                                       'N', '0', '0', '0', '1'};
  const uint8_t expected_vin_f103[] = {0x62U, 0xF1U, 0x90U, 'P', '2', 'F', '1',
                                       '0', '3', 'T', 'E', 'S', 'T', 'V', 'I',
                                       'N', '0', '0', '0', '1'};
  const uint8_t expected_hw_f407[] = {0x62U, 0xF1U, 0x93U, 'F', '4', '0', '7',
                                      '-', 'H', 'W', '-', 'R', 'E', 'V', '-', 'A'};
  const uint8_t expected_hw_f103[] = {0x62U, 0xF1U, 0x93U, 'F', '1', '0', '3',
                                      '-', 'H', 'W', '-', 'R', 'E', 'V', '-', 'A'};

  UdsEcu_Init(&f407, UDS_ECU_F407);
  UdsEcu_Init(&f103, UDS_ECU_F103);

  CHECK(UdsEcu_Process(&f407, 0U, read_vin, (uint16_t)sizeof(read_vin), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  expect_bytes(response, response_length, expected_vin_f407, (uint16_t)sizeof(expected_vin_f407));
  CHECK(UdsEcu_Process(&f103, 0U, read_vin, (uint16_t)sizeof(read_vin), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  expect_bytes(response, response_length, expected_vin_f103, (uint16_t)sizeof(expected_vin_f103));
  CHECK(UdsEcu_Process(&f407, 0U, read_hw, (uint16_t)sizeof(read_hw), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  expect_bytes(response, response_length, expected_hw_f407, (uint16_t)sizeof(expected_hw_f407));
  CHECK(UdsEcu_Process(&f103, 0U, read_hw, (uint16_t)sizeof(read_hw), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  expect_bytes(response, response_length, expected_hw_f103, (uint16_t)sizeof(expected_hw_f103));
}

static void test_security_write_and_routine_flow(void)
{
  UdsEcu_t ecu;
  const uint8_t session_extended[] = {0x10U, 0x03U};
  const uint8_t session_programming[] = {0x10U, 0x02U};
  const uint8_t session_default[] = {0x10U, 0x01U};
  const uint8_t request_seed[] = {0x27U, 0x01U};
  const uint8_t write_custom[] = {0x2EU, 0x01U, 0x00U, 0xDEU, 0xADU, 0xBEU, 0xEFU};
  const uint8_t read_custom[] = {0x22U, 0x01U, 0x00U};
  const uint8_t write_vin[] = {0x2EU, 0xF1U, 0x90U,
                               'P', '2', 'T', 'E', 'S', 'T', 'V', 'I',
                               'N', '0', '0', '0', '0', '0', '0', '0', '1'};
  const uint8_t start_self_test[] = {0x31U, 0x01U, 0x02U, 0x03U};
  const uint8_t start_erase[] = {0x31U, 0x01U, 0xFFU, 0x00U};
  const uint8_t results_erase[] = {0x31U, 0x03U, 0xFFU, 0x00U};
  uint8_t response[64] = {0};
  uint16_t response_length = 0U;
  uint32_t seed;
  uint32_t key;
  uint8_t send_key[6] = {0x27U, 0x02U, 0x00U, 0x00U, 0x00U, 0x00U};
  const uint8_t expected_write[] = {0x6EU, 0x01U, 0x00U};
  const uint8_t expected_read[] = {0x62U, 0x01U, 0x00U, 0xDEU, 0xADU, 0xBEU, 0xEFU};
  const uint8_t expected_self_test[] = {0x71U, 0x01U, 0x02U, 0x03U};
  const uint8_t expected_erase[] = {0x71U, 0x01U, 0xFFU, 0x00U};
  const uint8_t expected_erase_results[] = {0x71U, 0x03U, 0xFFU, 0x00U, 0x00U};

  UdsEcu_Init(&ecu, UDS_ECU_F407);

  expect_negative(&ecu.server, 0U, write_custom, (uint16_t)sizeof(write_custom), 0x2EU, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION);
  CHECK(run(&ecu.server, 0U, session_extended, (uint16_t)sizeof(session_extended), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  expect_negative(&ecu.server, 0U, write_custom, (uint16_t)sizeof(write_custom), 0x2EU, UDS_NRC_SECURITY_ACCESS_DENIED);

  CHECK(run(&ecu.server, 0U, request_seed, (uint16_t)sizeof(request_seed), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  CHECK(response_length == 6U);
  CHECK(response[0] == 0x67U);
  CHECK(response[1] == 0x01U);
  seed = ((uint32_t)response[2] << 24U) | ((uint32_t)response[3] << 16U) |
         ((uint32_t)response[4] << 8U) | (uint32_t)response[5];
  CHECK(seed == 0x87985AA5U);
  key = UdsSecurity_ComputeKey(seed, UDS_DEFAULT_SECURITY_SALT);
  CHECK(key == 0xE174F5DAU);
  send_key[2] = (uint8_t)(key >> 24U);
  send_key[3] = (uint8_t)(key >> 16U);
  send_key[4] = (uint8_t)(key >> 8U);
  send_key[5] = (uint8_t)key;
  CHECK(run(&ecu.server, 0U, send_key, (uint16_t)sizeof(send_key), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  CHECK(UdsServer_IsSecurityUnlocked(&ecu.server));

  CHECK(run(&ecu.server, 0U, write_custom, (uint16_t)sizeof(write_custom), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  expect_bytes(response, response_length, expected_write, (uint16_t)sizeof(expected_write));
  CHECK(run(&ecu.server, 0U, read_custom, (uint16_t)sizeof(read_custom), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  expect_bytes(response, response_length, expected_read, (uint16_t)sizeof(expected_read));

  expect_negative(&ecu.server, 0U, write_vin, (uint16_t)sizeof(write_vin), 0x2EU, UDS_NRC_CONDITIONS_NOT_CORRECT);
  CHECK(run(&ecu.server, 0U, session_programming, (uint16_t)sizeof(session_programming), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  expect_negative(&ecu.server, 0U, write_vin, (uint16_t)sizeof(write_vin), 0x2EU, UDS_NRC_SECURITY_ACCESS_DENIED);

  CHECK(run(&ecu.server, 0U, request_seed, (uint16_t)sizeof(request_seed), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  seed = ((uint32_t)response[2] << 24U) | ((uint32_t)response[3] << 16U) |
         ((uint32_t)response[4] << 8U) | (uint32_t)response[5];
  key = UdsSecurity_ComputeKey(seed, UDS_DEFAULT_SECURITY_SALT);
  send_key[2] = (uint8_t)(key >> 24U);
  send_key[3] = (uint8_t)(key >> 16U);
  send_key[4] = (uint8_t)(key >> 8U);
  send_key[5] = (uint8_t)key;
  CHECK(run(&ecu.server, 0U, send_key, (uint16_t)sizeof(send_key), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  CHECK(run(&ecu.server, 0U, write_vin, (uint16_t)sizeof(write_vin), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  CHECK(response_length == 3U);
  CHECK(response[0] == 0x6EU);
  CHECK(response[1] == 0xF1U);
  CHECK(response[2] == 0x90U);

  CHECK(run(&ecu.server, 0U, start_self_test, (uint16_t)sizeof(start_self_test), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  expect_bytes(response, response_length, expected_self_test, (uint16_t)sizeof(expected_self_test));
  CHECK(UdsServer_GetRoutineState(&ecu.server, 0x0203U) == (uint8_t)UDS_ROUTINE_RUNNING);

  {
    const uint8_t results_self_test[] = {0x31U, 0x03U, 0x02U, 0x03U};
    const uint8_t expected_results[] = {0x71U, 0x03U, 0x02U, 0x03U, 0x00U};
    CHECK(run(&ecu.server, 0U, results_self_test, (uint16_t)sizeof(results_self_test), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE_PENDING);
    CHECK(response_length == 3U);
    CHECK(response[0] == 0x7FU);
    CHECK(response[1] == 0x31U);
    CHECK(response[2] == 0x78U);
    CHECK(UdsEcu_PollPending(&ecu, 10U, response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
    expect_bytes(response, response_length, expected_results, (uint16_t)sizeof(expected_results));
    CHECK(UdsEcu_PollPending(&ecu, 20U, response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_NO_RESPONSE);
    CHECK(response_length == 0U);
  }

  CHECK(run(&ecu.server, 0U, session_default, (uint16_t)sizeof(session_default), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  CHECK(run(&ecu.server, 0U, session_extended, (uint16_t)sizeof(session_extended), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  expect_negative(&ecu.server, 0U, start_erase, (uint16_t)sizeof(start_erase), 0x31U, UDS_NRC_SECURITY_ACCESS_DENIED);
  CHECK(run(&ecu.server, 0U, request_seed, (uint16_t)sizeof(request_seed), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  seed = ((uint32_t)response[2] << 24U) | ((uint32_t)response[3] << 16U) |
         ((uint32_t)response[4] << 8U) | (uint32_t)response[5];
  key = UdsSecurity_ComputeKey(seed, UDS_DEFAULT_SECURITY_SALT);
  send_key[2] = (uint8_t)(key >> 24U);
  send_key[3] = (uint8_t)(key >> 16U);
  send_key[4] = (uint8_t)(key >> 8U);
  send_key[5] = (uint8_t)key;
  CHECK(run(&ecu.server, 0U, send_key, (uint16_t)sizeof(send_key), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  CHECK(run(&ecu.server, 0U, start_erase, (uint16_t)sizeof(start_erase), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  expect_bytes(response, response_length, expected_erase, (uint16_t)sizeof(expected_erase));
  CHECK(run(&ecu.server, 0U, results_erase, (uint16_t)sizeof(results_erase), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE_PENDING);
  CHECK(run(&ecu.server, 0U, results_erase, (uint16_t)sizeof(results_erase), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  expect_bytes(response, response_length, expected_erase_results, (uint16_t)sizeof(expected_erase_results));
}

static void test_security_attempt_limit(void)
{
  UdsEcu_t ecu;
  const uint8_t session_extended[] = {0x10U, 0x03U};
  const uint8_t request_seed[] = {0x27U, 0x01U};
  const uint8_t wrong_key[] = {0x27U, 0x02U, 0x00U, 0x00U, 0x00U, 0x00U};
  uint8_t response[16] = {0};
  uint16_t response_length = 0U;
  uint8_t attempt;

  UdsEcu_Init(&ecu, UDS_ECU_F407);
  CHECK(run(&ecu.server, 0U, session_extended, (uint16_t)sizeof(session_extended), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);

  for (attempt = 0U; attempt < 3U; attempt++)
  {
    CHECK(run(&ecu.server, 0U, request_seed, (uint16_t)sizeof(request_seed), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
    CHECK(run(&ecu.server, 0U, wrong_key, (uint16_t)sizeof(wrong_key), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
    if (attempt < 2U)
    {
      CHECK(response[2] == UDS_NRC_INVALID_KEY);
    }
    else
    {
      CHECK(response[2] == UDS_NRC_EXCEED_NUMBER_OF_ATTEMPTS);
    }
  }

  expect_negative(&ecu.server, 0U, request_seed, (uint16_t)sizeof(request_seed), 0x27U, UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED);
  CHECK(!UdsServer_IsSecurityUnlocked(&ecu.server));
}

static void test_routine_sequence_errors(void)
{
  UdsEcu_t ecu;
  const uint8_t session_extended[] = {0x10U, 0x03U};
  const uint8_t start_self_test[] = {0x31U, 0x01U, 0x02U, 0x03U};
  const uint8_t results_self_test[] = {0x31U, 0x03U, 0x02U, 0x03U};
  const uint8_t stop_self_test[] = {0x31U, 0x02U, 0x02U, 0x03U};
  const uint8_t unknown_routine[] = {0x31U, 0x01U, 0x12U, 0x34U};
  const uint8_t bad_subfunction[] = {0x31U, 0x09U, 0x02U, 0x03U};
  uint8_t response[16] = {0};
  uint16_t response_length = 0U;

  UdsEcu_Init(&ecu, UDS_ECU_F407);
  expect_negative(&ecu.server, 0U, start_self_test, (uint16_t)sizeof(start_self_test), 0x31U, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION);
  expect_negative(&ecu.server, 0U, results_self_test, (uint16_t)sizeof(results_self_test), 0x31U, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION);

  CHECK(run(&ecu.server, 0U, session_extended, (uint16_t)sizeof(session_extended), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  expect_negative(&ecu.server, 0U, results_self_test, (uint16_t)sizeof(results_self_test), 0x31U, UDS_NRC_REQUEST_SEQUENCE_ERROR);
  CHECK(run(&ecu.server, 0U, start_self_test, (uint16_t)sizeof(start_self_test), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  expect_negative(&ecu.server, 0U, start_self_test, (uint16_t)sizeof(start_self_test), 0x31U, UDS_NRC_CONDITIONS_NOT_CORRECT);
  CHECK(run(&ecu.server, 0U, stop_self_test, (uint16_t)sizeof(stop_self_test), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  CHECK(response[0] == 0x71U);
  CHECK(response[1] == 0x02U);
  expect_negative(&ecu.server, 0U, results_self_test, (uint16_t)sizeof(results_self_test), 0x31U, UDS_NRC_REQUEST_SEQUENCE_ERROR);
  expect_negative(&ecu.server, 0U, unknown_routine, (uint16_t)sizeof(unknown_routine), 0x31U, UDS_NRC_REQUEST_OUT_OF_RANGE);
  expect_negative(&ecu.server, 0U, bad_subfunction, (uint16_t)sizeof(bad_subfunction), 0x31U, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
}

static void test_sequence_error_for_key_without_seed(void)
{
  UdsEcu_t ecu;
  const uint8_t session_extended[] = {0x10U, 0x03U};
  const uint8_t send_key[] = {0x27U, 0x02U, 0x00U, 0x00U, 0x00U, 0x00U};

  UdsEcu_Init(&ecu, UDS_ECU_F407);
  {
    uint8_t response[8] = {0};
    uint16_t response_length = 0U;
    CHECK(run(&ecu.server, 0U, session_extended, (uint16_t)sizeof(session_extended), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  }
  expect_negative(&ecu.server, 0U, send_key, (uint16_t)sizeof(send_key), 0x27U, UDS_NRC_REQUEST_SEQUENCE_ERROR);
}

typedef struct
{
  uint8_t vin[17];
  uint16_t vin_length;
  uint8_t custom[16];
  uint16_t custom_length;
  uint32_t load_count;
  uint32_t save_count;
  bool fail_save;
} StoredDidState_t;

static bool stored_did_load(uint16_t did,
                            uint8_t *data,
                            uint16_t capacity,
                            uint16_t *length,
                            void *context)
{
  StoredDidState_t *state = (StoredDidState_t *)context;
  const uint8_t *source = NULL;
  uint16_t source_length = 0U;

  state->load_count++;
  if (did == 0xF190U)
  {
    source = state->vin;
    source_length = state->vin_length;
  }
  else if (did == 0xF191U)
  {
    static const uint8_t dtc[] = {0x11U, 0x22U, 0x33U, 0x44U};
    source = dtc;
    source_length = (uint16_t)sizeof(dtc);
  }
  else if (did == 0x0100U)
  {
    source = state->custom;
    source_length = state->custom_length;
  }
  else
  {
    return false;
  }

  if ((source == NULL) || (source_length > capacity))
  {
    return false;
  }
  {
    uint16_t index;
    for (index = 0U; index < source_length; index++)
    {
      data[index] = source[index];
    }
  }
  *length = source_length;
  return true;
}

static bool stored_did_save(uint16_t did,
                            const uint8_t *data,
                            uint16_t length,
                            void *context)
{
  StoredDidState_t *state = (StoredDidState_t *)context;

  state->save_count++;
  if (state->fail_save)
  {
    return false;
  }
  if ((did == 0x0100U) && (length <= sizeof(state->custom)))
  {
    uint16_t index;
    for (index = 0U; index < length; index++)
    {
      state->custom[index] = data[index];
    }
    state->custom_length = length;
    return true;
  }
  return false;
}

static void test_ecu_storage_callbacks(void)
{
  UdsEcu_t ecu;
  StoredDidState_t state = {0};
  UdsDidStorage_t storage;
  const uint8_t loaded_vin[] = "PERSISTEDVIN00001";
  const uint8_t initial_custom[] = {0x10U, 0x20U};
  const uint8_t replacement[] = {0xCAU, 0xFEU};
  const uint8_t read_custom[] = {0x22U, 0x01U, 0x00U};
  const uint8_t write_custom[] = {0x2EU, 0x01U, 0x00U, 0xCAU, 0xFEU};
  const uint8_t session_extended[] = {0x10U, 0x03U};
  const uint8_t request_seed[] = {0x27U, 0x01U};
  uint8_t response[64] = {0};
  uint16_t response_length = 0U;
  uint8_t send_key[6] = {0x27U, 0x02U, 0x00U, 0x00U, 0x00U, 0x00U};
  uint32_t seed;
  uint32_t key;

  for (uint16_t index = 0U; index < (uint16_t)(sizeof(loaded_vin) - 1U); index++)
  {
    state.vin[index] = loaded_vin[index];
  }
  state.vin_length = (uint16_t)(sizeof(loaded_vin) - 1U);
  for (uint16_t index = 0U; index < (uint16_t)sizeof(initial_custom); index++)
  {
    state.custom[index] = initial_custom[index];
  }
  state.custom_length = (uint16_t)sizeof(initial_custom);
  storage.load = stored_did_load;
  storage.save = stored_did_save;
  storage.context = &state;

  UdsEcu_InitWithStorage(&ecu, UDS_ECU_F407, &storage);
  CHECK(state.load_count == 3U);

  CHECK(run(&ecu.server, 0U, read_custom, (uint16_t)sizeof(read_custom), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  CHECK(response_length == 5U);
  CHECK(response[3] == initial_custom[0]);
  CHECK(response[4] == initial_custom[1]);

  CHECK(run(&ecu.server, 0U, session_extended, (uint16_t)sizeof(session_extended), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  CHECK(run(&ecu.server, 0U, request_seed, (uint16_t)sizeof(request_seed), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  seed = ((uint32_t)response[2] << 24U) | ((uint32_t)response[3] << 16U) |
         ((uint32_t)response[4] << 8U) | (uint32_t)response[5];
  key = UdsSecurity_ComputeKey(seed, UDS_DEFAULT_SECURITY_SALT);
  send_key[2] = (uint8_t)(key >> 24U);
  send_key[3] = (uint8_t)(key >> 16U);
  send_key[4] = (uint8_t)(key >> 8U);
  send_key[5] = (uint8_t)key;
  CHECK(run(&ecu.server, 0U, send_key, (uint16_t)sizeof(send_key), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);

  CHECK(run(&ecu.server, 0U, write_custom, (uint16_t)sizeof(write_custom), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE_PENDING);
  CHECK(response_length == 3U);
  CHECK(response[0] == UDS_SID_NEGATIVE_RESPONSE);
  CHECK(response[1] == UDS_SID_WRITE_DATA_BY_IDENTIFIER);
  CHECK(response[2] == UDS_NRC_RESPONSE_PENDING);
  CHECK(state.save_count == 0U);
  CHECK(UdsEcu_PollPending(&ecu, 10U, response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  CHECK(response_length == 3U);
  CHECK(response[0] == UDS_SID_WRITE_DATA_BY_IDENTIFIER + 0x40U);
  CHECK(response[1] == 0x01U);
  CHECK(response[2] == 0x00U);
  CHECK(state.save_count == 1U);
  CHECK(state.custom_length == (uint16_t)sizeof(replacement));
  expect_bytes(state.custom, state.custom_length, replacement, (uint16_t)sizeof(replacement));

  state.fail_save = true;
  CHECK(run(&ecu.server,
            0U,
            (const uint8_t[]){0x2EU, 0x01U, 0x00U, 0x55U},
            4U,
            response,
            (uint16_t)sizeof(response),
            &response_length) == UDS_PROCESS_RESPONSE_PENDING);
  CHECK(UdsEcu_PollPending(&ecu, 20U, response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  CHECK(response_length == 3U);
  CHECK(response[0] == UDS_SID_NEGATIVE_RESPONSE);
  CHECK(response[1] == UDS_SID_WRITE_DATA_BY_IDENTIFIER);
  CHECK(response[2] == UDS_NRC_GENERAL_PROGRAMMING_FAILURE);
  CHECK(state.save_count == 2U);
  CHECK(state.custom_length == (uint16_t)sizeof(replacement));
  expect_bytes(state.custom, state.custom_length, replacement, (uint16_t)sizeof(replacement));

  CHECK(run(&ecu.server, 0U, read_custom, (uint16_t)sizeof(read_custom), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  CHECK(response_length == 5U);
  CHECK(response[3] == replacement[0]);
  CHECK(response[4] == replacement[1]);
}

typedef struct
{
  uint32_t self_test_calls;
  uint32_t erase_calls;
  uint8_t self_test_result;
  bool erase_result;
} RoutineHookState_t;

static uint8_t routine_self_test(void *context)
{
  RoutineHookState_t *state = (RoutineHookState_t *)context;
  state->self_test_calls++;
  return state->self_test_result;
}

static bool routine_erase(void *context)
{
  RoutineHookState_t *state = (RoutineHookState_t *)context;
  state->erase_calls++;
  return state->erase_result;
}

static void test_application_routine_hooks(void)
{
  UdsEcu_t ecu;
  RoutineHookState_t state = {0};
  UdsEcuHooks_t hooks;
  const uint8_t session_extended[] = {0x10U, 0x03U};
  const uint8_t request_seed[] = {0x27U, 0x01U};
  const uint8_t start_self_test[] = {0x31U, 0x01U, 0x02U, 0x03U};
  const uint8_t results_self_test[] = {0x31U, 0x03U, 0x02U, 0x03U};
  const uint8_t start_erase[] = {0x31U, 0x01U, 0xFFU, 0x00U};
  const uint8_t results_erase[] = {0x31U, 0x03U, 0xFFU, 0x00U};
  uint8_t response[32] = {0};
  uint16_t response_length = 0U;
  uint8_t send_key[6] = {0x27U, 0x02U, 0x00U, 0x00U, 0x00U, 0x00U};
  uint32_t seed;
  uint32_t key;

  state.self_test_result =
      UDS_ECU_SELFTEST_PARAM_FAILURE | UDS_ECU_SELFTEST_CAN_FAILURE;
  state.erase_result = true;
  hooks.self_test = routine_self_test;
  hooks.erase_params = routine_erase;
  hooks.context = &state;

  UdsEcu_InitWithHooks(&ecu, UDS_ECU_F407, NULL, &hooks);
  CHECK(run(&ecu.server, 0U, session_extended, (uint16_t)sizeof(session_extended), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  CHECK(run(&ecu.server, 0U, start_self_test, (uint16_t)sizeof(start_self_test), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  CHECK(run(&ecu.server, 0U, results_self_test, (uint16_t)sizeof(results_self_test), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE_PENDING);
  CHECK(UdsEcu_PollPending(&ecu, 10U, response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  CHECK(response_length == 5U);
  CHECK(response[2] == 0x02U);
  CHECK(response[3] == 0x03U);
  CHECK(response[4] == state.self_test_result);
  CHECK(state.self_test_calls == 1U);

  CHECK(run(&ecu.server, 0U, request_seed, (uint16_t)sizeof(request_seed), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  seed = ((uint32_t)response[2] << 24U) | ((uint32_t)response[3] << 16U) |
         ((uint32_t)response[4] << 8U) | (uint32_t)response[5];
  key = UdsSecurity_ComputeKey(seed, UDS_DEFAULT_SECURITY_SALT);
  send_key[2] = (uint8_t)(key >> 24U);
  send_key[3] = (uint8_t)(key >> 16U);
  send_key[4] = (uint8_t)(key >> 8U);
  send_key[5] = (uint8_t)key;
  CHECK(run(&ecu.server, 0U, send_key, (uint16_t)sizeof(send_key), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  CHECK(run(&ecu.server, 0U, start_erase, (uint16_t)sizeof(start_erase), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  CHECK(run(&ecu.server, 0U, results_erase, (uint16_t)sizeof(results_erase), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE_PENDING);
  CHECK(UdsEcu_PollPending(&ecu, 20U, response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  CHECK(response_length == 5U);
  CHECK(response[2] == 0xFFU);
  CHECK(response[3] == 0x00U);
  CHECK(response[4] == 0x00U);
  CHECK(state.erase_calls == 1U);

  state.erase_result = false;
  CHECK(run(&ecu.server, 0U, start_erase, (uint16_t)sizeof(start_erase), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  CHECK(run(&ecu.server, 0U, results_erase, (uint16_t)sizeof(results_erase), response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE_PENDING);
  CHECK(UdsEcu_PollPending(&ecu, 30U, response, (uint16_t)sizeof(response), &response_length) == UDS_PROCESS_RESPONSE);
  CHECK(response_length == 5U);
  CHECK(response[4] == 0x01U);
  CHECK(state.erase_calls == 2U);
}

int main(void)
{
  test_session_control();
  test_suppressed_session_response();
  test_read_dids();
  test_negative_responses();
  test_buffer_limits_and_validation();
  test_tester_present_keeps_session_alive();
  test_ecu_did_variants();
  test_security_write_and_routine_flow();
  test_security_attempt_limit();
  test_routine_sequence_errors();
  test_sequence_error_for_key_without_seed();
  test_ecu_storage_callbacks();
  test_application_routine_hooks();

  if (failures != 0)
  {
    (void)fprintf(stderr, "firmware UDS host tests: %d failure(s)\n", failures);
    return 1;
  }

  (void)printf("firmware UDS host tests: PASS\n");
  return 0;
}
