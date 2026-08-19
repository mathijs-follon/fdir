#include "worker.hpp"

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static constexpr int COPY_BUF_SIZE = 256 * 1024;

void JobQueue::push(CopyJob job)
{
    std::unique_lock lock(m_mu);
    m_not_full.wait(lock, [this] { return m_count < JOB_QUEUE_CAP; });
    m_jobs[m_tail] = std::move(job);
    m_tail = (m_tail + 1) % JOB_QUEUE_CAP;
    m_count++;
    m_not_empty.notify_one();
}

bool JobQueue::pop(CopyJob &out)
{
    std::unique_lock lock(m_mu);
    m_not_empty.wait(lock, [this] { return m_count > 0 || m_done; });
    if (m_count == 0) {
        m_not_empty.notify_all();
        return false;
    }
    out = std::move(m_jobs[m_head]);
    m_head = (m_head + 1) % JOB_QUEUE_CAP;
    m_count--;
    m_not_full.notify_one();
    return true;
}

void JobQueue::finish()
{
    std::unique_lock lock(m_mu);
    m_done = true;
    m_not_empty.notify_all();
}

static int copy_file(const std::string &src, const std::string &dst)
{
    static thread_local char buf[COPY_BUF_SIZE];

    int src_fd = open(src.c_str(), O_RDONLY);
    if (src_fd < 0) { return -1; }

    struct stat st;
    if (fstat(src_fd, &st) < 0) { close(src_fd); return -1; }

    int dst_fd = open(dst.c_str(), O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);
    if (dst_fd < 0) { close(src_fd); return -1; }

    ssize_t n;
    while ((n = read(src_fd, buf, sizeof(buf))) > 0) {
        const char *p = buf;
        while (n > 0) {
            ssize_t w = write(dst_fd, p, static_cast<size_t>(n));
            if (w < 0) { close(src_fd); close(dst_fd); return -1; }
            p += w;
            n -= w;
        }
    }

    close(src_fd);
    close(dst_fd);
    return n < 0 ? -1 : 0;
}

void worker_start(WorkerCtx &ctx, fdir::Supervisor &sup)
{
    ctx.thread = std::thread([&ctx, &sup] {
        CopyJob job;
        while (ctx.queue->pop(job)) {
            if (copy_file(job.src, job.dst) != 0) {
                ctx.entity->report_fault(fdir::Reason::IoError,
                                         static_cast<uint16_t>(errno),
                                         job.src);
            }
            ctx.entity->heartbeat();
        }
    });
}

void worker_wait_all(WorkerCtx *workers, int n)
{
    for (int i = 0; i < n; i++) {
        if (workers[i].thread.joinable()) {
            workers[i].thread.join();
        }
    }
}
