#ifndef PARAM_STORE_H
#define PARAM_STORE_H

#include <stdbool.h>
#include <stdint.h>

#define PARAM_STORE_MAX_ENTRIES 3U
#define PARAM_STORE_VALUE_MAX 17U
#define PARAM_STORE_IMAGE_SIZE 80U
#define PARAM_STORE_MAGIC 0x50324442U
#define PARAM_STORE_VERSION 1U

typedef bool (*ParamStoreRead_t)(uint32_t offset,
                                 uint8_t *data,
                                 uint16_t length,
                                 void *context);

typedef bool (*ParamStoreErase_t)(void *context);

typedef bool (*ParamStoreWrite_t)(uint32_t offset,
                                  const uint8_t *data,
                                  uint16_t length,
                                  void *context);

typedef struct
{
  ParamStoreRead_t read;
  ParamStoreErase_t erase;
  ParamStoreWrite_t write;
  void *context;
} ParamStoreIo_t;

typedef struct
{
  uint16_t did;
  uint8_t length;
  uint8_t data[PARAM_STORE_VALUE_MAX];
} ParamStoreEntry_t;

typedef struct
{
  ParamStoreEntry_t entries[PARAM_STORE_MAX_ENTRIES];
} ParamStore_t;

void ParamStore_Init(ParamStore_t *store);

bool ParamStore_Load(ParamStore_t *store, const ParamStoreIo_t *io);

bool ParamStore_Get(const ParamStore_t *store,
                    uint16_t did,
                    uint8_t *data,
                    uint16_t capacity,
                    uint16_t *length);

bool ParamStore_Update(ParamStore_t *store,
                       uint16_t did,
                       const uint8_t *data,
                       uint16_t length);

bool ParamStore_Encode(const ParamStore_t *store,
                       uint8_t image[PARAM_STORE_IMAGE_SIZE]);

bool ParamStore_Decode(ParamStore_t *store,
                       const uint8_t image[PARAM_STORE_IMAGE_SIZE]);

bool ParamStore_Set(ParamStore_t *store,
                    const ParamStoreIo_t *io,
                    uint16_t did,
                    const uint8_t *data,
                    uint16_t length);

#endif /* PARAM_STORE_H */
