#ifndef UDS_DID_DB_H
#define UDS_DID_DB_H

#include <stdint.h>

#include "uds_server.h"

#define UDS_DID_DB_MAX 8U
#define UDS_DID_CUSTOM_CAPACITY 16U
#define UDS_DID_DTC_CAPACITY 4U
#define UDS_DID_VIN_CAPACITY 17U

typedef enum
{
  UDS_ECU_F407 = 0,
  UDS_ECU_F103 = 1
} UdsEcuVariant_t;

typedef struct
{
  UdsDid_t dids[UDS_DID_DB_MAX];
  uint16_t did_count;
  uint8_t custom_storage[UDS_DID_CUSTOM_CAPACITY];
  uint8_t dtc_storage[UDS_DID_DTC_CAPACITY];
  uint8_t vin_storage[UDS_DID_VIN_CAPACITY];
} UdsDidDb_t;

void UdsDidDb_Init(UdsDidDb_t *database, UdsEcuVariant_t variant);

#endif /* UDS_DID_DB_H */
