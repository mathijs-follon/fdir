#include "health.h"
#include "health_internal.h"
#include "internal.h"
#include "port.h"
#include <string.h>

static fdir_health_snapshot_t g_health[FDIR_ENTITY_CAP];
static fdir_reason_t g_latched_reason[FDIR_ENTITY_CAP];
static fdir_bool_t g_fault_latched[FDIR_ENTITY_CAP];
static uint8_t g_entity_limit;

void fdir_health_init(void)
{
    memset(g_health, 0, sizeof(g_health));
    memset(g_latched_reason, 0, sizeof(g_latched_reason));
    memset(g_fault_latched, 0, sizeof(g_fault_latched));
    g_entity_limit = 0U;
}

void fdir_health_set_entity_limit(uint8_t limit)
{
    g_entity_limit = limit;
}

fdir_bool_t fdir_health_fault_is_latched_unsafe(fdir_entity_id_t id, fdir_reason_t reason)
{
    if (id >= g_entity_limit) {
        return 0U;
    }
    return (g_fault_latched[id] != 0U && g_latched_reason[id] == reason) ? 1U : 0U;
}

fdir_bool_t fdir_health_latch_fault_unsafe(fdir_entity_id_t id, fdir_reason_t reason)
{
    if (id >= g_entity_limit) {
        return 0U;
    }
    if (g_fault_latched[id] != 0U && g_latched_reason[id] == reason) {
        return 1U;
    }
    g_fault_latched[id] = 1U;
    g_latched_reason[id] = reason;
    return 0U;
}

fdir_reason_t fdir_health_clear_latch_unsafe(fdir_entity_id_t id)
{
    fdir_reason_t prev = (fdir_reason_t)0xFFU;

    if (id >= g_entity_limit) {
        return prev;
    }
    if (g_fault_latched[id] != 0U) {
        prev = g_latched_reason[id];
    }
    g_fault_latched[id] = 0U;
    g_latched_reason[id] = (fdir_reason_t)0;
    return prev;
}

fdir_bool_t fdir_health_fault_is_latched(fdir_entity_id_t id, fdir_reason_t reason)
{
    fdir_bool_t latched;

    fdir_port_sync_enter();
    latched = fdir_health_fault_is_latched_unsafe(id, reason);
    fdir_port_sync_exit();
    return latched;
}

void fdir_health_heartbeat_notify(fdir_entity_id_t id)
{
    fdir_port_sync_enter();

    if (id >= g_entity_limit) {
        fdir_port_sync_exit();
        return;
    }

    g_health[id].last_heartbeat_ms = fdir_get_now_ms();

    if (g_health[id].health == FDIR_HEALTH_FAILED) {
        fdir_port_sync_exit();
        return;
    }

    g_health[id].health = FDIR_HEALTH_OK;
    fdir_port_sync_exit();
}

void fdir_health_set(fdir_entity_id_t id, fdir_health_t health, uint16_t error_code, const char *detail)
{
    fdir_port_sync_enter();

    if (id >= g_entity_limit) {
        fdir_port_sync_exit();
        return;
    }

    g_health[id].health = health;
    g_health[id].error_code = error_code;
    fdir_internal_copy_detail(g_health[id].detail, sizeof(g_health[id].detail), detail);
    fdir_port_sync_exit();
}

void fdir_health_reset(fdir_entity_id_t id)
{
    fdir_port_sync_enter();

    if (id >= g_entity_limit) {
        fdir_port_sync_exit();
        return;
    }

    g_health[id].health = FDIR_HEALTH_OK;
    g_health[id].error_code = 0U;
    g_health[id].detail[0] = '\0';
    g_health[id].last_heartbeat_ms = fdir_get_now_ms();
    fdir_port_sync_exit();
}

const fdir_health_snapshot_t *fdir_health_snapshot(fdir_entity_id_t id)
{
    if (id >= g_entity_limit) {
        return NULL;
    }

    return &g_health[id];
}

fdir_bool_t fdir_health_snapshot_copy(fdir_entity_id_t id, fdir_health_snapshot_t *out)
{
    if (out == NULL || id >= g_entity_limit) {
        return 0U;
    }

    fdir_port_sync_enter();
    *out = g_health[id];
    fdir_port_sync_exit();
    return 1U;
}

fdir_bool_t fdir_health_is_stale(fdir_entity_id_t id, uint32_t now_ms, uint32_t max_age_ms)
{
    const fdir_health_snapshot_t *snapshot = fdir_health_snapshot(id);

    if (snapshot == NULL || snapshot->health == FDIR_HEALTH_FAILED) {
        return 0U;
    }

    return ((now_ms - snapshot->last_heartbeat_ms) > max_age_ms) ? 1U : 0U;
}

void fdir_health_set_unsafe(fdir_entity_id_t id, fdir_health_t health, uint16_t error_code, const char *detail)
{
    if (id >= g_entity_limit) {
        return;
    }

    g_health[id].health = health;
    g_health[id].error_code = error_code;
    fdir_internal_copy_detail(g_health[id].detail, sizeof(g_health[id].detail), detail);
}

void fdir_health_reset_unsafe(fdir_entity_id_t id)
{
    if (id >= g_entity_limit) {
        return;
    }

    g_health[id].health = FDIR_HEALTH_OK;
    g_health[id].error_code = 0U;
    g_health[id].detail[0] = '\0';
    g_health[id].last_heartbeat_ms = fdir_get_now_ms();
}
