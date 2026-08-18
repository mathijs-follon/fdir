#include "recovery.h"
#include "health.h"
#include "internal.h"
#include "port.h"
#include "subsystem.h"
#include "types.h"

#include <string.h>

typedef struct {
    fdir_entity_desc_t desc;
    uint8_t used;
    uint8_t restart_count;
    uint8_t watchdog_restart_count;
} fdir_entity_slot_t;

static fdir_entity_slot_t g_entities[FDIR_ENTITY_CAP];
static uint8_t g_entity_count;
static fdir_mode_t g_mode;

static void evaluate_dual_path(void)
{
    const fdir_config_t *cfg = fdir_internal_get_config();

    if (fdir_subsystems_critical_unavailable_count() >= cfg->safe_mode_critical_failure_threshold) {
        fdir_enter_safe_mode();
    }
}


static void set_mode(fdir_mode_t mode, fdir_entity_id_t entity, const char* detail)
{
    if (g_mode == mode) {
        return;
    }

    g_mode = mode;
    fdir_internal_emit_mode(FDIR_EVENT_MODE_CHANGE, g_mode, entity, FDIR_REASON_USER, 0U, detail);
}


static fdir_action_t default_decide(const fdir_entity_slot_t *slot, const fdir_failure_report_t *report)
{
    if (report->reason == FDIR_REASON_WATCHDOG) {
        if (slot->watchdog_restart_count < slot->desc.max_watchdog_restarts) {
            return FDIR_ACTION_RESTART;
        }
        return slot->desc.on_exhausted;
    }

    if (slot->restart_count < slot->desc.max_restarts) {
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
    if (slot->desc.restart == NULL) {
        return FDIR_ERR_PORT;
    }

    bump_restart(slot, reason);

    if (slot->desc.restart(id, slot->desc.user) != 0) {
        return FDIR_ERR_STATE;
    }

    fdir_health_reset(id);
    fdir_internal_emit_mode(FDIR_EVENT_RESTART, g_mode, id, reason, 0U, slot->desc.name);
    return FDIR_OK;
}


static void apply_action(fdir_entity_slot_t *slot, fdir_entity_id_t id, fdir_action_t action, const fdir_failure_report_t *report)
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
                fdir_subsystem_mark_degraded(sub);
            }
            fdir_enter_degraded_mode();
            break;

        case FDIR_ACTION_UNAVAILABLE:
            if (sub != FDIR_SUBSYSTEM_NONE) {
                fdir_subsystem_mark_unavailable(sub);
            }
            fdir_enter_degraded_mode();
            evaluate_dual_path();
            break;

        case FDIR_ACTION_SAFE:
            if (sub != FDIR_SUBSYSTEM_NONE) {
                fdir_subsystem_mark_unavailable(sub);
            }
            fdir_enter_safe_mode();
            break;

        case FDIR_ACTION_REBOOT:
            if (sub != FDIR_SUBSYSTEM_NONE) {
                fdir_subsystem_mark_unavailable(sub);
            }
            fdir_try_reboot(report->detail[0] != '\0' ? report->detail : "fdir_reboot");
            break;

        default:
            break;
    }
}


fdir_status_t fdir_init(const fdir_config_t *config)
{
    fdir_internal_set_config(config);

    // Reset static buffers
    fdir_health_init();
    fdir_subsystems_init();

    memset(g_entities, 0, sizeof(g_entities));
    g_entity_count = 0U;
    g_mode = FDIR_MODE_NOMINAL;
    return FDIR_OK;
}


fdir_status_t fdir_entity_register(const fdir_entity_desc_t *desc, fdir_entity_id_t *out_id)
{
    fdir_entity_id_t id;

    if (desc == NULL || out_id == NULL) {
        return FDIR_ERR_PARAM;
    }

    if (g_entity_count >= FDIR_ENTITY_CAP) {
        return FDIR_ERR_FULL;
    }

    id = g_entity_count++;
    g_entities[id].desc = *desc;
    g_entities[id].used = 1U;
    g_entities[id].restart_count = 0U;
    g_entities[id].watchdog_restart_count = 0U;

    // Open health IDs 0..count-1 so reset/heartbeat apply to this entity
    fdir_health_set_entity_limit(g_entity_count);
    fdir_health_reset(id);
    *out_id = id;
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
    return g_mode;
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


void fdir_enter_degraded_mode(void)
{
    if (g_mode == FDIR_MODE_NOMINAL) {
        set_mode(FDIR_MODE_DEGRADED, FDIR_ENTITY_NONE, "degraded");
    }
}


void fdir_enter_safe_mode(void)
{
    if (g_mode != FDIR_MODE_REBOOT_PENDING) {
        set_mode(FDIR_MODE_SAFE, FDIR_ENTITY_NONE, "safe");
    }
}


void fdir_try_reboot(const char *reason)
{
    const char *why = (reason != NULL) ? reason : "reboot";

    set_mode(FDIR_MODE_REBOOT_PENDING, FDIR_ENTITY_NONE, why);
    fdir_request_reboot(why);
}


void fdir_log_note(const char *note)
{
    fdir_internal_emit_mode(FDIR_EVENT_NOTE, g_mode, FDIR_ENTITY_NONE, FDIR_REASON_USER, 0U, note);
}


void fdir_log_queue_overflow(uint16_t queue_id, uint32_t overflow_delta, uint32_t depth_high_water)
{
    char detail[FDIR_DETAIL_SIZE];

    (void)overflow_delta;
    (void)depth_high_water;

    // Keep detail short and numeric-free of snprintf dependency.
    detail[0] = 'q';
    detail[1] = '\0';
    if (queue_id < 10U) {
        detail[1] = (char)('0' + queue_id);
        detail[2] = '\0';
    }

    fdir_internal_emit_mode(FDIR_EVENT_QUEUE_OVERFLOW, g_mode, FDIR_ENTITY_NONE, FDIR_REASON_QUEUE_OVERFLOW, queue_id, detail);
}


void fdir_handle_failure(const fdir_failure_report_t *report)
{
    fdir_entity_slot_t *slot;
    fdir_action_t action;

    if (report == NULL) {
        return;
    }

    fdir_internal_emit_mode(FDIR_EVENT_FAILURE, g_mode, report->entity, report->reason, report->error_code, report->detail);

    if (report->entity >= g_entity_count || !g_entities[report->entity].used) {
        fdir_enter_degraded_mode();
        return;
    }

    slot = &g_entities[report->entity];
    action = FDIR_ACTION_NONE;

    if (slot->desc.decide != NULL) {
        action = slot->desc.decide(report->entity, report, slot->restart_count, slot->watchdog_restart_count, slot->desc.user);
    }

    if (action == FDIR_ACTION_NONE) {
        action = default_decide(slot, report);
    }

    apply_action(slot, report->entity, action, report);
    evaluate_dual_path();
}


void fdir_check_watchdogs(void)
{
    const uint32_t now_ms = fdir_get_now_ms();
    const uint32_t max_age = fdir_heartbeat_max_age_ms();
    fdir_entity_id_t id;

    for (id = 0U; id < g_entity_count; id++) {
        const fdir_health_snapshot_t *snapshot;
        fdir_failure_report_t report;

        if (!g_entities[id].used) {
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
        fdir_internal_copy_detail(report.detail, sizeof(report.detail), "heartbeat_timeout");

        fdir_health_set(id, FDIR_HEALTH_FAILED, 0U, report.detail);
        fdir_internal_emit_mode(FDIR_EVENT_WATCHDOG, g_mode, id, FDIR_REASON_WATCHDOG, 0U, report.detail);
        fdir_handle_failure(&report);
    }
}
