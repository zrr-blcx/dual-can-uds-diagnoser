#ifndef LOAD_GENERATOR_H
#define LOAD_GENERATOR_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  uint32_t period_us;
  uint32_t target_frames;
  uint32_t sequence;
  uint32_t sent;
  uint32_t tx_failures;
  uint32_t missed_deadlines;
  uint32_t completed;
} LoadGeneratorStats_t;

void LoadGenerator_Init(LoadGeneratorStats_t *stats,
                        uint32_t period_us,
                        uint32_t target_frames);
void LoadGenerator_BuildFrame(uint32_t sequence, uint8_t frame[8]);
void LoadGenerator_RecordAttempt(LoadGeneratorStats_t *stats, bool sent);
void LoadGenerator_RecordSkipped(LoadGeneratorStats_t *stats,
                                 uint32_t skipped);

#endif /* LOAD_GENERATOR_H */
