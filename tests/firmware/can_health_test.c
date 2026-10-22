#include <stdint.h>
#include <stdio.h>

#include "can_health.h"

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

static void test_error_counters_drive_can_state(void)
{
  CanHealth_t health;
  uint8_t index;

  CanHealth_Init(&health, NULL);
  CHECK(health.config.passive_threshold == CAN_HEALTH_PASSIVE_THRESHOLD);
  CHECK(health.config.bus_off_threshold == CAN_HEALTH_BUS_OFF_THRESHOLD);
  CHECK(CanHealth_GetState(&health) == CAN_HEALTH_STATE_ERROR_ACTIVE);

  for (index = 0U; index < 15U; index++)
  {
    CanHealth_RecordError(&health, CAN_HEALTH_ERROR_ACK, true);
  }
  CHECK(health.tec == 120U);
  CHECK(CanHealth_GetState(&health) == CAN_HEALTH_STATE_ERROR_ACTIVE);

  CanHealth_RecordError(&health, CAN_HEALTH_ERROR_ACK, true);
  CHECK(health.tec == 128U);
  CHECK(CanHealth_GetState(&health) == CAN_HEALTH_STATE_ERROR_PASSIVE);

  for (index = 0U; index < 16U; index++)
  {
    CanHealth_RecordError(&health, CAN_HEALTH_ERROR_CRC, true);
  }
  CHECK(health.tec == 256U);
  CHECK(CanHealth_GetState(&health) == CAN_HEALTH_STATE_BUS_OFF);
  CHECK(health.bus_off_events == 1U);
  CHECK(health.error_counts[CAN_HEALTH_ERROR_ACK] == 16U);
  CHECK(health.error_counts[CAN_HEALTH_ERROR_CRC] == 16U);
  CHECK(health.error_counts[CAN_HEALTH_ERROR_BIT] == 0U);
}

static void test_receive_errors_advance_rec(void)
{
  CanHealth_t health;
  uint8_t index;

  CanHealth_Init(&health, NULL);
  for (index = 0U; index < 16U; index++)
  {
    CanHealth_RecordError(&health, CAN_HEALTH_ERROR_STUFF, false);
  }

  CHECK(health.tec == 0U);
  CHECK(health.rec == 128U);
  CHECK(CanHealth_GetState(&health) == CAN_HEALTH_STATE_ERROR_PASSIVE);
  CHECK(health.error_counts[CAN_HEALTH_ERROR_STUFF] == 16U);
}

static void test_success_decrements_counters(void)
{
  CanHealth_t health;

  CanHealth_Init(&health, NULL);
  CanHealth_RecordError(&health, CAN_HEALTH_ERROR_BIT, true);
  CHECK(health.tec == 8U);

  CanHealth_RecordSuccess(&health, true);
  CHECK(health.tec == 7U);

  CanHealth_EnterBusOff(&health);
  CanHealth_RecordSuccess(&health, true);
  CHECK(health.tec >= CAN_HEALTH_BUS_OFF_THRESHOLD);
  CHECK(CanHealth_GetState(&health) == CAN_HEALTH_STATE_BUS_OFF);
}

static void test_recovery_failures_request_reset(void)
{
  CanHealth_t health;
  uint8_t index;

  CanHealth_Init(&health, NULL);
  CanHealth_EnterBusOff(&health);

  for (index = 0U; index < 3U; index++)
  {
    CHECK(CanHealth_AttemptRecovery(&health, false) == false);
  }

  CHECK(health.recovery_attempts == 3U);
  CHECK(health.recovery_failures == 3U);
  CHECK(health.consecutive_recovery_failures == 3U);
  CHECK(health.reset_requests == 1U);
  CHECK(CanHealth_ResetRequested(&health) == true);

  CanHealth_AttemptRecovery(&health, false);
  CHECK(health.reset_requests == 2U);
}

static void test_successful_recovery_clears_state(void)
{
  CanHealth_t health;

  CanHealth_Init(&health, NULL);
  CanHealth_EnterBusOff(&health);
  CHECK(CanHealth_AttemptRecovery(&health, false) == false);
  CHECK(CanHealth_AttemptRecovery(&health, true) == true);

  CHECK(CanHealth_GetState(&health) == CAN_HEALTH_STATE_ERROR_ACTIVE);
  CHECK(health.tec == 0U);
  CHECK(health.rec == 0U);
  CHECK(health.recovery_successes == 1U);
  CHECK(health.consecutive_recovery_failures == 0U);
  CHECK(health.reset_requests == 0U);

  CanHealth_AttemptRecovery(&health, false);
  CHECK(health.consecutive_recovery_failures == 1U);
}

static void test_bus_off_event_counts_once(void)
{
  CanHealth_t health;

  CanHealth_Init(&health, NULL);
  CanHealth_EnterBusOff(&health);
  CanHealth_EnterBusOff(&health);
  CHECK(health.bus_off_events == 1U);

  CanHealth_AttemptRecovery(&health, true);
  CanHealth_EnterBusOff(&health);
  CHECK(health.bus_off_events == 2U);
}

static void test_hardware_counters_override_model(void)
{
  CanHealth_t health;

  CanHealth_Init(&health, NULL);
  CanHealth_RecordError(&health, CAN_HEALTH_ERROR_OTHER, true);
  CHECK(health.tec == 8U);

  CanHealth_SyncHardwareCounters(&health, 40U, 12U, false);
  CHECK(health.tec == 40U);
  CHECK(health.rec == 12U);
  CHECK(CanHealth_GetState(&health) == CAN_HEALTH_STATE_ERROR_ACTIVE);

  CanHealth_SyncHardwareCounters(&health, 130U, 0U, false);
  CHECK(CanHealth_GetState(&health) == CAN_HEALTH_STATE_ERROR_PASSIVE);

  CanHealth_SyncHardwareCounters(&health, 260U, 0U, true);
  CHECK(CanHealth_GetState(&health) == CAN_HEALTH_STATE_BUS_OFF);
  CHECK(health.tec == 260U);
  CHECK(health.bus_off_events == 1U);
}

static void test_configuration_fallback(void)
{
  CanHealth_t health;
  CanHealthConfig_t invalid;

  invalid.passive_threshold = 0U;
  invalid.bus_off_threshold = 0U;
  invalid.max_recovery_failures = 0U;

  CanHealth_Init(&health, &invalid);
  CHECK(health.config.passive_threshold == CAN_HEALTH_PASSIVE_THRESHOLD);
  CHECK(health.config.max_recovery_failures == CAN_HEALTH_MAX_RECOVERY_FAILURES);

  invalid.passive_threshold = 10U;
  invalid.bus_off_threshold = 20U;
  invalid.max_recovery_failures = 2U;
  CanHealth_Init(&health, &invalid);
  CHECK(health.config.bus_off_threshold == 20U);
  CHECK(health.config.max_recovery_failures == 2U);
}

static void test_snapshot_copies_state(void)
{
  CanHealth_t health;
  CanHealthSnapshot_t snapshot;
  uint8_t index;

  CanHealth_Init(&health, NULL);
  for (index = 0U; index < 4U; index++)
  {
    CanHealth_RecordError(&health, CAN_HEALTH_ERROR_FORM, true);
  }
  CanHealth_EnterBusOff(&health);
  CanHealth_AttemptRecovery(&health, true);
  CanHealth_GetSnapshot(&health, &snapshot);

  CHECK(snapshot.state == CAN_HEALTH_STATE_ERROR_ACTIVE);
  CHECK(snapshot.error_counts[CAN_HEALTH_ERROR_FORM] == 4U);
  CHECK(snapshot.bus_off_events == 1U);
  CHECK(snapshot.recovery_attempts == 1U);
  CHECK(snapshot.recovery_successes == 1U);
  CHECK(snapshot.reset_requests == 0U);
}

static void test_unknown_error_kind_counts_as_other(void)
{
  CanHealth_t health;

  CanHealth_Init(&health, NULL);
  CanHealth_RecordError(&health, (CanHealthErrorKind_t)99, true);
  CHECK(health.error_counts[CAN_HEALTH_ERROR_OTHER] == 1U);
}

static void test_error_flag_classification(void)
{
  CHECK(CanHealth_KindFromErrorFlags(0U) == CAN_HEALTH_ERROR_OTHER);
  CHECK(CanHealth_KindFromErrorFlags(CAN_HEALTH_ERROR_FLAG_STUFF) == CAN_HEALTH_ERROR_STUFF);
  CHECK(CanHealth_KindFromErrorFlags(CAN_HEALTH_ERROR_FLAG_FORM) == CAN_HEALTH_ERROR_FORM);
  CHECK(CanHealth_KindFromErrorFlags(CAN_HEALTH_ERROR_FLAG_ACK) == CAN_HEALTH_ERROR_ACK);
  CHECK(CanHealth_KindFromErrorFlags(CAN_HEALTH_ERROR_FLAG_CRC) == CAN_HEALTH_ERROR_CRC);
  CHECK(CanHealth_KindFromErrorFlags(CAN_HEALTH_ERROR_FLAG_BIT_RECESSIVE) == CAN_HEALTH_ERROR_BIT);
  CHECK(CanHealth_KindFromErrorFlags(CAN_HEALTH_ERROR_FLAG_BIT_DOMINANT) == CAN_HEALTH_ERROR_BIT);

  /* priority: bit beats everything, then stuff, ack, crc, form */
  CHECK(CanHealth_KindFromErrorFlags(CAN_HEALTH_ERROR_FLAG_ACK | CAN_HEALTH_ERROR_FLAG_CRC) == CAN_HEALTH_ERROR_ACK);
  CHECK(CanHealth_KindFromErrorFlags(CAN_HEALTH_ERROR_FLAG_FORM | CAN_HEALTH_ERROR_FLAG_STUFF) == CAN_HEALTH_ERROR_STUFF);
  CHECK(CanHealth_KindFromErrorFlags(CAN_HEALTH_ERROR_FLAG_BIT_DOMINANT | CAN_HEALTH_ERROR_FLAG_STUFF) == CAN_HEALTH_ERROR_BIT);

  /* warning/passive status bits are not error classes */
  CHECK(CanHealth_KindFromErrorFlags(0x00000001U | 0x00000002U) == CAN_HEALTH_ERROR_OTHER);
}

int main(void)
{
  test_error_counters_drive_can_state();
  test_receive_errors_advance_rec();
  test_success_decrements_counters();
  test_recovery_failures_request_reset();
  test_successful_recovery_clears_state();
  test_bus_off_event_counts_once();
  test_hardware_counters_override_model();
  test_configuration_fallback();
  test_snapshot_copies_state();
  test_unknown_error_kind_counts_as_other();
  test_error_flag_classification();

  if (failures != 0)
  {
    (void)fprintf(stderr, "firmware CAN health host tests: %d failure(s)\n", failures);
    return 1;
  }

  (void)printf("firmware CAN health host tests: PASS\n");
  return 0;
}
