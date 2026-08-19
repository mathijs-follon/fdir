# API reference

[Integration guide](integration.md) | [v1.0.0 changelog](changenotes/v1.0.0.md)

## C API

`include/fdir.h` is the C11 interface. Link against `libfdir.a` and supply the three required platform callbacks through a `fdir_port_t` passed to `fdir_init()`.

### Integrating

```c
#include "fdir.h"

static uint32_t platform_now_ms(void) { return platform_ticks_ms(); }
static void platform_emit(const fdir_event_t *e) { log_event(e); }
static void platform_reboot(const char *reason) { platform_reset(reason); }

fdir_port_t port = {
    .get_now_ms     = platform_now_ms,
    .emit_event     = platform_emit,
    .request_reboot = platform_reboot,
};

fdir_config_t cfg = fdir_config_default();
fdir_status_t status = fdir_init(&cfg, &port);
if (status != FDIR_OK) {
    /* FDIR_ERR_PORT: port NULL or missing required callback */
    log("fdir_init: %s", fdir_status_string(status));
}

fdir_entity_desc_t desc = {
    .name                  = "sensor",
    .max_restarts          = 3,
    .max_watchdog_restarts = 1,
    .on_exhausted          = FDIR_ACTION_DEGRADE,
    .restart               = sensor_restart_cb,
};
fdir_entity_id_t id;
fdir_entity_register(&desc, &id);

/* Worker loop: poll before each unit of work, heartbeat after success. */
while (fdir_worker_may_run(id)) {
    if (!do_one_cycle()) {
        fdir_report_fault(id, FDIR_REASON_IO_ERROR, errno, "detail");
        break;
    }
    fdir_health_heartbeat_notify(id);
}

/* Supervisor loop */
fdir_supervisor_tick();
```

### Port hooks

Three callbacks are required. Register them in a `fdir_port_t` and pass it to `fdir_init()`. If `port` is NULL or any function pointer is NULL, `fdir_init()` returns `FDIR_ERR_PORT`.

| Hook | Purpose |
|------|---------|
| `get_now_ms` | Return monotonic time in milliseconds |
| `emit_event` | Log or downlink an fdir event |
| `request_reboot` | Trigger a hardware reset |

```c
typedef struct {
    uint32_t (*get_now_ms)(void);
    void (*emit_event)(const fdir_event_t *event);
    void (*request_reboot)(const char *reason);
    void (*lock)(void);    /* optional */
    void (*unlock)(void);  /* optional */
} fdir_port_t;
```

Failure reports are queued inside the library (default depth 8, override with `FDIR_FAILURE_QUEUE_CAP`). Workers call `fdir_report_fault()`; the supervisor calls `fdir_supervisor_tick()` to drain the queue and run watchdog checks.

### Events

Each `fdir_event_t` includes:

| Field | Meaning |
|-------|---------|
| `anomaly_id` | Entity key: `FDIR_ANOMALY_ID(entity, reason)`; subsystem key: `FDIR_SUBSYSTEM_ANOMALY_ID(sub, reason)` |
| `severity` | `FDIR_SEVERITY_*` |
| `phase` | `FDIR_ANOMALY_RAISED` or `FDIR_ANOMALY_CLEARED` (after restart or ground mark) |
| `level` | `FDIR_LEVEL_ENTITY`, `SUBSYSTEM`, or `SYSTEM` |
| `subsystem` | Subsystem id when `level == FDIR_LEVEL_SUBSYSTEM`; otherwise `FDIR_SUBSYSTEM_NONE` |

Repeated reports for the same latched entity+reason are suppressed (ECSS 5.7.5.3c style). Query latch state with `fdir_health_fault_is_latched()`.

### FDIR levels

| Level | Hook / mechanism |
|-------|------------------|
| Entity | `fdir_entity_desc_t.decide`, restart budgets |
| Subsystem | `fdir_subsystem_desc_t.on_entity_exhausted` (optional) |
| System | Mode escalation, `fdir_config_t.on_system_escalation` (optional) |

Zero-initialise descriptor structs before filling fields.

### Restart budgets

| Field | Meaning |
|-------|---------|
| `max_restarts` | Non-watchdog restart attempts before `on_exhausted` (0 = none) |
| `max_watchdog_restarts` | Watchdog-triggered restart attempts before `on_exhausted` |
| `FDIR_RESTART_UNLIMITED` (`255`) | Either budget field may be set to this sentinel for uncapped restarts |

Counters increment on each supervisor restart and persist until the entity is re-registered. They do not reset when the entity runs cleanly again.

### Mode (ground de-escalation)

```c
fdir_status_t fdir_set_system_mode(fdir_mode_t mode);      /* ground command */
fdir_status_t fdir_deescalate_system_mode(void);           /* SAFE->DEGRADED->NOMINAL */
```

`REBOOT_PENDING` is only entered via `fdir_try_reboot()`.

### Supervisor bypass

```c
fdir_bool_t fdir_set_supervision_enabled(fdir_bool_t enabled);  /* returns previous state */
fdir_bool_t fdir_supervision_enabled(void);
```

When `enabled == 0`, FDIR recovery is inhibited: workers are not gated, faults are not queued, supervisor tick and watchdog scan are no-ops. Pending queue entries are discarded on disable. See [integration.md](integration.md#supervisor-bypass-ground-inhibit).

### Critical fault queue policy

`fdir_failure_report_t.flags` may include `FDIR_FAULT_FLAG_CRITICAL`. `fdir_report_fault()` sets this by default and returns `FDIR_ERR_BUSY` if the internal failure queue is full. Query `fdir_failure_queue_full_latched()` after overflow. See [safety/seooC.md](safety/seooC.md).

### Initialisation

```c
fdir_config_t fdir_config_default(void);
fdir_status_t fdir_init(const fdir_config_t *config, const fdir_port_t *port);
const char   *fdir_status_string(fdir_status_t status);
```

### Entities

```c
fdir_status_t    fdir_entity_register(const fdir_entity_desc_t *desc, fdir_entity_id_t *out_id);
fdir_entity_id_t fdir_entity_count(void);
const char      *fdir_entity_name(fdir_entity_id_t id);
```

### Worker loop

```c
fdir_bool_t fdir_worker_may_run(fdir_entity_id_t id);  /* worker loops: heartbeats in SAFE/REBOOT */
fdir_bool_t fdir_entity_may_run(fdir_entity_id_t id); /* read-only gate, no side effects */
```

See [integration.md](integration.md).

### Health and heartbeat

```c
void fdir_health_heartbeat_notify(fdir_entity_id_t id);
void fdir_health_set(fdir_entity_id_t id, fdir_health_t health, uint16_t error_code, const char *detail);
void fdir_health_reset(fdir_entity_id_t id);
const fdir_health_snapshot_t *fdir_health_snapshot(fdir_entity_id_t id);
fdir_bool_t fdir_health_fault_is_latched(fdir_entity_id_t id, fdir_reason_t reason);
```

Heartbeats do not clear `FDIR_HEALTH_FAILED`. Call `fdir_report_fault()` from the faulting worker to mark the entity failed and enqueue a report in one step.

After peripheral init or self-test succeeds, call `fdir_health_reset()` and heartbeat so the entity is not left in a stale failed state. See [integration.md](integration.md#peripheral-bring-up-and-health). `fdir_health_reset()` does not clear fault latches or restart budgets.

```c
void fdir_report_fault(fdir_entity_id_t entity, fdir_reason_t reason,
                       uint16_t error_code, const char *detail);

fdir_status_t fdir_report_fault_ex(fdir_entity_id_t entity, fdir_reason_t reason,
                                   uint16_t error_code, const char *detail,
                                   uint8_t flags);
```

`fdir_report_fault()` always sets `FDIR_FAULT_FLAG_CRITICAL`. Use `fdir_report_fault_ex()` when non-critical reports may be dropped (`FDIR_ERR_STATE`) if the internal queue is full.

### Supervisor loop

```c
void fdir_supervisor_tick(void);   /* drain internal queue + watchdog scan */
void fdir_handle_failure(const fdir_failure_report_t *report);
void fdir_check_watchdogs(void);
fdir_bool_t fdir_failure_queue_full_latched(void);
```

```c
fdir_status_t fdir_submit_failure(const fdir_failure_report_t *report);
```

`fdir_submit_failure()` enqueues without changing entity health (advanced use). Prefer `fdir_report_fault()` from workers.

### Mode

```c
fdir_mode_t fdir_system_mode(void);
void        fdir_enter_degraded_mode(void);
void        fdir_enter_safe_mode(void);
void        fdir_reassess_system_mode(void);
void        fdir_try_reboot(const char *reason);
```

### Subsystems

```c
fdir_status_t fdir_subsystem_register(const fdir_subsystem_desc_t *desc, fdir_subsystem_id_t *out_id);
void          fdir_subsystem_mark_available(fdir_subsystem_id_t id);
void          fdir_subsystem_mark_unavailable(fdir_subsystem_id_t id);
void          fdir_subsystem_mark_degraded(fdir_subsystem_id_t id);
```

### Logging

```c
void fdir_log_note(const char *note);
void fdir_log_queue_overflow(uint16_t queue_id, uint32_t overflow_delta, uint32_t depth_high_water);
```
