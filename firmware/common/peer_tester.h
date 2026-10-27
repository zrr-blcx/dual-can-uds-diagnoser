#ifndef PEER_TESTER_H
#define PEER_TESTER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  uint32_t interval_ms;
  uint32_t response_timeout_ms;
  uint32_t requests_sent;
  uint32_t responses_ok;
  uint32_t timeouts;
  uint32_t tx_failures;
  uint32_t invalid_responses;
  uint32_t last_rtt_ms;
  uint32_t min_rtt_ms;
  uint32_t max_rtt_ms;
  uint32_t rtt_total_ms;
} PeerTesterStats_t;

void PeerTester_Init(PeerTesterStats_t *stats,
                     uint32_t interval_ms,
                     uint32_t response_timeout_ms);
void PeerTester_BuildRequest(uint8_t frame[8]);
void PeerTester_RecordRequest(PeerTesterStats_t *stats);
void PeerTester_RecordTxFailure(PeerTesterStats_t *stats);
void PeerTester_RecordTimeout(PeerTesterStats_t *stats);
void PeerTester_RecordLatency(PeerTesterStats_t *stats, uint32_t elapsed_ms);
bool PeerTester_AcceptResponse(PeerTesterStats_t *stats,
                               uint32_t received_id,
                               const uint8_t data[8],
                               uint32_t data_length,
                               uint32_t expected_response_id);

#endif /* PEER_TESTER_H */
