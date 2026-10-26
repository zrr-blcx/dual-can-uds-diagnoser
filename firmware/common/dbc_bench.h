#ifndef DBC_BENCH_H
#define DBC_BENCH_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  DBC_BENCH_F407 = 0,
  DBC_BENCH_F103 = 1
} DbcBenchNode_t;

typedef struct
{
  DbcBenchNode_t node;
  uint32_t period_ms;
  uint32_t last_tx_ms;
  bool started;
} DbcBench_t;

void DbcBench_Init(DbcBench_t *bench, DbcBenchNode_t node, uint32_t period_ms);

bool DbcBench_Poll(DbcBench_t *bench,
                   uint32_t now_ms,
                   uint32_t *arbitration_id,
                   uint8_t data[8]);

#endif /* DBC_BENCH_H */
