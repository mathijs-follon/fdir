# fdir

Failure Detection, Isolation, and Recovery (FDIR) library for embedded systems and RTOS applications, written in C11.

## Concepts

**Entity** - a task, thread, or software component registered with the framework. Each entity has a restart budget and a watchdog heartbeat. When a fault is reported fdir selects a recovery action (restart, degrade, safe mode, reboot) based on the entity's descriptor and the current system state.

**Subsystem** - a logical grouping of entities that can be marked available, degraded, or unavailable. Critical-path subsystems participate in the safe-mode threshold check.

**Mode** - the global operating mode, driven only by the supervisor recovery path:

| Mode | Value | Meaning |
|------|-------|---------|
| NOMINAL | 0 | All entities within budget |
| DEGRADED | 1 | At least one entity exhausted its restart budget |
| SAFE | 2 | Too many critical subsystems unavailable |
| REBOOT_PENDING | 3 | Reboot has been requested |

## Building

```
make          # build libfdir.a and compile_commands.json
make test     # build and run all tests
make clean
```

## Port hooks

Five weak functions in `src/port.c` must be overridden for your target. Three are required (the defaults abort); two are optional (the defaults are no-ops):

| Hook | Required | Default | Purpose |
|------|----------|---------|---------|
| `fdir_get_now_ms` | yes | abort | Return monotonic time in milliseconds |
| `fdir_post_failure` | yes | abort | Enqueue a failure report for the supervisor |
| `fdir_isolate_current_worker` | yes | abort | Stop the faulting task/thread |
| `fdir_emit_event` | no | no-op | Log or downlink an fdir event |
| `fdir_request_reboot` | no | no-op | Trigger a hardware reset |

Implement strong definitions of these in a any source file in your application. See `examples/getting_started/main.c` for a minimal POSIX implementation.

## Integrating

```c
#include "fdir.h"

// 1. configure
fdir_config_t cfg = fdir_config_default();
cfg.health_check_period_ms               = 1000;
cfg.missed_heartbeat_tolerance           = 3;
cfg.safe_mode_critical_failure_threshold = 2;
fdir_init(&cfg);

// 2. register an entity
fdir_entity_desc_t desc = {
    .name                  = "sensor",
    .max_restarts          = 3,
    .max_watchdog_restarts = 1,
    .on_exhausted          = FDIR_ACTION_DEGRADE,
    .linked_subsystem      = FDIR_SUBSYSTEM_NONE,
    .restart               = sensor_restart_cb,
};
fdir_entity_id_t id;
fdir_entity_register(&desc, &id);

// 3. heartbeat from the entity's context (e.g. end of each task loop)
fdir_health_heartbeat_notify(id);

// 4. report a fault (from any context; queued to the supervisor)
fdir_failure_report_t r = {
    .entity    = id,
    .reason    = FDIR_REASON_IO_ERROR,
    .error_code = errno,
};
fdir_post_failure(&r);

// 5. supervisor loop (dedicated task or periodic call)
fdir_failure_report_t report;
while (queue_pop(&report))
    fdir_handle_failure(&report);
fdir_check_watchdogs();
```

## Examples

### getting_started

Minimal single-file example with no RTOS and no threads. Covers fault/restart, budget exhaustion, and watchdog detection. Port hooks are implemented inline at the bottom of the file.

```
make getting_started
./build/getting_started
```

### filecopy

Parallel directory copy CLI using worker threads supervised by fdir. Demonstrates multi-entity registration, per-worker heartbeating, and fault-driven restart/degrade on I/O errors.

```
make filecopy
./build/fcopy <src> <dst> [--workers N]
```

### FreeRTOS

Full FreeRTOS integration on the POSIX/Linux simulator. Three FreeRTOS tasks (worker, supervisor, scenario driver) drive watchdog miss, budget exhaustion, and dual-path SAFE mode. Port hooks are wired to FreeRTOS queue and task primitives.

```
git submodule update --init examples/FreeRTOS/FreeRTOS-Kernel
make freertos
./build/example_FreeRTOS
```

## API reference

### Initialisation

```c
fdir_config_t fdir_config_default(void);
fdir_status_t fdir_init(const fdir_config_t *config);
```

### Entities

```c
fdir_status_t    fdir_entity_register(const fdir_entity_desc_t *desc, fdir_entity_id_t *out_id);
fdir_entity_id_t fdir_entity_count(void);
const char      *fdir_entity_name(fdir_entity_id_t id);
```

### Health and heartbeat

```c
void fdir_health_heartbeat_notify(fdir_entity_id_t id);
void fdir_health_set(fdir_entity_id_t id, fdir_health_t health, uint16_t error_code const char *detail);
const fdir_health_snapshot_t *fdir_health_snapshot(fdir_entity_id_t id);
```

### Supervisor

```c
void fdir_handle_failure(const fdir_failure_report_t *report);
void fdir_check_watchdogs(void);
```

### Mode

```c
fdir_mode_t fdir_system_mode(void);
void        fdir_enter_degraded_mode(void);
void        fdir_enter_safe_mode(void);
void        fdir_try_reboot(const char *reason);
```

### Subsystems

```c
fdir_status_t fdir_subsystem_register(const fdir_subsystem_desc_t *desc, fdir_subsystem_id_t *out_id);
void          fdir_subsystem_mark_available(fdir_subsystem_id_t id);
void          fdir_subsystem_mark_unavailable(fdir_subsystem_id_t id);
void          fdir_subsystem_mark_degraded(fdir_subsystem_id_t id);
```

### Port hooks

```c
uint32_t fdir_get_now_ms(void);
int      fdir_post_failure(const fdir_failure_report_t *report);
void     fdir_isolate_current_worker(void);
void     fdir_emit_event(const fdir_event_t *event);
void     fdir_request_reboot(const char *reason);
```

### Logging

```c
void fdir_log_note(const char *note);
void fdir_log_queue_overflow(uint16_t queue_id, uint32_t overflow_delta, uint32_t depth_high_water);
```
