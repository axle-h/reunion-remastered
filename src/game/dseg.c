#include "game/dseg.h"

#include <SDL3/SDL.h>

#include "game/dsfields.h"
#include "platform/fs.h"

// File segment of DS in REUNION.PRG (Ghidra's 4a81 minus its 1000h load base).
enum : uint32_t { DS_FILE_SEG = 0x3A81 };

DSeg ds;

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

// Copies one field (or record field) of the load image; pointers are left to
// the relocations.
static void copy_field(const DsField *f, uint8_t *dst, const uint8_t *src)
{
    if (f->flags & DSF_HOST)
        return;
    const DsRecord *r = f->rec >= 0 ? &ds_records[f->rec] : nullptr;
    if (!r || r->size == r->port_size) {
        SDL_memcpy(dst, src, (size_t)f->elem * f->count);
        return;
    }
    for (uint32_t e = 0; e < f->count; e++)
        for (int i = 0; i < r->nfields; i++)
            copy_field(&r->fields[i], dst + e * r->port_size + r->fields[i].at,
                       src + e * r->size + r->fields[i].ofs);
}

// DOS adds the load segment to every word listed in the MZ relocation table.
// In DS those are the segments of far pointers (the galaxy and menu tables
// into DS, Scale's row procedures into code); a pointer into DS also gets
// its offset moved to where the port keeps that byte.
static bool apply_relocations(const uint8_t *exe, size_t size, const uint8_t *image)
{
    size_t count = exe[6] | exe[7] << 8;
    size_t table = exe[0x18] | exe[0x19] << 8;
    if (table + count * 4 > size)
        return false;
    for (size_t i = 0; i < count; i++) {
        const uint8_t *e = exe + table + i * 4;
        uint32_t linear = (uint32_t)(e[2] | e[3] << 8) * 16 + (e[0] | e[1] << 8);
        uint32_t base = DS_FILE_SEG * 16;
        if (linear < base + 2 || linear + 2 > base + DSEG_INIT_SIZE)
            continue;
        uint16_t at = (uint16_t)(linear - base - 2);  // the far pointer's offset word
        uint16_t off = (uint16_t)(image[at] | image[at + 1] << 8);
        uint16_t seg = (uint16_t)((image[at + 2] | image[at + 3] << 8) + MEM_LOAD_SEG);
        DsByte where;
        if (!locate(at, &where))
            continue;
        uint8_t *target = seg == MEM_DS_SEG ? dseg_orig(off) : nullptr;
        if (where.field->flags & DSF_HOST) {
            // A host pointer (into DS; nothing else is relocated there).
            SDL_memcpy(where.at + where.rest / 4 * sizeof(void *), &target, sizeof(void *));
        } else {
            if (target)
                off = (uint16_t)(target - (uint8_t *)&ds);
            FarPtr p = far_ptr(seg, off);
            SDL_memcpy(where.at + where.rest, &p, 4);
        }
    }
    return true;
}

bool dseg_init(void)
{
    // The rest of the segment is zero, as after System's ClearBss.
    SDL_memset(&ds, 0, sizeof ds);
    size_t size;
    uint8_t *exe = fs_load("GRWAR\\REUNION.PRG", &size);
    if (!exe)
        return false;

    // Typed constants are part of the load image, which starts after the
    // MZ header (its size in paragraphs is at offset 8). They are copied
    // field by field into the port's layout.
    bool ok = size >= 0x1C && exe[0] == 'M' && exe[1] == 'Z';
    if (ok) {
        size_t start = (size_t)(exe[8] | exe[9] << 8) * 16 + DS_FILE_SEG * 16;
        ok = start + DSEG_INIT_SIZE <= size;
        if (ok) {
            const uint8_t *image = exe + start;
            for (int i = 0; i < ds_field_count && ds_fields[i].ofs < DSEG_INIT_SIZE; i++) {
                const DsField *f = &ds_fields[i];
                // The typed constants end where the next field begins.
                if (f->ofs + (uint32_t)f->elem * f->count > DSEG_INIT_SIZE)
                    SDL_memcpy((uint8_t *)&ds + f->at, image + f->ofs, DSEG_INIT_SIZE - f->ofs);
                else
                    copy_field(f, (uint8_t *)&ds + f->at, image + f->ofs);
            }
            ok = apply_relocations(exe, size, image);
        }
    }
    SDL_free(exe);
    return ok;
}
