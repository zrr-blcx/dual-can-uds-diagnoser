#ifndef CAN_RX_QUEUE_H
#define CAN_RX_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#define CAN_RX_QUEUE_DEPTH 16U
#define CAN_RX_QUEUE_MAX_DATA 8U

typedef struct
{
  uint32_t id;
  uint8_t length;
  uint8_t data[CAN_RX_QUEUE_MAX_DATA];
} CanRxFrame_t;

typedef struct
{
  CanRxFrame_t frames[CAN_RX_QUEUE_DEPTH + 1U];
  volatile uint16_t head;
  volatile uint16_t tail;
  volatile uint32_t dropped;
} CanRxQueue_t;

void CanRxQueue_Init(CanRxQueue_t *queue);

/* ISR-safe producer. Returns false and increments dropped when invalid/full. */
bool CanRxQueue_Push(CanRxQueue_t *queue,
                     uint32_t id,
                     const uint8_t *data,
                     uint8_t length);

/* Main-loop consumer. Returns false when the queue is empty. */
bool CanRxQueue_Pop(CanRxQueue_t *queue, CanRxFrame_t *frame);

#endif /* CAN_RX_QUEUE_H */
