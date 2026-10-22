#ifndef CAN_HEALTH_H
#define CAN_HEALTH_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Portable CAN error and Bus-Off policy core.
 *
 * The bxCAN controller keeps TEC/REC in hardware, while this module mirrors the
 * policy contract used by the Python model in diagnoser/tools/can_health.py so
 * the firmware and the host tests share one set of rules.
 *
 * The module is HAL-free: can_driver.c reports error events and the hardware
 * counter values, and the application decides when to ask for a recovery
 * attempt or to reset the MCU.
 */

#define CAN_HEALTH_PASSIVE_THRESHOLD 128U
#define CAN_HEALTH_BUS_OFF_THRESHOLD 256U
#define CAN_HEALTH_MAX_RECOVERY_FAILURES 3U

/* Each transmit error advances TEC and each receive error advances REC, which
   matches the error counter behaviour modelled by the Python reference. */
#define CAN_HEALTH_ERROR_COUNTER_STEP 8U

/* Error flag bits as reported by the STM32 F1/F4 CAN HAL (HAL_CAN_ERROR_*).
   can_driver.c asserts at compile time that the HAL uses these same bits, so
   the classifier below stays host-testable without pulling in a HAL. */
#define CAN_HEALTH_ERROR_FLAG_STUFF 0x00000008U
#define CAN_HEALTH_ERROR_FLAG_FORM 0x00000010U
#define CAN_HEALTH_ERROR_FLAG_ACK 0x00000020U
#define CAN_HEALTH_ERROR_FLAG_BIT_RECESSIVE 0x00000040U
#define CAN_HEALTH_ERROR_FLAG_BIT_DOMINANT 0x00000080U
#define CAN_HEALTH_ERROR_FLAG_CRC 0x00000100U

typedef enum
{
  CAN_HEALTH_ERROR_BIT = 0,
  CAN_HEALTH_ERROR_STUFF,
  CAN_HEALTH_ERROR_ACK,
  CAN_HEALTH_ERROR_CRC,
  CAN_HEALTH_ERROR_FORM,
  CAN_HEALTH_ERROR_OTHER,
  CAN_HEALTH_ERROR_KIND_COUNT
} CanHealthErrorKind_t;

typedef enum
{
  CAN_HEALTH_STATE_ERROR_ACTIVE = 0,
  CAN_HEALTH_STATE_ERROR_PASSIVE,
  CAN_HEALTH_STATE_BUS_OFF
} CanHealthState_t;

typedef struct
{
  uint16_t passive_threshold;
  uint16_t bus_off_threshold;
  uint8_t max_recovery_failures;
} CanHealthConfig_t;

typedef struct
{
  CanHealthState_t state;
  uint16_t tec;
  uint16_t rec;
  uint32_t error_counts[CAN_HEALTH_ERROR_KIND_COUNT];
  uint32_t bus_off_events;
  uint32_t recovery_attempts;
  uint32_t recovery_successes;
  uint32_t recovery_failures;
  uint8_t consecutive_recovery_failures;
  uint32_t reset_requests;
} CanHealthSnapshot_t;

typedef struct
{
  CanHealthConfig_t config;
  CanHealthState_t state;
  uint16_t tec;
  uint16_t rec;
  uint32_t error_counts[CAN_HEALTH_ERROR_KIND_COUNT];
  uint32_t bus_off_events;
  uint32_t recovery_attempts;
  uint32_t recovery_successes;
  uint32_t recovery_failures;
  uint8_t consecutive_recovery_failures;
  uint32_t reset_requests;
} CanHealth_t;

CanHealthConfig_t CanHealth_DefaultConfig(void);

/* A NULL or invalid config falls back to the defaults above. */
void CanHealth_Init(CanHealth_t *health, const CanHealthConfig_t *config);

/* Classification counters plus the modelled TEC/REC values. Use these when the
   hardware counters are unavailable, for example in host tests. */
void CanHealth_RecordError(CanHealth_t *health, CanHealthErrorKind_t kind, bool transmit);
void CanHealth_RecordSuccess(CanHealth_t *health, bool transmit);

/* Overwrite TEC/REC with the values read back from the controller and
   re-evaluate the error state. Firmware should prefer this over the modelled
   counters once the receive/transmit paths are live. */
void CanHealth_SyncHardwareCounters(CanHealth_t *health,
                                    uint16_t tec,
                                    uint16_t rec,
                                    bool bus_off_active);

void CanHealth_EnterBusOff(CanHealth_t *health);
void CanHealth_MarkRecovered(CanHealth_t *health);

/* Call after the driver attempted to reinitialize the controller.
   Returns true when the recovery succeeded. When the configured consecutive
   failure limit is reached, reset_requests is incremented so the caller can
   trigger the MCU reset. */
bool CanHealth_AttemptRecovery(CanHealth_t *health, bool recovered);

/* Pick one error class out of a HAL error-code bitmask. Bit, stuff, ack, crc
   and form are checked in that order so a frame with several flags lands in a
   stable category; nothing recognised falls back to OTHER. */
CanHealthErrorKind_t CanHealth_KindFromErrorFlags(uint32_t flags);

CanHealthState_t CanHealth_GetState(const CanHealth_t *health);
bool CanHealth_ResetRequested(const CanHealth_t *health);
void CanHealth_GetSnapshot(const CanHealth_t *health, CanHealthSnapshot_t *snapshot);

#endif /* CAN_HEALTH_H */
