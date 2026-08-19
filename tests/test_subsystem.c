#include "test_harness.h"
#include "test_port.h"

#include "fdir.h"

static void setup(void)
{
    fdir_config_t cfg = fdir_config_default();
    (void)test_fdir_init(&cfg);
}

TEST(test_register_subsystem_assigns_id_and_name)
{
    fdir_subsystem_desc_t desc;
    fdir_subsystem_id_t id;

    setup();
    desc.name = "downlink";
    desc.is_critical_path = 1U;

    ASSERT_EQ_INT(fdir_subsystem_register(&desc, &id), FDIR_OK);
    ASSERT_EQ_INT(id, 0);
    ASSERT_STR_EQ(fdir_subsystem_name(id), "downlink");
}

TEST(test_register_subsystem_starts_available)
{
    fdir_subsystem_desc_t desc;
    fdir_subsystem_id_t id;

    setup();
    desc.name = "storage";
    desc.is_critical_path = 0U;

    ASSERT_EQ_INT(fdir_subsystem_register(&desc, &id), FDIR_OK);
    ASSERT_EQ_INT(fdir_subsystem_is_available(id), 1);
    ASSERT_EQ_INT(fdir_subsystem_is_degraded(id), 0);
}

TEST(test_mark_degraded_sets_only_degraded_flag)
{
    fdir_subsystem_desc_t desc;
    fdir_subsystem_id_t id;

    setup();
    desc.name = "payload";
    desc.is_critical_path = 0U;
    ASSERT_EQ_INT(fdir_subsystem_register(&desc, &id), FDIR_OK);

    fdir_subsystem_mark_degraded(id);

    ASSERT_EQ_INT(fdir_subsystem_is_available(id), 1);
    ASSERT_EQ_INT(fdir_subsystem_is_degraded(id), 1);
}

TEST(test_mark_unavailable_clears_available_and_sets_degraded)
{
    fdir_subsystem_desc_t desc;
    fdir_subsystem_id_t id;

    setup();
    desc.name = "nav";
    desc.is_critical_path = 1U;
    ASSERT_EQ_INT(fdir_subsystem_register(&desc, &id), FDIR_OK);

    fdir_subsystem_mark_unavailable(id);

    ASSERT_EQ_INT(fdir_subsystem_is_available(id), 0);
    ASSERT_EQ_INT(fdir_subsystem_is_degraded(id), 1);
}

TEST(test_mark_available_restores_nominal_state)
{
    fdir_subsystem_desc_t desc;
    fdir_subsystem_id_t id;

    setup();
    desc.name = "nav";
    desc.is_critical_path = 1U;
    ASSERT_EQ_INT(fdir_subsystem_register(&desc, &id), FDIR_OK);

    fdir_subsystem_mark_unavailable(id);
    fdir_subsystem_mark_available(id);

    ASSERT_EQ_INT(fdir_subsystem_is_available(id), 1);
    ASSERT_EQ_INT(fdir_subsystem_is_degraded(id), 0);
}

TEST(test_critical_path_flag_is_reported)
{
    fdir_subsystem_desc_t critical;
    fdir_subsystem_desc_t noncritical;
    fdir_subsystem_id_t id_a;
    fdir_subsystem_id_t id_b;

    setup();
    critical.name = "a";
    critical.is_critical_path = 1U;
    noncritical.name = "b";
    noncritical.is_critical_path = 0U;

    ASSERT_EQ_INT(fdir_subsystem_register(&critical, &id_a), FDIR_OK);
    ASSERT_EQ_INT(fdir_subsystem_register(&noncritical, &id_b), FDIR_OK);

    ASSERT_EQ_INT(fdir_subsystem_is_critical_path(id_a), 1);
    ASSERT_EQ_INT(fdir_subsystem_is_critical_path(id_b), 0);
}

TEST(test_critical_unavailable_count_tracks_state)
{
    fdir_subsystem_desc_t desc;
    fdir_subsystem_id_t id_a;
    fdir_subsystem_id_t id_b;
    fdir_subsystem_id_t id_c;

    setup();
    desc.is_critical_path = 1U;

    desc.name = "a";
    ASSERT_EQ_INT(fdir_subsystem_register(&desc, &id_a), FDIR_OK);
    desc.name = "b";
    ASSERT_EQ_INT(fdir_subsystem_register(&desc, &id_b), FDIR_OK);

    desc.name = "c";
    desc.is_critical_path = 0U;
    ASSERT_EQ_INT(fdir_subsystem_register(&desc, &id_c), FDIR_OK);

    ASSERT_EQ_INT(fdir_subsystems_critical_unavailable_count(), 0);

    fdir_subsystem_mark_unavailable(id_a);
    ASSERT_EQ_INT(fdir_subsystems_critical_unavailable_count(), 1);

    fdir_subsystem_mark_unavailable(id_b);
    ASSERT_EQ_INT(fdir_subsystems_critical_unavailable_count(), 2);

    fdir_subsystem_mark_unavailable(id_c);
    ASSERT_EQ_INT(fdir_subsystems_critical_unavailable_count(), 2);

    fdir_subsystem_mark_available(id_a);
    ASSERT_EQ_INT(fdir_subsystems_critical_unavailable_count(), 1);
}

TEST(test_invalid_subsystem_queries_are_safe)
{
    setup();

    ASSERT_STR_EQ(fdir_subsystem_name(FDIR_SUBSYSTEM_NONE), "unknown");
    ASSERT_EQ_INT(fdir_subsystem_is_available(FDIR_SUBSYSTEM_NONE), 0);
    ASSERT_EQ_INT(fdir_subsystem_is_degraded(FDIR_SUBSYSTEM_NONE), 0);
    ASSERT_EQ_INT(fdir_subsystem_is_critical_path(FDIR_SUBSYSTEM_NONE), 0);

    fdir_subsystem_mark_available(FDIR_SUBSYSTEM_NONE);
    fdir_subsystem_mark_unavailable(FDIR_SUBSYSTEM_NONE);
    fdir_subsystem_mark_degraded(FDIR_SUBSYSTEM_NONE);
}

int main(void)
{
    RUN(test_register_subsystem_assigns_id_and_name);
    RUN(test_register_subsystem_starts_available);
    RUN(test_mark_degraded_sets_only_degraded_flag);
    RUN(test_mark_unavailable_clears_available_and_sets_degraded);
    RUN(test_mark_available_restores_nominal_state);
    RUN(test_critical_path_flag_is_reported);
    RUN(test_critical_unavailable_count_tracks_state);
    RUN(test_invalid_subsystem_queries_are_safe);
    return test_harness_summary();
}
