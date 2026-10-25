#include "uds_did_db.h"

#include <stddef.h>
#include <string.h>

static const uint8_t g_vin_f407[] = "P2F407TESTVIN0001";
static const uint8_t g_vin_f103[] = "P2F103TESTVIN0001";
static const uint8_t g_hw_f407[] = "F407-HW-REV-A";
static const uint8_t g_hw_f103[] = "F103-HW-REV-A";
static const uint8_t g_name_f407[] = "ecu1";
static const uint8_t g_name_f103[] = "ecu2";
static const uint8_t g_software_version[] = "V1.0";
static const uint8_t g_custom_default[] = {0x00U, 0x01U};
static const uint8_t g_dtc_default[] = {0x00U, 0x00U, 0x00U, 0x00U};

void UdsDidDb_Init(UdsDidDb_t *database, UdsEcuVariant_t variant)
{
  const uint8_t *vin;
  const uint8_t *hardware;
  const uint8_t *name;
  uint16_t vin_length;
  uint16_t hardware_length;
  uint16_t name_length;
  uint16_t index = 0U;

  if (database == NULL)
  {
    return;
  }
  (void)memset(database, 0, sizeof(*database));

  if (variant == UDS_ECU_F103)
  {
    vin = g_vin_f103;
    hardware = g_hw_f103;
    name = g_name_f103;
    vin_length = (uint16_t)(sizeof(g_vin_f103) - 1U);
    hardware_length = (uint16_t)(sizeof(g_hw_f103) - 1U);
    name_length = (uint16_t)(sizeof(g_name_f103) - 1U);
  }
  else
  {
    vin = g_vin_f407;
    hardware = g_hw_f407;
    name = g_name_f407;
    vin_length = (uint16_t)(sizeof(g_vin_f407) - 1U);
    hardware_length = (uint16_t)(sizeof(g_hw_f407) - 1U);
    name_length = (uint16_t)(sizeof(g_name_f407) - 1U);
  }

  database->custom_storage[0] = g_custom_default[0];
  database->custom_storage[1] = g_custom_default[1];
  for (index = 0U; index < UDS_DID_DTC_CAPACITY; index++)
  {
    database->dtc_storage[index] = g_dtc_default[index];
  }
  for (index = 0U; index < UDS_DID_VIN_CAPACITY; index++)
  {
    database->vin_storage[index] = vin[index];
  }

  database->dids[0].did = 0xF190U;
  database->dids[0].data = vin;
  database->dids[0].length = vin_length;
  database->dids[0].storage = database->vin_storage;
  database->dids[0].storage_capacity = UDS_DID_VIN_CAPACITY;
  database->dids[0].read_sessions_mask = 0U;
  database->dids[0].write_sessions_mask = UDS_EXTENDED_PROGRAMMING_MASK;
  database->dids[0].write_required_session = UDS_SESSION_PROGRAMMING;
  database->dids[0].write_requires_unlock = true;

  database->dids[1].did = 0xF193U;
  database->dids[1].data = hardware;
  database->dids[1].length = hardware_length;
  database->dids[1].storage = NULL;
  database->dids[1].storage_capacity = 0U;
  database->dids[1].read_sessions_mask = 0U;
  database->dids[1].write_sessions_mask = 0U;
  database->dids[1].write_required_session = 0U;
  database->dids[1].write_requires_unlock = false;

  database->dids[2].did = 0x0100U;
  database->dids[2].data = g_custom_default;
  database->dids[2].length = (uint16_t)sizeof(g_custom_default);
  database->dids[2].storage = database->custom_storage;
  database->dids[2].storage_capacity = UDS_DID_CUSTOM_CAPACITY;
  database->dids[2].read_sessions_mask = 0U;
  database->dids[2].write_sessions_mask = UDS_EXTENDED_PROGRAMMING_MASK;
  database->dids[2].write_required_session = 0U;
  database->dids[2].write_requires_unlock = true;

  database->dids[3].did = 0xF191U;
  database->dids[3].data = g_dtc_default;
  database->dids[3].length = (uint16_t)sizeof(g_dtc_default);
  database->dids[3].storage = database->dtc_storage;
  database->dids[3].storage_capacity = UDS_DID_DTC_CAPACITY;
  database->dids[3].read_sessions_mask = 0U;
  database->dids[3].write_sessions_mask = UDS_EXTENDED_PROGRAMMING_MASK;
  database->dids[3].write_required_session = 0U;
  database->dids[3].write_requires_unlock = true;

  database->dids[4].did = 0xF19AU;
  database->dids[4].data = name;
  database->dids[4].length = name_length;
  database->dids[4].storage = NULL;
  database->dids[4].storage_capacity = 0U;
  database->dids[4].read_sessions_mask = 0U;
  database->dids[4].write_sessions_mask = 0U;
  database->dids[4].write_required_session = 0U;
  database->dids[4].write_requires_unlock = false;

  database->dids[5].did = 0xF19BU;
  database->dids[5].data = g_software_version;
  database->dids[5].length = (uint16_t)(sizeof(g_software_version) - 1U);
  database->dids[5].storage = NULL;
  database->dids[5].storage_capacity = 0U;
  database->dids[5].read_sessions_mask = 0U;
  database->dids[5].write_sessions_mask = 0U;
  database->dids[5].write_required_session = 0U;
  database->dids[5].write_requires_unlock = false;

  database->did_count = 6U;
}

void UdsDidDb_AttachStorage(UdsDidDb_t *database,
                            const UdsDidStorage_t *storage)
{
  uint16_t index;

  if ((database == NULL) || (storage == NULL))
  {
    return;
  }

  for (index = 0U; index < database->did_count; index++)
  {
    UdsDid_t *did = &database->dids[index];
    uint8_t loaded[UDS_DID_VIN_CAPACITY];
    uint16_t loaded_length = 0U;

    if ((did->write_sessions_mask == 0U) ||
        (did->storage == NULL) ||
        (did->storage_capacity == 0U))
    {
      continue;
    }

    did->storage_load = storage->load;
    did->storage_save = storage->save;
    did->storage_context = storage->context;

    if ((storage->load != NULL) &&
        storage->load(did->did,
                      loaded,
                      (uint16_t)sizeof(loaded),
                      &loaded_length,
                      storage->context) &&
        (loaded_length <= did->storage_capacity))
    {
      uint16_t data_index;
      for (data_index = 0U; data_index < loaded_length; data_index++)
      {
        did->storage[data_index] = loaded[data_index];
      }
      did->data = did->storage;
      did->length = loaded_length;
    }
  }
}
