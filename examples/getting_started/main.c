/*
 * getting_started - minimal fdir example, no RTOS, no threads
 *
 * Build: make getting_started
 * Run:   ./build/getting_started
 *
 * Registers one entity ("sensor") and drives it through three scenarios:
 *   1. Single fault   - fdir restarts the entity
 *   2. Three faults   - restart budget exhausted, mode -> DEGRADED
 *   3. Watchdog miss  - stale heartbeat detected, entity restarted
 *
 * Port hooks are implemented at the bottom of this file. On a real target,
 * move them to a separate port.c and adapt to your platform.
 */

#define _POSIX_C_SOURCE 200809L
#include "fdir.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/*
 * Single-threaded failure queue. fdir_post_failure() pushes here;
 * the main loop pops and calls fdir_handle_failure(). On an RTOS this
 * would be a message queue drained by a dedicated supervisor task.
 */
#define QUEUE_CAP 16
static fdir_failure_report_t g_queue[QUEUE_CAP];
static int g_qhead, g_qtail, g_qcount;

static void queue_push(const fdir_failure_report_t *r)
{
    if (g_qcount == QUEUE_CAP) { return; }
    g_queue[g_qtail] = *r;
    g_qtail = (g_qtail + 1) % QUEUE_CAP;
    g_qcount++;
}

static int queue_pop(fdir_failure_report_t *out)
{
    if (g_qcount == 0) { return 0; }
    *out = g_queue[g_qhead];
    g_qhead = (g_qhead + 1) % QUEUE_CAP;
    g_qcount--;
    return 1;
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

static void report_fault(fdir_entity_id_t entity, const char *detail)
{
    printf("  [app] fault: %s\n", detail);

    fdir_failure_report_t r;
    memset(&r, 0, sizeof(r));
    r.entity       = entity;
    r.reason       = FDIR_REASON_IO_ERROR;
    r.error_code   = 1;
    r.timestamp_ms = fdir_get_now_ms();
    strncpy(r.detail, detail, FDIR_DETAIL_SIZE - 1);
    fdir_post_failure(&r);

    fdir_failure_report_t pending;
    while (queue_pop(&pending)) {
        fdir_handle_failure(&pending);
    }
}

static int sensor_restart(fdir_entity_id_t id, void *user)
{
    (void)id;
    (void)user;
    printf("  [app] sensor restarted\n");
    return 0;
}

int main(void)
{
    fdir_config_t cfg = fdir_config_default();
    cfg.health_check_period_ms            = 1000;
    cfg.missed_heartbeat_tolerance        = 2;
    cfg.safe_mode_critical_failure_threshold = 1;

    if (fdir_init(&cfg) != FDIR_OK) {
        fprintf(stderr, "fdir_init failed\n");
        return 1;
    }

    fdir_entity_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.name                  = "sensor";
    desc.max_restarts          = 2;
    desc.max_watchdog_restarts = 1;
    desc.on_exhausted          = FDIR_ACTION_DEGRADE;
    desc.linked_subsystem      = FDIR_SUBSYSTEM_NONE;
    desc.restart               = sensor_restart;

    fdir_entity_id_t sensor;
    if (fdir_entity_register(&desc, &sensor) != FDIR_OK) {
        fprintf(stderr, "fdir_entity_register failed\n");
        return 1;
    }

    fdir_health_heartbeat_notify(sensor);
    printf("mode: %s\n\n", mode_name(fdir_system_mode()));

    printf("--- fault 1: restart ---\n");
    report_fault(sensor, "read timeout");
    printf("mode: %s\n\n", mode_name(fdir_system_mode()));

    printf("--- faults 2+3: budget exhausted -> DEGRADED ---\n");
    fdir_health_heartbeat_notify(sensor);
    report_fault(sensor, "read timeout");
    fdir_health_heartbeat_notify(sensor);
    report_fault(sensor, "read timeout");
    printf("mode: %s\n\n", mode_name(fdir_system_mode()));

    printf("--- watchdog miss -> restart ---\n");
    fdir_init(&cfg);
    fdir_entity_register(&desc, &sensor);
    fdir_health_heartbeat_notify(sensor);

    struct timespec ts = { .tv_sec = 3, .tv_nsec = 500000000 };
    nanosleep(&ts, NULL);

    fdir_check_watchdogs();
    printf("mode: %s\n", mode_name(fdir_system_mode()));

    return 0;
}

/*
 * Port hook overrides. The weak defaults in src/port.c abort() for the three
 * required hooks (fdir_get_now_ms, fdir_post_failure, fdir_isolate_current_worker)
 * and no-op for the two optional ones (fdir_emit_event, fdir_request_reboot).
 */

uint32_t fdir_get_now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}

int fdir_post_failure(const fdir_failure_report_t *report)
{
    queue_push(report);
    return 0;
}

void fdir_isolate_current_worker(void)
{
    /* Single-threaded: no worker to isolate. Override is required because
     * the default weak implementation aborts(). */
}

void fdir_emit_event(const fdir_event_t *event)
{
    static const char *kinds[] = {
        "FAILURE", "MODE_CHANGE", "RESTART", "WATCHDOG", "NOTE", "QUEUE_OVF",
    };
    const char *kind = (event->kind < 6U) ? kinds[event->kind] : "?";
    printf("  [fdir] %-12s entity=%-3u \"%s\"\n",
           kind, (unsigned)event->entity, event->detail);
}

void fdir_request_reboot(const char *reason)
{
    printf("  [fdir] reboot: %s\n", reason != NULL ? reason : "");
}
