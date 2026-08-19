/*
 * FreeRTOS + fdir demo (POSIX/Linux host build)
 *
 * Three scenarios run sequentially:
 *   1. Watchdog miss   - worker stops heartbeating, supervisor detects and restarts
 *   2. Budget exhaust  - worker faults 3 times, budget exhausted, mode -> DEGRADED
 *   3. Dual-path fail  - two critical subsystems marked unavailable, mode -> SAFE
 *
 * Build: make freertos
 * Run:   ./build/example_FreeRTOS
 */

#include "fdir.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "timers.h"

#include <stdio.h>
#include <string.h>

QueueHandle_t g_failure_queue;

static fdir_entity_id_t    g_worker_id   = FDIR_ENTITY_NONE;
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
    vTaskResume(g_worker_task);
    return 0;
}

static void worker_task(void *param)
{
    (void)param;

    for (;;) {
        if (!g_stop_heartbeat) {
            fdir_health_heartbeat_notify(g_worker_id);
        }

        if (g_inject_fault) {
            g_inject_fault = 0;
            fdir_failure_report_t r;
            memset(&r, 0, sizeof(r));
            r.entity      = g_worker_id;
            r.reason      = FDIR_REASON_IO_ERROR;
            r.error_code  = 42;
            r.timestamp_ms = fdir_get_now_ms();
            strncpy(r.detail, "simulated I/O error", FDIR_DETAIL_SIZE - 1);
            fdir_submit_failure(&r);
        }

        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

static void supervisor_task(void *param)
{
    (void)param;

    for (;;) {
        fdir_failure_report_t report;
        while (xQueueReceive(g_failure_queue, &report, 0) == pdTRUE) {
            fdir_handle_failure(&report);
        }
        fdir_check_watchdogs();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void scenario_task(void *param)
{
    (void)param;

    /* scenario 1: watchdog miss */
    printf("\n--- Scenario 1: watchdog miss -> restart ---\n");
    vTaskDelay(pdMS_TO_TICKS(400));
    g_stop_heartbeat = 1;
    printf("[demo] worker stopped heartbeating\n");
    vTaskDelay(pdMS_TO_TICKS(3000));

    /* scenario 2: fault budget exhaustion */
    printf("\n--- Scenario 2: 3 faults -> DEGRADED ---\n");
    for (int i = 0; i < 3; i++) {
        g_inject_fault = 1;
        printf("[demo] injecting fault %d/3\n", i + 1);
        vTaskDelay(pdMS_TO_TICKS(800));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* scenario 3: dual critical-path subsystems unavailable */
    printf("\n--- Scenario 3: dual-path unavailable -> SAFE ---\n");
    fdir_subsystem_mark_unavailable(g_sub_downlink);
    fdir_subsystem_mark_unavailable(g_sub_storage);

    fdir_failure_report_t r;
    memset(&r, 0, sizeof(r));
    r.entity        = g_worker_id;
    r.reason       = FDIR_REASON_USER;
    r.timestamp_ms = fdir_get_now_ms();
    strncpy(r.detail, "dual-path forced", FDIR_DETAIL_SIZE - 1);
    fdir_submit_failure(&r);

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

    if (fdir_init(&cfg) != FDIR_OK) {
        printf("fdir_init failed\n");
        return 1;
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

    g_failure_queue = xQueueCreate(8, sizeof(fdir_failure_report_t));

    /* Startup self-test before the scheduler runs. Report a fault immediately
     * on failure; fdir applies normal restart/degrade logic when the supervisor
     * task drains the queue after vTaskStartScheduler(). */
    const int worker_ok = 1;
    if (!worker_ok) {
        fdir_failure_report_t r;
        memset(&r, 0, sizeof(r));
        r.entity       = g_worker_id;
        r.reason       = FDIR_REASON_IO_ERROR;
        r.timestamp_ms = 0;
        strncpy(r.detail, "startup self-test failed", FDIR_DETAIL_SIZE - 1);
        xQueueSend(g_failure_queue, &r, 0);
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
