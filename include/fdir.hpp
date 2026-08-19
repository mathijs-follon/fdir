#ifndef FDIR_HPP
#define FDIR_HPP

#include "fdir.h"

#include <cstdint>
#include <cstring>
#include <functional>
#include <string_view>

#if __cplusplus >= 202302L
#include <expected>
#endif

namespace fdir {

/*
 * Expected<T, E> -- on C++23 this is std::expected. On C++20 it is a minimal
 * shim with the same interface: operator bool, value(), error(), operator*, operator->.
 */
#if __cplusplus >= 202302L

template <typename T, typename E>
using Expected = std::expected<T, E>;

template <typename T, typename E>
Expected<T, E> make_unexpected(E e)
{
    return std::unexpected(std::move(e));
}

#else

template <typename T, typename E>
class Expected {
public:
    Expected(T val) : m_has_val(true), m_val(std::move(val)) {}

    struct ErrorTag {};
    Expected(ErrorTag, E err) : m_has_val(false), m_err(std::move(err)) {}

    explicit operator bool() const { return m_has_val; }
    bool has_value()          const { return m_has_val; }

    T       &value()       { return m_val; }
    const T &value() const { return m_val; }
    T       &operator*()       { return m_val; }
    const T &operator*() const { return m_val; }
    T       *operator->()       { return &m_val; }
    const T *operator->() const { return &m_val; }

    E       &error()       { return m_err; }
    const E &error() const { return m_err; }

private:
    bool m_has_val;
    union { T m_val; E m_err; };
};

template <typename T, typename E>
Expected<T, E> make_unexpected(E e)
{
    return Expected<T, E>(typename Expected<T, E>::ErrorTag{}, std::move(e));
}

#endif

enum class Status {
    Ok       = FDIR_OK,
    Param    = FDIR_ERR_PARAM,
    Full     = FDIR_ERR_FULL,
    NotFound = FDIR_ERR_NOT_FOUND,
    State    = FDIR_ERR_STATE,
    Port     = FDIR_ERR_PORT,
    Busy     = FDIR_ERR_BUSY,
};

enum class Mode {
    Nominal       = FDIR_MODE_NOMINAL,
    Degraded      = FDIR_MODE_DEGRADED,
    Safe          = FDIR_MODE_SAFE,
    RebootPending = FDIR_MODE_REBOOT_PENDING,
};

enum class Health {
    Ok       = FDIR_HEALTH_OK,
    Degraded = FDIR_HEALTH_DEGRADED,
    Failed   = FDIR_HEALTH_FAILED,
};

enum class Reason {
    InitFailed    = FDIR_REASON_INIT_FAILED,
    IoError       = FDIR_REASON_IO_ERROR,
    Timeout       = FDIR_REASON_TIMEOUT,
    ProtocolError = FDIR_REASON_PROTOCOL_ERROR,
    Watchdog      = FDIR_REASON_WATCHDOG,
    QueueOverflow = FDIR_REASON_QUEUE_OVERFLOW,
    User          = FDIR_REASON_USER,
};

enum class Action {
    None        = FDIR_ACTION_NONE,
    Restart     = FDIR_ACTION_RESTART,
    Degrade     = FDIR_ACTION_DEGRADE,
    Unavailable = FDIR_ACTION_UNAVAILABLE,
    Safe        = FDIR_ACTION_SAFE,
    Reboot      = FDIR_ACTION_REBOOT,
};

enum class EventKind {
    Failure       = FDIR_EVENT_FAILURE,
    ModeChange    = FDIR_EVENT_MODE_CHANGE,
    Restart       = FDIR_EVENT_RESTART,
    Watchdog      = FDIR_EVENT_WATCHDOG,
    Note          = FDIR_EVENT_NOTE,
    QueueOverflow = FDIR_EVENT_QUEUE_OVERFLOW,
};

enum class EntityId    : uint8_t { None = 0xFF };
enum class SubsystemId : uint8_t { None = 0xFF };

struct FailureReport {
    EntityId entity       = EntityId::None;
    Reason   reason       = Reason::User;
    uint16_t error_code   = 0;
    uint32_t timestamp_ms = 0;
    char     detail[FDIR_DETAIL_SIZE] = {};

    void set_detail(std::string_view s)
    {
        std::size_t n = s.size() < FDIR_DETAIL_SIZE - 1 ? s.size() : FDIR_DETAIL_SIZE - 1;
        std::memcpy(detail, s.data(), n);
        detail[n] = '\0';
    }

    fdir_failure_report_t to_c() const
    {
        fdir_failure_report_t r{};
        r.entity       = static_cast<fdir_entity_id_t>(entity);
        r.reason       = static_cast<fdir_reason_t>(reason);
        r.error_code   = error_code;
        r.timestamp_ms = timestamp_ms;
        std::memcpy(r.detail, detail, FDIR_DETAIL_SIZE);
        return r;
    }

    static FailureReport from_c(const fdir_failure_report_t &r)
    {
        FailureReport fr{};
        fr.entity       = static_cast<EntityId>(r.entity);
        fr.reason       = static_cast<Reason>(r.reason);
        fr.error_code   = r.error_code;
        fr.timestamp_ms = r.timestamp_ms;
        std::memcpy(fr.detail, r.detail, FDIR_DETAIL_SIZE);
        return fr;
    }
};

struct Event {
    EventKind kind;
    Mode      mode;
    EntityId  entity;
    Reason    reason;
    uint16_t  error_code;
    uint32_t  timestamp_ms;
    char      detail_buf[FDIR_DETAIL_SIZE];

    std::string_view detail() const { return detail_buf; }

    static Event from_c(const fdir_event_t &e)
    {
        Event ev{};
        ev.kind         = static_cast<EventKind>(e.kind);
        ev.mode         = static_cast<Mode>(e.mode);
        ev.entity       = static_cast<EntityId>(e.entity);
        ev.reason       = static_cast<Reason>(e.reason);
        ev.error_code   = e.error_code;
        ev.timestamp_ms = e.timestamp_ms;
        std::memcpy(ev.detail_buf, e.detail, FDIR_DETAIL_SIZE);
        return ev;
    }
};

struct Port {
    std::function<uint32_t()>                 get_now_ms;
    std::function<int(const FailureReport &)> post_failure;
    std::function<void()>                     isolate_current_worker;
    std::function<void(const Event &)>        emit_event;
    std::function<void(std::string_view)>     request_reboot;
};

struct Config {
    uint32_t health_check_period_ms               = 500;
    uint8_t  missed_heartbeat_tolerance           = 3;
    uint8_t  safe_mode_critical_failure_threshold = 2;

    fdir_config_t to_c() const
    {
        fdir_config_t c{};
        c.health_check_period_ms               = health_check_period_ms;
        c.missed_heartbeat_tolerance           = missed_heartbeat_tolerance;
        c.safe_mode_critical_failure_threshold = safe_mode_critical_failure_threshold;
        return c;
    }
};

class Supervisor;

class Entity {
public:
    EntityId id() const { return m_id; }

    void heartbeat()
    {
        fdir_health_heartbeat_notify(static_cast<fdir_entity_id_t>(m_id));
    }

    void set_health(Health h, uint16_t error_code = 0, std::string_view detail = {})
    {
        char buf[FDIR_DETAIL_SIZE] = {};
        std::size_t n = detail.size() < FDIR_DETAIL_SIZE - 1 ? detail.size() : FDIR_DETAIL_SIZE - 1;
        std::memcpy(buf, detail.data(), n);
        fdir_health_set(static_cast<fdir_entity_id_t>(m_id),
                        static_cast<fdir_health_t>(h), error_code, buf);
    }

    void report_fault(Reason r, uint16_t error_code = 0, std::string_view detail = {})
    {
        char buf[FDIR_DETAIL_SIZE] = {};
        std::size_t n = detail.size() < FDIR_DETAIL_SIZE - 1 ? detail.size() : FDIR_DETAIL_SIZE - 1;
        std::memcpy(buf, detail.data(), n);
        fdir_report_fault(static_cast<fdir_entity_id_t>(m_id),
                          static_cast<fdir_reason_t>(r), error_code, buf);
    }

    /* Construct a lightweight handle from a known entity id. */
    static Entity from_id(EntityId id) { return Entity(id); }

private:
    friend class Supervisor;
    explicit Entity(EntityId id) : m_id(id) {}
    EntityId m_id;
};

class Subsystem {
public:
    SubsystemId id() const { return m_id; }

    void mark_available()   { fdir_subsystem_mark_available(raw()); }
    void mark_unavailable() { fdir_subsystem_mark_unavailable(raw()); }
    void mark_degraded()    { fdir_subsystem_mark_degraded(raw()); }

    bool is_available()     const { return fdir_subsystem_is_available(raw()) != 0; }
    bool is_degraded()      const { return fdir_subsystem_is_degraded(raw()) != 0; }
    bool is_critical_path() const { return fdir_subsystem_is_critical_path(raw()) != 0; }

    static Subsystem from_id(SubsystemId id) { return Subsystem(id); }

private:
    friend class Supervisor;
    explicit Subsystem(SubsystemId id) : m_id(id) {}
    fdir_subsystem_id_t raw() const { return static_cast<fdir_subsystem_id_t>(m_id); }
    SubsystemId m_id;
};

struct EntityDesc {
    std::string_view name;
    uint8_t          max_restarts          = 3;
    uint8_t          max_watchdog_restarts = 1;
    Action           on_exhausted          = Action::Degrade;
    SubsystemId      linked_subsystem      = SubsystemId::None;

    std::function<int(EntityId)>                                              restart;
    /* Return Action::None to fall through to the default restart-budget path. */
    std::function<Action(EntityId, const FailureReport &, uint8_t, uint8_t)> decide = {};
};

namespace detail {

/*
 * The Port struct is stored here at Supervisor::create() time. The five C port
 * hook definitions (compiled in when FDIR_HPP_IMPL is defined) call into it.
 */
inline Port &active_port()
{
    static Port p;
    return p;
}

/*
 * Per-entity callback storage. The C API takes raw function pointers with a
 * void* user argument; we encode the entity's slot index in that pointer and
 * use it to look up the std::function callbacks stored here.
 */
struct EntityCallbacks {
    std::function<int(EntityId)>                                              restart;
    std::function<Action(EntityId, const FailureReport &, uint8_t, uint8_t)> decide;
};

inline EntityCallbacks &entity_callbacks(uint8_t slot)
{
    static EntityCallbacks cbs[FDIR_ENTITY_CAP];
    return cbs[slot];
}

} /* namespace detail */

class Supervisor {
public:
    Supervisor(const Supervisor &)            = delete;
    Supervisor &operator=(const Supervisor &) = delete;

    Supervisor(Supervisor &&o) noexcept : m_valid(o.m_valid) { o.m_valid = false; }
    Supervisor &operator=(Supervisor &&o) noexcept
    {
        if (this != &o) { m_valid = o.m_valid; o.m_valid = false; }
        return *this;
    }

    static Expected<Supervisor, Status> create(Port port, Config cfg = {})
    {
        detail::active_port() = std::move(port);
        fdir_config_t c = cfg.to_c();
        fdir_status_t s = fdir_init(&c);
        if (s != FDIR_OK) {
            return make_unexpected<Supervisor>(static_cast<Status>(s));
        }
        return Expected<Supervisor, Status>(Supervisor(true));
    }

    Expected<Entity, Status> register_entity(EntityDesc desc)
    {
        /*
         * fdir_entity_count() gives the next slot index before registration.
         * Store the callbacks there so the C trampolines below can reach them
         * via the void* user pointer.
         */
        uint8_t slot = static_cast<uint8_t>(fdir_entity_count());
        detail::entity_callbacks(slot).restart = desc.restart;
        detail::entity_callbacks(slot).decide  = desc.decide;

        fdir_entity_desc_t d{};
        d.name                  = desc.name.data();
        d.max_restarts          = desc.max_restarts;
        d.max_watchdog_restarts = desc.max_watchdog_restarts;
        d.on_exhausted          = static_cast<fdir_action_t>(desc.on_exhausted);
        d.linked_subsystem      = static_cast<fdir_subsystem_id_t>(desc.linked_subsystem);
        d.user                  = reinterpret_cast<void *>(static_cast<uintptr_t>(slot));

        if (desc.restart) {
            d.restart = [](fdir_entity_id_t id, void *user) -> int {
                uint8_t s = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(user));
                auto &cb = detail::entity_callbacks(s);
                return cb.restart ? cb.restart(static_cast<EntityId>(id)) : 0;
            };
        }

        if (desc.decide) {
            d.decide = [](fdir_entity_id_t id, const fdir_failure_report_t *r,
                          uint8_t restarts, uint8_t wd_restarts,
                          void *user) -> fdir_action_t {
                uint8_t s = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(user));
                auto &cb = detail::entity_callbacks(s);
                if (!cb.decide) { return FDIR_ACTION_NONE; }
                return static_cast<fdir_action_t>(
                    cb.decide(static_cast<EntityId>(id),
                              FailureReport::from_c(*r),
                              restarts, wd_restarts));
            };
        }

        fdir_entity_id_t out{};
        fdir_status_t s = fdir_entity_register(&d, &out);
        if (s != FDIR_OK) {
            return make_unexpected<Entity>(static_cast<Status>(s));
        }
        return Expected<Entity, Status>(Entity(static_cast<EntityId>(out)));
    }

    Expected<Subsystem, Status> register_subsystem(std::string_view name, bool critical_path)
    {
        fdir_subsystem_desc_t d{};
        d.name             = name.data();
        d.is_critical_path = critical_path ? 1 : 0;
        fdir_subsystem_id_t out{};
        fdir_status_t s = fdir_subsystem_register(&d, &out);
        if (s != FDIR_OK) {
            return make_unexpected<Subsystem>(static_cast<Status>(s));
        }
        return Expected<Subsystem, Status>(Subsystem(static_cast<SubsystemId>(out)));
    }

    void handle_failure(const FailureReport &r)
    {
        fdir_failure_report_t c = r.to_c();
        fdir_handle_failure(&c);
    }

    void check_watchdogs() { fdir_check_watchdogs(); }

    Mode mode()           const { return static_cast<Mode>(fdir_system_mode()); }
    void enter_degraded()       { fdir_enter_degraded_mode(); }
    void enter_safe()           { fdir_enter_safe_mode(); }

    void try_reboot(std::string_view reason)
    {
        char buf[FDIR_DETAIL_SIZE] = {};
        std::size_t n = reason.size() < FDIR_DETAIL_SIZE - 1 ? reason.size() : FDIR_DETAIL_SIZE - 1;
        std::memcpy(buf, reason.data(), n);
        fdir_try_reboot(buf);
    }

    void log_note(std::string_view note)
    {
        char buf[FDIR_DETAIL_SIZE] = {};
        std::size_t n = note.size() < FDIR_DETAIL_SIZE - 1 ? note.size() : FDIR_DETAIL_SIZE - 1;
        std::memcpy(buf, note.data(), n);
        fdir_log_note(buf);
    }

private:
    explicit Supervisor(bool valid) : m_valid(valid) {}
    bool m_valid;
};

} /* namespace fdir */

/*
 * Define FDIR_HPP_IMPL in exactly one translation unit that links against
 * libfdir. This emits the five strong C port hook definitions that forward
 * into the Port struct stored in fdir::detail::active_port().
 */
#ifdef FDIR_HPP_IMPL

extern "C" {

uint32_t fdir_get_now_ms(void)
{
    return fdir::detail::active_port().get_now_ms();
}

int fdir_post_failure(const fdir_failure_report_t *report)
{
    auto &fn = fdir::detail::active_port().post_failure;
    if (!fn) { return -1; }
    return fn(fdir::FailureReport::from_c(*report));
}

void fdir_isolate_current_worker(void)
{
    auto &fn = fdir::detail::active_port().isolate_current_worker;
    if (fn) { fn(); }
}

void fdir_emit_event(const fdir_event_t *event)
{
    auto &fn = fdir::detail::active_port().emit_event;
    if (fn) { fn(fdir::Event::from_c(*event)); }
}

void fdir_request_reboot(const char *reason)
{
    auto &fn = fdir::detail::active_port().request_reboot;
    if (fn) { fn(reason != nullptr ? reason : ""); }
}

} /* extern "C" */

#endif /* FDIR_HPP_IMPL */

#endif /* FDIR_HPP */
