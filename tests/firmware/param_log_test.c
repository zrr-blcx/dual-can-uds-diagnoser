#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "param_log.h"

typedef struct
{
  uint8_t bytes[256];
  uint32_t erase_count;
  uint32_t write_count;
  bool fail_write;
} FakeLogFlash_t;

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
  FakeLogFlash_t *flash = (FakeLogFlash_t *)context;
  if ((offset + length) > sizeof(flash->bytes))
  {
    return false;
  }
  (void)memcpy(data, &flash->bytes[offset], length);
  return true;
}

static bool fake_erase(void *context)
{
  FakeLogFlash_t *flash = (FakeLogFlash_t *)context;
  (void)memset(flash->bytes, 0xFF, sizeof(flash->bytes));
  flash->erase_count++;
  return true;
}

static bool fake_write(uint32_t offset,
                       const uint8_t *data,
                       uint16_t length,
                       void *context)
{
  FakeLogFlash_t *flash = (FakeLogFlash_t *)context;
  if (flash->fail_write || ((offset + length) > sizeof(flash->bytes)))
  {
    return false;
  }
  (void)memcpy(&flash->bytes[offset], data, length);
  flash->write_count++;
  return true;
}

static bool read_value(const ParamLog_t *log,
                       uint16_t did,
                       uint8_t *value,
                       uint16_t capacity,
                       uint16_t *length)
{
  return ParamLog_Get(log, did, value, capacity, length);
}

static void test_append_and_reload(void)
{
  FakeLogFlash_t flash = {0};
  ParamStoreIo_t io = {fake_read, fake_erase, fake_write, &flash};
  ParamLog_t log;
  ParamLog_t reloaded;
  const uint8_t first[] = {0x11U, 0x22U};
  const uint8_t second[] = {0x33U, 0x44U};
  uint8_t actual[PARAM_STORE_VALUE_MAX] = {0};
  uint16_t actual_length = 0U;

  (void)memset(flash.bytes, 0xFF, sizeof(flash.bytes));
  CHECK(ParamLog_Load(&log, &io, 2U));
  CHECK(log.next_slot == 0U);
  CHECK(flash.erase_count == 0U);

  CHECK(ParamLog_Set(&log, &io, 0x0100U, first, (uint16_t)sizeof(first)));
  CHECK(log.next_slot == 1U);
  CHECK(flash.erase_count == 0U);
  CHECK(flash.write_count == 1U);

  CHECK(ParamLog_Load(&reloaded, &io, 2U));
  CHECK(reloaded.next_slot == 1U);
  CHECK(read_value(&reloaded, 0x0100U, actual, (uint16_t)sizeof(actual), &actual_length));
  CHECK(actual_length == (uint16_t)sizeof(first));
  CHECK(memcmp(actual, first, sizeof(first)) == 0);

  CHECK(ParamLog_Set(&reloaded, &io, 0x0100U, second, (uint16_t)sizeof(second)));
  CHECK(reloaded.next_slot == 2U);
  CHECK(flash.erase_count == 0U);
  CHECK(flash.write_count == 2U);

  CHECK(ParamLog_Load(&log, &io, 2U));
  CHECK(flash.erase_count == 1U);
  CHECK(log.next_slot == 1U);
  CHECK(read_value(&log, 0x0100U, actual, (uint16_t)sizeof(actual), &actual_length));
  CHECK(actual_length == (uint16_t)sizeof(second));
  CHECK(memcmp(actual, second, sizeof(second)) == 0);
}

static void test_write_failure_keeps_log_position(void)
{
  FakeLogFlash_t flash = {0};
  ParamStoreIo_t io = {fake_read, fake_erase, fake_write, &flash};
  ParamLog_t log;
  const uint8_t initial[] = {0xA5U};
  const uint8_t replacement[] = {0x5AU};
  uint8_t actual[PARAM_STORE_VALUE_MAX] = {0};
  uint16_t actual_length = 0U;

  (void)memset(flash.bytes, 0xFF, sizeof(flash.bytes));
  CHECK(ParamLog_Load(&log, &io, 2U));
  CHECK(ParamLog_Set(&log, &io, 0x0100U, initial, (uint16_t)sizeof(initial)));
  flash.fail_write = true;
  CHECK(!ParamLog_Set(&log, &io, 0x0100U, replacement, (uint16_t)sizeof(replacement)));
  CHECK(log.next_slot == 1U);
  CHECK(read_value(&log, 0x0100U, actual, (uint16_t)sizeof(actual), &actual_length));
  CHECK(actual_length == (uint16_t)sizeof(initial));
  CHECK(actual[0] == initial[0]);
}

static void test_partial_record_is_repaired_at_boot(void)
{
  FakeLogFlash_t flash = {0};
  ParamStoreIo_t io = {fake_read, fake_erase, fake_write, &flash};
  ParamLog_t log;
  const uint8_t value[] = {0x10U, 0x20U};
  uint8_t actual[PARAM_STORE_VALUE_MAX] = {0};
  uint16_t actual_length = 0U;

  (void)memset(flash.bytes, 0xFF, sizeof(flash.bytes));
  CHECK(ParamLog_Load(&log, &io, 2U));
  CHECK(ParamLog_Set(&log, &io, 0x0100U, value, (uint16_t)sizeof(value)));
  flash.bytes[PARAM_STORE_IMAGE_SIZE] = 0x12U;
  flash.bytes[PARAM_STORE_IMAGE_SIZE + 1U] = 0x34U;

  CHECK(ParamLog_Load(&log, &io, 2U));
  CHECK(flash.erase_count == 1U);
  CHECK(log.next_slot == 1U);
  CHECK(read_value(&log, 0x0100U, actual, (uint16_t)sizeof(actual), &actual_length));
  CHECK(actual_length == (uint16_t)sizeof(value));
  CHECK(memcmp(actual, value, sizeof(value)) == 0);
}

static void test_clear_appends_empty_snapshot(void)
{
  FakeLogFlash_t flash = {0};
  ParamStoreIo_t io = {fake_read, fake_erase, fake_write, &flash};
  ParamLog_t log;
  ParamLog_t reloaded;
  const uint8_t value[] = {0x55U, 0xAAU};
  uint8_t actual[PARAM_STORE_VALUE_MAX] = {0};
  uint16_t actual_length = 0U;

  (void)memset(flash.bytes, 0xFF, sizeof(flash.bytes));
  CHECK(ParamLog_Load(&log, &io, 3U));
  CHECK(ParamLog_Set(&log, &io, 0x0100U, value, (uint16_t)sizeof(value)));
  CHECK(ParamLog_Clear(&log, &io));
  CHECK(log.next_slot == 2U);
  CHECK(flash.erase_count == 0U);
  CHECK(!ParamLog_Get(&log,
                      0x0100U,
                      actual,
                      (uint16_t)sizeof(actual),
                      &actual_length));

  CHECK(ParamLog_Load(&reloaded, &io, 3U));
  CHECK(flash.erase_count == 0U);
  CHECK(reloaded.next_slot == 2U);
  CHECK(!ParamLog_Get(&reloaded,
                      0x0100U,
                      actual,
                      (uint16_t)sizeof(actual),
                      &actual_length));
}

int main(void)
{
  test_append_and_reload();
  test_write_failure_keeps_log_position();
  test_partial_record_is_repaired_at_boot();
  test_clear_appends_empty_snapshot();

  if (failures != 0)
  {
    (void)fprintf(stderr, "parameter log host tests: %d failure(s)\n", failures);
    return 1;
  }

  (void)printf("parameter log host tests: PASS\n");
  return 0;
}
