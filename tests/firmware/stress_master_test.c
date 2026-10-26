#include <stdint.h>
#include <stdio.h>

#include "stress_master.h"

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

int main(void)
{
  StressMasterStats_t stats;
  uint8_t frame[8] = {0};
  const uint8_t response[8] = {
      0x07U, 0x62U, 0xF1U, 0x9AU, 0x65U, 0x63U, 0x75U, 0x32U};
  const uint8_t wrong_did[8] = {
      0x07U, 0x62U, 0xF1U, 0x90U, 0x65U, 0x63U, 0x75U, 0x32U};

  StressMaster_Init(&stats, 10U);
  CHECK(stats.state == STRESS_MASTER_READY);
  CHECK(stats.requests_target == 10U);

  StressMaster_BuildRequest(frame);
  CHECK(frame[0] == 0x03U);
  CHECK(frame[1] == 0x22U);
  CHECK(frame[2] == 0xF1U);
  CHECK(frame[3] == 0x9AU);

  StressMaster_BuildBackground(0x01020304U, frame);
  CHECK(frame[0] == 0x01U && frame[1] == 0x02U);
  CHECK(frame[2] == 0x03U && frame[3] == 0x04U);
  CHECK(frame[4] == 0x55U && frame[5] == 0x55U);
  CHECK(frame[6] == 0x55U && frame[7] == 0x55U);

  StressMaster_RecordRequest(&stats);
  CHECK(stats.requests_sent == 1U);
  CHECK(StressMaster_AcceptResponse(&stats, 0x7E9U, response, 8U, 0x7E9U));
  CHECK(stats.responses_ok == 1U);
  CHECK(stats.last_response_length == 8U);
  CHECK(!StressMaster_AcceptResponse(&stats, 0x7E9U, wrong_did, 8U, 0x7E9U));

  StressMaster_RecordTimeout(&stats);
  CHECK(stats.timeouts == 1U);
  StressMaster_Finish(&stats);
  CHECK(stats.state == STRESS_MASTER_DONE);

  if (failures != 0)
  {
    (void)fprintf(stderr, "stress master host tests: %d failure(s)\n", failures);
    return 1;
  }

  (void)printf("stress master host tests: PASS\n");
  return 0;
}
