#include "test_harness.h"
#include "test_port.h"

#include "fdir.h"

#include <stdint.h>
#include <string.h>

static fdir_entity_id_t setup_entity(void)
{
    test_port_set_now_ms(0);

    fdir_config_t cfg = fdir_config_default();
    cfg.health_check_period_ms = 1000;
    cfg.missed_heartbeat_tolerance = 2;

    fdir_init(&cfg, test_port_default());

    fdir_entity_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.name = "worker";
    desc.max_restarts = 3;
    desc.max_watchdog_restarts = 1;
    desc.on_exhausted = FDIR_ACTION_DEGRADE;

    fdir_entity_id_t id;
    fdir_entity_register(&desc, &id);
    return id;
}

TEST(test_worker_may_run_when_healthy)
{
    fdir_entity_id_t id = setup_entity();

    test_port_set_now_ms(100);
    fdir_health_heartbeat_notify(id);

    ASSERT_EQ_INT(fdir_worker_may_run(id), 1);
}

TEST(test_worker_may_run_false_when_failed)
{
    fdir_entity_id_t id = setup_entity();

    fdir_health_set(id, FDIR_HEALTH_FAILED, 1, "fault");

    ASSERT_EQ_INT(fdir_worker_may_run(id), 0);
}

TEST(test_worker_may_run_heartbeats_in_safe_mode)
{
    fdir_entity_id_t id = setup_entity();

    test_port_set_now_ms(200);
    fdir_health_heartbeat_notify(id);
    fdir_enter_safe_mode();

    ASSERT_EQ_INT(fdir_worker_may_run(id), 0);

    const fdir_health_snapshot_t *s = fdir_health_snapshot(id);
    ASSERT(s != NULL);
    ASSERT_EQ_INT(s->last_heartbeat_ms, 200);
}

TEST(test_worker_may_run_false_in_reboot_pending)
{
    fdir_entity_id_t id = setup_entity();

    fdir_try_reboot("test");

    ASSERT_EQ_INT(fdir_worker_may_run(id), 0);
}

TEST(test_status_string_all_codes)
{
    ASSERT_STR_EQ(fdir_status_string(FDIR_OK), "ok");
    ASSERT_STR_EQ(fdir_status_string(FDIR_ERR_PARAM), "invalid parameter");
    ASSERT_STR_EQ(fdir_status_string(FDIR_ERR_FULL), "capacity full");
    ASSERT_STR_EQ(fdir_status_string(FDIR_ERR_NOT_FOUND), "not found");
    ASSERT_STR_EQ(fdir_status_string(FDIR_ERR_STATE), "invalid state");
    ASSERT_STR_EQ(fdir_status_string(FDIR_ERR_PORT), "port NULL or missing required callback");
    ASSERT_STR_EQ(fdir_status_string(FDIR_ERR_BUSY), "failure queue full (critical fault)");
    ASSERT_STR_EQ(fdir_status_string((fdir_status_t)-99), "unknown error");
}

TEST(test_reassess_system_mode_enters_safe)
{
    fdir_config_t cfg = fdir_config_default();
    cfg.safe_mode_critical_failure_threshold = 2;
    fdir_init(&cfg, test_port_default());

    fdir_subsystem_desc_t sub_a = { .name = "a", .is_critical_path = 1 };
    fdir_subsystem_desc_t sub_b = { .name = "b", .is_critical_path = 1 };
    fdir_subsystem_id_t id_a;
    fdir_subsystem_id_t id_b;
    fdir_subsystem_register(&sub_a, &id_a);
    fdir_subsystem_register(&sub_b, &id_b);

    fdir_subsystem_mark_unavailable(id_a);
    fdir_subsystem_mark_unavailable(id_b);
    fdir_reassess_system_mode();

    ASSERT_EQ_INT(fdir_system_mode(), FDIR_MODE_SAFE);
}

int main(void)
{
    RUN(test_worker_may_run_when_healthy);
    RUN(test_worker_may_run_false_when_failed);
    RUN(test_worker_may_run_heartbeats_in_safe_mode);
    RUN(test_worker_may_run_false_in_reboot_pending);
    RUN(test_status_string_all_codes);
    RUN(test_reassess_system_mode_enters_safe);
    return test_harness_summary();
}
