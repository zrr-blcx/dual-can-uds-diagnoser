#include "app_param_storage.h"

#include <stddef.h>
#include <string.h>

#include "main.h"

#define APP_PARAM_FLASH_BASE 0x080E0000UL
#define APP_PARAM_FLASH_SIZE (128UL * 1024UL)
#define APP_PARAM_FLASH_SECTOR FLASH_SECTOR_11
#define APP_PARAM_SLOT_COUNT \
  (uint16_t)(APP_PARAM_FLASH_SIZE / PARAM_STORE_IMAGE_SIZE)

#ifndef APP_PARAM_FORCE_SAVE_FAILURE
#define APP_PARAM_FORCE_SAVE_FAILURE 0U
#endif

_Static_assert((APP_PARAM_FLASH_BASE % 4UL) == 0UL,
               "parameter flash base must be word aligned");
_Static_assert((APP_PARAM_FLASH_SIZE % PARAM_STORE_IMAGE_SIZE) == 32U,
               "parameter sector size must leave only one partial slot");

static void refresh_flash_caches(void)
{
  __HAL_FLASH_DATA_CACHE_DISABLE();
  __HAL_FLASH_DATA_CACHE_RESET();
  __HAL_FLASH_DATA_CACHE_ENABLE();
}

static bool app_flash_read(uint32_t offset,
                           uint8_t *data,
                           uint16_t length,
                           void *context)
{
  (void)context;
  if ((data == NULL) ||
      ((offset + length) > APP_PARAM_FLASH_SIZE))
  {
    return false;
  }
  (void)memcpy(data,
               (const void *)(uintptr_t)(APP_PARAM_FLASH_BASE + offset),
               length);
  return true;
}

static bool app_flash_erase(void *context)
{
  FLASH_EraseInitTypeDef erase;
  uint32_t sector_error = 0U;
  HAL_StatusTypeDef status;

  (void)context;
  erase.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase.Banks = FLASH_BANK_1;
  erase.Sector = APP_PARAM_FLASH_SECTOR;
  erase.NbSectors = 1U;
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

  if (HAL_FLASH_Unlock() != HAL_OK)
  {
    return false;
  }
  status = HAL_FLASHEx_Erase(&erase, &sector_error);
  (void)HAL_FLASH_Lock();
  refresh_flash_caches();
  return (status == HAL_OK) && (sector_error == 0xFFFFFFFFUL);
}

static bool app_flash_write(uint32_t offset,
                            const uint8_t *data,
                            uint16_t length,
                            void *context)
{
  uint32_t address;
  uint32_t index;
  bool success = true;

  (void)context;
  if ((data == NULL) ||
      ((offset % 4U) != 0U) ||
      ((length % 4U) != 0U) ||
      ((offset + length) > APP_PARAM_FLASH_SIZE))
  {
    return false;
  }

  address = APP_PARAM_FLASH_BASE + offset;
  if (HAL_FLASH_Unlock() != HAL_OK)
  {
    return false;
  }

  for (index = 0U; index < length; index += 4U)
  {
    uint32_t target = address + index;
    uint32_t word =
        (uint32_t)data[index] |
        ((uint32_t)data[index + 1U] << 8U) |
        ((uint32_t)data[index + 2U] << 16U) |
        ((uint32_t)data[index + 3U] << 24U);

    if (*(const volatile uint32_t *)(uintptr_t)target != 0xFFFFFFFFUL)
    {
      success = false;
      break;
    }
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, target, word) != HAL_OK)
    {
      success = false;
      break;
    }
  }

  (void)HAL_FLASH_Lock();
  refresh_flash_caches();
  return success;
}

static bool app_param_load(uint16_t did,
                           uint8_t *data,
                           uint16_t capacity,
                           uint16_t *length,
                           void *context)
{
  AppParamStorage_t *storage = (AppParamStorage_t *)context;
  return ParamLog_Get(&storage->log,
                      did,
                      data,
                      capacity,
                      length);
}

static bool app_param_save(uint16_t did,
                           const uint8_t *data,
                           uint16_t length,
                           void *context)
{
  AppParamStorage_t *storage = (AppParamStorage_t *)context;
#if APP_PARAM_FORCE_SAVE_FAILURE
  (void)did;
  (void)data;
  (void)length;
  (void)storage;
  return false;
#else
  return ParamLog_Set(&storage->log,
                      &storage->io,
                      did,
                      data,
                      length);
#endif
}

void AppParamStorage_Init(AppParamStorage_t *storage)
{
  if (storage == NULL)
  {
    return;
  }

  (void)memset(storage, 0, sizeof(*storage));
  storage->io.read = app_flash_read;
  storage->io.erase = app_flash_erase;
  storage->io.write = app_flash_write;
  storage->io.context = storage;

  storage->valid = ParamLog_Load(&storage->log,
                                 &storage->io,
                                 APP_PARAM_SLOT_COUNT);

  storage->uds_storage.load = app_param_load;
  storage->uds_storage.save = app_param_save;
  storage->uds_storage.context = storage;
}

const UdsDidStorage_t *AppParamStorage_GetUdsStorage(
    AppParamStorage_t *storage)
{
  return (storage == NULL) ? NULL : &storage->uds_storage;
}

bool AppParamStorage_IsValid(const AppParamStorage_t *storage)
{
  return (storage != NULL) && storage->valid;
}

bool AppParamStorage_Clear(AppParamStorage_t *storage)
{
  bool cleared;

  if (storage == NULL)
  {
    return false;
  }
  cleared = ParamLog_Clear(&storage->log, &storage->io);
  if (cleared)
  {
    storage->valid = true;
  }
  return cleared;
}
