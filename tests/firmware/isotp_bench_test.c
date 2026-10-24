#include <stdint.h>
#include <stdio.h>

#include "isotp_bench.h"

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
  uint8_t payload[ISOTP_BENCH_PAYLOAD_SIZE];
  uint16_t bad_index = 0xFFFFU;

  IsotpBench_BuildPayload(payload, sizeof(payload));
  CHECK(payload[0] == 0x03U);
  CHECK(payload[1] == 0x0AU);
  CHECK(payload[99] == (uint8_t)((99U * 7U + 3U) & 0xFFU));
  CHECK(IsotpBench_ValidatePayload(payload, sizeof(payload), &bad_index));
  CHECK(bad_index == 0xFFFFU);

  payload[42] ^= 0x5AU;
  CHECK(!IsotpBench_ValidatePayload(payload, sizeof(payload), &bad_index));
  CHECK(bad_index == 42U);

  CHECK(!IsotpBench_ValidatePayload(payload, 7U, &bad_index));

  {
    uint8_t status_frame[8] = {0};
    IsotpBench_BuildStatusFrame(status_frame, ISOTP_BENCH_STATUS_OK);
    CHECK(status_frame[0] == 0x01U);
    CHECK(status_frame[1] == ISOTP_BENCH_STATUS_OK);
    CHECK(status_frame[2] == 0xCCU);
    CHECK(status_frame[7] == 0xCCU);
  }

  if (failures != 0)
  {
    (void)fprintf(stderr, "firmware ISO-TP bench tests: %d failure(s)\n", failures);
    return 1;
  }

  (void)printf("firmware ISO-TP bench tests: PASS\n");
  return 0;
}
