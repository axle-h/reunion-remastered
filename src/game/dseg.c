#include "game/dseg.h"

#include <SDL3/SDL.h>

#include "platform/fs.h"

// File segment of DS in REUNION.PRG (Ghidra's 4a81 minus its 1000h load base).
enum : uint32_t { DS_FILE_SEG = 0x3A81 };

// DOS adds the load segment to every word listed in the MZ relocation table.
// Some typed constants are far pointers (e.g. the galaxy tables), so the
// copies of those in DS need the same fix-up.
static bool apply_relocations(const uint8_t *exe, size_t size)
{
    size_t count = exe[6] | exe[7] << 8;
    size_t table = exe[0x18] | exe[0x19] << 8;
    if (table + count * 4 > size)
        return false;
    for (size_t i = 0; i < count; i++) {
        const uint8_t *e = exe + table + i * 4;
        uint32_t linear = (uint32_t)(e[2] | e[3] << 8) * 16 + (e[0] | e[1] << 8);
        uint32_t base = DS_FILE_SEG * 16;
        if (linear < base || linear + 2 > base + DSEG_INIT_SIZE)
            continue;
        uint8_t *w = (uint8_t *)&ds + (linear - base);
        uint16_t v = (uint16_t)(w[0] | w[1] << 8) + MEM_LOAD_SEG;
        w[0] = (uint8_t)v;
        w[1] = (uint8_t)(v >> 8);
    }
    return true;
}

bool dseg_init(void)
{
    // The rest of the segment is zero, as after System's ClearBss; mem_init()
    // must have run.
    size_t size;
    uint8_t *exe = fs_load("GRWAR\\REUNION.PRG", &size);
    if (!exe)
        return false;

    // Typed constants are part of the load image, which starts after the
    // MZ header (its size in paragraphs is at offset 8).
    bool ok = size >= 0x1C && exe[0] == 'M' && exe[1] == 'Z';
    if (ok) {
        size_t start = (size_t)(exe[8] | exe[9] << 8) * 16 + DS_FILE_SEG * 16;
        ok = start + DSEG_INIT_SIZE <= size;
        if (ok) {
            SDL_memcpy(&ds, exe + start, DSEG_INIT_SIZE);
            ok = apply_relocations(exe, size);
        }
    }
    SDL_free(exe);
    return ok;
}
