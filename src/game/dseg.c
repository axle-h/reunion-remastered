#include "game/dseg.h"

#include <SDL3/SDL.h>

#include "game/dsfields.h"

DSeg ds;

void dseg_clear(void)
{
    SDL_memset(&ds, 0, sizeof ds);
}

// The field (or, for an element of a record holding pointers, the record
// field) that holds the original's byte orig, where its element starts in
// the port's DS, and the byte's offset in the original's element.
typedef struct {
    const DsField *field;
    uint8_t *at;
    uint16_t rest;
} DsByte;

static bool locate(uint16_t orig, DsByte *out)
{
    int lo = 0, hi = ds_field_count - 1;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (ds_fields[mid].ofs <= orig)
            lo = mid;
        else
            hi = mid - 1;
    }
    const DsField *f = &ds_fields[lo];
    uint32_t k = (uint32_t)(orig - f->ofs);
    if (orig < f->ofs || k >= (uint32_t)f->elem * f->count)
        return false;
    uint8_t *base = (uint8_t *)&ds + f->at;
    const DsRecord *r = f->rec >= 0 ? &ds_records[f->rec] : nullptr;
    if (r && r->size != r->port_size) {
        base += k / r->size * r->port_size;
        k %= r->size;
        for (int i = 0; i < r->nfields; i++) {
            const DsField *sf = &r->fields[i];
            if (k >= sf->ofs && k < sf->ofs + (uint32_t)sf->elem * sf->count) {
                *out = (DsByte){sf, base + sf->at, (uint16_t)(k - sf->ofs)};
                return true;
            }
        }
        return false;
    }
    *out = (DsByte){f, base, (uint16_t)k};
    return true;
}

uint8_t *dseg_orig(uint16_t orig)
{
    DsByte b;
    if (!locate(orig, &b) || (b.field->flags & DSF_HOST))
        return nullptr;
    return b.at + b.rest;
}

void dseg_init(void)
{
    ds = ds_initial;
}
