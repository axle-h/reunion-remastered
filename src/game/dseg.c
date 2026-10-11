#include "game/dseg.h"

#include <SDL3/SDL.h>

#include "game/dsfields.h"
#include "platform/fs.h"

// File segment of DS in REUNION.PRG (Ghidra's 4a81 minus its 1000h load base).
enum : uint32_t { DS_FILE_SEG = 0x3A81 };

DSeg ds;

uint8_t *dseg_orig(uint16_t orig)
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
    if (orig >= f->ofs + (uint32_t)f->elem * f->count)
        return nullptr;
    return (uint8_t *)&ds + f->at + (orig - f->ofs);
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
        if (seg == MEM_DS_SEG) {
            uint8_t *target = dseg_orig(off);
            off = target ? (uint16_t)(target - (uint8_t *)&ds) : off;
        }
        FarPtr p = far_ptr(seg, off);
        SDL_memcpy(dseg_orig(at), &p, 4);
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
                uint32_t n = SDL_min((uint32_t)f->elem * f->count, (uint32_t)(DSEG_INIT_SIZE - f->ofs));
                SDL_memcpy((uint8_t *)&ds + f->at, image + f->ofs, n);
            }
            ok = apply_relocations(exe, size, image);
        }
    }
    SDL_free(exe);
    return ok;
}
