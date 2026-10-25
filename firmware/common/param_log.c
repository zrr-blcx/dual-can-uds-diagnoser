#include "param_log.h"

#include <stddef.h>
#include <string.h>

static uint32_t slot_offset(uint16_t slot)
{
  return (uint32_t)slot * PARAM_STORE_IMAGE_SIZE;
}

static bool image_is_erased(const uint8_t *image)
{
  uint16_t index;

  for (index = 0U; index < PARAM_STORE_IMAGE_SIZE; index++)
  {
    if (image[index] != 0xFFU)
    {
      return false;
    }
  }
  return true;
}

void ParamLog_Init(ParamLog_t *log)
{
  if (log != NULL)
  {
    (void)memset(log, 0, sizeof(*log));
    ParamStore_Init(&log->store);
  }
}

bool ParamLog_Load(ParamLog_t *log,
                   const ParamStoreIo_t *io,
                   uint16_t slot_count)
{
  uint8_t image[PARAM_STORE_IMAGE_SIZE];
  uint8_t latest_image[PARAM_STORE_IMAGE_SIZE];
  bool latest_valid = false;
  uint16_t slot;

  if ((log == NULL) || (io == NULL) ||
      (io->read == NULL) || (io->erase == NULL) || (io->write == NULL) ||
      (slot_count == 0U) || (slot_count > PARAM_LOG_MAX_SLOTS))
  {
    return false;
  }

  ParamLog_Init(log);
  log->slot_count = slot_count;

  for (slot = 0U; slot < slot_count; slot++)
  {
    ParamStore_t candidate;

    if (!io->read(slot_offset(slot),
                  image,
                  (uint16_t)sizeof(image),
                  io->context))
    {
      return false;
    }
    if (image_is_erased(image))
    {
      log->next_slot = slot;
      return true;
    }
    if (ParamStore_Decode(&candidate, image))
    {
      log->store = candidate;
      (void)memcpy(latest_image, image, sizeof(latest_image));
      latest_valid = true;
      log->next_slot = (uint16_t)(slot + 1U);
    }
    else
    {
      break;
    }
  }

  /* Compaction runs only at startup, before either watchdog is enabled. */
  if (!io->erase(io->context))
  {
    ParamLog_Init(log);
    return false;
  }
  log->slot_count = slot_count;
  if (latest_valid)
  {
    uint8_t verification[PARAM_STORE_IMAGE_SIZE];

    if (!io->write(0U,
                   latest_image,
                   (uint16_t)sizeof(latest_image),
                   io->context) ||
        !io->read(0U,
                  verification,
                  (uint16_t)sizeof(verification),
                  io->context) ||
        (memcmp(latest_image, verification, sizeof(latest_image)) != 0))
    {
      ParamLog_Init(log);
      return false;
    }
    log->next_slot = 1U;
  }
  return true;
}

bool ParamLog_Get(const ParamLog_t *log,
                  uint16_t did,
                  uint8_t *data,
                  uint16_t capacity,
                  uint16_t *length)
{
  return (log == NULL)
             ? false
             : ParamStore_Get(&log->store, did, data, capacity, length);
}

bool ParamLog_Set(ParamLog_t *log,
                  const ParamStoreIo_t *io,
                  uint16_t did,
                  const uint8_t *data,
                  uint16_t length)
{
  ParamStore_t candidate;
  uint8_t image[PARAM_STORE_IMAGE_SIZE];
  uint8_t verification[PARAM_STORE_IMAGE_SIZE];
  uint32_t offset;

  if ((log == NULL) || (io == NULL) ||
      (io->read == NULL) || (io->write == NULL) ||
      (log->next_slot >= log->slot_count))
  {
    return false;
  }

  candidate = log->store;
  if (!ParamStore_Update(&candidate, did, data, length) ||
      !ParamStore_Encode(&candidate, image))
  {
    return false;
  }

  offset = slot_offset(log->next_slot);
  if (!io->write(offset, image, (uint16_t)sizeof(image), io->context) ||
      !io->read(offset,
                verification,
                (uint16_t)sizeof(verification),
                io->context) ||
      (memcmp(image, verification, sizeof(image)) != 0))
  {
    return false;
  }

  log->store = candidate;
  log->next_slot++;
  return true;
}

bool ParamLog_Clear(ParamLog_t *log, const ParamStoreIo_t *io)
{
  ParamStore_t empty;
  uint8_t image[PARAM_STORE_IMAGE_SIZE];
  uint8_t verification[PARAM_STORE_IMAGE_SIZE];
  uint32_t offset;

  if ((log == NULL) || (io == NULL) ||
      (io->read == NULL) || (io->write == NULL) ||
      (log->next_slot >= log->slot_count))
  {
    return false;
  }

  ParamStore_Init(&empty);
  if (!ParamStore_Encode(&empty, image))
  {
    return false;
  }

  offset = slot_offset(log->next_slot);
  if (!io->write(offset, image, (uint16_t)sizeof(image), io->context) ||
      !io->read(offset,
                verification,
                (uint16_t)sizeof(verification),
                io->context) ||
      (memcmp(image, verification, sizeof(image)) != 0))
  {
    return false;
  }

  ParamStore_Init(&log->store);
  log->next_slot++;
  return true;
}
