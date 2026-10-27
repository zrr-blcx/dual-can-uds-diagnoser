#include <stdint.h>
#include <stdio.h>

#include "load_generator.h"

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
  LoadGeneratorStats_t stats;
  uint8_t frame[8] = {0};

  LoadGenerator_Init(&stats, 228U, 2U);
  CHECK(stats.period_us == 228U);
  CHECK(stats.target_frames == 2U);
  CHECK(stats.sequence == 0U);

  LoadGenerator_BuildFrame(0x01020304U, frame);
  CHECK(frame[0] == 0x01U && frame[1] == 0x02U);
  CHECK(frame[2] == 0x03U && frame[3] == 0x04U);
  CHECK(frame[4] == 0x55U && frame[7] == 0x55U);

  LoadGenerator_RecordAttempt(&stats, true);
  CHECK(stats.sequence == 1U);
  CHECK(stats.sent == 1U);
  CHECK(stats.tx_failures == 0U);

  LoadGenerator_RecordAttempt(&stats, false);
  CHECK(stats.sequence == 2U);
  CHECK(stats.sent == 1U);
  CHECK(stats.tx_failures == 1U);
  CHECK(stats.completed == 0U);

  LoadGenerator_RecordAttempt(&stats, true);
  CHECK(stats.sequence == 3U);
  CHECK(stats.sent == 2U);
  CHECK(stats.completed == 1U);

  LoadGenerator_RecordSkipped(&stats, 3U);
  CHECK(stats.sequence == 6U);
  CHECK(stats.missed_deadlines == 3U);
  CHECK(stats.completed == 1U);

  if (failures != 0)
  {
    (void)fprintf(stderr, "load generator host tests: %d failure(s)\n", failures);
    return 1;
  }

  (void)printf("load generator host tests: PASS\n");
  return 0;
}
