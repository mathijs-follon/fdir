#include "test_harness.h"

#include "fdir.h"

#include <stdint.h>

static uint32_t g_now_ms;
static int g_restart_calls;
static int g_note_events;
static char g_last_note[FDIR_DETAIL_SIZE];

static uint32_t port_now(void)
{
    return g_now_ms;
}

static void port_emit(const fdir_event_t *event)
{
    if (event->kind == FDIR_EVENT_NOTE) {
        g_note_events++;
        for (int i = 0; i < FDIR_DETAIL_SIZE - 1 && event->detail[i] != '\0'; i++) {
            g_last_note[i] = event->detail[i];
            g_last_note[i + 1] = '\0';
        }
    }
}

static void port_reboot(const char *reason)
{
    (void)reason;
}

static int restart_ok(fdir_entity_id_t id, void *user)
{
    (void)id;
    (void)user;
    g_restart_calls++;
    return 0;
}

static fdir_entity_id_t setup_entity_with_budget(uint8_t max_restarts)
{
    fdir_config_t cfg = fdir_config_default();
    fdir_entity_desc_t desc;
    fdir_entity_id_t id;
    fdir_port_t port = {
        .get_now_ms     = port_now,
        .emit_event     = port_emit,
        .request_reboot = port_reboot,
    };

    g_now_ms = 0U;
    g_restart_calls = 0;
    g_note_events = 0;
    g_last_note[0] = '\0';

    (void)fdir_init(&cfg, &port);

    desc.name = "worker";
    desc.max_restarts = max_restarts;
    desc.max_watchdog_restarts = 1U;
    desc.on_exhausted = FDIR_ACTION_DEGRADE;
    desc.linked_subsystem = FDIR_SUBSYSTEM_NONE;
    desc.restart = restart_ok;
    desc.decide = NULL;
    desc.user = NULL;
    (void)fdir_entity_register(&desc, &id);
    return id;
}

static fdir_entity_id_t setup_entity(void)
{
    return setup_entity_with_budget(0U);
}

TEST(test_supervision_disabled_allows_failed_entity_to_run)
{
    fdir_entity_id_t id = setup_entity();

    fdir_report_fault(id, FDIR_REASON_IO_ERROR, 1U, "fail");
    fdir_supervisor_tick();
    ASSERT_EQ_INT(fdir_entity_may_run(id), 0);

    (void)fdir_set_supervision_enabled(0U);
    ASSERT_EQ_INT(fdir_supervision_enabled(), 0);
    ASSERT_EQ_INT(fdir_worker_may_run(id), 1);
    ASSERT_EQ_INT(fdir_entity_may_run(id), 1);
}

TEST(test_supervision_disabled_ignores_new_faults)
{
    fdir_entity_id_t id = setup_entity();

    (void)fdir_set_supervision_enabled(0U);
    fdir_report_fault(id, FDIR_REASON_IO_ERROR, 1U, "ignored");
    fdir_supervisor_tick();

    ASSERT_EQ_INT(g_restart_calls, 0);
    ASSERT_EQ_INT(fdir_health_snapshot(id)->health, FDIR_HEALTH_OK);
}

TEST(test_supervision_disable_emits_note_and_clears_queue)
{
    fdir_entity_id_t id = setup_entity();

    fdir_report_fault(id, FDIR_REASON_IO_ERROR, 1U, "queued");
    ASSERT_EQ_INT(g_restart_calls, 0);

    (void)fdir_set_supervision_enabled(0U);
    ASSERT_STR_EQ(g_last_note, "supervision_disabled");
    fdir_supervisor_tick();
    ASSERT_EQ_INT(g_restart_calls, 0);

    (void)fdir_set_supervision_enabled(1U);
    ASSERT_STR_EQ(g_last_note, "supervision_enabled");
    fdir_supervisor_tick();
    ASSERT_EQ_INT(g_restart_calls, 0);
}

TEST(test_supervision_reenable_resumes_normal_handling)
{
    fdir_entity_id_t id = setup_entity_with_budget(1U);

    (void)fdir_set_supervision_enabled(0U);
    fdir_report_fault(id, FDIR_REASON_IO_ERROR, 1U, "ignored");
    (void)fdir_set_supervision_enabled(1U);

    fdir_report_fault(id, FDIR_REASON_TIMEOUT, 2U, "handled");
    fdir_supervisor_tick();
    ASSERT_EQ_INT(g_restart_calls, 1);
}

TEST(test_supervision_enable_noop_when_already_on)
{
    setup_entity();
    g_note_events = 0;
    ASSERT_EQ_INT(fdir_set_supervision_enabled(1U), 1U);
    ASSERT_EQ_INT(g_note_events, 0);
}

int main(void)
{
    RUN(test_supervision_disabled_allows_failed_entity_to_run);
    RUN(test_supervision_disabled_ignores_new_faults);
    RUN(test_supervision_disable_emits_note_and_clears_queue);
    RUN(test_supervision_reenable_resumes_normal_handling);
    RUN(test_supervision_enable_noop_when_already_on);
    return test_harness_summary();
}
