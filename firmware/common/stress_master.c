#include "stress_master.h"

#include <string.h>

#define STRESS_REQUEST_DID 0xF19AU

void StressMaster_Init(StressMasterStats_t *stats, uint32_t requests_target)
{
  (void)memset(stats, 0, sizeof(*stats));
  stats->state = STRESS_MASTER_READY;
  stats->requests_target = requests_target;
  stats->min_rtt_cycles = 0xFFFFFFFFU;
}

void StressMaster_BuildRequest(uint8_t frame[8])
{
  (void)memset(frame, 0, 8U);
  frame[0] = 0x03U;
  frame[1] = 0x22U;
  frame[2] = 0xF1U;
  frame[3] = 0x9AU;
}

void StressMaster_BuildBackground(uint32_t sequence, uint8_t frame[8])
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

void StressMaster_RecordRequest(StressMasterStats_t *stats)
{
  stats->state = STRESS_MASTER_RUNNING;
  stats->requests_sent++;
}

bool StressMaster_AcceptResponse(StressMasterStats_t *stats,
                                 uint32_t received_id,
                                 const uint8_t data[8],
                                 uint8_t data_length,
                                 uint32_t expected_response_id)
{
  if ((received_id != expected_response_id) ||
      (data_length != 8U) ||
      ((data[0] & 0xF0U) != 0U) ||
      ((data[0] & 0x0FU) != 7U) ||
      (data[1] != 0x62U) ||
      (data[2] != (uint8_t)(STRESS_REQUEST_DID >> 8U)) ||
      (data[3] != (uint8_t)(STRESS_REQUEST_DID & 0xFFU)))
  {
    return false;
  }

  stats->responses_ok++;
  stats->last_response_length = 8U;
  (void)memcpy(stats->last_response, data, 8U);
  return true;
}

void StressMaster_RecordLatency(StressMasterStats_t *stats, uint32_t cycles)
{
  stats->last_rtt_cycles = cycles;
  stats->rtt_total_cycles += cycles;
  if (cycles < stats->min_rtt_cycles)
  {
    stats->min_rtt_cycles = cycles;
  }
  if (cycles > stats->max_rtt_cycles)
  {
    stats->max_rtt_cycles = cycles;
  }
}

void StressMaster_RecordTimeout(StressMasterStats_t *stats)
{
  stats->timeouts++;
}

void StressMaster_Finish(StressMasterStats_t *stats)
{
  stats->state = STRESS_MASTER_DONE;
}
