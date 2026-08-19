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

/** Set on fdir_failure_report_t.flags; queue must not drop silently. */
#define FDIR_FAULT_FLAG_CRITICAL 0x01U

/** max_restarts / max_watchdog_restarts: never exhaust the restart budget. */
#define FDIR_RESTART_UNLIMITED ((uint8_t)255U)

#ifndef FDIR_DETAIL_SIZE
#define FDIR_DETAIL_SIZE 32
#endif

#ifndef FDIR_NAME_SIZE
#define FDIR_NAME_SIZE 32
#endif

#ifndef FDIR_ENTITY_CAP
#define FDIR_ENTITY_CAP 16
#endif

#ifndef FDIR_SUBSYSTEM_CAP
#define FDIR_SUBSYSTEM_CAP 8
#endif

typedef uint8_t fdir_bool_t;

typedef enum {
    FDIR_OK = 0,
    FDIR_ERR_PARAM = -1,
    FDIR_ERR_FULL = -2,
    FDIR_ERR_NOT_FOUND = -3,
    FDIR_ERR_STATE = -4,
    /** port is NULL or a required fdir_port_t callback is NULL */
    FDIR_ERR_PORT = -5,
    FDIR_ERR_BUSY = -6,
} fdir_status_t;

/** Human-readable status for logging (static string, never NULL). */
const char *fdir_status_string(fdir_status_t status);

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

typedef enum {
    FDIR_ACTION_NONE = 0,
    FDIR_ACTION_RESTART,
    FDIR_ACTION_DEGRADE,
    FDIR_ACTION_UNAVAILABLE,
    FDIR_ACTION_SAFE,
    FDIR_ACTION_REBOOT,
} fdir_action_t;

/** FDIR hierarchy level that raised or handled an event. */
typedef enum {
    FDIR_LEVEL_ENTITY = 0,
    FDIR_LEVEL_SUBSYSTEM,
    FDIR_LEVEL_SYSTEM,
} fdir_level_t;

typedef enum {
    FDIR_SEVERITY_INFO = 0,
    FDIR_SEVERITY_WARNING,
    FDIR_SEVERITY_ERROR,
    FDIR_SEVERITY_CRITICAL,
} fdir_severity_t;

typedef enum {
    FDIR_ANOMALY_RAISED = 0,
    FDIR_ANOMALY_CLEARED,
} fdir_anomaly_phase_t;

/** Stable ground-facing anomaly key derived from entity and reason. */
#define FDIR_ANOMALY_ID(entity, reason) \
    ((uint16_t)(((uint16_t)(entity) << 8) | (uint16_t)(reason)))

/** Subsystem-scoped anomaly key (bit 15 set to distinguish from entity keys). */
#define FDIR_SUBSYSTEM_ANOMALY_ID(sub, reason) \
    ((uint16_t)(0x8000U | ((uint16_t)(sub) << 8) | (uint16_t)(reason)))

typedef struct {
    fdir_entity_id_t entity;
    fdir_reason_t    reason;
    uint16_t         error_code;
    char             detail[FDIR_DETAIL_SIZE];
    uint32_t         timestamp_ms;
    uint8_t          flags;
} fdir_failure_report_t;

typedef struct {
    fdir_health_t health;
    uint32_t      last_heartbeat_ms;
    uint16_t      error_code;
    char          detail[FDIR_DETAIL_SIZE];
} fdir_health_snapshot_t;

/**
 * Optional system-level hook invoked on mode escalation (SAFE, REBOOT_PENDING)
 * and on ground-requested mode changes via fdir_set_system_mode().
 * report may be NULL for mode-only transitions.
 */
typedef void (*fdir_system_handler_fn)(fdir_mode_t mode,
                                       fdir_level_t level,
                                       const fdir_failure_report_t *report,
                                       void *user);

typedef struct {
    uint32_t health_check_period_ms;
    uint8_t  missed_heartbeat_tolerance;
    uint8_t  safe_mode_critical_failure_threshold;
    fdir_system_handler_fn on_system_escalation;
    void *system_user;
} fdir_config_t;

typedef enum __attribute__((packed)) {
    FDIR_EVENT_FAILURE = 0,
    FDIR_EVENT_MODE_CHANGE,
    FDIR_EVENT_RESTART,
    FDIR_EVENT_WATCHDOG,
    FDIR_EVENT_NOTE,
    FDIR_EVENT_QUEUE_OVERFLOW,
} fdir_event_kind_t;

typedef struct {
    fdir_event_kind_t   kind;
    fdir_mode_t         mode;
    fdir_entity_id_t    entity;
    fdir_subsystem_id_t subsystem;
    fdir_reason_t       reason;
    uint16_t            error_code;
    uint32_t            timestamp_ms;
    char                detail[FDIR_DETAIL_SIZE];
    fdir_level_t        level;
    fdir_severity_t     severity;
    fdir_anomaly_phase_t phase;
    uint16_t            anomaly_id;
} fdir_event_t;

#ifdef __cplusplus
}
#endif

#endif /* FDIR_TYPES_H */
