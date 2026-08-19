#include "test_harness.h"
#include "test_port.h"

#include "fdir.h"

#include <stdint.h>

static fdir_entity_id_t setup(void)
{
    test_port_set_now_ms(0);

    fdir_config_t cfg = fdir_config_default();
    cfg.health_check_period_ms = 1000;
    cfg.missed_heartbeat_tolerance = 2;

    fdir_init(&cfg, test_port_default());

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

TEST(test_heartbeat_sets_ok)
{
    fdir_entity_id_t id = setup();

    test_port_set_now_ms(100);
    fdir_health_heartbeat_notify(id);

    const fdir_health_snapshot_t *s = fdir_health_snapshot(id);
    ASSERT(s != NULL);
    ASSERT_EQ_INT(s->health, FDIR_HEALTH_OK);
    ASSERT_EQ_INT(s->last_heartbeat_ms, 100);
}

TEST(test_heartbeat_does_not_clear_failed)
{
    fdir_entity_id_t id = setup();

    fdir_health_set(id, FDIR_HEALTH_FAILED, 42, "injected");
    fdir_health_heartbeat_notify(id);

    const fdir_health_snapshot_t *s = fdir_health_snapshot(id);
    ASSERT(s != NULL);
    ASSERT_EQ_INT(s->health, FDIR_HEALTH_FAILED);
}

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

TEST(test_health_reset)
{
    fdir_entity_id_t id = setup();

    fdir_health_set(id, FDIR_HEALTH_FAILED, 1, "fault");
    test_port_set_now_ms(500);
    fdir_health_reset(id);

    const fdir_health_snapshot_t *s = fdir_health_snapshot(id);
    ASSERT(s != NULL);
    ASSERT_EQ_INT(s->health, FDIR_HEALTH_OK);
    ASSERT_EQ_INT(s->error_code, 0);
    ASSERT_EQ_INT(s->last_heartbeat_ms, 500);
}

TEST(test_not_stale_after_heartbeat)
{
    fdir_entity_id_t id = setup();

    test_port_set_now_ms(0);
    fdir_health_heartbeat_notify(id);

    test_port_set_now_ms(999);
    fdir_bool_t stale = fdir_health_is_stale(id, 999, 1000);
    ASSERT_EQ_INT(stale, 0);
}

TEST(test_stale_after_timeout)
{
    fdir_entity_id_t id = setup();

    test_port_set_now_ms(0);
    fdir_health_heartbeat_notify(id);

    test_port_set_now_ms(1001);
    fdir_bool_t stale = fdir_health_is_stale(id, 1001, 1000);
    ASSERT_EQ_INT(stale, 1);
}

TEST(test_failed_entity_not_stale)
{
    fdir_entity_id_t id = setup();

    fdir_health_set(id, FDIR_HEALTH_FAILED, 0, "");
    test_port_set_now_ms(9999);
    fdir_bool_t stale = fdir_health_is_stale(id, 9999, 0);
    ASSERT_EQ_INT(stale, 0);
}

TEST(test_snapshot_out_of_range)
{
    setup();
    const fdir_health_snapshot_t *s = fdir_health_snapshot(FDIR_ENTITY_NONE);
    ASSERT(s == NULL);
}

TEST(test_health_ignores_out_of_range_entity)
{
    setup();
    fdir_health_heartbeat_notify(FDIR_ENTITY_NONE);
    fdir_health_set(FDIR_ENTITY_NONE, FDIR_HEALTH_FAILED, 1U, "x");
    fdir_health_reset(FDIR_ENTITY_NONE);
    ASSERT(fdir_health_snapshot(FDIR_ENTITY_NONE) == NULL);
}

TEST(test_health_snapshot_copy)
{
    fdir_entity_id_t id = setup();
    fdir_health_snapshot_t copied;

    fdir_health_set(id, FDIR_HEALTH_DEGRADED, 9U, "copy me");
    ASSERT_EQ_INT(fdir_health_snapshot_copy(id, &copied), 1);
    ASSERT_EQ_INT(copied.health, FDIR_HEALTH_DEGRADED);
    ASSERT_EQ_INT(copied.error_code, 9U);
    ASSERT_STR_EQ(copied.detail, "copy me");
    ASSERT_EQ_INT(fdir_health_snapshot_copy(FDIR_ENTITY_NONE, &copied), 0);
    ASSERT_EQ_INT(fdir_health_snapshot_copy(id, NULL), 0);
}

TEST(test_anomaly_id_entity_subsystem_namespaces)
{
    const uint16_t entity_key = FDIR_ANOMALY_ID(127U, FDIR_REASON_INIT_FAILED);
    const uint16_t sub_key = FDIR_SUBSYSTEM_ANOMALY_ID(72U, FDIR_REASON_INIT_FAILED);

    ASSERT((entity_key & 0x8000U) == 0U);
    ASSERT((sub_key & 0x8000U) != 0U);
    ASSERT(entity_key != sub_key);
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
    RUN(test_health_ignores_out_of_range_entity);
    RUN(test_health_snapshot_copy);
    RUN(test_anomaly_id_entity_subsystem_namespaces);
    return test_harness_summary();
}
