#include "peer_tester.h"

#include <string.h>

#define PEER_TEST_DID 0xF19AU

void PeerTester_Init(PeerTesterStats_t *stats,
                     uint32_t interval_ms,
                     uint32_t response_timeout_ms)
{
  (void)memset(stats, 0, sizeof(*stats));
  stats->interval_ms = interval_ms;
  stats->response_timeout_ms = response_timeout_ms;
  stats->min_rtt_ms = 0xFFFFFFFFU;
}

void PeerTester_BuildRequest(uint8_t frame[8])
{
  (void)memset(frame, 0xCC, 8U);
  frame[0] = 0x03U;
  frame[1] = 0x22U;
  frame[2] = (uint8_t)((PEER_TEST_DID >> 8U) & 0xFFU);
  frame[3] = (uint8_t)(PEER_TEST_DID & 0xFFU);
}

void PeerTester_RecordRequest(PeerTesterStats_t *stats)
{
  stats->requests_sent++;
}

void PeerTester_RecordTxFailure(PeerTesterStats_t *stats)
{
  stats->tx_failures++;
}

void PeerTester_RecordTimeout(PeerTesterStats_t *stats)
{
  stats->timeouts++;
}

void PeerTester_RecordLatency(PeerTesterStats_t *stats, uint32_t elapsed_ms)
{
  stats->last_rtt_ms = elapsed_ms;
  stats->rtt_total_ms += elapsed_ms;
  if (elapsed_ms < stats->min_rtt_ms)
  {
    stats->min_rtt_ms = elapsed_ms;
  }
  if (elapsed_ms > stats->max_rtt_ms)
  {
    stats->max_rtt_ms = elapsed_ms;
  }
}

bool PeerTester_AcceptResponse(PeerTesterStats_t *stats,
                               uint32_t received_id,
                               const uint8_t data[8],
                               uint32_t data_length,
                               uint32_t expected_response_id)
{
  if ((received_id != expected_response_id) ||
      (data_length != 8U) ||
      (data[0] != 0x07U) ||
      (data[1] != 0x62U) ||
      (data[2] != (uint8_t)((PEER_TEST_DID >> 8U) & 0xFFU)) ||
      (data[3] != (uint8_t)(PEER_TEST_DID & 0xFFU)))
  {
    stats->invalid_responses++;
    return false;
  }

  stats->responses_ok++;
  return true;
}
