#include "load_generator.h"

#include <string.h>

void LoadGenerator_Init(LoadGeneratorStats_t *stats,
                        uint32_t period_us,
                        uint32_t target_frames)
{
  (void)memset(stats, 0, sizeof(*stats));
  stats->period_us = period_us;
  stats->target_frames = target_frames;
}

void LoadGenerator_BuildFrame(uint32_t sequence, uint8_t frame[8])
{
  frame[0] = (uint8_t)(sequence >> 24U);
  frame[1] = (uint8_t)(sequence >> 16U);
  frame[2] = (uint8_t)(sequence >> 8U);
  frame[3] = (uint8_t)sequence;
  frame[4] = 0x55U;
  frame[5] = 0x55U;
  frame[6] = 0x55U;
  frame[7] = 0x55U;
}

void LoadGenerator_RecordAttempt(LoadGeneratorStats_t *stats, bool sent)
{
  stats->sequence++;
  if (sent)
  {
    stats->sent++;
    if ((stats->target_frames != 0U) &&
        (stats->sent >= stats->target_frames))
    {
      stats->completed = 1U;
    }
  }
  else
  {
    stats->tx_failures++;
  }
}

void LoadGenerator_RecordSkipped(LoadGeneratorStats_t *stats,
                                 uint32_t skipped)
{
  stats->missed_deadlines += skipped;
  stats->sequence += skipped;
}
