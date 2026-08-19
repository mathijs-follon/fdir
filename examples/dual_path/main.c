/*
 * dual_path: dual critical-path FDIR example
 *
 * Simulates downlink + storage entities with mission policy expressed via
 * entity decide() hooks.
 *
 * Build: make dual_path
 * Run:   ./build/dual_path
 */

#define _POSIX_C_SOURCE 200809L
#include "fdir.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static fdir_entity_id_t    g_downlink;
static fdir_entity_id_t    g_storage;
static fdir_subsystem_id_t g_sub_downlink;
static fdir_subsystem_id_t g_sub_storage;

static int g_link_up = 1;
static int g_downlink_restarts;
static int g_storage_restarts;

static uint32_t platform_now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}

static void platform_emit(const fdir_event_t *event)
{
    static const char *kinds[] = {
        "FAILURE", "MODE_CHANGE", "RESTART", "WATCHDOG", "NOTE", "QUEUE_OVF",
    };
    const char *kind = (event->kind < 6U) ? kinds[event->kind] : "?";
    printf("  [fdir] %-12s entity=%-3u \"%s\"\n",
           kind, (unsigned)event->entity, event->detail);
}

static void platform_reboot(const char *reason)
{
    printf("  [fdir] reboot: %s\n", reason != NULL ? reason : "");
}

static const char *mode_name(fdir_mode_t m)
{
    switch (m) {
    case FDIR_MODE_NOMINAL:        return "NOMINAL";
    case FDIR_MODE_DEGRADED:       return "DEGRADED";
    case FDIR_MODE_SAFE:           return "SAFE";
    case FDIR_MODE_REBOOT_PENDING: return "REBOOT_PENDING";
    default:                       return "?";
    }
}

static int downlink_restart(fdir_entity_id_t id, void *user)
{
    (void)id;
    (void)user;
    g_downlink_restarts++;
    printf("  [app] downlink restarted (%d)\n", g_downlink_restarts);
    return 0;
}

static int storage_restart(fdir_entity_id_t id, void *user)
{
    (void)id;
    (void)user;
    g_storage_restarts++;
    printf("  [app] storage restarted (%d)\n", g_storage_restarts);
    return 0;
}

/* Link loss: degrade only, do not restart. */
static fdir_action_t downlink_decide(fdir_entity_id_t id,
                                     const fdir_failure_report_t *report,
                                     uint8_t restarts_used, uint8_t wd_restarts_used,
                                     void *user)
{
    (void)id;
    (void)restarts_used;
    (void)wd_restarts_used;
    (void)user;

    if (!g_link_up && report->reason != FDIR_REASON_WATCHDOG) {
        return FDIR_ACTION_DEGRADE;
    }
    return FDIR_ACTION_NONE;
}

/* Init failure: mark unavailable immediately, no restart budget. */
static fdir_action_t storage_decide(fdir_entity_id_t id,
                                    const fdir_failure_report_t *report,
                                    uint8_t restarts_used, uint8_t wd_restarts_used,
                                    void *user)
{
    (void)id;
    (void)restarts_used;
    (void)wd_restarts_used;
    (void)user;

    if (report->reason == FDIR_REASON_INIT_FAILED) {
        return FDIR_ACTION_UNAVAILABLE;
    }
    return FDIR_ACTION_NONE;
}

static fdir_port_t app_port(void)
{
    static fdir_port_t port;
    static int initialized;

    if (initialized) {
        return port;
    }

    port = (fdir_port_t){
        .get_now_ms     = platform_now_ms,
        .emit_event     = platform_emit,
        .request_reboot = platform_reboot,
    };
    initialized = 1;
    return port;
}

static int init_fdir(const fdir_config_t *cfg)
{
    fdir_port_t port = app_port();
    fdir_status_t status = fdir_init(cfg, &port);
    if (status != FDIR_OK) {
        /* FDIR_ERR_PORT: port NULL or missing required callback */
        fprintf(stderr, "fdir_init failed: %s\n", fdir_status_string(status));
        return -1;
    }
    return 0;
}

static void worker_loop(fdir_entity_id_t id, int work_iterations)
{
    for (int i = 0; i < work_iterations; i++) {
        if (!fdir_worker_may_run(id)) {
            printf("  [app] entity %u waiting (mode=%s)\n",
                   (unsigned)id, mode_name(fdir_system_mode()));
            return;
        }
        fdir_health_heartbeat_notify(id);
    }
}

static void supervisor(void)
{
    fdir_supervisor_tick();
}

int main(void)
{
    fdir_config_t cfg = fdir_config_default();
    cfg.health_check_period_ms               = 500;
    cfg.missed_heartbeat_tolerance         = 2;
    cfg.safe_mode_critical_failure_threshold = 2;

    if (init_fdir(&cfg) != 0) {
        return 1;
    }

    fdir_subsystem_desc_t sub_dl = { .name = "downlink", .is_critical_path = 1 };
    fdir_subsystem_desc_t sub_st = { .name = "storage",  .is_critical_path = 1 };
    fdir_subsystem_register(&sub_dl, &g_sub_downlink);
    fdir_subsystem_register(&sub_st, &g_sub_storage);

    fdir_entity_desc_t dl_desc;
    memset(&dl_desc, 0, sizeof(dl_desc));
    dl_desc.name                  = "downlink";
    dl_desc.max_restarts          = 1;
    dl_desc.max_watchdog_restarts = 1;
    dl_desc.on_exhausted          = FDIR_ACTION_REBOOT;
    dl_desc.linked_subsystem      = g_sub_downlink;
    dl_desc.restart               = downlink_restart;
    dl_desc.decide                = downlink_decide;

    fdir_entity_desc_t st_desc;
    memset(&st_desc, 0, sizeof(st_desc));
    st_desc.name                  = "storage";
    st_desc.max_restarts          = 1;
    st_desc.max_watchdog_restarts = 1;
    st_desc.on_exhausted          = FDIR_ACTION_UNAVAILABLE;
    st_desc.linked_subsystem      = g_sub_storage;
    st_desc.restart               = storage_restart;
    st_desc.decide                = storage_decide;

    fdir_entity_register(&dl_desc, &g_downlink);
    fdir_entity_register(&st_desc, &g_storage);

    fdir_health_heartbeat_notify(g_downlink);
    fdir_health_heartbeat_notify(g_storage);
    printf("mode: %s\n\n", mode_name(fdir_system_mode()));

    printf("--- scenario 1: link loss -> DEGRADED (no restart) ---\n");
    g_link_up = 0;
    fdir_report_fault(g_downlink, FDIR_REASON_IO_ERROR, 1, "link_los");
    supervisor();
    worker_loop(g_downlink, 3);
    printf("mode: %s  downlink_restarts=%d\n\n",
           mode_name(fdir_system_mode()), g_downlink_restarts);

    printf("--- scenario 2: storage init fail -> unavailable ---\n");
    g_link_up = 1;
    if (init_fdir(&cfg) != 0) {
        return 1;
    }
    fdir_subsystem_register(&sub_dl, &g_sub_downlink);
    fdir_subsystem_register(&sub_st, &g_sub_storage);
    fdir_entity_register(&dl_desc, &g_downlink);
    fdir_entity_register(&st_desc, &g_storage);
    fdir_report_fault(g_storage, FDIR_REASON_INIT_FAILED, 0, "sd_card_init");
    supervisor();
    printf("mode: %s  storage available=%d\n\n",
           mode_name(fdir_system_mode()),
           (int)fdir_subsystem_is_available(g_sub_storage));

    printf("--- scenario 3: both critical paths unavailable -> SAFE ---\n");
    if (init_fdir(&cfg) != 0) {
        return 1;
    }
    fdir_subsystem_register(&sub_dl, &g_sub_downlink);
    fdir_subsystem_register(&sub_st, &g_sub_storage);
    fdir_entity_register(&dl_desc, &g_downlink);
    fdir_entity_register(&st_desc, &g_storage);
    fdir_health_heartbeat_notify(g_downlink);
    fdir_health_heartbeat_notify(g_storage);

    fdir_subsystem_mark_unavailable(g_sub_downlink);
    fdir_subsystem_mark_unavailable(g_sub_storage);
    fdir_reassess_system_mode();
    worker_loop(g_downlink, 2);
    printf("mode: %s\n", mode_name(fdir_system_mode()));

    return 0;
}
