#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "param_store.h"

typedef struct
{
  uint8_t bytes[128];
  uint32_t erase_count;
  uint32_t write_count;
  bool fail_write;
} FakeFlash_t;

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

static bool fake_read(uint32_t offset,
                      uint8_t *data,
                      uint16_t length,
                      void *context)
{
  FakeFlash_t *flash = (FakeFlash_t *)context;
  if ((offset + length) > sizeof(flash->bytes))
  {
    return false;
  }
  (void)memcpy(data, &flash->bytes[offset], length);
  return true;
}

static bool fake_erase(void *context)
{
  FakeFlash_t *flash = (FakeFlash_t *)context;
  (void)memset(flash->bytes, 0xFF, sizeof(flash->bytes));
  flash->erase_count++;
  return true;
}

static bool fake_write(uint32_t offset,
                       const uint8_t *data,
                       uint16_t length,
                       void *context)
{
  FakeFlash_t *flash = (FakeFlash_t *)context;
  if (flash->fail_write || ((offset + length) > sizeof(flash->bytes)))
  {
    return false;
  }
  (void)memcpy(&flash->bytes[offset], data, length);
  flash->write_count++;
  return true;
}

static uint32_t crc32(const uint8_t *data, uint32_t length)
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

static void write_valid_custom_image(FakeFlash_t *flash)
{
  uint8_t *image = flash->bytes;
  const uint8_t custom[] = {0xA5U, 0x5AU};

  (void)memset(image, 0xFF, sizeof(flash->bytes));
  put_u32(&image[0], PARAM_STORE_MAGIC);
  put_u16(&image[4], PARAM_STORE_VERSION);
  put_u16(&image[6], 1U);
  put_u16(&image[8], 0x0100U);
  put_u16(&image[10], (uint16_t)sizeof(custom));
  (void)memcpy(&image[12], custom, sizeof(custom));
  put_u32(&image[76], crc32(image, 76U));
}

static void test_round_trip_and_load(void)
{
  FakeFlash_t flash = {0};
  ParamStoreIo_t io = {fake_read, fake_erase, fake_write, &flash};
  ParamStore_t store;
  ParamStore_t reloaded;
  const uint8_t value[] = {0xDEU, 0xADU, 0xBEU, 0xEFU};
  uint8_t actual[PARAM_STORE_VALUE_MAX] = {0};
  uint16_t actual_length = 0U;

  (void)memset(flash.bytes, 0xFF, sizeof(flash.bytes));
  ParamStore_Init(&store);
  CHECK(ParamStore_Set(&store, &io, 0x0100U, value, (uint16_t)sizeof(value)));
  CHECK(flash.erase_count == 1U);
  CHECK(flash.write_count == 1U);
  CHECK(ParamStore_Get(&store, 0x0100U, actual, (uint16_t)sizeof(actual), &actual_length));
  CHECK(actual_length == (uint16_t)sizeof(value));
  CHECK(memcmp(actual, value, sizeof(value)) == 0);

  ParamStore_Init(&reloaded);
  CHECK(ParamStore_Load(&reloaded, &io));
  (void)memset(actual, 0, sizeof(actual));
  CHECK(ParamStore_Get(&reloaded, 0x0100U, actual, (uint16_t)sizeof(actual), &actual_length));
  CHECK(actual_length == (uint16_t)sizeof(value));
  CHECK(memcmp(actual, value, sizeof(value)) == 0);
}

static void test_crc_rejects_corruption(void)
{
  FakeFlash_t flash = {0};
  ParamStoreIo_t io = {fake_read, fake_erase, fake_write, &flash};
  ParamStore_t store;
  uint8_t actual[PARAM_STORE_VALUE_MAX] = {0};
  uint16_t actual_length = 0U;

  write_valid_custom_image(&flash);
  ParamStore_Init(&store);
  CHECK(ParamStore_Load(&store, &io));
  flash.bytes[12] ^= 0x01U;
  ParamStore_Init(&store);
  CHECK(!ParamStore_Load(&store, &io));
  CHECK(!ParamStore_Get(&store, 0x0100U, actual, (uint16_t)sizeof(actual), &actual_length));
}

static void test_write_failure_preserves_cached_value(void)
{
  FakeFlash_t flash = {0};
  ParamStoreIo_t io = {fake_read, fake_erase, fake_write, &flash};
  ParamStore_t store;
  const uint8_t initial[] = {0x11U, 0x22U};
  const uint8_t replacement[] = {0x33U, 0x44U};
  uint8_t actual[PARAM_STORE_VALUE_MAX] = {0};
  uint16_t actual_length = 0U;

  (void)memset(flash.bytes, 0xFF, sizeof(flash.bytes));
  ParamStore_Init(&store);
  CHECK(ParamStore_Set(&store, &io, 0x0100U, initial, (uint16_t)sizeof(initial)));
  flash.fail_write = true;
  CHECK(!ParamStore_Set(&store, &io, 0x0100U, replacement, (uint16_t)sizeof(replacement)));
  CHECK(ParamStore_Get(&store, 0x0100U, actual, (uint16_t)sizeof(actual), &actual_length));
  CHECK(actual_length == (uint16_t)sizeof(initial));
  CHECK(memcmp(actual, initial, sizeof(initial)) == 0);
}

int main(void)
{
  test_round_trip_and_load();
  test_crc_rejects_corruption();
  test_write_failure_preserves_cached_value();

  if (failures != 0)
  {
    (void)fprintf(stderr, "parameter store host tests: %d failure(s)\n", failures);
    return 1;
  }

  (void)printf("parameter store host tests: PASS\n");
  return 0;
}
