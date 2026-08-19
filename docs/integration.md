# Integration guide

Portable wiring for `fdir`: three required port hooks, internal failure queue, cooperative workers.

## Minimal setup

```c
#include "fdir.h"

static uint32_t platform_now_ms(void) { return ticks_ms(); }
static void platform_emit(const fdir_event_t *e) { log_event(e); }
static void platform_reboot(const char *reason) { platform_reset(reason); }

static fdir_port_t port = {
    .get_now_ms     = platform_now_ms,
    .emit_event     = platform_emit,
    .request_reboot = platform_reboot,
};

fdir_status_t status = fdir_init(&cfg, &port);
if (status != FDIR_OK) {
    log("fdir_init: %s", fdir_status_string(status));
    return;
}
```

## Roles

| Role | Calls |
|------|-------|
| **Supervisor** | `fdir_supervisor_tick()` |
| **Worker** | `fdir_worker_may_run()`, `fdir_report_fault()`, `fdir_health_heartbeat_notify()` |
| **Init / ground** | `fdir_entity_register()`, `fdir_set_system_mode()` |

## Worker loop

```c
for (;;) {
    if (!fdir_worker_may_run(entity_id)) {
        osDelay(HEALTH_CHECK_PERIOD_MS);
        continue;
    }

    if (!do_one_unit_of_work()) {
        fdir_report_fault(entity_id, FDIR_REASON_IO_ERROR, code, "detail");
        continue;
    }
    fdir_health_heartbeat_notify(entity_id);
}
```

`fdir_worker_may_run()` heartbeats automatically in SAFE/REBOOT mode. Use `fdir_entity_may_run()` only for read-only checks (no side effects).

## Supervisor loop

```c
for (;;) {
    fdir_supervisor_tick();
    osDelay(1);
}
```

## Policy

Express recovery rules in entity `decide()` callbacks and subsystem registration. See [examples/dual_path/main.c](../examples/dual_path/main.c).

### Restart budgets

`max_restarts` and `max_watchdog_restarts` are per-entity limits on supervisor-driven restarts. Set either field to **`FDIR_RESTART_UNLIMITED`** (`255`) to disable that budget: the supervisor will keep choosing `RESTART` until your `decide()` hook returns something else or `on_exhausted` applies through another path. Use **`0`** to skip restarts entirely and go straight to `on_exhausted` on the first matching fault.

Restart counters accumulate across successful restarts; they are **not** cleared when an entity starts working again. System mode and subsystem marks likewise **do not** de-escalate automatically when faults stop: only ground APIs (`fdir_deescalate_system_mode`, `fdir_set_system_mode`) lower global mode.

### Peripheral bring-up and health

When starting a peripheral or re-initializing hardware outside the supervisor `restart()` callback, confirm the device is healthy before taking work, then establish a clean entity baseline:

```c
if (peripheral_init() != 0) {
    fdir_report_fault(entity_id, FDIR_REASON_INIT_FAILED, code, "init failed");
} else {
    fdir_health_reset(entity_id);
    fdir_health_heartbeat_notify(entity_id);
}
```

Call **`fdir_health_reset()`** after a successful self-test or init sequence so stale `FAILED` health or error detail from an earlier fault does not block the worker gate. This does **not** clear the fault latch (set only after supervisor handling) or restart budgets: that is intentional; an entity that failed repeatedly and recovers once should not silently erase its recovery history unless you deliberately re-register it or ground clears mode.

If an entity recovered through **`restart()`**, the supervisor already resets health and clears the latch; an extra `fdir_health_reset()` is optional.

Do **not** assume that "working again" implies nominal system mode. De-escalate mode from ground when mission rules allow.

### Supervisor bypass (ground inhibit)

If FDIR itself is suspect, or you need workers to run without gating, restarts, or watchdog escalation: disable supervision from ground:

```c
fdir_set_supervision_enabled(0);  /* bypass: workers run, faults ignored */
/* ... investigate ...
fdir_set_supervision_enabled(1);  /* restore normal FDIR */
```

While disabled:

- `fdir_worker_may_run()` / `fdir_entity_may_run()` always allow work
- `fdir_report_fault()` and `fdir_supervisor_tick()` have no recovery effect
- The internal failure queue is cleared on disable
- A NOTE event (`supervision_disabled` / `supervision_enabled`) is emitted

State persists until re-enabled or `fdir_init()` on reboot.

## Further reading

- [api.md](api.md)
- [threading.md](threading.md)
- [safety/seooC.md](safety/seooC.md)
