#ifndef UDS_FAULT_TEST_H
#define UDS_FAULT_TEST_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  UDS_FAULT_PASS = 0,
  UDS_FAULT_HANDLED = 1,
  UDS_FAULT_DROP = 2
} UdsFaultAction_t;

typedef struct
{
  uint32_t suppress_until_ms;
  uint8_t dtc_status[4];
  bool dtc_active;
  uint8_t nrc_service;
  uint8_t nrc;
  bool nrc_active;
} UdsFaultTest_t;

void UdsFaultTest_Init(UdsFaultTest_t *fault);

UdsFaultAction_t UdsFaultTest_Process(UdsFaultTest_t *fault,
                                      const uint8_t *request,
                                      uint16_t request_length,
                                      uint32_t now_ms,
                                      uint8_t *response,
                                      uint16_t response_capacity,
                                      uint16_t *response_length);

#endif /* UDS_FAULT_TEST_H */
