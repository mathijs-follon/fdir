#include "internal.h"
#include "config.h"
#include "types.h"
#include <string.h>

static fdir_config_t g_config;

void fdir_internal_set_config(const fdir_config_t *config)
{
    if (config == NULL) {
        g_config = fdir_config_default();
        return;
    }
    g_config = *config;
}


void fdir_internal_copy_detail(char *dst, size_t dst_len, const char *src)
{
    if (dst == NULL || dst_len == 0U) {
        return;
    }

    dst[0] = '\0';
    if (src == NULL) {
        return;
    }

    (void)strncpy(dst, src, dst_len - 1U);
    dst[dst_len - 1U] = '\0';
}
