/*
 * FreeRTOS + fdir demo (POSIX/Linux host build)
 *
 * Three scenarios run sequentially:
 *   1. Watchdog miss:   worker stops heartbeating, supervisor detects and restarts
 *   2. Budget exhaust:  worker faults 3 times, budget exhausted, mode -> DEGRADED
 *   3. Dual-path fail:  two critical subsystems marked unavailable, mode -> SAFE
 *
 * Build: make freertos
 * Run:   ./build/example_FreeRTOS
 */

#include "fdir.h"

#include "FreeRTOS.h"
#include "port.h"
#include "task.h"
#include "timers.h"

#include <stdio.h>
#include <string.h>

fdir_port_t fdir_app_port(void);

static fdir_entity_id_t    g_worker_id    = FDIR_ENTITY_NONE;
static TaskHandle_t        g_worker_task;
static fdir_subsystem_id_t g_sub_downlink = FDIR_SUBSYSTEM_NONE;
static fdir_subsystem_id_t g_sub_storage  = FDIR_SUBSYSTEM_NONE;

static volatile int g_stop_heartbeat = 0;
static volatile int g_inject_fault   = 0;

static int worker_restart(fdir_entity_id_t id, void *user)
{
    (void)id;
    (void)user;
    printf("[demo] restarting worker\n");
    g_stop_heartbeat = 0;
    g_inject_fault   = 0;
    return 0;
}

static void worker_task(void *param)
{
    (void)param;

    for (;;) {
        if (!fdir_worker_may_run(g_worker_id)) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        if (!g_stop_heartbeat) {
            fdir_health_heartbeat_notify(g_worker_id);
        }

        if (g_inject_fault) {
            g_inject_fault = 0;
            fdir_report_fault(g_worker_id, FDIR_REASON_IO_ERROR, 42, "simulated I/O error");
        }

        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

static void supervisor_task(void *param)
{
    (void)param;

    for (;;) {
        fdir_supervisor_tick();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void scenario_task(void *param)
{
    (void)param;

    printf("\n--- Scenario 1: watchdog miss -> restart ---\n");
    vTaskDelay(pdMS_TO_TICKS(400));
    g_stop_heartbeat = 1;
    printf("[demo] worker stopped heartbeating\n");
    vTaskDelay(pdMS_TO_TICKS(3000));

    printf("\n--- Scenario 2: 3 faults -> DEGRADED ---\n");
    for (int i = 0; i < 3; i++) {
        g_inject_fault = 1;
        printf("[demo] injecting fault %d/3\n", i + 1);
        vTaskDelay(pdMS_TO_TICKS(800));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));

    printf("\n--- Scenario 3: dual-path unavailable -> SAFE ---\n");
    fdir_subsystem_mark_unavailable(g_sub_downlink);
    fdir_subsystem_mark_unavailable(g_sub_storage);
    fdir_report_fault(g_worker_id, FDIR_REASON_USER, 0, "dual-path forced");

    vTaskDelay(pdMS_TO_TICKS(1500));

    printf("\n--- Demo complete (mode=%u) ---\n", (unsigned)fdir_system_mode());
    vTaskEndScheduler();
    vTaskDelete(NULL);
}

int main(void)
{
    fdir_config_t cfg = fdir_config_default();
    cfg.health_check_period_ms = 500;
    cfg.missed_heartbeat_tolerance = 2;
    cfg.safe_mode_critical_failure_threshold = 2;

    fdir_port_t port = fdir_app_port(); // From port.c
    fdir_status_t init_status = fdir_init(&cfg, &port);
    if (init_status != FDIR_OK) {
        fdir_request_reboot("fdir_init failed\n");
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    fdir_subsystem_desc_t sub_dl = { .name = "downlink", .is_critical_path = 1 };
    fdir_subsystem_desc_t sub_st = { .name = "storage",  .is_critical_path = 1 };
    fdir_subsystem_register(&sub_dl, &g_sub_downlink);
    fdir_subsystem_register(&sub_st, &g_sub_storage);

    fdir_entity_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.name                  = "worker";
    desc.max_restarts          = 2;
    desc.max_watchdog_restarts = 1;
    desc.on_exhausted          = FDIR_ACTION_DEGRADE;
    desc.linked_subsystem      = FDIR_SUBSYSTEM_NONE;
    desc.restart               = worker_restart;

    if (fdir_entity_register(&desc, &g_worker_id) != FDIR_OK) {
        printf("fdir_entity_register failed\n");
        return 1;
    }

    fdir_health_heartbeat_notify(g_worker_id);

    xTaskCreate(worker_task,     "worker",     512, NULL, 2, &g_worker_task);
    xTaskCreate(supervisor_task, "supervisor", 512, NULL, 3, NULL);
    xTaskCreate(scenario_task,   "scenario",   512, NULL, 1, NULL);

    printf("fdir FreeRTOS demo starting\n");
    vTaskStartScheduler();

    printf("scheduler stopped\n");
    return 0;
}
