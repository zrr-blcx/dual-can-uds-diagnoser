#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "isotp.h"

/* ------------------------------------------------------------------ */
/* Host-side fake channel: a shared FIFO bus and a controllable tick.  */
/* Both endpoints (sender and receiver) live on the same bus, so a     */
/* full send->receive round trip can be driven step by step.           */
/* ------------------------------------------------------------------ */

#define FAKE_QUEUE_SIZE 128U
#define FAKE_FRAME_LEN ISOTP_CAN_DLC

typedef struct
{
  uint8_t frames[FAKE_QUEUE_SIZE][FAKE_FRAME_LEN];
  uint32_t ids[FAKE_QUEUE_SIZE];
  uint16_t head;
  uint16_t tail;
  uint16_t count;
} FakeBus_t;

typedef struct
{
  FakeBus_t *bus;
  uint32_t tick;
  bool tx_pending;
  uint16_t send_busy_count;
} FakeCtx_t;

static FakeBus_t g_bus;
static FakeCtx_t g_ctx;

static void fake_bus_init(FakeBus_t *bus)
{
  memset(bus, 0, sizeof(*bus));
}

static bool fake_send(void *ctx, uint32_t txid, const uint8_t data[ISOTP_CAN_DLC])
{
  FakeCtx_t *c = (FakeCtx_t *)ctx;
  if (c->send_busy_count > 0U)
  {
    c->send_busy_count--;
    return false;
  }
  if (c->bus->count >= FAKE_QUEUE_SIZE)
  {
    return false;
  }
  c->bus->ids[c->bus->head] = txid;
  memcpy(c->bus->frames[c->bus->head], data, FAKE_FRAME_LEN);
  c->bus->head = (uint16_t)((c->bus->head + 1U) % FAKE_QUEUE_SIZE);
  c->bus->count++;
  return true;
}

static bool fake_receive(void *ctx, uint32_t *id, uint8_t data[ISOTP_CAN_DLC])
{
  FakeCtx_t *c = (FakeCtx_t *)ctx;
  if (c->bus->count == 0U)
  {
    return false;
  }
  *id = c->bus->ids[c->bus->tail];
  memcpy(data, c->bus->frames[c->bus->tail], FAKE_FRAME_LEN);
  c->bus->tail = (uint16_t)((c->bus->tail + 1U) % FAKE_QUEUE_SIZE);
  c->bus->count--;
  return true;
}

static uint32_t fake_tick(void *ctx)
{
  FakeCtx_t *c = (FakeCtx_t *)ctx;
  return c->tick;
}

static bool fake_tx_pending(void *ctx)
{
  FakeCtx_t *c = (FakeCtx_t *)ctx;
  return c->tx_pending;
}

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

/* Pump the shared bus between the two endpoints until the sender is done.
   Returns the number of iterations (bounded) and leaves the receiver state
   populated. Each iteration first drains every frame pending on the bus to
   its destination, then drives the sender. */
static int pump_bus(IsotpTx_t *tx, IsotpRx_t *rx, uint16_t max_steps)
{
  uint16_t step;
  uint8_t frame[ISOTP_CAN_DLC];
  uint32_t id;
  bool any;

  for (step = 0U; step < max_steps; step++)
  {
    g_ctx.tick += 1U;

    /* Deliver every frame currently on the bus. */
    any = true;
    while (any && fake_receive(&g_ctx, &id, frame))
    {
      uint8_t pci = (uint8_t)(frame[0] >> 4);
      if ((pci == ISOTP_PCI_FF) || (pci == ISOTP_PCI_CF) || (pci == ISOTP_PCI_SF))
      {
        (void)IsotpRx_OnFrame(rx, frame);
      }
      else if (pci == ISOTP_PCI_FC)
      {
        (void)IsotpTx_OnFrame(tx, frame);
      }
    }
    any = false;

    /* Drive the sender; the receiver only reacts to frames. */
    (void)IsotpTx_Poll(tx);

    if (!IsotpTx_IsBusy(tx))
    {
      /* Drain any final frames the sender emitted. */
      while (fake_receive(&g_ctx, &id, frame))
      {
        uint8_t pci = (uint8_t)(frame[0] >> 4);
        if ((pci == ISOTP_PCI_FF) || (pci == ISOTP_PCI_CF) || (pci == ISOTP_PCI_SF))
        {
          (void)IsotpRx_OnFrame(rx, frame);
        }
      }
      return (int)step;
    }
  }
  return (int)max_steps;
}

static void configure_channel(IsotpChannel_t *chan)
{
  memset(&g_bus, 0, sizeof(g_bus));
  fake_bus_init(&g_bus);
  g_ctx.bus = &g_bus;
  g_ctx.tick = 0U;
  g_ctx.tx_pending = false;
  g_ctx.send_busy_count = 0U;

  chan->send = fake_send;
  chan->receive = fake_receive;
  chan->tick_ms = fake_tick;
  chan->tx_pending = fake_tx_pending;
  chan->ctx = &g_ctx;
}

/* ---------- single frame ---------- */
static void test_single_frame(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpTx_t tx;
  IsotpRx_t rx;
  uint8_t payload[] = {0x10U, 0x03U, 0xAAU};
  uint8_t rx_buf[64];
  uint8_t out[64];
  uint16_t out_len = 0U;
  uint8_t frame[ISOTP_CAN_DLC];
  uint32_t id;

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E0U;
  cfg.rxid = 0x7E8U;
  cfg.rx_buffer = rx_buf;
  cfg.rx_buffer_size = sizeof(rx_buf);

  IsotpTx_Init(&tx, &chan, &cfg);
  IsotpRx_Init(&rx, &chan, &cfg);

  CHECK(IsotpTx_Start(&tx, payload, 3U) == ISOTP_OK);
  CHECK(!IsotpTx_IsBusy(&tx)); /* SF completes immediately */

  CHECK(fake_receive(&g_ctx, &id, frame));
  CHECK(id == 0x7E0U);
  CHECK((frame[0] >> 4) == ISOTP_PCI_SF);
  CHECK((frame[0] & 0x0FU) == 3U);
  CHECK(frame[1] == 0x10U && frame[2] == 0x03U && frame[3] == 0xAAU);

  CHECK(IsotpRx_OnFrame(&rx, frame) == ISOTP_OK);
  CHECK(IsotpRx_MessageReady(&rx));
  CHECK(IsotpRx_Read(&rx, out, sizeof(out), &out_len) == ISOTP_OK);
  CHECK(out_len == 3U);
  CHECK(out[0] == 0x10U && out[1] == 0x03U && out[2] == 0xAAU);
}

static void test_single_frame_rejects_zero_length(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpRx_t rx;
  uint8_t rx_buf[32];
  uint8_t frame[ISOTP_CAN_DLC] = {0};

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E8U;
  cfg.rxid = 0x7E0U;
  cfg.rx_buffer = rx_buf;
  cfg.rx_buffer_size = sizeof(rx_buf);
  IsotpRx_Init(&rx, &chan, &cfg);

  CHECK(IsotpRx_OnFrame(&rx, frame) == ISOTP_ERR_INVALID);
  CHECK(!IsotpRx_MessageReady(&rx));
}

static void test_single_frame_does_not_overwrite_unread_message(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpRx_t rx;
  uint8_t rx_buf[32];
  uint8_t first[ISOTP_CAN_DLC] = {0x03U, 0x10U, 0x03U, 0xAAU};
  uint8_t second[ISOTP_CAN_DLC] = {0x02U, 0x22U, 0xF1U};

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E8U;
  cfg.rxid = 0x7E0U;
  cfg.rx_buffer = rx_buf;
  cfg.rx_buffer_size = sizeof(rx_buf);
  IsotpRx_Init(&rx, &chan, &cfg);

  CHECK(IsotpRx_OnFrame(&rx, first) == ISOTP_OK);
  CHECK(IsotpRx_OnFrame(&rx, second) == ISOTP_ERR_BUSY);
  CHECK(IsotpRx_MessageReady(&rx));
}

static void test_first_frame_rejects_short_declared_length(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpRx_t rx;
  uint8_t rx_buf[32];
  uint8_t frame[ISOTP_CAN_DLC] = {0};

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E8U;
  cfg.rxid = 0x7E0U;
  cfg.rx_buffer = rx_buf;
  cfg.rx_buffer_size = sizeof(rx_buf);
  IsotpRx_Init(&rx, &chan, &cfg);

  frame[0] = 0x10U;
  frame[1] = 7U;
  CHECK(IsotpRx_OnFrame(&rx, frame) == ISOTP_ERR_INVALID);
  CHECK(!IsotpRx_IsBusy(&rx));
  CHECK(g_bus.count == 0U);
}

/* ---------- multi-frame, 100 bytes, BS=8, STmin=0 ---------- */
static void test_multi_frame_100(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpTx_t tx;
  IsotpRx_t rx;
  uint8_t payload[100];
  uint8_t rx_buf[200];
  uint8_t out[200];
  uint16_t out_len = 0U;
  uint16_t i;
  int steps;

  for (i = 0U; i < sizeof(payload); i++)
  {
    payload[i] = (uint8_t)i;
  }

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E0U;
  cfg.rxid = 0x7E8U;
  cfg.block_size = ISOTP_DEFAULT_BLOCK_SIZE;
  cfg.stmin_ms = 0U;
  cfg.rx_buffer = rx_buf;
  cfg.rx_buffer_size = sizeof(rx_buf);

  IsotpTx_Init(&tx, &chan, &cfg);
  IsotpRx_Init(&rx, &chan, &cfg);

  CHECK(IsotpTx_Start(&tx, payload, sizeof(payload)) == ISOTP_OK);
  CHECK(IsotpTx_IsBusy(&tx));

  steps = pump_bus(&tx, &rx, 500U);
  CHECK(steps < 500U);

  CHECK(!IsotpTx_IsBusy(&tx));
  CHECK(IsotpRx_MessageReady(&rx));
  CHECK(IsotpRx_Read(&rx, out, sizeof(out), &out_len) == ISOTP_OK);
  CHECK(out_len == sizeof(payload));
  CHECK(memcmp(out, payload, sizeof(payload)) == 0);
}

/* ---------- SN wraparound: 120 bytes -> 17 CF, SN hits 0x0F then 0x00 ---- */
static void test_sn_wraparound_120(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpTx_t tx;
  IsotpRx_t rx;
  uint8_t payload[120];
  uint8_t rx_buf[256];
  uint8_t out[256];
  uint16_t out_len = 0U;
  uint16_t i;
  int steps;

  for (i = 0U; i < sizeof(payload); i++)
  {
    payload[i] = (uint8_t)(0x5AU ^ (i & 0xFFU));
  }

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E0U;
  cfg.rxid = 0x7E8U;
  cfg.block_size = 0U; /* BS=0: send everything after one FC */
  cfg.stmin_ms = 0U;
  cfg.rx_buffer = rx_buf;
  cfg.rx_buffer_size = sizeof(rx_buf);

  IsotpTx_Init(&tx, &chan, &cfg);
  IsotpRx_Init(&rx, &chan, &cfg);

  CHECK(IsotpTx_Start(&tx, payload, sizeof(payload)) == ISOTP_OK);
  steps = pump_bus(&tx, &rx, 500U);
  CHECK(steps < 500U);
  CHECK(!IsotpTx_IsBusy(&tx));
  CHECK(IsotpRx_MessageReady(&rx));
  CHECK(IsotpRx_Read(&rx, out, sizeof(out), &out_len) == ISOTP_OK);
  CHECK(out_len == sizeof(payload));
  CHECK(memcmp(out, payload, sizeof(payload)) == 0);
}

/* ---------- N_BS timeout: FF sent, no FC arrives ---------- */
static void test_n_bs_timeout(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpTx_t tx;
  uint8_t payload[20];
  uint16_t i;

  for (i = 0U; i < sizeof(payload); i++)
  {
    payload[i] = (uint8_t)i;
  }

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E0U;
  cfg.rxid = 0x7E8U;
  cfg.n_bs_ms = 50U;

  IsotpTx_Init(&tx, &chan, &cfg);

  CHECK(IsotpTx_Start(&tx, payload, sizeof(payload)) == ISOTP_OK);
  CHECK(IsotpTx_IsBusy(&tx));

  /* Advance the clock past N_BS without any FC. */
  g_ctx.tick += 60U;
  CHECK(IsotpTx_Poll(&tx) == ISOTP_ERR_TIMEOUT);
  CHECK(!IsotpTx_IsBusy(&tx));
}

/* ---------- SN error: CF arrives out of order ---------- */
static void test_sn_error(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpRx_t rx;
  uint8_t rx_buf[64];
  uint8_t ff[8];
  uint8_t bad_cf[8];

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E0U;
  cfg.rxid = 0x7E8U;
  cfg.rx_buffer = rx_buf;
  cfg.rx_buffer_size = sizeof(rx_buf);

  IsotpRx_Init(&rx, &chan, &cfg);

  /* FF: 20 bytes total. */
  ff[0] = (uint8_t)((ISOTP_PCI_FF << 4) | 0x00U);
  ff[1] = 20U;
  ff[2] = 0x11U; ff[3] = 0x22U; ff[4] = 0x33U; ff[5] = 0x44U; ff[6] = 0x55U; ff[7] = 0x66U;
  CHECK(IsotpRx_OnFrame(&rx, ff) == ISOTP_OK);

  /* First CF must carry SN=1. */
  bad_cf[0] = (uint8_t)((ISOTP_PCI_CF << 4) | 0x02U); /* wrong SN */
  bad_cf[1] = 0xAAU; bad_cf[2] = 0xBBU; bad_cf[3] = 0xCCU; bad_cf[4] = 0xDDU;
  bad_cf[5] = 0xEEU; bad_cf[6] = 0xFFU; bad_cf[7] = 0xCCU;
  CHECK(IsotpRx_OnFrame(&rx, bad_cf) == ISOTP_ERR_INVALID);
  CHECK(!IsotpRx_MessageReady(&rx));
  CHECK(!IsotpRx_IsBusy(&rx));
  bad_cf[0] = (uint8_t)((ISOTP_PCI_CF << 4) | 0x01U);
  CHECK(IsotpRx_OnFrame(&rx, bad_cf) == ISOTP_ERR_INVALID);
}

static void test_duplicate_first_frame_aborts_transfer(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpRx_t rx;
  uint8_t rx_buf[64];
  uint8_t ff[ISOTP_CAN_DLC] = {0};

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E8U;
  cfg.rxid = 0x7E0U;
  cfg.rx_buffer = rx_buf;
  cfg.rx_buffer_size = sizeof(rx_buf);
  IsotpRx_Init(&rx, &chan, &cfg);

  ff[0] = 0x10U;
  ff[1] = 20U;
  CHECK(IsotpRx_OnFrame(&rx, ff) == ISOTP_OK);
  CHECK(IsotpRx_IsBusy(&rx));
  CHECK(IsotpRx_OnFrame(&rx, ff) == ISOTP_ERR_INVALID);
  CHECK(!IsotpRx_IsBusy(&rx));
}

/* ---------- buffer overflow: declared length exceeds buffer ---------- */
static void test_buffer_overflow(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpRx_t rx;
  uint8_t rx_buf[32];
  uint8_t ff[8];

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E0U;
  cfg.rxid = 0x7E8U;
  cfg.rx_buffer = rx_buf;
  cfg.rx_buffer_size = sizeof(rx_buf);

  IsotpRx_Init(&rx, &chan, &cfg);

  /* FF declares 100 bytes but buffer holds only 32. */
  ff[0] = (uint8_t)((ISOTP_PCI_FF << 4) | 0x00U);
  ff[1] = 100U;
  ff[2] = 0U; ff[3] = 0U; ff[4] = 0U; ff[5] = 0U; ff[6] = 0U; ff[7] = 0U;
  CHECK(IsotpRx_OnFrame(&rx, ff) == ISOTP_ERR_OVERFLOW);
  CHECK(!IsotpRx_IsBusy(&rx));
}

/* ---------- FC WAIT keeps the sender waiting ---------- */
static void test_fc_wait(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpTx_t tx;
  uint8_t payload[20];
  uint8_t fc[8];
  uint16_t i;

  for (i = 0U; i < sizeof(payload); i++)
  {
    payload[i] = (uint8_t)i;
  }

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E0U;
  cfg.rxid = 0x7E8U;
  cfg.n_bs_ms = 200U;

  IsotpTx_Init(&tx, &chan, &cfg);
  CHECK(IsotpTx_Start(&tx, payload, sizeof(payload)) == ISOTP_OK);
  CHECK(IsotpTx_IsBusy(&tx));

  /* Peer replies WAIT: sender keeps waiting, does not time out yet. */
  fc[0] = (uint8_t)((ISOTP_PCI_FC << 4) | ISOTP_FC_WAIT);
  fc[1] = 0U; fc[2] = 0U; fc[3] = 0U; fc[4] = 0U; fc[5] = 0U; fc[6] = 0U; fc[7] = 0U;
  CHECK(IsotpTx_OnFrame(&tx, fc) == ISOTP_OK);
  CHECK(IsotpTx_IsBusy(&tx));

  g_ctx.tick += 100U; /* still within N_BS after the WAIT refresh */
  CHECK(IsotpTx_Poll(&tx) == ISOTP_OK);
  CHECK(IsotpTx_IsBusy(&tx));

  /* Now CTS grants the transfer. */
  fc[0] = (uint8_t)((ISOTP_PCI_FC << 4) | ISOTP_FC_CTS);
  fc[1] = 0U; /* BS=0: send all */
  fc[2] = 0U;
  CHECK(IsotpTx_OnFrame(&tx, fc) == ISOTP_OK);
  /* The sender emits the remaining CFs on subsequent polls. */
  g_ctx.tick += 1U;
  CHECK(IsotpTx_Poll(&tx) == ISOTP_OK);
}

/* ---------- FC OVERFLOW aborts the sender ---------- */
static void test_fc_overflow(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpTx_t tx;
  uint8_t payload[20];
  uint8_t fc[8];
  uint16_t i;

  for (i = 0U; i < sizeof(payload); i++)
  {
    payload[i] = (uint8_t)i;
  }

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E0U;
  cfg.rxid = 0x7E8U;

  IsotpTx_Init(&tx, &chan, &cfg);
  CHECK(IsotpTx_Start(&tx, payload, sizeof(payload)) == ISOTP_OK);
  CHECK(IsotpTx_IsBusy(&tx));

  fc[0] = (uint8_t)((ISOTP_PCI_FC << 4) | ISOTP_FC_OVERFLOW);
  fc[1] = 0U; fc[2] = 0U; fc[3] = 0U; fc[4] = 0U; fc[5] = 0U; fc[6] = 0U; fc[7] = 0U;
  CHECK(IsotpTx_OnFrame(&tx, fc) == ISOTP_ERR_OVERFLOW);
  CHECK(!IsotpTx_IsBusy(&tx));
}

/* ---------- STmin: CFs are spaced by the peer's STmin ---------- */
static void test_stmin_spacing(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpTx_t tx;
  uint8_t payload[20];
  uint8_t fc[8];
  uint16_t i;
  uint8_t frame[ISOTP_CAN_DLC];
  uint32_t id;
  uint16_t cf_before, cf_after;

  for (i = 0U; i < sizeof(payload); i++)
  {
    payload[i] = (uint8_t)i;
  }

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E0U;
  cfg.rxid = 0x7E8U;
  cfg.n_bs_ms = 200U;
  cfg.stmin_ms = 10U; /* the sender obeys the STmin from the FC */

  IsotpTx_Init(&tx, &chan, &cfg);
  CHECK(IsotpTx_Start(&tx, payload, sizeof(payload)) == ISOTP_OK);

  /* Consume the FF. */
  CHECK(fake_receive(&g_ctx, &id, frame));

  /* CTS with STmin=10. */
  fc[0] = (uint8_t)((ISOTP_PCI_FC << 4) | ISOTP_FC_CTS);
  fc[1] = 0U;
  fc[2] = 10U;
  CHECK(IsotpTx_OnFrame(&tx, fc) == ISOTP_OK);

  /* Right away: only one CF allowed (the first). */
  cf_before = g_ctx.bus->count;
  (void)IsotpTx_Poll(&tx);
  cf_after = g_ctx.bus->count;
  CHECK(cf_after == cf_before + 1U);

  /* Advance < STmin: still no new CF. */
  g_ctx.tick += 5U;
  cf_before = g_ctx.bus->count;
  (void)IsotpTx_Poll(&tx);
  CHECK(g_ctx.bus->count == cf_before);

  /* Advance past STmin: next CF allowed. */
  g_ctx.tick += 6U;
  cf_before = g_ctx.bus->count;
  (void)IsotpTx_Poll(&tx);
  CHECK(g_ctx.bus->count == cf_before + 1U);
}

/* ---------- STmin also spans a BS block boundary ---------- */
static void test_stmin_spans_block_boundary(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpTx_t tx;
  uint8_t payload[20];
  uint8_t frame[ISOTP_CAN_DLC];
  uint8_t fc[ISOTP_CAN_DLC] = {0};
  uint32_t id = 0U;
  uint16_t i;

  for (i = 0U; i < sizeof(payload); i++)
  {
    payload[i] = (uint8_t)i;
  }

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E8U;
  cfg.rxid = 0x7E0U;

  IsotpTx_Init(&tx, &chan, &cfg);
  CHECK(IsotpTx_Start(&tx, payload, sizeof(payload)) == ISOTP_OK);
  CHECK(fake_receive(&g_ctx, &id, frame));

  fc[0] = (uint8_t)((ISOTP_PCI_FC << 4) | ISOTP_FC_CTS);
  fc[1] = 1U;
  fc[2] = 20U;
  CHECK(IsotpTx_OnFrame(&tx, fc) == ISOTP_OK);
  CHECK(IsotpTx_Poll(&tx) == ISOTP_OK);
  CHECK(fake_receive(&g_ctx, &id, frame));
  CHECK((frame[0] >> 4) == ISOTP_PCI_CF);
  CHECK(IsotpTx_Poll(&tx) == ISOTP_OK);

  g_ctx.tick += 5U;
  fc[1] = 0U;
  CHECK(IsotpTx_OnFrame(&tx, fc) == ISOTP_OK);
  CHECK(IsotpTx_Poll(&tx) == ISOTP_OK);
  CHECK(g_ctx.bus->count == 0U);

  g_ctx.tick += 15U;
  CHECK(IsotpTx_Poll(&tx) == ISOTP_OK);
  CHECK(fake_receive(&g_ctx, &id, frame));
  CHECK((frame[0] >> 4) == ISOTP_PCI_CF);
}

/* ---------- FC direction: request on 0x7E0, FC on 0x7E8 ---------- */
static void test_flow_control_uses_response_id(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpRx_t rx;
  uint8_t rx_buf[32];
  uint8_t ff[8] = {0};
  uint8_t fc[ISOTP_CAN_DLC];
  uint32_t id = 0U;

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E8U;
  cfg.rxid = 0x7E0U;
  cfg.rx_buffer = rx_buf;
  cfg.rx_buffer_size = sizeof(rx_buf);

  IsotpRx_Init(&rx, &chan, &cfg);

  ff[0] = (uint8_t)((ISOTP_PCI_FF << 4) | 0x00U);
  ff[1] = 20U;
  CHECK(IsotpRx_OnFrame(&rx, ff) == ISOTP_OK);
  CHECK(fake_receive(&g_ctx, &id, fc));
  CHECK(id == cfg.txid);
  CHECK((fc[0] >> 4) == ISOTP_PCI_FC);
}

/* ---------- frame dispatch: FC resumes the sender, data reaches the receiver ---------- */
static void test_frame_dispatch_routes_flow_control_to_sender(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpTx_t tx;
  IsotpRx_t rx;
  uint8_t payload[20];
  uint8_t rx_buf[32];
  uint8_t frame[ISOTP_CAN_DLC];
  uint8_t fc[ISOTP_CAN_DLC] = {0};
  uint32_t id = 0U;
  uint16_t i;

  for (i = 0U; i < sizeof(payload); i++)
  {
    payload[i] = (uint8_t)i;
  }

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E8U;
  cfg.rxid = 0x7E0U;
  cfg.rx_buffer = rx_buf;
  cfg.rx_buffer_size = sizeof(rx_buf);

  IsotpTx_Init(&tx, &chan, &cfg);
  IsotpRx_Init(&rx, &chan, &cfg);
  CHECK(IsotpTx_Start(&tx, payload, sizeof(payload)) == ISOTP_OK);

  CHECK(fake_receive(&g_ctx, &id, frame));
  CHECK(id == cfg.txid);
  CHECK((frame[0] >> 4) == ISOTP_PCI_FF);

  fc[0] = (uint8_t)((ISOTP_PCI_FC << 4) | ISOTP_FC_CTS);
  fc[1] = ISOTP_DEFAULT_BLOCK_SIZE;
  fc[2] = ISOTP_DEFAULT_STMIN_MS;
  CHECK(Isotp_RouteFrame(&tx, &rx, fc) == ISOTP_OK);

  CHECK(IsotpTx_Poll(&tx) == ISOTP_OK);
  CHECK(fake_receive(&g_ctx, &id, frame));
  CHECK((frame[0] >> 4) == ISOTP_PCI_CF);
}

/* ---------- shared defaults: BS=8, STmin=20 ms, padding=0xCC ---------- */
static void test_config_init_applies_project_defaults(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpRx_t rx;
  uint8_t rx_buf[32];
  uint8_t ff[ISOTP_CAN_DLC] = {0};
  uint8_t fc[ISOTP_CAN_DLC];
  uint32_t id = 0U;

  configure_channel(&chan);
  IsotpConfig_Init(&cfg, 0x7E8U, 0x7E0U, rx_buf, sizeof(rx_buf));

  CHECK(cfg.txid == 0x7E8U);
  CHECK(cfg.rxid == 0x7E0U);
  CHECK(cfg.block_size == 8U);
  CHECK(cfg.stmin_ms == 20U);
  CHECK(cfg.padding == 0xCCU);
  CHECK(cfg.n_as_ms == ISOTP_DEFAULT_N_AS_MS);
  CHECK(cfg.n_bs_ms == ISOTP_DEFAULT_N_BS_MS);
  CHECK(cfg.n_cr_ms == ISOTP_DEFAULT_N_CR_MS);
  CHECK(cfg.n_ar_ms == ISOTP_DEFAULT_N_AR_MS);

  IsotpRx_Init(&rx, &chan, &cfg);
  ff[0] = (uint8_t)((ISOTP_PCI_FF << 4) | 0x00U);
  ff[1] = 20U;
  CHECK(IsotpRx_OnFrame(&rx, ff) == ISOTP_OK);
  CHECK(fake_receive(&g_ctx, &id, fc));
  CHECK(id == 0x7E8U);
  CHECK(fc[1] == 8U);
  CHECK(fc[2] == 20U);
}

static void test_config_allows_zero_block_size(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpRx_t rx;
  uint8_t rx_buf[32];
  uint8_t ff[ISOTP_CAN_DLC] = {0};
  uint8_t fc[ISOTP_CAN_DLC];
  uint32_t id = 0U;

  configure_channel(&chan);
  IsotpConfig_Init(&cfg, 0x7E8U, 0x7E0U, rx_buf, sizeof(rx_buf));
  cfg.block_size = 0U;
  IsotpRx_Init(&rx, &chan, &cfg);

  ff[0] = 0x10U;
  ff[1] = 20U;
  CHECK(IsotpRx_OnFrame(&rx, ff) == ISOTP_OK);
  CHECK(fake_receive(&g_ctx, &id, fc));
  CHECK(fc[1] == 0U);
}

/* ---------- N_Cr: receiver stalls waiting for the next CF ---------- */
static void test_n_cr_receiver_timeout(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpRx_t rx;
  uint8_t rx_buf[32];
  uint8_t ff[ISOTP_CAN_DLC] = {0};

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E8U;
  cfg.rxid = 0x7E0U;
  cfg.n_cr_ms = 50U;
  cfg.rx_buffer = rx_buf;
  cfg.rx_buffer_size = sizeof(rx_buf);

  IsotpRx_Init(&rx, &chan, &cfg);
  ff[0] = (uint8_t)((ISOTP_PCI_FF << 4) | 0x00U);
  ff[1] = 20U;
  CHECK(IsotpRx_OnFrame(&rx, ff) == ISOTP_OK);

  g_ctx.tick += 51U;
  CHECK(IsotpRx_Poll(&rx) == ISOTP_ERR_TIMEOUT);
  CHECK(!IsotpRx_IsBusy(&rx));
}

/* ---------- after a transmitted FC, CF waiting uses N_Cr, not N_Ar ---------- */
static void test_receiver_uses_n_cr_after_flow_control(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpRx_t rx;
  uint8_t rx_buf[32];
  uint8_t ff[ISOTP_CAN_DLC] = {0};

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E8U;
  cfg.rxid = 0x7E0U;
  cfg.n_ar_ms = 10U;
  cfg.n_cr_ms = 1000U;
  cfg.rx_buffer = rx_buf;
  cfg.rx_buffer_size = sizeof(rx_buf);

  IsotpRx_Init(&rx, &chan, &cfg);
  ff[0] = (uint8_t)((ISOTP_PCI_FF << 4) | 0x00U);
  ff[1] = 20U;
  CHECK(IsotpRx_OnFrame(&rx, ff) == ISOTP_OK);

  g_ctx.tick += 11U;
  CHECK(IsotpRx_Poll(&rx) == ISOTP_OK);
  CHECK(IsotpRx_IsBusy(&rx));
}

/* ---------- N_Ar: FC cannot finish transmitting ---------- */
static void test_n_ar_timeout_while_fc_pending(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpRx_t rx;
  uint8_t rx_buf[32];
  uint8_t ff[ISOTP_CAN_DLC] = {0};

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E8U;
  cfg.rxid = 0x7E0U;
  cfg.n_ar_ms = 10U;
  cfg.n_cr_ms = 1000U;
  cfg.rx_buffer = rx_buf;
  cfg.rx_buffer_size = sizeof(rx_buf);

  IsotpRx_Init(&rx, &chan, &cfg);
  g_ctx.tx_pending = true;
  ff[0] = (uint8_t)((ISOTP_PCI_FF << 4) | 0x00U);
  ff[1] = 20U;
  CHECK(IsotpRx_OnFrame(&rx, ff) == ISOTP_OK);

  g_ctx.tick += 11U;
  CHECK(IsotpRx_Poll(&rx) == ISOTP_ERR_TIMEOUT);
  CHECK(!IsotpRx_IsBusy(&rx));
}

static void test_timeout_across_tick_wrap(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpRx_t rx;
  uint8_t rx_buf[32];
  uint8_t ff[ISOTP_CAN_DLC] = {0};

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E8U;
  cfg.rxid = 0x7E0U;
  cfg.n_ar_ms = 10U;
  cfg.rx_buffer = rx_buf;
  cfg.rx_buffer_size = sizeof(rx_buf);
  IsotpRx_Init(&rx, &chan, &cfg);

  g_ctx.tx_pending = true;
  g_ctx.tick = 0xFFFFFFF0U;
  ff[0] = 0x10U;
  ff[1] = 20U;
  CHECK(IsotpRx_OnFrame(&rx, ff) == ISOTP_OK);
  g_ctx.tick = 0x00000010U;
  CHECK(IsotpRx_Poll(&rx) == ISOTP_ERR_TIMEOUT);
  CHECK(!IsotpRx_IsBusy(&rx));
}

/* ---------- N_Cr: sender stalls waiting for the next block FC ---------- */
static void test_n_cr_timeout(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpTx_t tx;
  uint8_t payload[20];
  uint8_t frame[ISOTP_CAN_DLC];
  uint8_t fc[ISOTP_CAN_DLC] = {0};
  uint32_t id = 0U;
  uint16_t i;

  for (i = 0U; i < sizeof(payload); i++)
  {
    payload[i] = (uint8_t)i;
  }

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E8U;
  cfg.rxid = 0x7E0U;
  cfg.n_cr_ms = 50U;
  cfg.stmin_ms = 0U;

  IsotpTx_Init(&tx, &chan, &cfg);
  CHECK(IsotpTx_Start(&tx, payload, sizeof(payload)) == ISOTP_OK);
  CHECK(fake_receive(&g_ctx, &id, frame));

  fc[0] = (uint8_t)((ISOTP_PCI_FC << 4) | ISOTP_FC_CTS);
  fc[1] = 1U;
  fc[2] = 0U;
  CHECK(IsotpTx_OnFrame(&tx, fc) == ISOTP_OK);
  CHECK(IsotpTx_Poll(&tx) == ISOTP_OK);
  CHECK(fake_receive(&g_ctx, &id, frame));
  CHECK(IsotpTx_Poll(&tx) == ISOTP_OK);

  g_ctx.tick += 51U;
  CHECK(IsotpTx_Poll(&tx) == ISOTP_ERR_TIMEOUT);
  CHECK(!IsotpTx_IsBusy(&tx));
}

/* ---------- N_As: the CAN frame never finishes transmitting ---------- */
static void test_n_as_timeout(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpTx_t tx;
  uint8_t payload[20];
  uint16_t i;

  for (i = 0U; i < sizeof(payload); i++)
  {
    payload[i] = (uint8_t)i;
  }

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E8U;
  cfg.rxid = 0x7E0U;
  cfg.n_as_ms = 10U;
  cfg.n_bs_ms = 1000U;

  IsotpTx_Init(&tx, &chan, &cfg);
  g_ctx.tx_pending = true;
  CHECK(IsotpTx_Start(&tx, payload, sizeof(payload)) == ISOTP_OK);
  CHECK(IsotpTx_IsBusy(&tx));

  g_ctx.tick += 11U;
  CHECK(IsotpTx_Poll(&tx) == ISOTP_ERR_TIMEOUT);
  CHECK(!IsotpTx_IsBusy(&tx));
}

static void test_tx_retries_busy_transmit_mailbox(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpTx_t tx;
  uint8_t payload[20];
  uint8_t frame[ISOTP_CAN_DLC];
  uint32_t id = 0U;
  uint16_t i;

  for (i = 0U; i < sizeof(payload); i++)
  {
    payload[i] = (uint8_t)i;
  }

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E8U;
  cfg.rxid = 0x7E0U;
  cfg.n_as_ms = 100U;
  IsotpTx_Init(&tx, &chan, &cfg);

  g_ctx.send_busy_count = 1U;
  CHECK(IsotpTx_Start(&tx, payload, sizeof(payload)) == ISOTP_OK);
  CHECK(IsotpTx_IsBusy(&tx));
  CHECK(g_ctx.bus->count == 0U);

  CHECK(IsotpTx_Poll(&tx) == ISOTP_OK);
  CHECK(fake_receive(&g_ctx, &id, frame));
  CHECK(id == 0x7E8U);
  CHECK((frame[0] >> 4) == ISOTP_PCI_FF);
}

static void test_tx_times_out_waiting_for_mailbox(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpTx_t tx;
  uint8_t payload[20];
  uint16_t i;

  for (i = 0U; i < sizeof(payload); i++)
  {
    payload[i] = (uint8_t)i;
  }

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E8U;
  cfg.rxid = 0x7E0U;
  cfg.n_as_ms = 10U;
  IsotpTx_Init(&tx, &chan, &cfg);

  g_ctx.send_busy_count = 100U;
  CHECK(IsotpTx_Start(&tx, payload, sizeof(payload)) == ISOTP_OK);
  g_ctx.tick += 11U;
  CHECK(IsotpTx_Poll(&tx) == ISOTP_ERR_TIMEOUT);
  CHECK(!IsotpTx_IsBusy(&tx));
}

static void test_rx_retries_busy_flow_control_enqueue(void)
{
  IsotpChannel_t chan;
  IsotpConfig_t cfg;
  IsotpRx_t rx;
  uint8_t rx_buf[32];
  uint8_t ff[ISOTP_CAN_DLC] = {0};
  uint8_t fc[ISOTP_CAN_DLC];
  uint32_t id = 0U;

  configure_channel(&chan);
  memset(&cfg, 0, sizeof(cfg));
  cfg.txid = 0x7E8U;
  cfg.rxid = 0x7E0U;
  cfg.rx_buffer = rx_buf;
  cfg.rx_buffer_size = sizeof(rx_buf);
  cfg.n_ar_ms = 100U;
  IsotpRx_Init(&rx, &chan, &cfg);

  g_ctx.send_busy_count = 1U;
  ff[0] = 0x10U;
  ff[1] = 20U;
  CHECK(IsotpRx_OnFrame(&rx, ff) == ISOTP_OK);
  CHECK(IsotpRx_IsBusy(&rx));
  CHECK(g_ctx.bus->count == 0U);

  CHECK(IsotpRx_Poll(&rx) == ISOTP_OK);
  CHECK(fake_receive(&g_ctx, &id, fc));
  CHECK(id == 0x7E8U);
  CHECK((fc[0] >> 4) == ISOTP_PCI_FC);
}

int main(void)
{
  test_single_frame();
  test_single_frame_rejects_zero_length();
  test_single_frame_does_not_overwrite_unread_message();
  test_first_frame_rejects_short_declared_length();
  test_multi_frame_100();
  test_sn_wraparound_120();
  test_n_bs_timeout();
  test_sn_error();
  test_duplicate_first_frame_aborts_transfer();
  test_buffer_overflow();
  test_fc_wait();
  test_fc_overflow();
  test_stmin_spacing();
  test_stmin_spans_block_boundary();
  test_flow_control_uses_response_id();
  test_frame_dispatch_routes_flow_control_to_sender();
  test_config_init_applies_project_defaults();
  test_config_allows_zero_block_size();
  test_n_cr_receiver_timeout();
  test_receiver_uses_n_cr_after_flow_control();
  test_n_ar_timeout_while_fc_pending();
  test_timeout_across_tick_wrap();
  test_n_cr_timeout();
  test_n_as_timeout();
  test_tx_retries_busy_transmit_mailbox();
  test_tx_times_out_waiting_for_mailbox();
  test_rx_retries_busy_flow_control_enqueue();

  if (failures != 0)
  {
    (void)fprintf(stderr, "firmware ISO-TP host tests: %d failure(s)\n", failures);
    return 1;
  }

  (void)printf("firmware ISO-TP host tests: PASS\n");
  return 0;
}
