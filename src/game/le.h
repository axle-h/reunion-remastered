// Words and longwords at any byte address. The original's records are packed
// (game/dseg.h) and its byte buffers hold words at odd offsets, so a typed
// pointer to one may be misaligned; these read and write them byte-wise
// instead. Little-endian, as the host is (dseg.h checks).
#pragma once

#include <stdint.h>
#include <string.h>

static inline int16_t rd16le(const void *p)
{
    int16_t v;
    memcpy(&v, p, sizeof v);
    return v;
}

static inline void wr16le(void *p, int16_t v)
{
    memcpy(p, &v, sizeof v);
}

static inline int32_t rd32le(const void *p)
{
    int32_t v;
    memcpy(&v, p, sizeof v);
    return v;
}

static inline void wr32le(void *p, int32_t v)
{
    memcpy(p, &v, sizeof v);
}
