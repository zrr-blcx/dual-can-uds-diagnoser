#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "can_rx_queue.h"

static int failures = 0;

#define CHECK(condition)                                                          \
  do                                                                              \
  {                                                                               \
    if (!(condition))                                                             \
    {                                                                             \
      (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
      failures++;                                                                 \
    }                                                                             \
  } while (0)

int main(void)
{
  CanRxQueue_t queue;
  CanRxFrame_t frame;
  uint8_t data_a[8] = {0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U};
  uint8_t data_b[8] = {7U, 6U, 5U, 4U, 3U, 2U, 1U, 0U};
  uint8_t index;

  CanRxQueue_Init(&queue);
  for (index = 0U; index < (CAN_RX_QUEUE_DEPTH + 2U); index++)
  {
    bool pushed = CanRxQueue_Push(&queue, 0x100U + index, data_a, 8U);
    CHECK(pushed == (index < CAN_RX_QUEUE_DEPTH));
  }
  CHECK(queue.dropped == 2U);

  CHECK(CanRxQueue_Pop(&queue, &frame));
  CHECK(frame.id == 0x100U);
  CHECK(frame.length == 8U);
  CHECK(memcmp(frame.data, data_a, 8U) == 0);

  CHECK(CanRxQueue_Push(&queue, 0x200U, data_b, 3U));
  CHECK(!CanRxQueue_Push(&queue, 0x201U, data_a, 9U));
  CHECK(queue.dropped == 3U);

  for (index = 0U; index < CAN_RX_QUEUE_DEPTH; index++)
  {
    CHECK(CanRxQueue_Pop(&queue, &frame));
  }
  CHECK(frame.id == 0x200U);
  CHECK(frame.length == 3U);
  CHECK(memcmp(frame.data, data_b, 3U) == 0);
  CHECK(!CanRxQueue_Pop(&queue, &frame));

  if (failures != 0)
  {
    (void)fprintf(stderr, "firmware CAN RX queue tests: %d failure(s)\n", failures);
    return 1;
  }

  (void)printf("firmware CAN RX queue tests: PASS\n");
  return 0;
}
