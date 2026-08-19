#include "recovery.h"
#include "failure_queue.h"
#include "health.h"
#include "health_internal.h"
#include "internal.h"
#include "port.h"
#include "subsystem.h"
#include "subsystem_internal.h"
#include "types.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    fdir_entity_desc_t desc;
    char               name[FDIR_NAME_SIZE];
    uint8_t            used;
    uint8_t            restart_count;
    uint8_t            watchdog_restart_count;
} fdir_entity_slot_t;

static fdir_entity_slot_t g_entities[FDIR_ENTITY_CAP];
static uint8_t g_entity_count;
static fdir_mode_t g_mode;
static fdir_bool_t g_supervision_enabled;

static fdir_severity_t severity_for_event(fdir_event_kind_t kind, fdir_reason_t reason, fdir_mode_t mode)
{
    if (kind == FDIR_EVENT_MODE_CHANGE) {
        if (mode >= FDIR_MODE_SAFE) {
            return FDIR_SEVERITY_CRITICAL;
        }
        if (mode == FDIR_MODE_DEGRADED) {
            return FDIR_SEVERITY_WARNING;
        }
        return FDIR_SEVERITY_INFO;
    }
    if (kind == FDIR_EVENT_RESTART) {
        return FDIR_SEVERITY_INFO;
    }
    if (kind == FDIR_EVENT_QUEUE_OVERFLOW) {
        return FDIR_SEVERITY_WARNING;
    }
    if (reason == FDIR_REASON_WATCHDOG || reason == FDIR_REASON_INIT_FAILED) {
        return FDIR_SEVERITY_ERROR;
    }
    if (kind == FDIR_EVENT_FAILURE || kind == FDIR_EVENT_WATCHDOG) {
        return FDIR_SEVERITY_ERROR;
    }
    return FDIR_SEVERITY_INFO;
}

static void emit_event_full(fdir_event_kind_t kind, fdir_mode_t mode, fdir_entity_id_t entity,
                            fdir_reason_t reason, uint16_t error_code, fdir_level_t level,
                            fdir_anomaly_phase_t phase, const char *detail)
{
    fdir_event_t event;

    memset(&event, 0, sizeof(event));
    event.kind = kind;
    event.mode = mode;
    event.entity = entity;
    event.reason = reason;
    event.error_code = error_code;
    event.timestamp_ms = fdir_get_now_ms();
    event.level = level;
    event.phase = phase;
    event.subsystem = FDIR_SUBSYSTEM_NONE;
    event.severity = severity_for_event(kind, reason, mode);
    if (entity != FDIR_ENTITY_NONE) {
        event.anomaly_id = FDIR_ANOMALY_ID(entity, reason);
    }
    fdir_internal_copy_detail(event.detail, sizeof(event.detail), detail);
    fdir_internal_emit_event(&event);
}

void fdir_internal_emit_subsystem_state(fdir_subsystem_id_t sub,
                                        fdir_entity_id_t entity,
                                        fdir_reason_t reason,
                                        uint16_t error_code,
                                        fdir_anomaly_phase_t phase,
                                        const char *detail)
{
    fdir_event_t event;
    fdir_severity_t severity = FDIR_SEVERITY_WARNING;

    if (phase == FDIR_ANOMALY_CLEARED) {
        severity = FDIR_SEVERITY_INFO;
    } else if (detail != NULL &&
               (strcmp(detail, "unavailable") == 0 || strcmp(detail, "policy_override") == 0)) {
        severity = FDIR_SEVERITY_ERROR;
    }

    memset(&event, 0, sizeof(event));
    event.kind = FDIR_EVENT_FAILURE;
    event.mode = g_mode;
    event.entity = entity;
    event.subsystem = sub;
    event.reason = reason;
    event.error_code = error_code;
    event.timestamp_ms = fdir_get_now_ms();
    event.level = FDIR_LEVEL_SUBSYSTEM;
    event.phase = phase;
    event.severity = severity;
    event.anomaly_id = FDIR_SUBSYSTEM_ANOMALY_ID(sub, reason);
    fdir_internal_copy_detail(event.detail, sizeof(event.detail), detail);
    fdir_internal_emit_event(&event);
}

static void notify_system_handler(fdir_mode_t mode, const fdir_failure_report_t *report)
{
    const fdir_config_t *cfg = fdir_internal_get_config();

    if (cfg->on_system_escalation != NULL) {
        cfg->on_system_escalation(mode, FDIR_LEVEL_SYSTEM, report, cfg->system_user);
    }
}

static void evaluate_dual_path(void)
{
    const fdir_config_t *cfg = fdir_internal_get_config();

    if (fdir_subsystems_critical_unavailable_count() >= cfg->safe_mode_critical_failure_threshold) {
        fdir_enter_safe_mode();
    }
}

static void set_mode(fdir_mode_t mode, fdir_entity_id_t entity, const char *detail,
                     const fdir_failure_report_t *report)
{
    if (g_mode == mode) {
        return;
    }

    g_mode = mode;
    emit_event_full(FDIR_EVENT_MODE_CHANGE, g_mode, entity, FDIR_REASON_USER, 0U,
                    FDIR_LEVEL_SYSTEM, FDIR_ANOMALY_RAISED, detail);
    notify_system_handler(g_mode, report);
}

static fdir_action_t default_decide(const fdir_entity_slot_t *slot, const fdir_failure_report_t *report)
{
    if (report->reason == FDIR_REASON_WATCHDOG) {
        if (slot->desc.max_watchdog_restarts == FDIR_RESTART_UNLIMITED
            || slot->watchdog_restart_count < slot->desc.max_watchdog_restarts) {
            return FDIR_ACTION_RESTART;
        }
        return slot->desc.on_exhausted;
    }

    if (slot->desc.max_restarts == FDIR_RESTART_UNLIMITED
        || slot->restart_count < slot->desc.max_restarts) {
        return FDIR_ACTION_RESTART;
    }

    return slot->desc.on_exhausted;
}

static void bump_restart(fdir_entity_slot_t *slot, fdir_reason_t reason)
{
    if (reason == FDIR_REASON_WATCHDOG) {
        if (slot->watchdog_restart_count < 255U) {
            slot->watchdog_restart_count++;
        }
    } else if (slot->restart_count < 255U) {
        slot->restart_count++;
    }
}

static fdir_status_t try_restart(fdir_entity_slot_t *slot, fdir_entity_id_t id, fdir_reason_t reason)
{
    fdir_reason_t cleared;

    if (slot->desc.restart == NULL) {
        return FDIR_ERR_PORT;
    }

    bump_restart(slot, reason);

    if (slot->desc.restart(id, slot->desc.user) != 0) {
        return FDIR_ERR_STATE;
    }

    cleared = fdir_health_clear_latch_unsafe(id);
    fdir_health_reset_unsafe(id);
    if (cleared != (fdir_reason_t)0xFFU) {
        emit_event_full(FDIR_EVENT_FAILURE, g_mode, id, cleared, 0U, FDIR_LEVEL_ENTITY,
                        FDIR_ANOMALY_CLEARED, slot->desc.name);
    }
    emit_event_full(FDIR_EVENT_RESTART, g_mode, id, reason, 0U, FDIR_LEVEL_ENTITY,
                    FDIR_ANOMALY_RAISED, slot->desc.name);
    return FDIR_OK;
}

static fdir_action_t resolve_exhausted_action(const fdir_entity_slot_t *slot, fdir_entity_id_t id,
                                              fdir_action_t action, const fdir_failure_report_t *report)
{
    fdir_action_t sub_action;

    if (action == FDIR_ACTION_RESTART || action == FDIR_ACTION_NONE) {
        return action;
    }

    if (slot->desc.linked_subsystem == FDIR_SUBSYSTEM_NONE) {
        return action;
    }

    sub_action = fdir_subsystem_on_entity_exhausted(slot->desc.linked_subsystem, id, report);
    if (sub_action != FDIR_ACTION_NONE) {
        fdir_internal_emit_subsystem_state(slot->desc.linked_subsystem, id, report->reason,
                                           report->error_code, FDIR_ANOMALY_RAISED,
                                           "policy_override");
        return sub_action;
    }

    return action;
}

static void apply_action(fdir_entity_slot_t *slot, fdir_entity_id_t id, fdir_action_t action,
                         const fdir_failure_report_t *report)
{
    const fdir_subsystem_id_t sub = slot->desc.linked_subsystem;

    switch (action) {
        case FDIR_ACTION_NONE:
            break;

        case FDIR_ACTION_RESTART:
            if (try_restart(slot, id, report->reason) != 0) {
                apply_action(slot, id, slot->desc.on_exhausted, report);
            }
            break;

        case FDIR_ACTION_DEGRADE:
            if (sub != FDIR_SUBSYSTEM_NONE) {
                fdir_subsystem_mark_degraded_unsafe(sub);
                fdir_internal_emit_subsystem_state(sub, id, report->reason, report->error_code,
                                                   FDIR_ANOMALY_RAISED, "degraded");
            }
            fdir_enter_degraded_mode();
            break;

        case FDIR_ACTION_UNAVAILABLE:
            if (sub != FDIR_SUBSYSTEM_NONE) {
                fdir_subsystem_mark_unavailable_unsafe(sub);
                fdir_internal_emit_subsystem_state(sub, id, report->reason, report->error_code,
                                                   FDIR_ANOMALY_RAISED, "unavailable");
            }
            fdir_enter_degraded_mode();
            evaluate_dual_path();
            break;

        case FDIR_ACTION_SAFE:
            if (sub != FDIR_SUBSYSTEM_NONE) {
                fdir_subsystem_mark_unavailable_unsafe(sub);
                fdir_internal_emit_subsystem_state(sub, id, report->reason, report->error_code,
                                                   FDIR_ANOMALY_RAISED, "unavailable");
            }
            fdir_enter_safe_mode();
            break;

        case FDIR_ACTION_REBOOT:
            if (sub != FDIR_SUBSYSTEM_NONE) {
                fdir_subsystem_mark_unavailable_unsafe(sub);
                fdir_internal_emit_subsystem_state(sub, id, report->reason, report->error_code,
                                                   FDIR_ANOMALY_RAISED, "unavailable");
            }
            fdir_try_reboot(report->detail[0] != '\0' ? report->detail : "fdir_reboot");
            break;

        default:
            break;
    }
}

fdir_status_t fdir_init(const fdir_config_t *config, const fdir_port_t *port)
{
    fdir_status_t port_status = fdir_port_bind(port);
    if (port_status != FDIR_OK) {
        return port_status;
    }

    fdir_internal_set_config(config);

    fdir_health_init();
    fdir_subsystems_init();
    fdir_failure_queue_init();

    memset(g_entities, 0, sizeof(g_entities));
    g_entity_count = 0U;
    g_mode = FDIR_MODE_NOMINAL;
    g_supervision_enabled = 1U;
    return FDIR_OK;
}

fdir_status_t fdir_entity_register(const fdir_entity_desc_t *desc, fdir_entity_id_t *out_id)
{
    fdir_entity_id_t id;

    fdir_port_sync_enter();

    if (desc == NULL || out_id == NULL) {
        fdir_port_sync_exit();
        return FDIR_ERR_PARAM;
    }

    if (g_entity_count >= FDIR_ENTITY_CAP) {
        fdir_port_sync_exit();
        return FDIR_ERR_FULL;
    }

    id = g_entity_count++;
    g_entities[id].desc = *desc;
    fdir_internal_copy_detail(g_entities[id].name, sizeof(g_entities[id].name), desc->name);
    g_entities[id].desc.name = g_entities[id].name;
    g_entities[id].used = 1U;
    g_entities[id].restart_count = 0U;
    g_entities[id].watchdog_restart_count = 0U;

    fdir_health_set_entity_limit(g_entity_count);
    fdir_health_reset(id);
    *out_id = id;

    fdir_port_sync_exit();
    return FDIR_OK;
}

fdir_entity_id_t fdir_entity_count(void)
{
    return g_entity_count;
}

const char *fdir_entity_name(fdir_entity_id_t id)
{
    if (id >= g_entity_count || !g_entities[id].used) {
        return "unknown";
    }
    if (g_entities[id].desc.name == NULL) {
        return "unnamed";
    }
    return g_entities[id].desc.name;
}

fdir_mode_t fdir_system_mode(void)
{
    fdir_mode_t mode;

    fdir_port_sync_enter();
    mode = g_mode;
    fdir_port_sync_exit();
    return mode;
}

fdir_bool_t fdir_supervision_enabled(void)
{
    fdir_bool_t enabled;

    fdir_port_sync_enter();
    enabled = g_supervision_enabled;
    fdir_port_sync_exit();
    return enabled;
}

fdir_bool_t fdir_set_supervision_enabled(fdir_bool_t enabled)
{
    fdir_bool_t prev;

    fdir_port_sync_enter();
    prev = g_supervision_enabled;
    if (prev == enabled) {
        fdir_port_sync_exit();
        return prev;
    }

    g_supervision_enabled = enabled;
    if (enabled == 0U) {
        fdir_failure_queue_init();
    }

    emit_event_full(FDIR_EVENT_NOTE, g_mode, FDIR_ENTITY_NONE, FDIR_REASON_USER, 0U,
                    FDIR_LEVEL_SYSTEM, FDIR_ANOMALY_RAISED,
                    enabled != 0U ? "supervision_enabled" : "supervision_disabled");
    fdir_port_sync_exit();
    return prev;
}

fdir_bool_t fdir_entity_may_run(fdir_entity_id_t id)
{
    const fdir_health_snapshot_t *snapshot;
    fdir_bool_t may_run;

    fdir_port_sync_enter();

    if (g_supervision_enabled == 0U) {
        fdir_port_sync_exit();
        return 1U;
    }

    if (g_mode >= FDIR_MODE_SAFE) {
        fdir_port_sync_exit();
        return 0U;
    }

    snapshot = fdir_health_snapshot(id);
    if (snapshot == NULL || snapshot->health == FDIR_HEALTH_FAILED) {
        fdir_port_sync_exit();
        return 0U;
    }

    may_run = 1U;
    fdir_port_sync_exit();
    return may_run;
}

const fdir_config_t *fdir_config(void)
{
    return fdir_internal_get_config();
}

uint32_t fdir_heartbeat_max_age_ms(void)
{
    const fdir_config_t *cfg = fdir_internal_get_config();

    return (uint32_t)cfg->missed_heartbeat_tolerance * cfg->health_check_period_ms;
}

fdir_status_t fdir_set_system_mode(fdir_mode_t mode)
{
    fdir_port_sync_enter();

    if (mode > FDIR_MODE_REBOOT_PENDING) {
        fdir_port_sync_exit();
        return FDIR_ERR_PARAM;
    }
    if (mode == FDIR_MODE_REBOOT_PENDING) {
        fdir_port_sync_exit();
        return FDIR_ERR_STATE;
    }

    set_mode(mode, FDIR_ENTITY_NONE, "ground_mode_set", NULL);
    fdir_port_sync_exit();
    return FDIR_OK;
}

fdir_status_t fdir_deescalate_system_mode(void)
{
    fdir_status_t status = FDIR_OK;

    fdir_port_sync_enter();

    if (g_mode == FDIR_MODE_SAFE) {
        set_mode(FDIR_MODE_DEGRADED, FDIR_ENTITY_NONE, "ground_deescalate", NULL);
    } else if (g_mode == FDIR_MODE_DEGRADED) {
        set_mode(FDIR_MODE_NOMINAL, FDIR_ENTITY_NONE, "ground_deescalate", NULL);
    } else if (g_mode == FDIR_MODE_NOMINAL) {
        status = FDIR_ERR_STATE;
    } else {
        status = FDIR_ERR_STATE;
    }

    fdir_port_sync_exit();
    return status;
}

void fdir_enter_degraded_mode(void)
{
    if (g_mode == FDIR_MODE_NOMINAL) {
        set_mode(FDIR_MODE_DEGRADED, FDIR_ENTITY_NONE, "degraded", NULL);
    }
}

void fdir_enter_safe_mode(void)
{
    if (g_mode != FDIR_MODE_REBOOT_PENDING) {
        set_mode(FDIR_MODE_SAFE, FDIR_ENTITY_NONE, "safe", NULL);
    }
}

void fdir_reassess_system_mode(void)
{
    fdir_port_sync_enter();
    evaluate_dual_path();
    fdir_port_sync_exit();
}

void fdir_try_reboot(const char *reason)
{
    const char *why = (reason != NULL) ? reason : "reboot";

    set_mode(FDIR_MODE_REBOOT_PENDING, FDIR_ENTITY_NONE, why, NULL);
    fdir_request_reboot(why);
}

void fdir_log_note(const char *note)
{
    fdir_port_sync_enter();
    emit_event_full(FDIR_EVENT_NOTE, g_mode, FDIR_ENTITY_NONE, FDIR_REASON_USER, 0U,
                    FDIR_LEVEL_SYSTEM, FDIR_ANOMALY_RAISED, note);
    fdir_port_sync_exit();
}

void fdir_log_queue_overflow(uint16_t queue_id, uint32_t overflow_delta, uint32_t depth_high_water)
{
    char detail[FDIR_DETAIL_SIZE];

    (void)snprintf(detail, sizeof(detail), "q%u d%lu h%lu",
                   (unsigned)queue_id,
                   (unsigned long)overflow_delta,
                   (unsigned long)depth_high_water);

    fdir_port_sync_enter();
    emit_event_full(FDIR_EVENT_QUEUE_OVERFLOW, g_mode, FDIR_ENTITY_NONE,
                    FDIR_REASON_QUEUE_OVERFLOW, queue_id, FDIR_LEVEL_SYSTEM,
                    FDIR_ANOMALY_RAISED, detail);
    fdir_port_sync_exit();
}

void fdir_supervisor_tick(void)
{
    fdir_failure_report_t report;

    if (fdir_supervision_enabled() == 0U) {
        return;
    }

    while (fdir_failure_queue_get(&report) == 0) {
        fdir_handle_failure(&report);
    }

    fdir_check_watchdogs();
}

void fdir_handle_failure(const fdir_failure_report_t *report)
{
    fdir_entity_slot_t *slot;
    fdir_action_t action;

    if (report == NULL) {
        return;
    }

    if (fdir_supervision_enabled() == 0U) {
        return;
    }

    fdir_port_sync_enter();

    if (fdir_health_fault_is_latched_unsafe(report->entity, report->reason)) {
        fdir_port_sync_exit();
        return;
    }

    {
        const fdir_event_kind_t event_kind =
            (report->reason == FDIR_REASON_WATCHDOG) ? FDIR_EVENT_WATCHDOG : FDIR_EVENT_FAILURE;

        emit_event_full(event_kind, g_mode, report->entity, report->reason,
                        report->error_code, FDIR_LEVEL_ENTITY, FDIR_ANOMALY_RAISED, report->detail);
    }

    if (report->entity >= g_entity_count || !g_entities[report->entity].used) {
        fdir_enter_degraded_mode();
        fdir_port_sync_exit();
        return;
    }

    (void)fdir_health_latch_fault_unsafe(report->entity, report->reason);
    fdir_health_set_unsafe(report->entity, FDIR_HEALTH_FAILED, report->error_code, report->detail);

    slot = &g_entities[report->entity];
    action = FDIR_ACTION_NONE;

    if (slot->desc.decide != NULL) {
        action = slot->desc.decide(report->entity, report, slot->restart_count,
                                   slot->watchdog_restart_count, slot->desc.user);
    }

    if (action == FDIR_ACTION_NONE) {
        action = default_decide(slot, report);
    }

    action = resolve_exhausted_action(slot, report->entity, action, report);
    apply_action(slot, report->entity, action, report);
    evaluate_dual_path();

    fdir_port_sync_exit();
}

void fdir_check_watchdogs(void)
{
    const uint32_t now_ms = fdir_get_now_ms();
    const uint32_t max_age = fdir_heartbeat_max_age_ms();
    fdir_entity_id_t id;

    if (fdir_supervision_enabled() == 0U) {
        return;
    }

    fdir_port_sync_enter();

    for (id = 0U; id < g_entity_count; id++) {
        const fdir_health_snapshot_t *snapshot;
        fdir_failure_report_t report;

        if (!g_entities[id].used) {
            continue;
        }

        if (fdir_health_fault_is_latched_unsafe(id, FDIR_REASON_WATCHDOG)) {
            continue;
        }

        snapshot = fdir_health_snapshot(id);
        if (snapshot == NULL || snapshot->health == FDIR_HEALTH_FAILED) {
            continue;
        }

        if (!fdir_health_is_stale(id, now_ms, max_age)) {
            continue;
        }

        memset(&report, 0, sizeof(report));
        report.entity = id;
        report.reason = FDIR_REASON_WATCHDOG;
        report.error_code = 0U;
        report.timestamp_ms = now_ms;
        report.flags = FDIR_FAULT_FLAG_CRITICAL;
        fdir_internal_copy_detail(report.detail, sizeof(report.detail), "heartbeat_timeout");

        fdir_port_sync_exit();
        fdir_handle_failure(&report);
        fdir_port_sync_enter();
    }

    fdir_port_sync_exit();
}
