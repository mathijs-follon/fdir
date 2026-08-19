#include "port.h"

#include <stdlib.h>

static fdir_port_t g_port;
static int g_port_ready;

static int port_is_valid(const fdir_port_t *port)
{
    return port != NULL
        && port->get_now_ms != NULL
        && port->emit_event != NULL
        && port->request_reboot != NULL;
}

fdir_status_t fdir_port_bind(const fdir_port_t *port)
{
    if (!port_is_valid(port)) {
        g_port_ready = 0;
        return FDIR_ERR_PORT;
    }

    g_port = *port;
    g_port_ready = 1;
    return FDIR_OK;
}

void fdir_port_sync_enter(void)
{
    if (g_port_ready && g_port.lock != NULL) {
        g_port.lock();
    }
}

void fdir_port_sync_exit(void)
{
    if (g_port_ready && g_port.unlock != NULL) {
        g_port.unlock();
    }
}

uint32_t fdir_get_now_ms(void)
{
    if (!g_port_ready) {
        abort();
    }
    return g_port.get_now_ms();
}

void fdir_emit_event(const fdir_event_t *event)
{
    if (!g_port_ready) {
        abort();
    }
    g_port.emit_event(event);
}

void fdir_request_reboot(const char *reason)
{
    if (!g_port_ready) {
        abort();
    }
    g_port.request_reboot(reason);
}
