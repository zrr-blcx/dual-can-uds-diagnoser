#ifndef UDS_SERVER_H
#define UDS_SERVER_H

#include <stdbool.h>
#include <stdint.h>

/* Service identifiers */
#define UDS_SID_DIAGNOSTIC_SESSION_CONTROL 0x10U
#define UDS_SID_READ_DATA_BY_IDENTIFIER 0x22U
#define UDS_SID_SECURITY_ACCESS 0x27U
#define UDS_SID_WRITE_DATA_BY_IDENTIFIER 0x2EU
#define UDS_SID_ROUTINE_CONTROL 0x31U
#define UDS_SID_TESTER_PRESENT 0x3EU
#define UDS_SID_NEGATIVE_RESPONSE 0x7FU

/* Diagnostic sessions */
#define UDS_SESSION_DEFAULT 0x01U
#define UDS_SESSION_PROGRAMMING 0x02U
#define UDS_SESSION_EXTENDED 0x03U

/* Security access sub-functions */
#define UDS_SECURITY_REQUEST_SEED 0x01U
#define UDS_SECURITY_SEND_KEY 0x02U

/* Routine control sub-functions */
#define UDS_ROUTINE_START 0x01U
#define UDS_ROUTINE_STOP 0x02U
#define UDS_ROUTINE_REQUEST_RESULTS 0x03U

/* Negative response codes */
#define UDS_NRC_GENERAL_REJECT 0x10U
#define UDS_NRC_SERVICE_NOT_SUPPORTED 0x11U
#define UDS_NRC_SUBFUNCTION_NOT_SUPPORTED 0x12U
#define UDS_NRC_INCORRECT_LENGTH 0x13U
#define UDS_NRC_RESPONSE_TOO_LONG 0x14U
#define UDS_NRC_CONDITIONS_NOT_CORRECT 0x22U
#define UDS_NRC_REQUEST_SEQUENCE_ERROR 0x24U
#define UDS_NRC_REQUEST_OUT_OF_RANGE 0x31U
#define UDS_NRC_SECURITY_ACCESS_DENIED 0x33U
#define UDS_NRC_INVALID_KEY 0x35U
#define UDS_NRC_EXCEED_NUMBER_OF_ATTEMPTS 0x36U
#define UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED 0x37U
#define UDS_NRC_GENERAL_PROGRAMMING_FAILURE 0x72U
#define UDS_NRC_RESPONSE_PENDING 0x78U
#define UDS_NRC_SUBFUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION 0x7EU

/* Protocol limits */
#define UDS_SUPPRESS_POSITIVE_RESPONSE 0x80U
#define UDS_SUBFUNCTION_MASK 0x7FU
#define UDS_MAX_DIDS 16U
#define UDS_MAX_ROUTINES 8U
#define UDS_ROUTINE_RESULT_MAX 16U
#define UDS_PENDING_ROUTINE_DATA_MAX 16U
#define UDS_PENDING_DID_DATA_MAX 64U

/* Security limits and the Week 5 demo transform input */
#define UDS_SECURITY_SEED_SIZE 4U
#define UDS_SECURITY_KEY_SIZE 4U
#define UDS_SECURITY_MAX_ATTEMPTS 3U
#define UDS_SECURITY_DELAY_MS 10000U
#define UDS_DEFAULT_SECURITY_SALT 0x50324341U
#define UDS_DEFAULT_SEED 0x12345678U

/* Session bit masks use (1U << session_id) */
#define UDS_SESSION_MASK_DEFAULT (1U << UDS_SESSION_DEFAULT)
#define UDS_SESSION_MASK_PROGRAMMING (1U << UDS_SESSION_PROGRAMMING)
#define UDS_SESSION_MASK_EXTENDED (1U << UDS_SESSION_EXTENDED)
#define UDS_DEFAULT_SESSION_MASK \
  (UDS_SESSION_MASK_DEFAULT | UDS_SESSION_MASK_PROGRAMMING | UDS_SESSION_MASK_EXTENDED)
#define UDS_EXTENDED_PROGRAMMING_MASK \
  (UDS_SESSION_MASK_EXTENDED | UDS_SESSION_MASK_PROGRAMMING)

#define UDS_DEFAULT_P2_SERVER_MS 50U
#define UDS_DEFAULT_P2_STAR_SERVER_MS 5000U
#define UDS_DEFAULT_S3_SERVER_MS 5000U

typedef enum
{
  UDS_PROCESS_INVALID_ARGUMENT = 0,
  UDS_PROCESS_NO_RESPONSE = 1,
  UDS_PROCESS_RESPONSE = 2,
  UDS_PROCESS_BUFFER_TOO_SMALL = 3,
  UDS_PROCESS_RESPONSE_PENDING = 4
} UdsProcessResult_t;

typedef bool (*UdsDidLoadHandler_t)(uint16_t did,
                                    uint8_t *data,
                                    uint16_t capacity,
                                    uint16_t *length,
                                    void *context);

typedef bool (*UdsDidSaveHandler_t)(uint16_t did,
                                    const uint8_t *data,
                                    uint16_t length,
                                    void *context);

typedef struct
{
  uint16_t did;
  const uint8_t *data;
  uint16_t length;
  uint8_t *storage;
  uint16_t storage_capacity;
  uint8_t read_sessions_mask;  /* 0 = readable in every supported session */
  uint8_t write_sessions_mask; /* 0 = read-only DID */
  uint8_t write_required_session; /* 0 = no extra condition */
  bool write_requires_unlock;
  UdsDidLoadHandler_t storage_load;
  UdsDidSaveHandler_t storage_save;
  void *storage_context;
} UdsDid_t;

typedef enum
{
  UDS_ROUTINE_IDLE = 0,
  UDS_ROUTINE_RUNNING = 1,
  UDS_ROUTINE_COMPLETE = 2,
  UDS_ROUTINE_FAILED = 3
} UdsRoutineStatus_t;

typedef UdsRoutineStatus_t (*UdsRoutineHandler_t)(uint8_t subfunction,
                                                  const uint8_t *request_data,
                                                  uint16_t request_length,
                                                  uint8_t *result,
                                                  uint16_t result_capacity,
                                                  uint16_t *result_length,
                                                  void *context);

typedef struct
{
  uint16_t routine_id;
  uint8_t sessions_mask;
  bool requires_unlock;
  UdsRoutineHandler_t handler;
  void *context;
} UdsRoutine_t;

typedef struct
{
  const UdsDid_t *dids;
  uint16_t did_count;
  const UdsRoutine_t *routines;
  uint16_t routine_count;
  uint16_t p2_server_ms;
  uint16_t p2_star_server_ms;
  uint16_t s3_server_ms;
  uint8_t supported_sessions_mask;
  uint32_t security_salt;
} UdsServerConfig_t;

typedef struct
{
  UdsDid_t dids[UDS_MAX_DIDS];
  uint16_t did_count;
  UdsRoutine_t routines[UDS_MAX_ROUTINES];
  uint16_t routine_count;
  uint16_t p2_server_ms;
  uint16_t p2_star_server_ms;
  uint16_t s3_server_ms;
  uint8_t supported_sessions_mask;
  uint32_t security_salt;
  uint8_t session;
  bool s3_active;
  uint32_t s3_deadline_ms;
  uint32_t session_timeout_count;
  uint32_t seed_state;
  uint32_t pending_seed;
  bool seed_pending;
  bool security_unlocked;
  uint8_t security_attempts;
  uint32_t security_delay_until_ms;
  uint8_t routine_state[UDS_MAX_ROUTINES];
  uint8_t routine_result[UDS_MAX_ROUTINES][UDS_ROUTINE_RESULT_MAX];
  uint16_t routine_result_length[UDS_MAX_ROUTINES];
  bool pending_routine_response;
  uint8_t pending_routine_index;
  uint8_t pending_routine_subfunction;
  bool pending_routine_suppress_positive;
  uint8_t pending_routine_data[UDS_PENDING_ROUTINE_DATA_MAX];
  uint16_t pending_routine_data_length;
  bool pending_did_write;
  uint8_t pending_did_index;
  uint8_t pending_did_data[UDS_PENDING_DID_DATA_MAX];
  uint16_t pending_did_length;
} UdsServer_t;

bool UdsServer_Init(UdsServer_t *server, const UdsServerConfig_t *config);

void UdsServer_Tick(UdsServer_t *server, uint32_t now_ms);

UdsProcessResult_t UdsServer_Process(UdsServer_t *server,
                                     uint32_t now_ms,
                                     const uint8_t *request,
                                     uint16_t request_length,
                                     uint8_t *response,
                                     uint16_t response_capacity,
                                     uint16_t *response_length);

UdsProcessResult_t UdsServer_PollPending(UdsServer_t *server,
                                         uint32_t now_ms,
                                         uint8_t *response,
                                         uint16_t response_capacity,
                                         uint16_t *response_length);

uint8_t UdsServer_GetSession(const UdsServer_t *server);

uint32_t UdsServer_GetSessionTimeoutCount(const UdsServer_t *server);

bool UdsServer_IsSecurityUnlocked(const UdsServer_t *server);

uint8_t UdsServer_GetSecurityAttempts(const UdsServer_t *server);

uint8_t UdsServer_GetRoutineState(const UdsServer_t *server, uint16_t routine_id);

void UdsServer_LockSecurity(UdsServer_t *server);

uint32_t UdsSecurity_ComputeKey(uint32_t seed, uint32_t salt);

uint32_t UdsSecurity_NextSeed(uint32_t *state);

#endif /* UDS_SERVER_H */
