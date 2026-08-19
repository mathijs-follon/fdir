# API reference

[C API](#c-api) | [C++ API](#c-api-1)

## C API

`include/fdir.h` is the C11 interface. Link against `libfdir.a` and implement the five port hooks as strong overrides of the weak symbols in a file like `src/port.c`.

### Integrating

```c
#include "fdir.h"

// 1. implement port hooks in any .c file in your application
uint32_t fdir_get_now_ms(void)                              { return platform_ticks_ms(); }
int      fdir_submit_failure(const fdir_failure_report_t *r) { return queue_push(r); }
void     fdir_isolate_current_worker(void)                  { task_delete_self(); }
void     fdir_emit_event(const fdir_event_t *e)             { log_event(e); }      // optional
void     fdir_request_reboot(const char *reason)            { platform_reboot(); } // optional

// 2. configure and initialise
fdir_config_t cfg = fdir_config_default();
cfg.health_check_period_ms               = 1000;
cfg.missed_heartbeat_tolerance           = 3;
cfg.safe_mode_critical_failure_threshold = 2;
fdir_init(&cfg);

// 3. register an entity
fdir_entity_desc_t desc = {
    .name                  = "sensor",
    .max_restarts          = 3,
    .max_watchdog_restarts = 1,
    .on_exhausted          = FDIR_ACTION_DEGRADE,
    .restart               = sensor_restart_cb,
};
fdir_entity_id_t id;
fdir_entity_register(&desc, &id);

// 4. heartbeat from the entity's own context each cycle
fdir_health_heartbeat_notify(id);

// 5. report a fault from any context
fdir_failure_report_t r = {
    .entity     = id,
    .reason     = FDIR_REASON_IO_ERROR,
    .error_code = errno,
};
fdir_submit_failure(&r);

// 6. supervisor loop: dedicated task or periodic callback
fdir_failure_report_t report;
while (queue_pop(&report))
    fdir_handle_failure(&report);
fdir_check_watchdogs();
```

### Port hooks

Five weak functions in `src/port.c` must be overridden for your target. Three are required (the defaults abort); two are optional (the defaults are no-ops):

| Hook | Required | Default | Purpose |
|------|----------|---------|---------|
| `fdir_get_now_ms` | yes | abort | Return monotonic time in milliseconds |
| `fdir_submit_failure` | yes | abort | Enqueue a failure report for the supervisor |
| `fdir_isolate_current_worker` | yes | abort | Stop the faulting task/thread |
| `fdir_emit_event` | no | no-op | Log or downlink an fdir event |
| `fdir_request_reboot` | no | no-op | Trigger a hardware reset |

```c
uint32_t fdir_get_now_ms(void);
int      fdir_submit_failure(const fdir_failure_report_t *r);
void     fdir_isolate_current_worker(void);
void     fdir_emit_event(const fdir_event_t *event);
void     fdir_request_reboot(const char *reason);
```

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
void fdir_health_set(fdir_entity_id_t id, fdir_health_t health, uint16_t error_code, const char *detail);
const fdir_health_snapshot_t *fdir_health_snapshot(fdir_entity_id_t id);
```

### Supervisor loop

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

### Logging

```c
void fdir_log_note(const char *note);
void fdir_log_queue_overflow(uint16_t queue_id, uint32_t overflow_delta, uint32_t depth_high_water);
```

## C++ API

`include/fdir.hpp` provides an idiomatic C++ wrapper over the C library. It requires C++20 or C++23.

On C++23 `fdir::Expected<T,E>` is an alias for `std::expected`. On C++20 a minimal compatible shim is used with the same interface (`operator bool`, `value()`, `error()`, `operator*`, `operator->`), without exceptions.

Include `fdir.hpp` instead of `fdir.h`. In exactly one translation unit define `FDIR_HPP_IMPL` before the include. This emits the five strong port hook definitions that forward into the `fdir::Port` struct. All other translation units include `fdir.hpp` without the define.

Compiler flags:

```
g++ -std=c++20 ...    # uses built-in Expected shim
g++ -std=c++23 ...    # uses std::expected
```

### Integrating

```cpp
#define FDIR_HPP_IMPL
#include "fdir.hpp"

// 1. configure port hooks and create the supervisor
auto result = fdir::Supervisor::create(
    fdir::Port{
        .get_now_ms             = []() -> uint32_t { return platform_ticks_ms(); },
        .submit_failure         = [](const fdir::FailureReport &r) { return queue_push(r); },
        .isolate_current_worker = [] { task_delete_self(); },
        .emit_event             = [](const fdir::Event &e) { log_event(e); },     // optional
        .request_reboot         = [](std::string_view reason) { platform_reboot(); }, // optional
    },
    fdir::Config{
        .health_check_period_ms            = 1000,
        .missed_heartbeat_tolerance        = 3,
        .safe_mode_critical_failure_threshold = 2,
    }
);
if (!result) { /* result.error() is fdir::Status */ }
fdir::Supervisor &sup = *result;

// 2. register an entity
auto entity = sup.register_entity({
    .name                  = "sensor",
    .max_restarts          = 3,
    .max_watchdog_restarts = 1,
    .on_exhausted          = fdir::Action::Degrade,
    .restart               = [](fdir::EntityId id) { sensor_restart(); return 0; },
});
if (!entity) { /* entity.error() is fdir::Status */ }

// 3. heartbeat from the entity's own context each cycle
entity->heartbeat();

// 4. report a fault from any context
entity->report_fault(fdir::Reason::IoError, errno, "detail");

// 5. supervisor loop: dedicated task or periodic callback
fdir::FailureReport report;
while (queue_pop(report))
    sup.handle_failure(report);
sup.check_watchdogs();
```

### Key types

| C type | C++ type |
|---|---|
| `fdir_status_t` | `fdir::Status` |
| `fdir_mode_t` | `fdir::Mode` |
| `fdir_reason_t` | `fdir::Reason` |
| `fdir_action_t` | `fdir::Action` |
| `fdir_entity_id_t` | `fdir::EntityId` |
| `fdir_subsystem_id_t` | `fdir::SubsystemId` |
| `fdir_failure_report_t` | `fdir::FailureReport` |
| `fdir_event_t` | `fdir::Event` |

### Initialisation

```cpp
static Expected<Supervisor, Status> Supervisor::create(Port port, Config cfg = {});
```

### Entities

```cpp
Expected<Entity, Status> Supervisor::register_entity(EntityDesc desc);

void Entity::heartbeat();
void Entity::report_fault(Reason r, uint16_t error_code = 0, std::string_view detail = {});
void Entity::set_health(Health h, uint16_t error_code = 0, std::string_view detail = {});
```

### Subsystems

```cpp
Expected<Subsystem, Status> Supervisor::register_subsystem(std::string_view name, bool critical_path);

void Subsystem::mark_available();
void Subsystem::mark_unavailable();
void Subsystem::mark_degraded();
```

### Supervisor loop

```cpp
void      Supervisor::handle_failure(const FailureReport &r);
void      Supervisor::check_watchdogs();
Mode      Supervisor::mode() const;
void      Supervisor::enter_degraded();
void      Supervisor::enter_safe();
void      Supervisor::try_reboot(std::string_view reason);
```

