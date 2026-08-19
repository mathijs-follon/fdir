/*
 * FreeRTOS + fdir C++ API demo (POSIX/Linux host build)
 *
 * Three scenarios driven by three FreeRTOS tasks:
 *   1. Watchdog miss   - worker stops heartbeating, supervisor detects and restarts
 *   2. Budget exhaust  - worker faults 3 times, budget exhausted, mode -> DEGRADED
 *   3. Dual-path fail  - two critical subsystems marked unavailable, mode -> SAFE
 *
 * Build: make freertos_cxx
 * Run:   ./build/example_FreeRTOS_cxx
 *
 * FreeRTOS task functions are void (*)(void*) with no closure, so the
 * supervisor, entity, and subsystem handles are stored at file scope.
 * Everything else uses the C++ API directly.
 */

#define FDIR_HPP_IMPL
#include "fdir.hpp"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "timers.h"

#include <cstdio>
#include <optional>

static std::optional<fdir::Supervisor> g_sup;
static std::optional<fdir::Entity>     g_worker;
static std::optional<fdir::Subsystem>  g_downlink;
static std::optional<fdir::Subsystem>  g_storage;

static QueueHandle_t g_failure_queue;
static TaskHandle_t  g_worker_task;

static volatile bool g_stop_heartbeat = false;
static volatile bool g_inject_fault   = false;

static void worker_task(void *)
{
    for (;;) {
        if (!g_stop_heartbeat) {
            g_worker->heartbeat();
        }
        if (g_inject_fault) {
            g_inject_fault = false;
            g_worker->report_fault(fdir::Reason::IoError, 42, "simulated I/O error");
        }
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

static void supervisor_task(void *)
{
    for (;;) {
        fdir::FailureReport r;
        while (xQueueReceive(g_failure_queue, &r, 0) == pdTRUE) {
            g_sup->handle_failure(r);
        }
        g_sup->check_watchdogs();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void scenario_task(void *)
{
    printf("\n--- Scenario 1: watchdog miss -> restart ---\n");
    vTaskDelay(pdMS_TO_TICKS(400));
    g_stop_heartbeat = true;
    printf("[demo] worker stopped heartbeating\n");
    vTaskDelay(pdMS_TO_TICKS(3000));

    printf("\n--- Scenario 2: 3 faults -> DEGRADED ---\n");
    for (int i = 0; i < 3; i++) {
        g_inject_fault = true;
        printf("[demo] injecting fault %d/3\n", i + 1);
        vTaskDelay(pdMS_TO_TICKS(800));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));

    printf("\n--- Scenario 3: dual-path unavailable -> SAFE ---\n");
    g_downlink->mark_unavailable();
    g_storage->mark_unavailable();
    g_worker->report_fault(fdir::Reason::User, 0, "dual-path forced");
    vTaskDelay(pdMS_TO_TICKS(1500));

    printf("\n--- Demo complete ---\n");
    vTaskEndScheduler();
    vTaskDelete(nullptr);
}

int main()
{
    /*
     * The failure queue is created before the port because the port's
     * submit_failure hook captures g_failure_queue by value at lambda creation
     * time, but xQueueCreate hasn't run yet. Instead, the lambda reads
     * g_failure_queue at call time, which is after xQueueCreate below.
     */
    g_failure_queue = xQueueCreate(16, sizeof(fdir::FailureReport));

    fdir::Port port{
        .get_now_ms = []() -> uint32_t {
            return static_cast<uint32_t>(xTaskGetTickCount() * portTICK_PERIOD_MS);
        },
        .submit_failure = [](const fdir::FailureReport &r) {
            return xQueueSend(g_failure_queue, &r, 0) == pdTRUE ? 0 : -1;
        },
        .isolate_current_worker = [] { vTaskSuspend(nullptr); },
        .emit_event = [](const fdir::Event &e) {
            static constexpr const char *kinds[] = {
                "FAILURE", "MODE_CHANGE", "RESTART", "WATCHDOG", "NOTE", "QUEUE_OVF",
            };
            const char *kind = static_cast<int>(e.kind) < 6
                               ? kinds[static_cast<int>(e.kind)] : "?";
            printf("[fdir] %-12s entity=%u %.*s\n",
                   kind, static_cast<unsigned>(e.entity),
                   static_cast<int>(e.detail().size()), e.detail().data());
        },
        .request_reboot = [](std::string_view reason) {
            printf("[fdir] reboot: %.*s\n",
                   static_cast<int>(reason.size()), reason.data());
            vTaskEndScheduler();
        },
    };

    auto sup_result = fdir::Supervisor::create(std::move(port), fdir::Config{
        .health_check_period_ms            = 500,
        .missed_heartbeat_tolerance        = 2,
        .safe_mode_critical_failure_threshold = 2,
    });
    if (!sup_result) { printf("Supervisor::create failed\n"); return 1; }
    g_sup.emplace(std::move(*sup_result));

    auto dl = g_sup->register_subsystem("downlink", true);
    auto st = g_sup->register_subsystem("storage",  true);
    if (!dl || !st) { printf("register_subsystem failed\n"); return 1; }
    g_downlink.emplace(std::move(*dl));
    g_storage.emplace(std::move(*st));

    auto entity = g_sup->register_entity({
        .name                  = "worker",
        .max_restarts          = 2,
        .max_watchdog_restarts = 1,
        .on_exhausted          = fdir::Action::Degrade,
        .restart               = [](fdir::EntityId) {
            printf("[demo] restarting worker\n");
            g_stop_heartbeat = false;
            g_inject_fault   = false;
            vTaskResume(g_worker_task);
            return 0;
        },
    });
    if (!entity) { printf("register_entity failed\n"); return 1; }
    g_worker.emplace(std::move(*entity));

    /* Startup self-test before the scheduler runs. Report a fault immediately
     * on failure; fdir applies normal restart/degrade logic when the supervisor
     * task drains the queue after vTaskStartScheduler(). */
    const bool worker_ok = true;
    if (!worker_ok) {
        g_worker->report_fault(fdir::Reason::IoError, 0, "startup self-test failed");
    }

    g_worker->heartbeat();

    xTaskCreate(worker_task,     "worker",     512, nullptr, 2, &g_worker_task);
    xTaskCreate(supervisor_task, "supervisor", 512, nullptr, 3, nullptr);
    xTaskCreate(scenario_task,   "scenario",   512, nullptr, 1, nullptr);

    printf("fdir FreeRTOS C++ demo starting\n");
    vTaskStartScheduler();

    printf("scheduler stopped\n");
    return 0;
}

extern "C" {
void vAssertCalled(const char *file, unsigned long line)
{
    printf("ASSERT failed: %s:%lu\n", file, line);
    vTaskEndScheduler();
}
}
