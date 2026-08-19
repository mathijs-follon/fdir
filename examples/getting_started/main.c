/*
 * getting_started: minimal fdir example, no RTOS, no threads
 *
 * Build: make getting_started
 * Run:   ./build/getting_started
 *
 * Registers one entity ("sensor") and drives it through three scenarios:
 *   1. Single fault:   fdir restarts the entity
 *   2. Three faults: restart budget exhausted, mode -> DEGRADED
 *   3. Watchdog miss: stale heartbeat detected, entity restarted
 *
 * Port hooks are implemented at the bottom of this file. On a real target,
 * move them to a separate port.c and pass the resulting fdir_port_t to fdir_init().
 */

#define _POSIX_C_SOURCE 200809L
#include "fdir.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static uint32_t platform_now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}

static void platform_emit_event(const fdir_event_t *event)
{
    static const char *kinds[] = {
        "FAILURE", "MODE_CHANGE", "RESTART", "WATCHDOG", "NOTE", "QUEUE_OVF",
    };
    const char *kind = (event->kind < 6U) ? kinds[event->kind] : "?";
    printf("  [fdir] %-12s entity=%-3u \"%s\"\n",
           kind, (unsigned)event->entity, event->detail);
}

static void platform_request_reboot(const char *reason)
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

static void report_fault(fdir_entity_id_t entity, const char *detail)
{
    printf("  [app] fault: %s\n", detail);
    fdir_report_fault(entity, FDIR_REASON_IO_ERROR, 1, detail);
    fdir_supervisor_tick();
}

static int sensor_restart(fdir_entity_id_t id, void *user)
{
    (void)id;
    (void)user;
    printf("  [app] sensor restarted\n");
    return 0;
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
        .emit_event     = platform_emit_event,
        .request_reboot = platform_request_reboot,
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

int main(void)
{
    fdir_config_t cfg = fdir_config_default();
    cfg.health_check_period_ms            = 1000;
    cfg.missed_heartbeat_tolerance        = 2;
    cfg.safe_mode_critical_failure_threshold = 1;

    if (init_fdir(&cfg) != 0) {
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
    if (init_fdir(&cfg) != 0) {
        return 1;
    }
    fdir_entity_register(&desc, &sensor);
    fdir_health_heartbeat_notify(sensor);

    struct timespec ts = { .tv_sec = 3, .tv_nsec = 500000000 };
    nanosleep(&ts, NULL);

    fdir_supervisor_tick();
    printf("mode: %s\n", mode_name(fdir_system_mode()));

    return 0;
}
