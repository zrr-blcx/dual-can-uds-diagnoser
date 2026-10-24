#include "isotp_bench.h"

#include <stddef.h>

static uint8_t expected_byte(uint16_t index)
{
  return (uint8_t)((index * 7U + 3U) & 0xFFU);
}

void IsotpBench_BuildPayload(uint8_t *buffer, uint16_t length)
{
  uint16_t i;

  if (buffer == NULL)
  {
    return;
  }
  for (i = 0U; i < length; i++)
  {
    buffer[i] = expected_byte(i);
  }
}

bool IsotpBench_ValidatePayload(const uint8_t *buffer,
                                uint16_t length,
                                uint16_t *bad_index)
{
  uint16_t i;

  if (bad_index != NULL)
  {
    *bad_index = 0xFFFFU;
  }
  if ((buffer == NULL) || (length != ISOTP_BENCH_PAYLOAD_SIZE))
  {
    return false;
  }

  for (i = 0U; i < length; i++)
  {
    if (buffer[i] != expected_byte(i))
    {
      if (bad_index != NULL)
      {
        *bad_index = i;
      }
      return false;
    }
  }
  return true;
}

void IsotpBench_BuildStatusFrame(uint8_t frame[8], uint8_t status)
{
  uint8_t i;

  if (frame == NULL)
  {
    return;
  }
  frame[0] = 0x01U;
  frame[1] = status;
  for (i = 2U; i < 8U; i++)
  {
    frame[i] = 0xCCU;
  }
}
