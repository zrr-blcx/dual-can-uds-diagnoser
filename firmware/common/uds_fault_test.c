#include "uds_fault_test.h"

#include <string.h>

#define UDS_SID_READ_DATA 0x22U
#define UDS_SID_WRITE_DATA 0x2EU
#define UDS_SID_POSITIVE 0x62U
#define UDS_SID_WRITE_POSITIVE 0x6EU
#define UDS_DID_FAULT_CONTROL 0xF1A0U
#define UDS_DID_DTC_STATUS 0xF191U

static bool time_before(uint32_t now_ms, uint32_t deadline_ms)
{
  return (int32_t)(now_ms - deadline_ms) < 0;
}

static UdsFaultAction_t respond(uint8_t *response,
                                uint16_t response_capacity,
                                uint16_t *response_length,
                                const uint8_t *data,
                                uint16_t length)
{
  if (response_capacity < length)
  {
    *response_length = 0U;
    return UDS_FAULT_PASS;
  }
  (void)memcpy(response, data, length);
  *response_length = length;
  return UDS_FAULT_HANDLED;
}

void UdsFaultTest_Init(UdsFaultTest_t *fault)
{
  fault->suppress_until_ms = 0U;
  fault->dtc_status[0] = 0U;
  fault->dtc_status[1] = 0U;
  fault->dtc_status[2] = 0U;
  fault->dtc_status[3] = 0U;
  fault->dtc_active = false;
  fault->nrc_service = 0U;
  fault->nrc = 0U;
  fault->nrc_active = false;
}

UdsFaultAction_t UdsFaultTest_Process(UdsFaultTest_t *fault,
                                      const uint8_t *request,
                                      uint16_t request_length,
                                      uint32_t now_ms,
                                      uint8_t *response,
                                      uint16_t response_capacity,
                                      uint16_t *response_length)
{
  const uint8_t write_ack[3] = {
      UDS_SID_WRITE_POSITIVE, 0xF1U, 0xA0U};
  *response_length = 0U;

  if ((request_length >= 4U) &&
      (request[0] == UDS_SID_WRITE_DATA) &&
      ((((uint16_t)request[1] << 8U) | request[2]) == UDS_DID_FAULT_CONTROL))
  {
    uint8_t mode = request[3];
    if (mode == 0x00U)
    {
      UdsFaultTest_Init(fault);
    }
    else if ((mode == 0x01U) && (request_length >= 8U))
    {
      fault->dtc_status[0] = request[4];
      fault->dtc_status[1] = request[5];
      fault->dtc_status[2] = request[6];
      fault->dtc_status[3] = request[7];
      fault->dtc_active = true;
    }
    else if ((mode == 0x02U) && (request_length >= 5U))
    {
      fault->suppress_until_ms =
          now_ms + ((uint32_t)request[4] * 1000U);
    }
    else if ((mode == 0x04U) && (request_length >= 6U))
    {
      fault->nrc_service = request[4];
      fault->nrc = request[5];
      fault->nrc_active = true;
    }
    else
    {
      const uint8_t invalid[3] = {0x7FU, UDS_SID_WRITE_DATA, 0x31U};
      return respond(response,
                     response_capacity,
                     response_length,
                     invalid,
                     sizeof(invalid));
    }
    return respond(response,
                   response_capacity,
                   response_length,
                   write_ack,
                   sizeof(write_ack));
  }

  if (time_before(now_ms, fault->suppress_until_ms))
  {
    return UDS_FAULT_DROP;
  }

  if ((request_length >= 3U) &&
      (request[0] == UDS_SID_READ_DATA) &&
      ((((uint16_t)request[1] << 8U) | request[2]) == UDS_DID_DTC_STATUS) &&
      fault->dtc_active)
  {
    const uint8_t dtc_response[7] = {
        UDS_SID_POSITIVE, 0xF1U, 0x91U,
        fault->dtc_status[0], fault->dtc_status[1],
        fault->dtc_status[2], fault->dtc_status[3]};
    return respond(response,
                   response_capacity,
                   response_length,
                   dtc_response,
                   sizeof(dtc_response));
  }

  if ((request_length > 0U) &&
      fault->nrc_active &&
      (request[0] == fault->nrc_service))
  {
    const uint8_t nrc_response[3] = {0x7FU, request[0], fault->nrc};
    return respond(response,
                   response_capacity,
                   response_length,
                   nrc_response,
                   sizeof(nrc_response));
  }

  return UDS_FAULT_PASS;
}
