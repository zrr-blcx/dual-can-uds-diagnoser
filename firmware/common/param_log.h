#ifndef PARAM_LOG_H
#define PARAM_LOG_H

#include <stdbool.h>
#include <stdint.h>

#include "param_store.h"

#define PARAM_LOG_MAX_SLOTS 1638U

typedef struct
{
  ParamStore_t store;
  uint16_t next_slot;
  uint16_t slot_count;
} ParamLog_t;

void ParamLog_Init(ParamLog_t *log);

bool ParamLog_Load(ParamLog_t *log,
                   const ParamStoreIo_t *io,
                   uint16_t slot_count);

bool ParamLog_Get(const ParamLog_t *log,
                  uint16_t did,
                  uint8_t *data,
                  uint16_t capacity,
                  uint16_t *length);

bool ParamLog_Set(ParamLog_t *log,
                  const ParamStoreIo_t *io,
                  uint16_t did,
                  const uint8_t *data,
                  uint16_t length);

bool ParamLog_Clear(ParamLog_t *log, const ParamStoreIo_t *io);

#endif /* PARAM_LOG_H */
