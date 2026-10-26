#include "background_probe.h"

void BackgroundProbe_Init(BackgroundProbe_t *probe)
{
  BackgroundProbe_Reset(probe);
}

void BackgroundProbe_Reset(BackgroundProbe_t *probe)
{
  probe->stats.received = 0U;
  probe->stats.gaps = 0U;
  probe->stats.out_of_order = 0U;
  probe->stats.expected = 0U;
  probe->stats.last_sequence = 0U;
  probe->stats.initialized = 0U;
}

void BackgroundProbe_OnFrame(BackgroundProbe_t *probe, uint32_t sequence)
{
  BackgroundProbeStats_t *stats = &probe->stats;

  if (stats->initialized == 0U)
  {
    stats->initialized = 1U;
    stats->received = 1U;
    stats->expected = sequence + 1U;
    stats->last_sequence = sequence;
    return;
  }

  stats->last_sequence = sequence;
  {
    uint32_t delta = sequence - stats->expected;
    if (delta == 0U)
    {
      stats->received++;
      stats->expected = sequence + 1U;
    }
    else if (delta < 0x80000000U)
    {
      stats->gaps += delta;
      stats->received++;
      stats->expected = sequence + 1U;
    }
    else
    {
      stats->out_of_order++;
    }
  }
}
