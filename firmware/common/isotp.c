#include "isotp.h"

#include <string.h>

/*
 * ISO 15765-2 (ISO-TP) transport core.
 *
 * Implements the single-frame (SF), first-frame (FF), consecutive-frame (CF)
 * and flow-control (FC) PCI types, a sender state machine and a receiver
 * state machine. Flow control supports CTS/WAIT/OVERFLOW, per-block waits
 * (BS) and the minimum transmit separation time (STmin) from the peer's FC.
 * N_As/N_Bs/N_Cr/N_Ar timeouts are driven by the channel tick.
 *
 * All state lives in the IsotpTx_t / IsotpRx_t structures, so the module is
 * reentrant and host-testable with a synthetic channel.
 */

/* A message that fits in one CAN frame is sent as a single frame (SF). */
#define ISOTP_SF_DLC(data_len) (1U + (uint8_t)(data_len))

#define ISOTP_FC_DLC 3U

static uint8_t g_rx_fallback_buffer[ISOTP_MAX_TOTAL_LENGTH];
static bool g_rx_fallback_in_use = false;

static uint32_t isotp_tick(const IsotpChannel_t *channel)
{
  if ((channel != NULL) && (channel->tick_ms != NULL))
  {
    return channel->tick_ms(channel->ctx);
  }
  return 0U;
}

static bool isotp_tx_pending(const IsotpTx_t *tx)
{
  return (tx->channel.tx_pending != NULL) &&
         tx->channel.tx_pending(tx->channel.ctx);
}

static void isotp_note_frame_sent(IsotpTx_t *tx, uint32_t now_ms)
{
  tx->tx_frame_pending = isotp_tx_pending(tx);
  tx->tx_as_deadline_ms = now_ms + tx->n_as_ms;
}

static bool isotp_send_sf(IsotpTx_t *tx, const uint8_t *data, uint16_t len)
{
  uint8_t frame[ISOTP_CAN_DLC];
  uint8_t i;

  frame[0] = (uint8_t)((ISOTP_PCI_SF << 4) | (uint8_t)len);
  for (i = 0U; i < len; i++)
  {
    frame[1U + i] = data[i];
  }
  for (i = (uint8_t)(1U + len); i < ISOTP_CAN_DLC; i++)
  {
    frame[i] = tx->padding;
  }

  if ((tx->channel.send == NULL) ||
      !tx->channel.send(tx->channel.ctx, tx->txid, frame))
  {
    return false;
  }
  tx->tx_offset = len;
  isotp_note_frame_sent(tx, isotp_tick(&tx->channel));
  return true;
}

static bool isotp_send_ff(IsotpTx_t *tx)
{
  uint8_t frame[ISOTP_CAN_DLC];
  uint8_t i;
  uint16_t chunk = ISOTP_FF_PAYLOAD_MAX;

  frame[0] = (uint8_t)((ISOTP_PCI_FF << 4) | ((tx->tx_total_length >> 8) & 0x0FU));
  frame[1] = (uint8_t)(tx->tx_total_length & 0xFFU);

  if (tx->tx_length < chunk)
  {
    chunk = tx->tx_length;
  }
  for (i = 0U; i < chunk; i++)
  {
    frame[2U + i] = tx->tx_data[i];
  }
  for (i = (uint8_t)(2U + chunk); i < ISOTP_CAN_DLC; i++)
  {
    frame[i] = tx->padding;
  }

  if ((tx->channel.send == NULL) ||
      !tx->channel.send(tx->channel.ctx, tx->txid, frame))
  {
    return false;
  }
  tx->tx_offset = chunk;
  tx->tx_ff_sent = true;
  tx->tx_waiting_fc = true;
  tx->tx_waiting_after_block = false;
  tx->tx_deadline_ms = isotp_tick(&tx->channel) + tx->n_bs_ms;
  isotp_note_frame_sent(tx, isotp_tick(&tx->channel));
  return true;
}

static bool isotp_send_cf(IsotpTx_t *tx, uint32_t now_ms)
{
  uint8_t frame[ISOTP_CAN_DLC];
  uint8_t i;
  uint16_t chunk = ISOTP_CF_PAYLOAD_MAX;
  uint16_t remaining = tx->tx_length - tx->tx_offset;

  if (remaining < chunk)
  {
    chunk = remaining;
  }

  frame[0] = (uint8_t)((ISOTP_PCI_CF << 4) | (tx->tx_sn & ISOTP_SEQ_MASK));
  for (i = 0U; i < chunk; i++)
  {
    frame[1U + i] = tx->tx_data[tx->tx_offset + i];
  }
  for (i = (uint8_t)(1U + chunk); i < ISOTP_CAN_DLC; i++)
  {
    frame[i] = tx->padding;
  }

  if ((tx->channel.send == NULL) ||
      !tx->channel.send(tx->channel.ctx, tx->txid, frame))
  {
    return false;
  }

  tx->tx_offset += chunk;
  tx->tx_sn = (uint8_t)((tx->tx_sn + 1U) & ISOTP_SEQ_MASK);
  tx->tx_sent_in_block++;
  tx->tx_has_sent_cf = true;
  tx->tx_last_cf_ms = now_ms;
  isotp_note_frame_sent(tx, now_ms);
  return true;
}

static bool isotp_send_fc(IsotpRx_t *rx, uint8_t status)
{
  uint8_t frame[ISOTP_CAN_DLC];
  uint8_t i;

  frame[0] = (uint8_t)((ISOTP_PCI_FC << 4) | (status & 0x0FU));
  frame[1] = rx->block_size;
  frame[2] = rx->stmin_ms;
  for (i = ISOTP_FC_DLC; i < ISOTP_CAN_DLC; i++)
  {
    frame[i] = rx->padding;
  }

  if ((rx->channel.send == NULL) ||
      !rx->channel.send(rx->channel.ctx, rx->txid, frame))
  {
    return false;
  }

  rx->fc_pending = (rx->channel.tx_pending != NULL) &&
                   rx->channel.tx_pending(rx->channel.ctx);
  rx->fc_deadline_ms = isotp_tick(&rx->channel) + rx->n_ar_ms;
  if (!rx->fc_pending)
  {
    rx->rx_deadline_ms = isotp_tick(&rx->channel) + rx->n_cr_ms;
  }
  return true;
}

void IsotpConfig_Init(IsotpConfig_t *config,
                      uint32_t txid,
                      uint32_t rxid,
                      uint8_t *rx_buffer,
                      uint16_t rx_buffer_size)
{
  if (config == NULL)
  {
    return;
  }

  config->txid = txid;
  config->rxid = rxid;
  config->n_as_ms = ISOTP_DEFAULT_N_AS_MS;
  config->n_bs_ms = ISOTP_DEFAULT_N_BS_MS;
  config->n_cr_ms = ISOTP_DEFAULT_N_CR_MS;
  config->n_ar_ms = ISOTP_DEFAULT_N_AR_MS;
  config->block_size = ISOTP_DEFAULT_BLOCK_SIZE;
  config->stmin_ms = ISOTP_DEFAULT_STMIN_MS;
  config->padding = ISOTP_DEFAULT_PADDING;
  config->rx_buffer = rx_buffer;
  config->rx_buffer_size = rx_buffer_size;
}

void IsotpTx_Init(IsotpTx_t *tx, const IsotpChannel_t *channel, const IsotpConfig_t *config)
{
  if (tx == NULL)
  {
    return;
  }
  tx->channel = (channel != NULL) ? *channel : (IsotpChannel_t){0};
  tx->txid = config->txid;
  tx->rxid = config->rxid;
  tx->n_as_ms = config->n_as_ms ? config->n_as_ms : ISOTP_DEFAULT_N_AS_MS;
  tx->n_bs_ms = config->n_bs_ms ? config->n_bs_ms : ISOTP_DEFAULT_N_BS_MS;
  tx->n_cr_ms = config->n_cr_ms ? config->n_cr_ms : ISOTP_DEFAULT_N_CR_MS;
  tx->padding = config->padding ? config->padding : ISOTP_DEFAULT_PADDING;
  tx->block_size = 0U;
  tx->stmin_ms = 0U;
  tx->tx_data = NULL;
  tx->tx_length = 0U;
  tx->tx_total_length = 0U;
  tx->tx_offset = 0U;
  tx->tx_sn = 1U;
  tx->tx_sent_in_block = 0U;
  tx->tx_deadline_ms = 0U;
  tx->tx_as_deadline_ms = 0U;
  tx->tx_ff_sent = false;
  tx->tx_has_sent_cf = false;
  tx->tx_waiting_fc = false;
  tx->tx_waiting_after_block = false;
  tx->tx_frame_pending = false;
  tx->tx_ongoing = false;
}

void IsotpRx_Init(IsotpRx_t *rx, const IsotpChannel_t *channel, const IsotpConfig_t *config)
{
  if (rx == NULL)
  {
    return;
  }
  rx->channel = (channel != NULL) ? *channel : (IsotpChannel_t){0};
  rx->txid = config->txid;
  rx->rxid = config->rxid;
  rx->n_ar_ms = config->n_ar_ms ? config->n_ar_ms : ISOTP_DEFAULT_N_AR_MS;
  rx->n_cr_ms = config->n_cr_ms ? config->n_cr_ms : ISOTP_DEFAULT_N_CR_MS;
  rx->padding = config->padding ? config->padding : ISOTP_DEFAULT_PADDING;
  rx->block_size = config->block_size ? config->block_size : ISOTP_DEFAULT_BLOCK_SIZE;
  rx->stmin_ms = config->stmin_ms;

  if ((config->rx_buffer != NULL) && (config->rx_buffer_size > 0U))
  {
    rx->buffer = config->rx_buffer;
    rx->buffer_size = config->rx_buffer_size;
  }
  else if (!g_rx_fallback_in_use)
  {
    rx->buffer = g_rx_fallback_buffer;
    rx->buffer_size = (uint16_t)sizeof(g_rx_fallback_buffer);
    g_rx_fallback_in_use = true;
  }
  else
  {
    rx->buffer = NULL;
    rx->buffer_size = 0U;
  }

  rx->declared_length = 0U;
  rx->received = 0U;
  rx->expected_sn = 1U;
  rx->cf_in_block = 0U;
  rx->rx_deadline_ms = 0U;
  rx->fc_deadline_ms = 0U;
  rx->rx_in_progress = false;
  rx->rx_timed_out = false;
  rx->fc_pending = false;
  rx->message_ready = false;
  rx->message_length = 0U;
}

IsotpResult_t IsotpTx_Start(IsotpTx_t *tx, const uint8_t *data, uint16_t len)
{
  if ((tx == NULL) || (data == NULL) || (len == 0U))
  {
    return ISOTP_ERR_INVALID;
  }
  if (tx->tx_ongoing)
  {
    return ISOTP_ERR_BUSY;
  }
  if (len > ISOTP_MAX_TOTAL_LENGTH)
  {
    return ISOTP_ERR_TOO_LONG;
  }

  tx->tx_data = data;
  tx->tx_length = len;
  tx->tx_total_length = len;
  tx->tx_offset = 0U;
  tx->tx_sn = 1U;
  tx->tx_sent_in_block = 0U;
  tx->tx_ff_sent = false;
  tx->tx_has_sent_cf = false;
  tx->tx_waiting_fc = false;
  tx->tx_waiting_after_block = false;
  tx->tx_frame_pending = false;
  tx->tx_last_cf_ms = 0U;
  tx->tx_as_deadline_ms = 0U;
  tx->tx_ongoing = true;

  if (len <= ISOTP_MAX_SINGLE_FRAME)
  {
    if (!isotp_send_sf(tx, data, len))
    {
      tx->tx_ongoing = false;
      return ISOTP_ERR_STATE;
    }
    if (!tx->tx_frame_pending)
    {
      tx->tx_ongoing = false;
    }
    return ISOTP_OK;
  }

  if (!isotp_send_ff(tx))
  {
    tx->tx_ongoing = false;
    return ISOTP_ERR_STATE;
  }
  return ISOTP_OK;
}

bool IsotpTx_IsBusy(const IsotpTx_t *tx)
{
  return (tx != NULL) && tx->tx_ongoing;
}

IsotpResult_t IsotpTx_OnFrame(IsotpTx_t *tx, const uint8_t data[ISOTP_CAN_DLC])
{
  uint8_t pci;
  uint8_t status;
  bool after_block;

  if ((tx == NULL) || (data == NULL) || !tx->tx_ongoing)
  {
    return ISOTP_ERR_STATE;
  }

  after_block = (tx->block_size > 0U) &&
                (tx->tx_sent_in_block >= tx->block_size);
  if (!tx->tx_waiting_fc && !after_block)
  {
    return ISOTP_ERR_STATE;
  }

  pci = (uint8_t)(data[0] >> 4);
  if (pci != ISOTP_PCI_FC)
  {
    return ISOTP_ERR_INVALID;
  }

  status = (uint8_t)(data[0] & 0x0FU);
  if (status == ISOTP_FC_WAIT)
  {
    /* Peer not ready; keep waiting within the N_* budget. */
    tx->tx_deadline_ms = isotp_tick(&tx->channel) +
                         ((tx->tx_waiting_after_block || after_block)
                              ? tx->n_cr_ms
                              : tx->n_bs_ms);
    return ISOTP_OK;
  }
  if (status != ISOTP_FC_CTS)
  {
    /* OVERFLOW or a reserved status: abandon the transfer. */
    tx->tx_ongoing = false;
    return (status == ISOTP_FC_OVERFLOW) ? ISOTP_ERR_OVERFLOW : ISOTP_ERR_NO_RESPONSE;
  }

  tx->block_size = data[1];
  tx->stmin_ms = data[2];
  tx->tx_waiting_fc = false;
  tx->tx_waiting_after_block = false;
  tx->tx_sent_in_block = 0U;
  tx->tx_deadline_ms = isotp_tick(&tx->channel) + tx->n_as_ms;
  return ISOTP_OK;
}

IsotpResult_t IsotpTx_Poll(IsotpTx_t *tx)
{
  uint32_t now;
  uint32_t elapsed;

  if (tx == NULL)
  {
    return ISOTP_ERR_INVALID;
  }
  if (!tx->tx_ongoing)
  {
    return ISOTP_OK;
  }

  now = isotp_tick(&tx->channel);

  if (tx->tx_frame_pending)
  {
    if (isotp_tx_pending(tx))
    {
      elapsed = now - tx->tx_as_deadline_ms;
      if (elapsed < 0x80000000U)
      {
        tx->tx_ongoing = false;
        tx->tx_frame_pending = false;
        return ISOTP_ERR_TIMEOUT;
      }
      return ISOTP_OK;
    }
    tx->tx_frame_pending = false;
  }

  /* Waiting for a flow-control frame: only N_Bs/N_Cr applies. */
  if (tx->tx_waiting_fc)
  {
    elapsed = now - tx->tx_deadline_ms;
    if (elapsed < 0x80000000U)
    {
      tx->tx_ongoing = false;
      return ISOTP_ERR_TIMEOUT;
    }
    return ISOTP_OK;
  }

  /* A block was granted: if the block is full, request the next FC. */
  if ((tx->block_size > 0U) && (tx->tx_sent_in_block >= tx->block_size))
  {
    tx->tx_waiting_fc = true;
    tx->tx_waiting_after_block = true;
    tx->tx_deadline_ms = now + tx->n_cr_ms;
    return ISOTP_OK;
  }

  /* All payload bytes have been written. */
  if (tx->tx_offset >= tx->tx_length)
  {
    tx->tx_ongoing = false;
    return ISOTP_OK;
  }

  /* Enforce STmin between consecutive CFs within a block. */
  if (tx->tx_has_sent_cf && (tx->stmin_ms > 0U))
  {
    elapsed = now - tx->tx_last_cf_ms;
    if (elapsed < (uint32_t)tx->stmin_ms)
    {
      return ISOTP_OK; /* not yet allowed to send the next CF */
    }
  }

  if (!isotp_send_cf(tx, now))
  {
    tx->tx_ongoing = false;
    return ISOTP_ERR_STATE;
  }
  return ISOTP_OK;
}

void IsotpTx_Reset(IsotpTx_t *tx)
{
  if (tx != NULL)
  {
    tx->tx_ongoing = false;
    tx->tx_waiting_fc = false;
    tx->tx_waiting_after_block = false;
    tx->tx_frame_pending = false;
    tx->tx_ff_sent = false;
    tx->tx_has_sent_cf = false;
    tx->tx_data = NULL;
    tx->tx_length = 0U;
    tx->tx_offset = 0U;
  }
}

IsotpResult_t IsotpRx_OnFrame(IsotpRx_t *rx, const uint8_t data[ISOTP_CAN_DLC])
{
  uint8_t pci;
  uint16_t declared;
  uint16_t chunk;
  uint8_t i;

  if ((rx == NULL) || (data == NULL))
  {
    return ISOTP_ERR_INVALID;
  }

  pci = (uint8_t)(data[0] >> 4);

  if (pci == ISOTP_PCI_SF)
  {
    uint8_t sf_len = (uint8_t)(data[0] & 0x0FU);
    if (rx->rx_in_progress)
    {
      return ISOTP_ERR_INVALID; /* a multi-frame transfer is already in flight */
    }
    if (sf_len > ISOTP_SF_PAYLOAD_MAX)
    {
      return ISOTP_ERR_INVALID;
    }
    if (rx->buffer == NULL)
    {
      return ISOTP_ERR_OVERFLOW;
    }
    for (i = 0U; i < sf_len; i++)
    {
      rx->buffer[i] = data[1U + i];
    }
    rx->message_length = sf_len;
    rx->message_ready = true;
    return ISOTP_OK;
  }

  if (pci == ISOTP_PCI_FF)
  {
    if (rx->rx_in_progress)
    {
      return ISOTP_ERR_INVALID;
    }
    declared = (uint16_t)(((data[0] & 0x0FU) << 8) | data[1]);
    if (declared > ISOTP_MAX_TOTAL_LENGTH)
    {
      return ISOTP_ERR_TOO_LONG;
    }
    if (rx->buffer == NULL)
    {
      (void)isotp_send_fc(rx, ISOTP_FC_OVERFLOW);
      return ISOTP_ERR_OVERFLOW;
    }
    if (declared > rx->buffer_size)
    {
      (void)isotp_send_fc(rx, ISOTP_FC_OVERFLOW);
      rx->rx_in_progress = false;
      return ISOTP_ERR_OVERFLOW;
    }

    rx->declared_length = declared;
    chunk = ISOTP_FF_PAYLOAD_MAX;
    if (rx->declared_length < chunk)
    {
      chunk = rx->declared_length;
    }
    for (i = 0U; i < chunk; i++)
    {
      rx->buffer[i] = data[2U + i];
    }
    rx->received = chunk;
    rx->expected_sn = 1U;
    rx->cf_in_block = 0U;
    rx->rx_in_progress = true;

    if (!isotp_send_fc(rx, ISOTP_FC_CTS))
    {
      rx->rx_in_progress = false;
      return ISOTP_ERR_STATE;
    }

    if (rx->received >= rx->declared_length)
    {
      rx->rx_in_progress = false;
      rx->message_length = rx->declared_length;
      rx->message_ready = true;
    }
    return ISOTP_OK;
  }

  if (pci == ISOTP_PCI_CF)
  {
    uint8_t sn;

    if (!rx->rx_in_progress)
    {
      return ISOTP_ERR_INVALID; /* CF without a matching FF */
    }
    sn = (uint8_t)(data[0] & 0x0FU);
    if (sn != rx->expected_sn)
    {
      return ISOTP_ERR_INVALID; /* sequence number out of order */
    }

    chunk = ISOTP_CF_PAYLOAD_MAX;
    if ((uint16_t)(rx->declared_length - rx->received) < chunk)
    {
      chunk = (uint16_t)(rx->declared_length - rx->received);
    }
    for (i = 0U; i < chunk; i++)
    {
      rx->buffer[rx->received + i] = data[1U + i];
    }
    rx->received += chunk;
    rx->expected_sn = (uint8_t)((rx->expected_sn + 1U) & ISOTP_SEQ_MASK);
    rx->cf_in_block++;
    rx->rx_deadline_ms = isotp_tick(&rx->channel) + rx->n_cr_ms;

    if (rx->received >= rx->declared_length)
    {
      rx->rx_in_progress = false;
      rx->message_length = rx->declared_length;
      rx->message_ready = true;
      return ISOTP_OK;
    }

    /* Advertise the next block when the configured block size is reached. */
    if ((rx->block_size > 0U) && (rx->cf_in_block >= rx->block_size))
    {
      rx->cf_in_block = 0U;
      if (!isotp_send_fc(rx, ISOTP_FC_CTS))
      {
        rx->rx_in_progress = false;
        return ISOTP_ERR_STATE;
      }
    }
    return ISOTP_OK;
  }

  return ISOTP_ERR_INVALID;
}

IsotpResult_t Isotp_RouteFrame(IsotpTx_t *tx,
                               IsotpRx_t *rx,
                               const uint8_t data[ISOTP_CAN_DLC])
{
  uint8_t pci;

  if (data == NULL)
  {
    return ISOTP_ERR_INVALID;
  }

  pci = (uint8_t)(data[0] >> 4);
  if (pci == ISOTP_PCI_FC)
  {
    if ((tx == NULL) || !IsotpTx_IsBusy(tx))
    {
      return ISOTP_ERR_STATE;
    }
    return IsotpTx_OnFrame(tx, data);
  }

  if (rx == NULL)
  {
    return ISOTP_ERR_STATE;
  }
  return IsotpRx_OnFrame(rx, data);
}

IsotpResult_t IsotpRx_Poll(IsotpRx_t *rx)
{
  uint32_t now;
  uint32_t elapsed;

  if (rx == NULL)
  {
    return ISOTP_ERR_INVALID;
  }
  if (!rx->rx_in_progress)
  {
    return ISOTP_OK;
  }

  now = isotp_tick(&rx->channel);

  if (rx->fc_pending)
  {
    if ((rx->channel.tx_pending != NULL) &&
        rx->channel.tx_pending(rx->channel.ctx))
    {
      elapsed = now - rx->fc_deadline_ms;
      if (elapsed < 0x80000000U)
      {
        rx->rx_in_progress = false;
        rx->fc_pending = false;
        rx->rx_timed_out = true;
        return ISOTP_ERR_TIMEOUT;
      }
      return ISOTP_OK;
    }
    rx->fc_pending = false;
    rx->rx_deadline_ms = now + rx->n_cr_ms;
    return ISOTP_OK;
  }

  elapsed = now - rx->rx_deadline_ms;
  if (elapsed < 0x80000000U)
  {
    /* No next CF arrived within N_Cr. */
    rx->rx_in_progress = false;
    rx->rx_timed_out = true;
    return ISOTP_ERR_TIMEOUT;
  }
  return ISOTP_OK;
}

bool IsotpRx_MessageReady(const IsotpRx_t *rx)
{
  return (rx != NULL) && rx->message_ready;
}

IsotpResult_t IsotpRx_Read(IsotpRx_t *rx, uint8_t *out, uint16_t out_capacity, uint16_t *out_len)
{
  uint16_t n;

  if ((rx == NULL) || (out == NULL) || (out_len == NULL))
  {
    return ISOTP_ERR_INVALID;
  }
  if (!rx->message_ready)
  {
    return ISOTP_ERR_STATE;
  }

  n = rx->message_length;
  if (n > out_capacity)
  {
    return ISOTP_ERR_TOO_LONG;
  }
  memcpy(out, rx->buffer, n);
  *out_len = n;
  rx->message_ready = false;
  rx->message_length = 0U;
  return ISOTP_OK;
}

bool IsotpRx_IsBusy(const IsotpRx_t *rx)
{
  return (rx != NULL) && rx->rx_in_progress;
}

void IsotpRx_Reset(IsotpRx_t *rx)
{
  if (rx != NULL)
  {
    rx->rx_in_progress = false;
    rx->rx_timed_out = false;
    rx->fc_pending = false;
    rx->message_ready = false;
    rx->message_length = 0U;
    rx->received = 0U;
    rx->declared_length = 0U;
  }
}
