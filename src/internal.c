#include "internal.h"
#include <string.h>

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
