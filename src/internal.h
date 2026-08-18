#ifndef FDIR_INTERNAL_H
#define FDIR_INTERNAL_H

#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

void fdir_internal_copy_detail(char *dst, size_t dst_len, const char *src);

#ifdef __cplusplus
}
#endif

#endif /* FDIR_INTERNAL_H */
