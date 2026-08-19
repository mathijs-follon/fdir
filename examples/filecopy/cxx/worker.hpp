#pragma once

#include "fdir.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

static constexpr int JOB_QUEUE_CAP = 4096;
static constexpr int WORKER_MAX    = 16;

struct CopyJob {
    std::string src;
    std::string dst;
};

/*
 * Bounded blocking job queue. The tree walker pushes; workers pop.
 * finish() signals that no more jobs will be pushed.
 */
class JobQueue {
public:
    void push(CopyJob job);
    bool pop(CopyJob &out);
    void finish();

private:
    CopyJob                 m_jobs[JOB_QUEUE_CAP];
    int                     m_head  = 0;
    int                     m_tail  = 0;
    int                     m_count = 0;
    bool                    m_done  = false;
    std::mutex              m_mu;
    std::condition_variable m_not_empty;
    std::condition_variable m_not_full;
};

struct WorkerCtx {
    int                  index;
    std::optional<fdir::Entity> entity;
    std::thread          thread;
    JobQueue            *queue = nullptr;
};

void worker_start(WorkerCtx &ctx, fdir::Supervisor &sup);
void worker_wait_all(WorkerCtx *workers, int n);
