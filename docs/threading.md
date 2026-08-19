# Threading model

`fdir` keeps recovery state in static storage. Concurrent access without a
defined model is undefined behaviour.

## Default contract (no port lock)

When `fdir_port_t.lock` and `fdir_port_t.unlock` are NULL:

| Context | May call |
|---------|----------|
| **Supervisor task** (single owner) | `fdir_supervisor_tick`, `fdir_set_system_mode`, `fdir_deescalate_system_mode`, subsystem marks, entity register |
| **Worker tasks** | `fdir_worker_may_run`, `fdir_report_fault`, `fdir_health_heartbeat_notify` |
| **Any** | `fdir_system_mode`, `fdir_health_snapshot` (read-only snapshots; may be briefly stale) |

The integrator must ensure **at most one supervisor context** executes
`fdir_supervisor_tick()` at a time. Workers must not call recovery APIs.

## Optional port lock

When `lock` and `unlock` are provided, the library wraps internal state updates
(including the internal failure queue) and documents-safe worker entry points
(`heartbeat`, `may_run`, health queries) with the same lock. Mode changes through
`fdir_enter_degraded_mode`, `fdir_enter_safe_mode`, and `fdir_try_reboot` also
take the port lock when called from application code.

Use a mutex or RTOS critical section. Locks must not call back into `fdir`.
Port callbacks (`emit_event`, `request_reboot`) and entity hooks (`restart`, `decide`) must not re-enter the library while the port lock is held.
Re-entrant locks are not required.

See [docs/safety/seooC.md](safety/seooC.md) for assumptions placed on integrators.
