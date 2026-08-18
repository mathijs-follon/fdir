#ifndef FDIR_RECOVERY_H
#define FDIR_RECOVERY_H

#include "types.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Entity descriptor registered with the framework.
 * Application supplies restart and optional decide() policy.
 */
typedef struct {
    const char *name;

    /** Max non-watchdog restarts. */
    uint8_t max_restarts;

    /** Max watchdog-triggered restarts. */
    uint8_t max_watchdog_restarts;

    /**
     * Default action when restart budget is exhausted.
     * Typically FDIR_ACTION_DEGRADE, UNAVAILABLE, SAFE, or REBOOT.
     */
    fdir_action_t on_exhausted;

    /**
     * Optional subsystem to mark when degrading / unavailable.
     * FDIR_SUBSYSTEM_NONE to skip.
     */
    fdir_subsystem_id_t linked_subsystem;

    /**
     * Recreate / resume the entity. Return 0 on success.
     * Required if restart actions are used.
     */
    int (*restart)(fdir_entity_id_t id, void *user);

    /**
     * Optional policy override. If non-NULL, return value drives recovery
     * instead of the default restart-budget path (except ACTION_NONE falls
     * through to default).
     */
    fdir_action_t (*decide)(fdir_entity_id_t id,
                            const fdir_failure_report_t *report,
                            uint8_t restarts_used, uint8_t watchdog_restarts_used,
                            void *user);

    void *user;
} fdir_entity_desc_t;


/** Sensible defaults: 500 ms period, 3 misses, dual-path SAFE at 2. */
fdir_config_t fdir_config_default(void);

/**
 * Initialise FDIR. Copies config (NULL uses fdir_config_default()).
 * Override the required weak port hooks in port.h before calling
 * (fdir_get_now_ms, fdir_post_failure, fdir_isolate_current_worker);
 * the defaults abort().
 */
fdir_status_t fdir_init(const fdir_config_t *config);

fdir_status_t fdir_entity_register(const fdir_entity_desc_t *desc, fdir_entity_id_t *out_id);

fdir_entity_id_t fdir_entity_count(void);

const char *fdir_entity_name(fdir_entity_id_t id);

fdir_mode_t fdir_system_mode(void);

/** Pointer to the copied config. Valid after fdir_init(). */
const fdir_config_t *fdir_config(void);

uint32_t fdir_heartbeat_max_age_ms(void);

/**
 * Supervisor entry: handle one failure report (log + recover).
 * Call after draining the application failure queue / mailbox.
 */
void fdir_handle_failure(const fdir_failure_report_t *report);


/**
 * Supervisor watchdog scan. Raises FDIR_REASON_WATCHDOG for stale entities
 * and runs the same recovery path.
 */
void fdir_check_watchdogs(void);

/** Force DEGRADED if currently NOMINAL. */
void fdir_enter_degraded_mode(void);

/** Force SAFE mode. */
void fdir_enter_safe_mode(void);

/**
 * Enter REBOOT_PENDING, emit a mode-change event, then call
 * fdir_request_reboot() (weak port hook; default is a no-op).
 */
void fdir_try_reboot(const char *reason);

/** Emit a free-form note via the event sink. */
void fdir_log_note(const char *note);


/**
 * Report a queue overflow as an event (application identifies the queue).
 * Does not change mode by itself.
 */
void fdir_log_queue_overflow(uint16_t queue_id, uint32_t overflow_delta, uint32_t depth_high_water);

#ifdef __cplusplus
}
#endif

#endif /* FDIR_RECOVERY_H */
