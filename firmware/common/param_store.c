#include "param_store.h"

#include <stddef.h>
#include <string.h>

#define PARAM_STORE_HEADER_SIZE 8U
#define PARAM_STORE_ENTRY_SIZE 22U
#define PARAM_STORE_CRC_OFFSET 76U

static const uint16_t g_param_store_dids[PARAM_STORE_MAX_ENTRIES] = {
    0xF190U,
    0xF191U,
    0x0100U,
};

static void put_u16(uint8_t *destination, uint16_t value)
{
  destination[0] = (uint8_t)value;
  destination[1] = (uint8_t)(value >> 8U);
}

static void put_u32(uint8_t *destination, uint32_t value)
{
  destination[0] = (uint8_t)value;
  destination[1] = (uint8_t)(value >> 8U);
  destination[2] = (uint8_t)(value >> 16U);
  destination[3] = (uint8_t)(value >> 24U);
}

static uint16_t get_u16(const uint8_t *source)
{
  return (uint16_t)((uint16_t)source[0] | ((uint16_t)source[1] << 8U));
}

static uint32_t get_u32(const uint8_t *source)
{
  return (uint32_t)source[0] |
         ((uint32_t)source[1] << 8U) |
         ((uint32_t)source[2] << 16U) |
         ((uint32_t)source[3] << 24U);
}

static uint32_t param_crc32(const uint8_t *data, uint32_t length)
{
  uint32_t crc = 0xFFFFFFFFU;
  uint32_t bit;
  uint32_t index;

  for (index = 0U; index < length; index++)
  {
    crc ^= data[index];
    for (bit = 0U; bit < 8U; bit++)
    {
      crc = (crc & 1U) ? ((crc >> 1U) ^ 0xEDB88320U) : (crc >> 1U);
    }
  }
  return ~crc;
}

static bool known_did(uint16_t did)
{
  uint16_t index;

  for (index = 0U; index < PARAM_STORE_MAX_ENTRIES; index++)
  {
    if (g_param_store_dids[index] == did)
    {
      return true;
    }
  }
  return false;
}

static ParamStoreEntry_t *find_entry(ParamStore_t *store, uint16_t did)
{
  uint16_t index;

  for (index = 0U; index < PARAM_STORE_MAX_ENTRIES; index++)
  {
    if (store->entries[index].did == did)
    {
      return &store->entries[index];
    }
  }
  return NULL;
}

static const ParamStoreEntry_t *find_const_entry(const ParamStore_t *store,
                                                 uint16_t did)
{
  uint16_t index;

  for (index = 0U; index < PARAM_STORE_MAX_ENTRIES; index++)
  {
    if (store->entries[index].did == did)
    {
      return &store->entries[index];
    }
  }
  return NULL;
}

static bool serialize(const ParamStore_t *store, uint8_t image[PARAM_STORE_IMAGE_SIZE])
{
  uint16_t count = 0U;
  uint16_t index;
  uint32_t offset = PARAM_STORE_HEADER_SIZE;

  (void)memset(image, 0, PARAM_STORE_IMAGE_SIZE);
  put_u32(&image[0], PARAM_STORE_MAGIC);
  put_u16(&image[4], PARAM_STORE_VERSION);

  for (index = 0U; index < PARAM_STORE_MAX_ENTRIES; index++)
  {
    const ParamStoreEntry_t *entry = &store->entries[index];

    if (entry->length == 0U)
    {
      continue;
    }
    if ((entry->length > PARAM_STORE_VALUE_MAX) ||
        !known_did(entry->did) ||
        (offset + PARAM_STORE_ENTRY_SIZE > PARAM_STORE_CRC_OFFSET))
    {
      return false;
    }

    put_u16(&image[offset], entry->did);
    put_u16(&image[offset + 2U], entry->length);
    (void)memcpy(&image[offset + 4U], entry->data, entry->length);
    offset += PARAM_STORE_ENTRY_SIZE;
    count++;
  }

  put_u16(&image[6], count);
  put_u32(&image[PARAM_STORE_CRC_OFFSET],
          param_crc32(image, PARAM_STORE_CRC_OFFSET));
  return true;
}

static bool deserialize(ParamStore_t *store, const uint8_t *image)
{
  uint16_t count;
  uint16_t index;
  uint32_t offset = PARAM_STORE_HEADER_SIZE;

  ParamStore_Init(store);
  if ((get_u32(&image[0]) != PARAM_STORE_MAGIC) ||
      (get_u16(&image[4]) != PARAM_STORE_VERSION) ||
      (get_u32(&image[PARAM_STORE_CRC_OFFSET]) !=
       param_crc32(image, PARAM_STORE_CRC_OFFSET)))
  {
    return false;
  }

  count = get_u16(&image[6]);
  if (count > PARAM_STORE_MAX_ENTRIES)
  {
    return false;
  }

  for (index = 0U; index < count; index++)
  {
    uint16_t did = get_u16(&image[offset]);
    uint16_t length = get_u16(&image[offset + 2U]);
    ParamStoreEntry_t *entry;

    if ((length > PARAM_STORE_VALUE_MAX) ||
        !known_did(did) ||
        (find_entry(store, did) != NULL))
    {
      ParamStore_Init(store);
      return false;
    }

    entry = &store->entries[index];
    entry->did = did;
    entry->length = (uint8_t)length;
    (void)memcpy(entry->data, &image[offset + 4U], length);
    offset += PARAM_STORE_ENTRY_SIZE;
  }
  return true;
}

void ParamStore_Init(ParamStore_t *store)
{
  if (store != NULL)
  {
    (void)memset(store, 0, sizeof(*store));
  }
}

bool ParamStore_Load(ParamStore_t *store, const ParamStoreIo_t *io)
{
  uint8_t image[PARAM_STORE_IMAGE_SIZE];

  if ((store == NULL) || (io == NULL) ||
      (io->read == NULL) || (io->erase == NULL) || (io->write == NULL))
  {
    return false;
  }

  ParamStore_Init(store);
  if (!io->read(0U, image, (uint16_t)sizeof(image), io->context))
  {
    return false;
  }
  return deserialize(store, image);
}

bool ParamStore_Get(const ParamStore_t *store,
                    uint16_t did,
                    uint8_t *data,
                    uint16_t capacity,
                    uint16_t *length)
{
  const ParamStoreEntry_t *entry;

  if ((store == NULL) || (data == NULL) || (length == NULL))
  {
    return false;
  }
  *length = 0U;
  entry = find_const_entry(store, did);
  if ((entry == NULL) || (entry->length > capacity))
  {
    return false;
  }
  (void)memcpy(data, entry->data, entry->length);
  *length = entry->length;
  return true;
}

bool ParamStore_Update(ParamStore_t *store,
                       uint16_t did,
                       const uint8_t *data,
                       uint16_t length)
{
  ParamStore_t candidate;
  ParamStoreEntry_t *entry;

  if ((store == NULL) || (data == NULL) || (length == 0U) ||
      (length > PARAM_STORE_VALUE_MAX) ||
      !known_did(did))
  {
    return false;
  }

  candidate = *store;
  entry = find_entry(&candidate, did);
  if (entry == NULL)
  {
    uint16_t index;
    for (index = 0U; index < PARAM_STORE_MAX_ENTRIES; index++)
    {
      if (candidate.entries[index].length == 0U)
      {
        entry = &candidate.entries[index];
        entry->did = did;
        break;
      }
    }
  }
  if (entry == NULL)
  {
    return false;
  }

  entry->length = (uint8_t)length;
  (void)memset(entry->data, 0, sizeof(entry->data));
  (void)memcpy(entry->data, data, length);
  *store = candidate;
  return true;
}

bool ParamStore_Encode(const ParamStore_t *store,
                       uint8_t image[PARAM_STORE_IMAGE_SIZE])
{
  if ((store == NULL) || (image == NULL))
  {
    return false;
  }
  return serialize(store, image);
}

bool ParamStore_Decode(ParamStore_t *store,
                       const uint8_t image[PARAM_STORE_IMAGE_SIZE])
{
  if ((store == NULL) || (image == NULL))
  {
    return false;
  }
  return deserialize(store, image);
}

bool ParamStore_Set(ParamStore_t *store,
                    const ParamStoreIo_t *io,
                    uint16_t did,
                    const uint8_t *data,
                    uint16_t length)
{
  ParamStore_t candidate;
  uint8_t image[PARAM_STORE_IMAGE_SIZE];
  uint8_t verification[PARAM_STORE_IMAGE_SIZE];

  if ((store == NULL) || (io == NULL) ||
      (io->read == NULL) || (io->erase == NULL) || (io->write == NULL))
  {
    return false;
  }

  candidate = *store;
  if (!ParamStore_Update(&candidate, did, data, length))
  {
    return false;
  }
  if (!serialize(&candidate, image))
  {
    return false;
  }

  if (!io->erase(io->context) ||
      !io->write(0U, image, (uint16_t)sizeof(image), io->context) ||
      !io->read(0U, verification, (uint16_t)sizeof(verification), io->context) ||
      (memcmp(image, verification, sizeof(image)) != 0))
  {
    return false;
  }

  *store = candidate;
  return true;
}
