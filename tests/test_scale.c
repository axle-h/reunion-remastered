// scale: every ScaleRow16..32 against the column pattern read from the
// original's code (the unrolled movsb / movsw / inc si / rep movsw sequences
// at 3955:0000 in REUNION.PRG), and ScaleSprite's row selection.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/mem.h"
#include "game/scale.h"
#include "platform/fs.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static const FarPtr SRC = (FarPtr)0x50000000;  // 5000:0000, 320 wide
static const FarPtr DST = (FarPtr)0x60000000;

static const uint16_t entry[17] = {0x0000, 0x0034, 0x0068, 0x009a, 0x00ca, 0x00f8,
                                   0x0124, 0x0150, 0x017d, 0x01aa, 0x01dd, 0x0215,
                                   0x024a, 0x027c, 0x02ab, 0x02d3, 0x02f4};

// Simulates the string moves of ScaleRow<width>, giving the source column of
// each destination pixel. Returns the number of pixels written.
static int row_pattern(const uint8_t *code, int cols[64])
{
    // push bp / mov bp,sp / push ds / mov ds,[bp+0Ch] / mov si,[bp+0Ah] /
    // mov es,[bp+8] / mov di,[bp+6]
    static const uint8_t prologue[16] = {0x55, 0x89, 0xE5, 0x1E, 0x8E, 0x5E, 0x0C, 0x8B,
                                         0x76, 0x0A, 0x8E, 0x46, 0x08, 0x8B, 0x7E, 0x06};
    if (SDL_memcmp(code, prologue, sizeof prologue) != 0)
        return -1;
    int si = 0, n = 0;
    for (const uint8_t *p = code + 16; n < 64;) {
        if (*p == 0xA4) {  // movsb
            cols[n++] = si++;
            p++;
        } else if (*p == 0xA5) {  // movsw
            cols[n++] = si++;
            cols[n++] = si++;
            p++;
        } else if (*p == 0x46) {  // inc si
            si++;
            p++;
        } else if (p[0] == 0xB9 && p[3] == 0xF3 && p[4] == 0xA5) {  // mov cx,n / rep movsw
            for (int k = p[1] | p[2] << 8; k > 0 && n < 63; k--) {
                cols[n++] = si++;
                cols[n++] = si++;
            }
            p += 5;
        } else if (*p == 0x1F) {  // pop ds
            return n;
        } else {
            return -1;
        }
    }
    return -1;
}

static void fill_source(void)
{
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++)
            *((uint8_t *)far_host(SRC) + y * 320 + x) = (uint8_t)(y * 7 + x * 3 + 1);
}

static void rows(const uint8_t *code)
{
    for (int w = 16; w <= 32; w++) {
        int cols[64];
        int n = row_pattern(code + entry[w - 16], cols);
        CHECK(n == w);
        if (n != w)
            continue;
        mem_init();
        fill_source();
        SDL_memset(far_host(DST), 0xEE, 64);
        Scale_RowProcTable[w - 16](SRC, DST);
        const uint8_t *s = far_host(SRC), *d = far_host(DST);
        bool same = true;
        for (int i = 0; i < w; i++)
            same = same && d[i] == s[cols[i]];
        CHECK(same);
        CHECK(d[w] == 0xEE);  // nothing past the row
    }
}

// Rows kept by ScaleSprite: an error term starting at 2 * size - 32 that
// gains 2 * size per source row and loses 64 per kept row.
static void sprite(void)
{
    for (int size = 14; size <= 34; size++) {
        int clamped = size < 16 ? 16 : size > 32 ? 32 : size;
        mem_init();
        fill_source();
        Scale_ScaleSprite(SRC, DST, (int16_t)size);
        const uint8_t *d = far_host(DST);
        // Each destination row equals some source row (scaled the same way)
        // and the source rows are increasing.
        uint8_t ref[32];
        int kept = 0, err = 2 * clamped - 32;
        bool same = true;
        for (int r = 0; r < 32; r++) {
            if (err >= 0) {
                Scale_RowProcTable[clamped - 16](far_ptr(0x5000, (uint16_t)(r * 320)),
                                                 far_ptr(0x7000, 0));
                SDL_memcpy(ref, far_host(far_ptr(0x7000, 0)), (size_t)clamped);
                same = same && SDL_memcmp(ref, d + kept * 320, (size_t)clamped) == 0;
                kept++;
                err += 2 * clamped - 64;
            } else {
                err += 2 * clamped;
            }
        }
        CHECK(same);
        CHECK(kept == clamped);
        CHECK(d[kept * 320] == 0);  // nothing below the last row
    }
    // Full size is a plain copy.
    mem_init();
    fill_source();
    Scale_ScaleSprite(SRC, DST, 32);
    bool same = true;
    for (int y = 0; y < 32; y++)
        same = same && SDL_memcmp((uint8_t *)far_host(SRC) + y * 320,
                                  (uint8_t *)far_host(DST) + y * 320, 32) == 0;
    CHECK(same);
}

int main(int argc, char *argv[])
{
    sprite();
    size_t size = 0;
    uint8_t *exe = nullptr;
    if (argc >= 2 && fs_init(argv[1]))
        exe = fs_load("GRWAR\\REUNION.PRG", &size);
    if (!exe) {
        printf("no game data, skipping the row pattern tests\n");
        return ok ? 77 : 1;
    }
    // Segment 3955h (Ghidra) is file paragraph 2955h after the MZ header.
    size_t base = (size_t)(exe[8] | exe[9] << 8) * 16 + (size_t)(0x3955 - 0x1000) * 16;
    CHECK(base + 0x30E <= size);
    if (base + 0x30E <= size)
        rows(exe + base);
    SDL_free(exe);
    return ok ? 0 : 1;
}
