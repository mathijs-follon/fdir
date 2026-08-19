/*
 * getting_started (C++) - minimal fdir C++ API example, no RTOS, no threads
 *
 * Build: make getting_started_cxx
 * Run:   ./build/getting_started_cxx
 *
 * Three scenarios:
 *   1. Single fault   - fdir restarts the entity
 *   2. Three faults   - restart budget exhausted, mode -> DEGRADED
 *   3. Watchdog miss  - stale heartbeat detected, entity restarted
 *
 * Port hooks are supplied as a fdir::Port struct of lambdas.
 * Define FDIR_HPP_IMPL in exactly one translation unit (done here).
 */

#define _POSIX_C_SOURCE 200809L
#define FDIR_HPP_IMPL
#include "fdir.hpp"

#include <array>
#include <cstdio>
#include <time.h>

/*
 * Thread-safe-enough single-threaded failure queue. submit_failure pushes here;
 * drain() calls supervisor.handle_failure() for each pending report. On an RTOS
 * this role is filled by a kernel message queue.
 */
template <int Cap>
class FailureQueue {
public:
    void push(const fdir::FailureReport &r)
    {
        if (m_count == Cap) { return; }
        m_buf[m_tail] = r;
        m_tail = (m_tail + 1) % Cap;
        m_count++;
    }

    bool pop(fdir::FailureReport &out)
    {
        if (m_count == 0) { return false; }
        out = m_buf[m_head];
        m_head = (m_head + 1) % Cap;
        m_count--;
        return true;
    }

private:
    std::array<fdir::FailureReport, Cap> m_buf{};
    int m_head  = 0;
    int m_tail  = 0;
    int m_count = 0;
};

static FailureQueue<16> g_failures;

static void drain(fdir::Supervisor &sup)
{
    fdir::FailureReport r;
    while (g_failures.pop(r)) {
        sup.handle_failure(r);
    }
}

static const char *mode_name(fdir::Mode m)
{
    switch (m) {
    case fdir::Mode::Nominal:       return "NOMINAL";
    case fdir::Mode::Degraded:      return "DEGRADED";
    case fdir::Mode::Safe:          return "SAFE";
    case fdir::Mode::RebootPending: return "REBOOT_PENDING";
    }
    return "?";
}

int main()
{
    fdir::Port port{
        .get_now_ms = []() -> uint32_t {
            struct timespec t;
            clock_gettime(CLOCK_MONOTONIC, &t);
            return static_cast<uint32_t>(t.tv_sec * 1000 + t.tv_nsec / 1000000);
        },
        .submit_failure = [](const fdir::FailureReport &r) {
            g_failures.push(r);
            return 0;
        },
        .isolate_current_worker = [] {
            /* Single-threaded: nothing to isolate. The override is still required
             * because the default weak symbol calls abort(). */
        },
        .emit_event = [](const fdir::Event &e) {
            static constexpr const char *kinds[] = {
                "FAILURE", "MODE_CHANGE", "RESTART", "WATCHDOG", "NOTE", "QUEUE_OVF",
            };
            const char *kind = static_cast<int>(e.kind) < 6
                               ? kinds[static_cast<int>(e.kind)] : "?";
            printf("  [fdir] %-12s %.*s\n",
                   kind, static_cast<int>(e.detail().size()), e.detail().data());
        },
        .request_reboot = [](std::string_view reason) {
            printf("  [fdir] reboot: %.*s\n",
                   static_cast<int>(reason.size()), reason.data());
        },
    };

    auto sup_result = fdir::Supervisor::create(std::move(port), fdir::Config{
        .health_check_period_ms            = 1000,
        .missed_heartbeat_tolerance        = 2,
        .safe_mode_critical_failure_threshold = 1,
    });
    if (!sup_result) { fprintf(stderr, "Supervisor::create failed\n"); return 1; }
    auto &sup = *sup_result;

    auto sensor_result = sup.register_entity({
        .name                  = "sensor",
        .max_restarts          = 2,
        .max_watchdog_restarts = 1,
        .on_exhausted          = fdir::Action::Degrade,
        .restart               = [](fdir::EntityId) {
            printf("  [app] sensor restarted\n");
            return 0;
        },
    });
    if (!sensor_result) { fprintf(stderr, "register_entity failed\n"); return 1; }
    auto &sensor = *sensor_result;

    /* Startup self-test: report a fault immediately if the entity fails its
     * initialization check. fdir handles restart/degrade just like a runtime
     * fault. In a real application this would verify hardware or calibration. */
    const bool sensor_ok = true;
    if (!sensor_ok) {
        sensor.report_fault(fdir::Reason::IoError, 0, "startup self-test failed");
        drain(sup);
    }

    sensor.heartbeat();
    printf("mode: %s\n\n", mode_name(sup.mode()));

    printf("--- fault 1: restart ---\n");
    sensor.report_fault(fdir::Reason::IoError, 0, "read timeout");
    drain(sup);
    printf("mode: %s\n\n", mode_name(sup.mode()));

    printf("--- faults 2+3: budget exhausted -> DEGRADED ---\n");
    sensor.heartbeat();
    sensor.report_fault(fdir::Reason::IoError, 0, "read timeout");
    drain(sup);
    sensor.heartbeat();
    sensor.report_fault(fdir::Reason::IoError, 0, "read timeout");
    drain(sup);
    printf("mode: %s\n\n", mode_name(sup.mode()));

    printf("--- watchdog miss -> restart ---\n");
    sensor.heartbeat();
    struct timespec ts = { .tv_sec = 3, .tv_nsec = 500000000 };
    nanosleep(&ts, nullptr);
    sup.check_watchdogs();
    printf("mode: %s\n", mode_name(sup.mode()));

    return 0;
}
