#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "dbc_bench.h"

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
                         const uint8_t *expected,
                         uint32_t length)
{
  uint32_t index;
  for (index = 0U; index < length; index++)
  {
    CHECK(actual[index] == expected[index]);
  }
}

int main(void)
{
  const uint8_t engine_expected[8] = {
      0x13U, 0x4AU, 0x7DU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
  const uint8_t vehicle_expected[8] = {
      0x9AU, 0x10U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
  DbcBench_t bench;
  uint32_t id = 0U;
  uint32_t now = 1000U;
  uint8_t data[8] = {0};

  DbcBench_Init(&bench, DBC_BENCH_F407, 100U);
  CHECK(DbcBench_Poll(&bench, now, &id, data));
  CHECK(id == 0x180U);
  expect_bytes(data, engine_expected, 8U);
  now += 99U;
  CHECK(!DbcBench_Poll(&bench, now, &id, data));
  now += 1U;
  CHECK(DbcBench_Poll(&bench, now, &id, data));
  CHECK(id == 0x180U);

  DbcBench_Init(&bench, DBC_BENCH_F103, 250U);
  now = 500U;
  CHECK(DbcBench_Poll(&bench, now, &id, data));
  CHECK(id == 0x280U);
  expect_bytes(data, vehicle_expected, 8U);
  now += 249U;
  CHECK(!DbcBench_Poll(&bench, now, &id, data));
  now += 1U;
  CHECK(DbcBench_Poll(&bench, now, &id, data));

  if (failures != 0)
  {
    (void)fprintf(stderr, "DBC bench host tests: %d failure(s)\n", failures);
    return 1;
  }

  (void)printf("DBC bench host tests: PASS\n");
  return 0;
}
