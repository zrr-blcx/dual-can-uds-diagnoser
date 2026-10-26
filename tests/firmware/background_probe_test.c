#include <stdint.h>
#include <stdio.h>

#include "background_probe.h"

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
  BackgroundProbe_t probe;
  BackgroundProbe_Init(&probe);

  BackgroundProbe_OnFrame(&probe, 100U);
  BackgroundProbe_OnFrame(&probe, 101U);
  BackgroundProbe_OnFrame(&probe, 104U);
  BackgroundProbe_OnFrame(&probe, 105U);
  BackgroundProbe_OnFrame(&probe, 105U);

  CHECK(probe.stats.received == 4U);
  CHECK(probe.stats.gaps == 2U);
  CHECK(probe.stats.out_of_order == 1U);
  CHECK(probe.stats.expected == 106U);
  CHECK(probe.stats.last_sequence == 105U);
  CHECK(probe.stats.initialized == 1U);

  BackgroundProbe_Reset(&probe);
  BackgroundProbe_OnFrame(&probe, 0xFFFFFFFFU);
  BackgroundProbe_OnFrame(&probe, 0U);
  CHECK(probe.stats.received == 2U);
  CHECK(probe.stats.gaps == 0U);
  CHECK(probe.stats.expected == 1U);

  if (failures != 0)
  {
    (void)fprintf(stderr, "background probe host tests: %d failure(s)\n", failures);
    return 1;
  }

  (void)printf("background probe host tests: PASS\n");
  return 0;
}
