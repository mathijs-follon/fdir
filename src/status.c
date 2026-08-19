#include "types.h"

const char *fdir_status_string(fdir_status_t status)
{
    switch (status) {
    case FDIR_OK:           return "ok";
    case FDIR_ERR_PARAM:    return "invalid parameter";
    case FDIR_ERR_FULL:     return "capacity full";
    case FDIR_ERR_NOT_FOUND: return "not found";
    case FDIR_ERR_STATE:    return "invalid state";
    case FDIR_ERR_PORT:     return "port NULL or missing required callback";
    case FDIR_ERR_BUSY:     return "failure queue full (critical fault)";
    default:                return "unknown error";
    }
}
