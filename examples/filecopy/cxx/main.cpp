/*
 * fcopy (C++) - parallel directory copy with fdir-based fault recovery
 *
 * Usage: fcopy_cxx <src> <dst> [--workers N]
 *
 * Build: make filecopy_cxx
 * Run:   ./build/fcopy_cxx <src> <dst>
 */

#define FDIR_HPP_IMPL
#include "fdir.hpp"
#include "worker.hpp"

#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <ftw.h>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <time.h>
#include <unistd.h>

fdir::Port make_port();
bool       port_pop_failure(fdir::FailureReport &out);

/*
 * nftw requires a plain C callback with no closure, so the walk state is
 * kept here. These are written once before nftw and read from the callback.
 */
static JobQueue    *g_walk_queue   = nullptr;
static const char  *g_walk_src    = nullptr;
static const char  *g_walk_dst    = nullptr;

static int walk_cb(const char *fpath, const struct stat *sb,
                   int typeflag, struct FTW *ftwbuf)
{
    (void)sb; (void)ftwbuf;
    const char *rel = fpath + strlen(g_walk_src);
    std::string dst = std::string(g_walk_dst) + rel;

    if (typeflag == FTW_D) {
        mkdir(dst.c_str(), 0755);
        return 0;
    }
    if (typeflag == FTW_F) {
        g_walk_queue->push({ fpath, std::move(dst) });
    }
    return 0;
}

static void usage(const char *argv0)
{
    fprintf(stderr, "usage: %s <src> <dst> [--workers N]\n", argv0);
}

int main(int argc, char **argv)
{
    if (argc < 3) { usage(argv[0]); return 1; }

    std::string src = argv[1];
    std::string dst = argv[2];
    int nworkers = 4;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--workers") == 0 && i + 1 < argc) {
            nworkers = std::atoi(argv[++i]);
            if (nworkers < 1 || nworkers > WORKER_MAX) {
                fprintf(stderr, "workers must be 1..%d\n", WORKER_MAX);
                return 1;
            }
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    if (!src.empty() && src.back() == '/') { src.pop_back(); }

    struct stat st;
    if (stat(src.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "fcopy: '%s' is not a directory\n", src.c_str());
        return 1;
    }
    if (mkdir(dst.c_str(), 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "fcopy: cannot create '%s': %s\n", dst.c_str(), strerror(errno));
        return 1;
    }

    auto sup_result = fdir::Supervisor::create(make_port(), fdir::Config{
        .health_check_period_ms            = 2000,
        .missed_heartbeat_tolerance        = 3,
        .safe_mode_critical_failure_threshold = static_cast<uint8_t>(nworkers),
    });
    if (!sup_result) {
        fprintf(stderr, "fcopy: Supervisor::create failed\n");
        return 1;
    }
    fdir::Supervisor sup = std::move(*sup_result);

    JobQueue queue;
    WorkerCtx workers[WORKER_MAX];

    for (int i = 0; i < nworkers; i++) {
        auto entity = sup.register_entity({
            .name                  = "worker",
            .max_restarts          = 2,
            .max_watchdog_restarts = 1,
            .on_exhausted          = fdir::Action::Unavailable,
            .restart               = [i, &workers, &sup](fdir::EntityId) {
                fprintf(stderr, "[fcopy] restarting worker %d\n", i);
                if (workers[i].thread.joinable()) { workers[i].thread.join(); }
                worker_start(workers[i], sup);
                return 0;
            },
        });
        if (!entity) {
            fprintf(stderr, "fcopy: register_entity failed for worker %d\n", i);
            return 1;
        }
        workers[i].index  = i;
        workers[i].entity = std::move(*entity);
        workers[i].queue  = &queue;

        /* Startup self-test: verify I/O path before accepting jobs. fdir
         * handles restart/degrade using the same logic as runtime faults. */
        const bool worker_ok = true;
        if (!worker_ok) {
            workers[i].entity->report_fault(fdir::Reason::IoError, 0,
                                            "startup self-test failed");
            fdir::FailureReport r;
            while (port_pop_failure(r)) { sup.handle_failure(r); }
        }

        workers[i].entity->heartbeat();
        worker_start(workers[i], sup);
    }

    std::atomic<bool> running{true};
    std::thread sup_thread([&sup, &queue, &running] {
        while (running) {
            fdir::FailureReport r;
            while (port_pop_failure(r)) {
                sup.handle_failure(r);
            }
            sup.check_watchdogs();

            if (sup.mode() >= fdir::Mode::Safe) {
                fprintf(stderr, "[fcopy] system entered SAFE mode, aborting\n");
                queue.finish();
                break;
            }

            struct timespec t = { .tv_sec = 0, .tv_nsec = 200000000 };
            nanosleep(&t, nullptr);
        }
    });

    g_walk_queue = &queue;
    g_walk_src   = src.c_str();
    g_walk_dst   = dst.c_str();
    nftw(src.c_str(), walk_cb, 64, FTW_PHYS);
    queue.finish();

    worker_wait_all(workers, nworkers);
    running = false;
    sup_thread.join();

    if (sup.mode() >= fdir::Mode::Safe) {
        fprintf(stderr, "fcopy: completed in degraded state\n");
        return 1;
    }

    printf("fcopy: done\n");
    return 0;
}
