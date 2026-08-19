#include "fdir.h"

#include <stdint.h>

static uint32_t g_now_ms;

uint32_t test_port_set_now_ms(uint32_t now_ms)
{
    g_now_ms = now_ms;
    return g_now_ms;
}

static uint32_t test_get_now_ms(void)
{
    return g_now_ms;
}

static void test_emit_event(const fdir_event_t *event)
{
    (void)event;
}

static void test_request_reboot(const char *reason)
{
    (void)reason;
}

static const fdir_port_t g_test_port = {
    .get_now_ms      = test_get_now_ms,
    .emit_event      = test_emit_event,
    .request_reboot  = test_request_reboot,
};

const fdir_port_t *test_port_default(void)
{
    return &g_test_port;
}

fdir_status_t test_fdir_init(const fdir_config_t *cfg)
{
    return fdir_init(cfg, test_port_default());
}
