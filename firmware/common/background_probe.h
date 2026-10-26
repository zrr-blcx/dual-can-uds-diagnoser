#ifndef BACKGROUND_PROBE_H
#define BACKGROUND_PROBE_H

#include <stdint.h>

typedef struct
{
  uint32_t received;
  uint32_t gaps;
  uint32_t out_of_order;
  uint32_t expected;
  uint32_t last_sequence;
  uint32_t initialized;
} BackgroundProbeStats_t;

typedef struct
{
  BackgroundProbeStats_t stats;
} BackgroundProbe_t;

void BackgroundProbe_Init(BackgroundProbe_t *probe);
void BackgroundProbe_Reset(BackgroundProbe_t *probe);
void BackgroundProbe_OnFrame(BackgroundProbe_t *probe, uint32_t sequence);

#endif /* BACKGROUND_PROBE_H */
