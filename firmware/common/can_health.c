#include "can_health.h"

#include <string.h>

static uint16_t CanHealth_SaturatingAdd(uint16_t value, uint16_t delta)
{
  if ((uint32_t)value + (uint32_t)delta > 0xFFFFU)
  {
    return 0xFFFFU;
  }
  return (uint16_t)(value + delta);
}

static bool CanHealth_ConfigIsValid(const CanHealthConfig_t *config)
{
  return (config->passive_threshold > 0U) &&
         (config->bus_off_threshold > config->passive_threshold) &&
         (config->max_recovery_failures > 0U);
}

static void CanHealth_UpdateErrorState(CanHealth_t *health)
{
  uint16_t worst;

  if (health->state == CAN_HEALTH_STATE_BUS_OFF)
  {
    return;
  }

  if (health->tec >= health->config.bus_off_threshold)
  {
    CanHealth_EnterBusOff(health);
    return;
  }

  worst = (health->tec > health->rec) ? health->tec : health->rec;
  health->state = (worst >= health->config.passive_threshold)
                    ? CAN_HEALTH_STATE_ERROR_PASSIVE
                    : CAN_HEALTH_STATE_ERROR_ACTIVE;
}

CanHealthConfig_t CanHealth_DefaultConfig(void)
{
  CanHealthConfig_t config;

  config.passive_threshold = CAN_HEALTH_PASSIVE_THRESHOLD;
  config.bus_off_threshold = CAN_HEALTH_BUS_OFF_THRESHOLD;
  config.max_recovery_failures = CAN_HEALTH_MAX_RECOVERY_FAILURES;

  return config;
}

void CanHealth_Init(CanHealth_t *health, const CanHealthConfig_t *config)
{
  CanHealthConfig_t defaults;

  if (health == NULL)
  {
    return;
  }

  defaults = CanHealth_DefaultConfig();
  (void)memset(health, 0, sizeof(*health));
  health->config = (config != NULL && CanHealth_ConfigIsValid(config)) ? *config : defaults;
  health->state = CAN_HEALTH_STATE_ERROR_ACTIVE;
}

void CanHealth_RecordError(CanHealth_t *health, CanHealthErrorKind_t kind, bool transmit)
{
  uint8_t index;

  if (health == NULL)
  {
    return;
  }

  index = ((uint32_t)kind < (uint32_t)CAN_HEALTH_ERROR_KIND_COUNT)
            ? (uint8_t)kind
            : (uint8_t)CAN_HEALTH_ERROR_OTHER;
  health->error_counts[index]++;

  if (transmit)
  {
    health->tec = CanHealth_SaturatingAdd(health->tec, CAN_HEALTH_ERROR_COUNTER_STEP);
  }
  else
  {
    health->rec = CanHealth_SaturatingAdd(health->rec, CAN_HEALTH_ERROR_COUNTER_STEP);
  }

  CanHealth_UpdateErrorState(health);
}

void CanHealth_RecordSuccess(CanHealth_t *health, bool transmit)
{
  if (health == NULL)
  {
    return;
  }

  if (health->state == CAN_HEALTH_STATE_BUS_OFF)
  {
    return;
  }

  if (transmit && (health->tec > 0U))
  {
    health->tec--;
  }
  else if (!transmit && (health->rec > 0U))
  {
    health->rec--;
  }

  CanHealth_UpdateErrorState(health);
}

void CanHealth_SyncHardwareCounters(CanHealth_t *health,
                                    uint16_t tec,
                                    uint16_t rec,
                                    bool bus_off_active)
{
  if (health == NULL)
  {
    return;
  }

  health->tec = tec;
  health->rec = rec;

  if (bus_off_active)
  {
    CanHealth_EnterBusOff(health);
    return;
  }

  CanHealth_UpdateErrorState(health);
}

void CanHealth_EnterBusOff(CanHealth_t *health)
{
  if (health == NULL)
  {
    return;
  }

  if (health->state != CAN_HEALTH_STATE_BUS_OFF)
  {
    health->bus_off_events++;
  }

  health->state = CAN_HEALTH_STATE_BUS_OFF;
  if (health->tec < health->config.bus_off_threshold)
  {
    health->tec = health->config.bus_off_threshold;
  }
}

void CanHealth_MarkRecovered(CanHealth_t *health)
{
  if (health == NULL)
  {
    return;
  }

  health->state = CAN_HEALTH_STATE_ERROR_ACTIVE;
  health->tec = 0U;
  health->rec = 0U;
  health->recovery_successes++;
  health->consecutive_recovery_failures = 0U;
}

bool CanHealth_AttemptRecovery(CanHealth_t *health, bool recovered)
{
  if (health == NULL)
  {
    return false;
  }

  health->recovery_attempts++;

  if (recovered)
  {
    CanHealth_MarkRecovered(health);
    return true;
  }

  health->recovery_failures++;
  if (health->consecutive_recovery_failures < 0xFFU)
  {
    health->consecutive_recovery_failures++;
  }

  if (health->consecutive_recovery_failures >= health->config.max_recovery_failures)
  {
    health->reset_requests++;
  }

  return false;
}

CanHealthErrorKind_t CanHealth_KindFromErrorFlags(uint32_t flags)
{
  if ((flags & (CAN_HEALTH_ERROR_FLAG_BIT_RECESSIVE | CAN_HEALTH_ERROR_FLAG_BIT_DOMINANT)) != 0U)
  {
    return CAN_HEALTH_ERROR_BIT;
  }
  if ((flags & CAN_HEALTH_ERROR_FLAG_STUFF) != 0U)
  {
    return CAN_HEALTH_ERROR_STUFF;
  }
  if ((flags & CAN_HEALTH_ERROR_FLAG_ACK) != 0U)
  {
    return CAN_HEALTH_ERROR_ACK;
  }
  if ((flags & CAN_HEALTH_ERROR_FLAG_CRC) != 0U)
  {
    return CAN_HEALTH_ERROR_CRC;
  }
  if ((flags & CAN_HEALTH_ERROR_FLAG_FORM) != 0U)
  {
    return CAN_HEALTH_ERROR_FORM;
  }

  return CAN_HEALTH_ERROR_OTHER;
}

CanHealthState_t CanHealth_GetState(const CanHealth_t *health)
{
  return (health != NULL) ? health->state : CAN_HEALTH_STATE_ERROR_ACTIVE;
}

bool CanHealth_ResetRequested(const CanHealth_t *health)
{
  return (health != NULL) && (health->reset_requests > 0U);
}

void CanHealth_GetSnapshot(const CanHealth_t *health, CanHealthSnapshot_t *snapshot)
{
  if ((health == NULL) || (snapshot == NULL))
  {
    return;
  }

  snapshot->state = health->state;
  snapshot->tec = health->tec;
  snapshot->rec = health->rec;
  (void)memcpy(snapshot->error_counts,
               health->error_counts,
               sizeof(snapshot->error_counts));
  snapshot->bus_off_events = health->bus_off_events;
  snapshot->recovery_attempts = health->recovery_attempts;
  snapshot->recovery_successes = health->recovery_successes;
  snapshot->recovery_failures = health->recovery_failures;
  snapshot->consecutive_recovery_failures = health->consecutive_recovery_failures;
  snapshot->reset_requests = health->reset_requests;
}
