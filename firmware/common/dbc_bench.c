#include "dbc_bench.h"

#include "project_dbc.h"

void DbcBench_Init(DbcBench_t *bench, DbcBenchNode_t node, uint32_t period_ms)
{
  bench->node = node;
  bench->period_ms = (period_ms == 0U) ? 100U : period_ms;
  bench->last_tx_ms = 0U;
  bench->started = false;
}

bool DbcBench_Poll(DbcBench_t *bench,
                   uint32_t now_ms,
                   uint32_t *arbitration_id,
                   uint8_t data[8])
{
  if (bench->started &&
      ((uint32_t)(now_ms - bench->last_tx_ms) < bench->period_ms))
  {
    return false;
  }

  bench->started = true;
  bench->last_tx_ms = now_ms;
  if (bench->node == DBC_BENCH_F407)
  {
    const EngineData_t engine = {1234.5, 85.0};
    EngineData_pack(&engine, data);
    *arbitration_id = 0x180U;
  }
  else
  {
    const VehicleStatus_t vehicle = {42.5, 1U};
    VehicleStatus_pack(&vehicle, data);
    *arbitration_id = 0x280U;
  }
  return true;
}
