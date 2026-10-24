#ifndef ISOTP_BENCH_H
#define ISOTP_BENCH_H

#include <stdbool.h>
#include <stdint.h>

#define ISOTP_BENCH_PAYLOAD_SIZE 100U
#define ISOTP_BENCH_STATUS_OK 0x5AU
#define ISOTP_BENCH_STATUS_FAIL 0xA5U

void IsotpBench_BuildPayload(uint8_t *buffer, uint16_t length);

bool IsotpBench_ValidatePayload(const uint8_t *buffer,
                                uint16_t length,
                                uint16_t expected_length,
                                uint16_t *bad_index);

void IsotpBench_BuildStatusFrame(uint8_t frame[8], uint8_t status);

#endif /* ISOTP_BENCH_H */
