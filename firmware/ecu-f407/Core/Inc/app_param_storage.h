#ifndef APP_PARAM_STORAGE_H
#define APP_PARAM_STORAGE_H

#include "param_log.h"
#include "uds_did_db.h"

typedef struct
{
  ParamLog_t log;
  ParamStoreIo_t io;
  UdsDidStorage_t uds_storage;
  bool valid;
} AppParamStorage_t;

void AppParamStorage_Init(AppParamStorage_t *storage);

const UdsDidStorage_t *AppParamStorage_GetUdsStorage(
    AppParamStorage_t *storage);

bool AppParamStorage_IsValid(const AppParamStorage_t *storage);

bool AppParamStorage_Clear(AppParamStorage_t *storage);

#endif /* APP_PARAM_STORAGE_H */
