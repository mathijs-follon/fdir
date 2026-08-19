#include "fdir.hpp"

#include <cstdio>
#include <mutex>
#include <queue>
#include <time.h>
#include <unistd.h>

static std::mutex                      g_mu;
static std::queue<fdir::FailureReport> g_pending;

bool port_pop_failure(fdir::FailureReport &out)
{
    std::lock_guard lock(g_mu);
    if (g_pending.empty()) { return false; }
    out = g_pending.front();
    g_pending.pop();
    return true;
}

fdir::Port make_port()
{
    return fdir::Port{
        .get_now_ms = []() -> uint32_t {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            return static_cast<uint32_t>(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
        },
        .post_failure = [](const fdir::FailureReport &r) {
            std::lock_guard lock(g_mu);
            g_pending.push(r);
            return 0;
        },
        .isolate_current_worker = [] { pthread_exit(nullptr); },
        .emit_event = [](const fdir::Event &e) {
            static constexpr const char *modes[] = {
                "NOMINAL", "DEGRADED", "SAFE", "REBOOT_PENDING",
            };
            static constexpr const char *kinds[] = {
                "FAILURE", "MODE_CHANGE", "RESTART", "WATCHDOG", "NOTE", "QUEUE_OVF",
            };
            const char *mode = static_cast<int>(e.mode) < 4 ? modes[static_cast<int>(e.mode)] : "?";
            const char *kind = static_cast<int>(e.kind) < 6 ? kinds[static_cast<int>(e.kind)] : "?";
            fprintf(stderr, "[fdir] %-12s mode=%-16s entity=%u %.*s\n",
                    kind, mode, static_cast<unsigned>(e.entity),
                    static_cast<int>(e.detail().size()), e.detail().data());
        },
        .request_reboot = [](std::string_view reason) {
            fprintf(stderr, "[fdir] fatal: %.*s\n",
                    static_cast<int>(reason.size()), reason.data());
            _exit(1);
        },
    };
}
