#include "can_rx_queue.h"

#include <string.h>

void CanRxQueue_Init(CanRxQueue_t *queue)
{
  if (queue == NULL)
  {
    return;
  }
  queue->head = 0U;
  queue->tail = 0U;
  queue->dropped = 0U;
}

bool CanRxQueue_Push(CanRxQueue_t *queue,
                     uint32_t id,
                     const uint8_t *data,
                     uint8_t length)
{
  CanRxFrame_t *frame;
  uint16_t index;
  uint16_t next;

  if ((queue == NULL) || (data == NULL) || (length > CAN_RX_QUEUE_MAX_DATA))
  {
    if (queue != NULL)
    {
      queue->dropped++;
    }
    return false;
  }
  index = queue->head;
  next = (uint16_t)((index + 1U) % (CAN_RX_QUEUE_DEPTH + 1U));
  if (next == queue->tail)
  {
    queue->dropped++;
    return false;
  }

  frame = &queue->frames[index];
  frame->id = id;
  frame->length = length;
  memset(frame->data, 0, sizeof(frame->data));
  if (length > 0U)
  {
    memcpy(frame->data, data, length);
  }

  queue->head = next;
  return true;
}

bool CanRxQueue_Pop(CanRxQueue_t *queue, CanRxFrame_t *frame)
{
  uint16_t index;

  if ((queue == NULL) || (frame == NULL) || (queue->head == queue->tail))
  {
    return false;
  }

  index = queue->tail;
  *frame = queue->frames[index];
  queue->tail = (uint16_t)((index + 1U) % (CAN_RX_QUEUE_DEPTH + 1U));
  return true;
}
