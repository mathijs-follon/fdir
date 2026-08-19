# Threading model

`fdir` keeps recovery state in static storage. Concurrent access without a
defined model is undefined behaviour.

## Default contract (no port lock)

When `fdir_port_t.lock` and `fdir_port_t.unlock` are NULL, `fdir_port_sync_enter`
and `fdir_port_sync_exit` are no-ops. That is appropriate when a single
supervisor task and worker tasks never preempt each other during fdir calls
(for example a cooperative loop or a single-core RTOS with short critical
sections). It does **not** provide mutual exclusion on preemptive SMP hosts
(for example POSIX pthread workers plus a supervisor thread): supply
`lock`/`unlock` there. See `examples/filecopy/port.c`.

When `lock` and `unlock` are NULL:

| Context | May call |
|---------|----------|
| **Supervisor task** (single owner) | `fdir_supervisor_tick`, `fdir_set_system_mode`, `fdir_deescalate_system_mode`, subsystem marks, entity register |
| **Worker tasks** | `fdir_worker_may_run`, `fdir_report_fault`, `fdir_health_heartbeat_notify` |
| **Any** | `fdir_system_mode`, `fdir_health_snapshot_copy`, scalar subsystem queries (see below) |

The integrator must ensure **at most one supervisor context** executes
`fdir_supervisor_tick()` at a time. Workers must not call recovery APIs.

### Lock-free reads without a port lock

When only one supervisor and workers follow the table above, these reads are
typical without a port lock:

- `fdir_system_mode()` (internally locked; safe from any context)
- `fdir_health_snapshot()`: live pointer; numeric fields are usually fine;
  `detail[]` may tear if a worker reports a fault while you read the pointer.
  Prefer `fdir_health_snapshot_copy()` when tasks overlap.
- `fdir_subsystem_is_available`, `fdir_subsystem_is_degraded`,
  `fdir_subsystem_is_critical_path`, `fdir_subsystem_name`: unlocked reads of
  subsystem slots; treat as briefly stale under concurrent subsystem marks.

## Optional port lock

When `lock` and `unlock` are provided, the library wraps internal state updates
(including the internal failure queue) and documents-safe worker entry points
(`heartbeat`, `may_run`, health queries) with the same lock. Mode changes through
`fdir_enter_degraded_mode`, `fdir_enter_safe_mode`, and `fdir_try_reboot` also
take the port lock when called from application code.

Use a mutex or RTOS critical section. Locks must not call back into `fdir`.
Port callbacks (`emit_event`, `request_reboot`) and entity hooks (`restart`, `decide`) must not re-enter the library while the port lock is held.
`fdir_report_fault()` calls `get_now_ms()` while holding the port lock when a
lock is configured; keep that callback short and non-reentrant.
Re-entrant locks are not required.

See [docs/safety/seooC.md](safety/seooC.md) for assumptions placed on integrators.
