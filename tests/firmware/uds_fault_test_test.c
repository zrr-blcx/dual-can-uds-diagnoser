#include <stdint.h>
#include <stdio.h>

#include "uds_fault_test.h"

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

static UdsFaultAction_t process(UdsFaultTest_t *fault,
                                const uint8_t *request,
                                uint16_t request_length,
                                uint32_t now,
                                uint8_t *response,
                                uint16_t *response_length)
{
  return UdsFaultTest_Process(fault,
                              request,
                              request_length,
                              now,
                              response,
                              32U,
                              response_length);
}

int main(void)
{
  UdsFaultTest_t fault;
  uint8_t response[32];
  uint16_t response_length = 0U;
  const uint8_t read_vin[] = {0x22U, 0xF1U, 0x90U};
  const uint8_t read_dtc[] = {0x22U, 0xF1U, 0x91U};
  const uint8_t inject_dtc[] = {0x2EU, 0xF1U, 0xA0U, 0x01U, 0x01U, 0x02U, 0x03U, 0x2AU};
  const uint8_t inject_nrc[] = {0x2EU, 0xF1U, 0xA0U, 0x04U, 0x22U, 0x31U};
  const uint8_t arm_comm_loss[] = {0x2EU, 0xF1U, 0xA0U, 0x02U, 0x01U};
  const uint8_t clear[] = {0x2EU, 0xF1U, 0xA0U, 0x00U};

  UdsFaultTest_Init(&fault);
  CHECK(process(&fault, read_vin, sizeof(read_vin), 0U, response, &response_length) ==
        UDS_FAULT_PASS);

  CHECK(process(&fault, inject_dtc, sizeof(inject_dtc), 10U, response, &response_length) ==
        UDS_FAULT_HANDLED);
  CHECK(response_length == 3U);
  CHECK(response[0] == 0x6EU && response[1] == 0xF1U && response[2] == 0xA0U);

  CHECK(process(&fault, read_dtc, sizeof(read_dtc), 20U, response, &response_length) ==
        UDS_FAULT_HANDLED);
  CHECK(response_length == 7U);
  CHECK(response[0] == 0x62U && response[1] == 0xF1U && response[2] == 0x91U);
  CHECK(response[3] == 0x01U && response[4] == 0x02U && response[5] == 0x03U);
  CHECK(response[6] == 0x2AU);

  CHECK(process(&fault, inject_nrc, sizeof(inject_nrc), 30U, response, &response_length) ==
        UDS_FAULT_HANDLED);
  CHECK(process(&fault, read_vin, sizeof(read_vin), 40U, response, &response_length) ==
        UDS_FAULT_HANDLED);
  CHECK(response_length == 3U);
  CHECK(response[0] == 0x7FU && response[1] == 0x22U && response[2] == 0x31U);

  CHECK(process(&fault, clear, sizeof(clear), 50U, response, &response_length) ==
        UDS_FAULT_HANDLED);
  CHECK(process(&fault, arm_comm_loss, sizeof(arm_comm_loss), 100U, response, &response_length) ==
        UDS_FAULT_HANDLED);
  CHECK(process(&fault, read_vin, sizeof(read_vin), 500U, response, &response_length) ==
        UDS_FAULT_DROP);
  CHECK(process(&fault, read_vin, sizeof(read_vin), 1200U, response, &response_length) ==
        UDS_FAULT_PASS);

  if (failures != 0)
  {
    (void)fprintf(stderr, "UDS fault host tests: %d failure(s)\n", failures);
    return 1;
  }

  (void)printf("UDS fault host tests: PASS\n");
  return 0;
}
