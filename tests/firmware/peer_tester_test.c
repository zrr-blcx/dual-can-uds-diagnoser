#include <stdint.h>
#include <stdio.h>

#include "peer_tester.h"

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
  PeerTesterStats_t stats;
  uint8_t frame[8] = {0};
  const uint8_t response[8] = {
      0x07U, 0x62U, 0xF1U, 0x9AU, 0x65U, 0x63U, 0x75U, 0x32U};
  const uint8_t wrong[8] = {
      0x07U, 0x62U, 0xF1U, 0x90U, 0x65U, 0x63U, 0x75U, 0x32U};

  PeerTester_Init(&stats, 100U, 50U);
  CHECK(stats.interval_ms == 100U);
  CHECK(stats.response_timeout_ms == 50U);

  PeerTester_BuildRequest(frame);
  CHECK(frame[0] == 0x03U);
  CHECK(frame[1] == 0x22U);
  CHECK(frame[2] == 0xF1U);
  CHECK(frame[3] == 0x9AU);
  CHECK(frame[4] == 0xCCU);

  PeerTester_RecordRequest(&stats);
  CHECK(stats.requests_sent == 1U);
  CHECK(PeerTester_AcceptResponse(&stats, 0x7E8U, response, 8U, 0x7E8U));
  CHECK(stats.responses_ok == 1U);
  PeerTester_RecordLatency(&stats, 7U);
  CHECK(stats.last_rtt_ms == 7U);
  CHECK(stats.min_rtt_ms == 7U);
  CHECK(stats.max_rtt_ms == 7U);

  CHECK(!PeerTester_AcceptResponse(&stats, 0x7E8U, wrong, 8U, 0x7E8U));
  CHECK(stats.invalid_responses == 1U);
  PeerTester_RecordTimeout(&stats);
  PeerTester_RecordTxFailure(&stats);
  CHECK(stats.timeouts == 1U);
  CHECK(stats.tx_failures == 1U);

  if (failures != 0)
  {
    (void)fprintf(stderr, "peer tester host tests: %d failure(s)\n", failures);
    return 1;
  }

  (void)printf("peer tester host tests: PASS\n");
  return 0;
}
