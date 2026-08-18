#ifndef FDIR_TYPES_H
#define FDIR_TYPES_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t fdir_entity_id_t;
typedef uint8_t fdir_subsystem_id_t;

#define FDIR_ENTITY_NONE ((fdir_entity_id_t)0xFFU)
#define FDIR_SUBSYSTEM_NONE ((fdir_subsystem_id_t)0xFFU)

#ifndef FDIR_DETAIL_SIZE
#define FDIR_DETAIL_SIZE 32
#endif

typedef enum {
    FDIR_OK = 0,
    FDIR_ERR_PARAM = -1,
    FDIR_ERR_FULL = -2,
    FDIR_ERR_NOT_FOUND = -3,
    FDIR_ERR_STATE = -4,
    FDIR_ERR_PORT = -5,
    FDIR_ERR_BUSY = -6,
} fdir_status_t;

/**
 * System operating mode (dependability / FDIR).
 * Transitions are driven only by the supervisor recovery path.
 */
typedef enum {
    FDIR_MODE_NOMINAL = 0,
    FDIR_MODE_DEGRADED,
    FDIR_MODE_SAFE,
    FDIR_MODE_REBOOT_PENDING,
} fdir_mode_t;

typedef enum {
    FDIR_HEALTH_OK = 0,
    FDIR_HEALTH_DEGRADED,
    FDIR_HEALTH_FAILED,
} fdir_health_t;

typedef enum {
    FDIR_REASON_INIT_FAILED = 0,
    FDIR_REASON_IO_ERROR,
    FDIR_REASON_TIMEOUT,
    FDIR_REASON_PROTOCOL_ERROR,
    FDIR_REASON_WATCHDOG,
    FDIR_REASON_QUEUE_OVERFLOW,
    FDIR_REASON_USER,
} fdir_reason_t;

/**
 * Recovery actions chosen by policy or default restart budget logic.
 * Application maps these onto its own restart / degrade / reboot hooks.
 */
typedef enum {
    FDIR_ACTION_NONE = 0,
    FDIR_ACTION_RESTART,
    FDIR_ACTION_DEGRADE,
    FDIR_ACTION_UNAVAILABLE,
    FDIR_ACTION_SAFE,
    FDIR_ACTION_REBOOT,
} fdir_action_t;

typedef struct {
    fdir_entity_id_t entity;
    fdir_reason_t    reason;
    uint16_t         error_code;
    char             detail[FDIR_DETAIL_SIZE];
    uint32_t         timestamp_ms;
} fdir_failure_report_t;

typedef struct {
    fdir_health_t health;
    uint32_t      last_heartbeat_ms;
    uint16_t      error_code;
    char          detail[FDIR_DETAIL_SIZE];
} fdir_health_snapshot_t;


typedef struct {
    /** Supervisor health-check period in milliseconds (watchdog base). */
    uint32_t health_check_period_ms;

    /** Missed periods before a heartbeat is treated as watchdog fault. */
    uint8_t missed_heartbeat_tolerance;

    /**
     * Enter SAFE when this many critical-path subsystems are unavailable
     * (dual-path pattern, e.g. downlink + storage).
     */
    uint8_t safe_mode_critical_failure_threshold;
} fdir_config_t;

#ifdef __cplusplus
}
#endif

#endif /* FDIR_TYPES_H */
