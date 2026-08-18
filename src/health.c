#include "health.h"
#include "internal.h"
#include "port.h"

static fdir_health_snapshot_t g_health[FDIR_ENTITY_CAP];
static uint8_t g_entity_limit;


void fdir_health_heartbeat_notify(fdir_entity_id_t id)
{
    if (id >= g_entity_limit) {
        return;
    }

    g_health[id].last_heartbeat_ms = fdir_get_now_ms();

    if (g_health[id].health == FDIR_HEALTH_FAILED) {
        return;
    }

    g_health[id].health = FDIR_HEALTH_OK;
}

void fdir_health_set(fdir_entity_id_t id, fdir_health_t health, uint16_t error_code, const char *detail)
{
    if (id >= g_entity_limit) {
        return;
    }

    g_health[id].health = health;
    g_health[id].error_code = error_code;
    fdir_internal_copy_detail(g_health[id].detail, sizeof(g_health[id].detail), detail);
}

void fdir_health_reset(fdir_entity_id_t id)
{
    if (id >= g_entity_limit) {
        return;
    }

    g_health[id].health = FDIR_HEALTH_OK;
    g_health[id].error_code = 0U;
    g_health[id].detail[0] = '\0';
    g_health[id].last_heartbeat_ms = fdir_get_now_ms();
}

const fdir_health_snapshot_t *fdir_health_snapshot(fdir_entity_id_t id)
{
    if (id >= g_entity_limit) {
        return NULL;
    }

    return &g_health[id];
}

fdir_bool_t fdir_health_is_stale(fdir_entity_id_t id, uint32_t now_ms, uint32_t max_age_ms)
{
    const fdir_health_snapshot_t *snapshot = fdir_health_snapshot(id);

    if (snapshot == NULL || snapshot->health == FDIR_HEALTH_FAILED) {
        return 0U;
    }

    return ((now_ms - snapshot->last_heartbeat_ms) > max_age_ms) ? 1U : 0U;
}
