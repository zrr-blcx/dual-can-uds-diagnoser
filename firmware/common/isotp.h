#ifndef ISOTP_H
#define ISOTP_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Portable ISO 15765-2 (ISO-TP) transport core for the dual-node CAN/UDS
 * project.
 *
 * The module is HAL-free. It models a single sender (IsotpTx) and a single
 * receiver (IsotpRx) as event-driven state machines. The CAN driver or the
 * application feeds raw frames in and pulls complete payloads out; a host test
 * can therefore compile the same sources with a synthetic frame source.
 *
 * The behaviour mirrors the Python reference in diagnoser/transport/isotp.py,
 * but with the flow-control semantics that the Python side currently lacks:
 * CTS/WAIT/OVERFLOW, per-block waits, and respect for BS/STmin in the FC.
 *
 * Configuration constants are frozen here (Week 3 W3-01). Diagnostic IDs are
 * not hard-coded: each endpoint is given its own txid/rxid, so the two nodes
 * use 0x7E0/0x7E8 and 0x7E1/0x7E9 respectively.
 */

/* PCI types (high nibble of the first data byte). */
#define ISOTP_PCI_SF 0x0U
#define ISOTP_PCI_FF 0x1U
#define ISOTP_PCI_CF 0x2U
#define ISOTP_PCI_FC 0x3U

/* Flow control status values (low nibble of the first FC byte). */
#define ISOTP_FC_CTS 0x0U
#define ISOTP_FC_WAIT 0x1U
#define ISOTP_FC_OVERFLOW 0x2U

/* Frame payload capacities (bytes). */
#define ISOTP_SF_PAYLOAD_MAX 7U
#define ISOTP_FF_PAYLOAD_MAX 6U
#define ISOTP_CF_PAYLOAD_MAX 7U
#define ISOTP_CAN_DLC 8U

/* Protocol defaults (frozen). */
#define ISOTP_DEFAULT_PADDING 0xCCU
#define ISOTP_DEFAULT_BLOCK_SIZE 8U
#define ISOTP_DEFAULT_STMIN_MS 20U

/* Sequence number wraps 0x0..0xF. */
#define ISOTP_SEQ_MASK 0x0FU

/* A single-frame payload fits in one CAN DLC (7 data bytes + 1 PCI). */
#define ISOTP_MAX_SINGLE_FRAME (ISOTP_SF_PAYLOAD_MAX)
/* Total payload length field in the FF is 12 bits -> 0xFFF = 4095. */
#define ISOTP_MAX_TOTAL_LENGTH 0x0FFFU

/* Timeout budget values in milliseconds. Frozen defaults; override per
   instance if a slower bench requires it. */
#define ISOTP_DEFAULT_N_AS_MS 1000U /* sender: frame written to the bus */
#define ISOTP_DEFAULT_N_BS_MS 1000U /* sender: wait for the first FC */
#define ISOTP_DEFAULT_N_CR_MS 1000U /* sender: wait for a subsequent FC */
#define ISOTP_DEFAULT_N_AR_MS 1000U /* receiver: wait for the next CF */

/* Outcome codes shared by both state machines. */
typedef enum
{
  ISOTP_OK = 0,
  ISOTP_ERR_BUSY,        /* state machine already owns the buffer / a transfer */
  ISOTP_ERR_TOO_LONG,    /* payload or declared length exceeds a limit */
  ISOTP_ERR_INVALID,     /* malformed frame, bad SN, bad length, overflow */
  ISOTP_ERR_TIMEOUT,     /* N_As/N_Bs/N_Cr/N_Ar elapsed */
  ISOTP_ERR_OVERFLOW,    /* receiver told the peer FC_OVERFLOW */
  ISOTP_ERR_NO_RESPONSE, /* sender's peer rejected the transfer */
  ISOTP_ERR_STATE        /* internal state machine error */
} IsotpResult_t;

/* Message source/sink abstraction so the core stays host-testable.
   The application binds these to CanDriver_SendStd and the CAN RX callback. */
typedef struct
{
  /* Send one raw 8-byte CAN frame. Return true on success. */
  bool (*send)(void *ctx, uint32_t txid, const uint8_t data[ISOTP_CAN_DLC]);
  /* Return true if a full 8-byte frame is pending; if so, fills data and id.
     Returns false when no frame is available. */
  bool (*receive)(void *ctx, uint32_t *id, uint8_t data[ISOTP_CAN_DLC]);
  /* Return a monotonically increasing tick in milliseconds. */
  uint32_t (*tick_ms)(void *ctx);
  /* Return true while the last accepted CAN frame is still transmitting. */
  bool (*tx_pending)(void *ctx);
  void *ctx;
} IsotpChannel_t;

typedef struct
{
  uint32_t txid;
  uint32_t rxid;
  uint16_t n_as_ms;
  uint16_t n_bs_ms;
  uint16_t n_cr_ms;
  uint16_t n_ar_ms;
  /* Receiver configuration: block size and STmin sent back in the FC. */
  uint8_t block_size;   /* 0 means "send everything after one FC" (RFC ISO 15765-2) */
  uint8_t stmin_ms;
  uint8_t padding;
  /* Optional buffer for multi-frame reception. If null, a static fallback
     buffer of ISOTP_MAX_TOTAL_LENGTH is used. */
  uint8_t *rx_buffer;
  uint16_t rx_buffer_size;
} IsotpConfig_t;

/* Fill a configuration with the project's Week 3 defaults. */
void IsotpConfig_Init(IsotpConfig_t *config,
                      uint32_t txid,
                      uint32_t rxid,
                      uint8_t *rx_buffer,
                      uint16_t rx_buffer_size);

/* ---------------- Sender (request side) ---------------- */

typedef struct
{
  IsotpChannel_t channel;
  uint32_t txid;
  uint32_t rxid;
  uint16_t n_as_ms;
  uint16_t n_bs_ms;
  uint16_t n_cr_ms;
  uint8_t block_size;   /* from the peer's FC */
  uint8_t stmin_ms;     /* from the peer's FC */
  uint8_t padding;

  const uint8_t *tx_data;   /* payload to send */
  uint16_t tx_length;
  uint16_t tx_total_length;
  uint16_t tx_offset;       /* bytes already written to CF payloads */
  uint8_t tx_sn;            /* next CF sequence index */
  uint16_t tx_sent_in_block;/* CFs sent since the last FC */
  uint32_t tx_deadline_ms;  /* active N_As/N_Bs/N_Cr deadline */
  uint32_t tx_as_deadline_ms;
  uint32_t tx_last_cf_ms;   /* tick of the last CF, for STmin spacing */
  bool tx_ff_sent;
  bool tx_has_sent_cf;
  bool tx_waiting_fc;       /* waiting for the first or a subsequent FC */
  bool tx_waiting_after_block;
  bool tx_frame_pending;
  bool tx_ongoing;
} IsotpTx_t;

void IsotpTx_Init(IsotpTx_t *tx, const IsotpChannel_t *channel, const IsotpConfig_t *config);

/* Start sending payload (len bytes). The payload buffer must stay valid for
   the duration of the transfer. Returns ISOTP_OK or ISOTP_ERR_BUSY/TOO_LONG. */
IsotpResult_t IsotpTx_Start(IsotpTx_t *tx, const uint8_t *data, uint16_t len);

/* Drive the sender: transmit frames when due and process any received FC.
   Call repeatedly from the main loop. Returns ISOTP_OK when the transfer is
   still in progress, a completion result when it finished, or an error. */
IsotpResult_t IsotpTx_Poll(IsotpTx_t *tx);

/* True while a transfer is in progress (between Start and a terminal result). */
bool IsotpTx_IsBusy(const IsotpTx_t *tx);

/* Handle an FC frame received for this endpoint. Returns true if consumed. */
IsotpResult_t IsotpTx_OnFrame(IsotpTx_t *tx, const uint8_t data[ISOTP_CAN_DLC]);

/* ---------------- Receiver (response side) ---------------- */

typedef struct
{
  IsotpChannel_t channel;
  uint32_t txid;
  uint32_t rxid;
  uint16_t n_ar_ms;
  uint16_t n_cr_ms;
  uint8_t padding;
  uint8_t block_size;   /* BS value we advertise in FC frames */
  uint8_t stmin_ms;     /* STmin value we advertise in FC frames */

  uint8_t *buffer;
  uint16_t buffer_size;
  uint16_t declared_length; /* total payload length from the FF */
  uint16_t received;        /* payload bytes received so far */
  uint8_t expected_sn;      /* next CF sequence index expected */
  uint8_t cf_in_block;      /* CFs received since the last FC we sent */
  uint32_t rx_deadline_ms;  /* N_Cr anchor, refreshed on every received frame */
  uint32_t fc_deadline_ms;  /* N_Ar anchor for FC transmission */
  bool rx_in_progress;
  bool rx_timed_out;
  bool fc_pending;
  /* Complete payload waiting for the application to consume. */
  bool message_ready;
  uint16_t message_length;
} IsotpRx_t;

void IsotpRx_Init(IsotpRx_t *rx, const IsotpChannel_t *channel, const IsotpConfig_t *config);

/* Route one CAN frame by PCI type. Flow-control frames drive the sender when
   a transfer is active; SF/FF/CF frames drive the receiver. */
IsotpResult_t Isotp_RouteFrame(IsotpTx_t *tx,
                               IsotpRx_t *rx,
                               const uint8_t data[ISOTP_CAN_DLC]);

/* Feed a raw frame for this endpoint. Handles SF directly, starts a
   multi-frame transfer on FF, and appends CFs. Returns ISOTP_OK when the
   frame was consumed, or an error. */
IsotpResult_t IsotpRx_OnFrame(IsotpRx_t *rx, const uint8_t data[ISOTP_CAN_DLC]);

/* True when a complete message is buffered and waiting. */
bool IsotpRx_MessageReady(const IsotpRx_t *rx);

/* Copy the received message out. Returns ISOTP_OK and sets *out_len.
   Clears the ready flag so the next message can arrive. */
IsotpResult_t IsotpRx_Read(IsotpRx_t *rx, uint8_t *out, uint16_t out_capacity, uint16_t *out_len);

/* Drive receiver timeouts. Call from the main loop. */
IsotpResult_t IsotpRx_Poll(IsotpRx_t *rx);

/* True while a multi-frame reception is in progress. */
bool IsotpRx_IsBusy(const IsotpRx_t *rx);

/* Abort the current transfer and reset the endpoint. */
void IsotpRx_Reset(IsotpRx_t *rx);
void IsotpTx_Reset(IsotpTx_t *tx);

#endif /* ISOTP_H */
