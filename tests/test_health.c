#include "test_harness.h"

#include "fdir.h"

#include <stdint.h>

/* Minimal port stubs. fdir_get_now_ms returns an incrementing value so tests
 * can simulate time passing without a real clock. */
static uint32_t g_now_ms = 0;

uint32_t fdir_get_now_ms(void) { return g_now_ms; }

static fdir_failure_report_t g_posted;
static int g_post_count;

int fdir_submit_failure(const fdir_failure_report_t *r)
{
    g_posted = *r;
    g_post_count++;
    return 0;
}

void fdir_isolate_current_worker(void) {}
void fdir_emit_event(const fdir_event_t *e) { (void)e; }
void fdir_request_reboot(const char *reason) { (void)reason; }

/* Helper: initialise fdir and register one entity, returning its id. */
static fdir_entity_id_t setup(void)
{
    g_now_ms    = 0;
    g_post_count = 0;

    fdir_config_t cfg = fdir_config_default();
    cfg.health_check_period_ms = 1000;
    cfg.missed_heartbeat_tolerance = 2;
    fdir_init(&cfg);

    static int dummy_restarts;
    fdir_entity_desc_t desc = {
        .name                  = "sensor",
        .max_restarts          = 3,
        .max_watchdog_restarts = 1,
        .on_exhausted          = FDIR_ACTION_DEGRADE,
        .linked_subsystem      = FDIR_SUBSYSTEM_NONE,
        .restart               = NULL,
        .user                  = &dummy_restarts,
    };
    fdir_entity_id_t id;
    fdir_entity_register(&desc, &id);
    return id;
}

/* heartbeat sets health to OK and updates last_heartbeat_ms */
TEST(test_heartbeat_sets_ok)
{
    fdir_entity_id_t id = setup();

    g_now_ms = 100;
    fdir_health_heartbeat_notify(id);

    const fdir_health_snapshot_t *s = fdir_health_snapshot(id);
    ASSERT(s != NULL);
    ASSERT_EQ_INT(s->health, FDIR_HEALTH_OK);
    ASSERT_EQ_INT(s->last_heartbeat_ms, 100);
}

/* heartbeat does not clear FAILED health; only fdir_health_reset does */
TEST(test_heartbeat_does_not_clear_failed)
{
    fdir_entity_id_t id = setup();

    fdir_health_set(id, FDIR_HEALTH_FAILED, 42, "injected");
    fdir_health_heartbeat_notify(id);

    const fdir_health_snapshot_t *s = fdir_health_snapshot(id);
    ASSERT(s != NULL);
    ASSERT_EQ_INT(s->health, FDIR_HEALTH_FAILED);
}

/* fdir_health_set stores health, error_code, and detail */
TEST(test_health_set_stores_fields)
{
    fdir_entity_id_t id = setup();

    fdir_health_set(id, FDIR_HEALTH_DEGRADED, 7, "voltage low");

    const fdir_health_snapshot_t *s = fdir_health_snapshot(id);
    ASSERT(s != NULL);
    ASSERT_EQ_INT(s->health, FDIR_HEALTH_DEGRADED);
    ASSERT_EQ_INT(s->error_code, 7);
    ASSERT_STR_EQ(s->detail, "voltage low");
}

/* fdir_health_reset clears health back to OK and refreshes heartbeat time */
TEST(test_health_reset)
{
    fdir_entity_id_t id = setup();

    fdir_health_set(id, FDIR_HEALTH_FAILED, 1, "fault");
    g_now_ms = 500;
    fdir_health_reset(id);

    const fdir_health_snapshot_t *s = fdir_health_snapshot(id);
    ASSERT(s != NULL);
    ASSERT_EQ_INT(s->health, FDIR_HEALTH_OK);
    ASSERT_EQ_INT(s->error_code, 0);
    ASSERT_EQ_INT(s->last_heartbeat_ms, 500);
}

/* fdir_health_is_stale returns 0 immediately after a heartbeat */
TEST(test_not_stale_after_heartbeat)
{
    fdir_entity_id_t id = setup();

    g_now_ms = 0;
    fdir_health_heartbeat_notify(id);

    g_now_ms = 999;
    fdir_bool_t stale = fdir_health_is_stale(id, g_now_ms, 1000);
    ASSERT_EQ_INT(stale, 0);
}

/* fdir_health_is_stale returns 1 when heartbeat age exceeds max_age_ms */
TEST(test_stale_after_timeout)
{
    fdir_entity_id_t id = setup();

    g_now_ms = 0;
    fdir_health_heartbeat_notify(id);

    g_now_ms = 1001;
    fdir_bool_t stale = fdir_health_is_stale(id, g_now_ms, 1000);
    ASSERT_EQ_INT(stale, 1);
}

/* fdir_health_is_stale returns 0 for FAILED entities (already handled) */
TEST(test_failed_entity_not_stale)
{
    fdir_entity_id_t id = setup();

    fdir_health_set(id, FDIR_HEALTH_FAILED, 0, "");
    g_now_ms = 9999;
    fdir_bool_t stale = fdir_health_is_stale(id, g_now_ms, 0);
    ASSERT_EQ_INT(stale, 0);
}

/* fdir_health_snapshot returns NULL for an out-of-range id */
TEST(test_snapshot_out_of_range)
{
    setup();
    const fdir_health_snapshot_t *s = fdir_health_snapshot(FDIR_ENTITY_NONE);
    ASSERT(s == NULL);
}

int main(void)
{
    RUN(test_heartbeat_sets_ok);
    RUN(test_heartbeat_does_not_clear_failed);
    RUN(test_health_set_stores_fields);
    RUN(test_health_reset);
    RUN(test_not_stale_after_heartbeat);
    RUN(test_stale_after_timeout);
    RUN(test_failed_entity_not_stale);
    RUN(test_snapshot_out_of_range);
    return test_harness_summary();
}
