#ifndef STRESS_MASTER_H
#define STRESS_MASTER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  STRESS_MASTER_READY = 0,
  STRESS_MASTER_RUNNING = 1,
  STRESS_MASTER_DONE = 2
} StressMasterState_t;

typedef struct
{
  uint32_t state;
  uint32_t requests_target;
  uint32_t requests_sent;
  uint32_t responses_ok;
  uint32_t timeouts;
  uint32_t invalid_responses;
  uint32_t background_sent;
  uint32_t background_missed;
  uint32_t last_rtt_cycles;
  uint32_t min_rtt_cycles;
  uint32_t max_rtt_cycles;
  uint32_t rtt_total_cycles;
  uint32_t runtime_cycles;
  uint16_t last_response_length;
  uint8_t last_response[8];
} StressMasterStats_t;

void StressMaster_Init(StressMasterStats_t *stats, uint32_t requests_target);
void StressMaster_BuildRequest(uint8_t frame[8]);
void StressMaster_BuildBackground(uint32_t sequence, uint8_t frame[8]);
void StressMaster_RecordRequest(StressMasterStats_t *stats);
bool StressMaster_AcceptResponse(StressMasterStats_t *stats,
                                 uint32_t received_id,
                                 const uint8_t data[8],
                                 uint8_t data_length,
                                 uint32_t expected_response_id);
void StressMaster_RecordLatency(StressMasterStats_t *stats, uint32_t cycles);
void StressMaster_RecordTimeout(StressMasterStats_t *stats);
void StressMaster_Finish(StressMasterStats_t *stats);

#endif /* STRESS_MASTER_H */
