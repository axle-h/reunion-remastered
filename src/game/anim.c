#include "game/anim.h"

#include <SDL3/SDL.h>

#include "game/dseg.h"
#include "game/gfx.h"
#include "game/system.h"

// The animation record (Anim::Record, TAnimRecord) and the frame counts of
// the three kinds: SpWar::SpriteRects (war), Cc::Doors (main) and Cc::Rooms
// (gr). Frames count from 1.

// Shared body of the three loaders: <prefix><id>.ani, frame count from the
// first byte of the table entry. keep_nil: the SA loader leaves the pointer
// alone when the chunk was skipped; the others store the nil.
static void load_anim(uint16_t id, const char *prefix, uint8_t frames, bool keep_nil)
{
    TAnimRecord *rec = ds.Anim_Record;
    if (rec->Loaded == 0) {
        rec->Loaded = 1;
        char path[64];
        SDL_snprintf(path, sizeof path, "%s%d.ani", prefix, (int16_t)id);
        Gfx_OpenDataFile(path);
        uint16_t count = frames;
        if (count != 0) {
            for (uint16_t i = 1;; i++) {
                uint8_t *buf;
                uint16_t size;
                Gfx_ReadChunk(&buf, &size);
                rec->Sizes[i - 1] = size;
                if (!keep_nil || size != 0)
                    rec->Frames[i - 1] = buf;
                if (i == count)
                    break;
            }
        }
        Gfx_CloseDataFile();
    }
}

// Shared body of the three Free routines. Main/Gr skip frames whose size is
// 0; War frees every frame, but FreeMem with size 0 does nothing, so all
// three skip them.
static void free_anim(uint8_t frames)
{
    TAnimRecord *rec = ds.Anim_Record;
    if (rec->Loaded != 0) {
        rec->Loaded = 0;
        uint16_t count = frames;
        if (count != 0) {
            for (uint16_t i = 1;; i++) {
                if (rec->Sizes[i - 1] != 0)
                    game_free(rec->Frames[i - 1]);
                if (i == count)
                    break;
            }
        }
    }
}

// 3c64:0010
void Anim_LoadWarAnim(uint16_t id)
{
    load_anim(id, "war\\war\\sa", ds.SpWar_SpriteRects[id - 1].Frames, true);
}

// 3c64:011e
void Anim_FreeWarAnim(uint16_t id)
{
    free_anim(ds.SpWar_SpriteRects[id - 1].Frames);
}

// 3c64:019c
void Anim_LoadMainAnim(uint16_t id)
{
    load_anim(id, "anim\\main", ds.Cc_Doors[id - 1].Frames, false);
}

// 3c64:028d
void Anim_FreeMainAnim(uint16_t id)
{
    free_anim(ds.Cc_Doors[id - 1].Frames);
}

// 3c64:031d
void Anim_LoadGrAnim(uint16_t id)
{
    load_anim(id, "war\\grwar\\granim", ds.Cc_Rooms[id - 1].Frames, false);
}

// 3c64:0415
void Anim_FreeGrAnim(uint16_t id)
{
    free_anim(ds.Cc_Rooms[id - 1].Frames);
}

// 3c64:0496
void Anim_DrawAnimKeyed(uint16_t frame, uint8_t *dest)
{
    const TAnimRecord *rec = ds.Anim_Record;
    if (rec->Loaded != 0) {
        Anim_DecodeAnimFrame(rec->Frames[0], dest, 0x40);
        uint16_t f = frame;
        while (rec->Sizes[f - 1] == 0 && (int16_t)f > 1)
            f--;
        Anim_DecodeAnimFrame(rec->Frames[f - 1], dest, 0x40);
    }
}

// 3c64:0516
void Anim_DrawAnimFrame(uint16_t frame, uint8_t *dest, bool *ok)
{
    *ok = true;
    const TAnimRecord *rec = ds.Anim_Record;
    if (rec->Loaded != 0) {
        if (rec->Frames[frame - 1] == nullptr)
            *ok = false;
        else
            Anim_DecodeAnimFrame(rec->Frames[frame - 1], dest, 0x40);
    }
}

// 3c64:057c: width and height are the words at block offsets 9 and 11; the
// 16-bit pixel counter starts at width * height and decoding stops when an
// opcode brings it to exactly 0.
void Anim_DecodeAnimFrame(const uint8_t *frame, uint8_t *dest, uint8_t colourOffset)
{
    uint16_t width = (uint16_t)(frame[10] << 8) + frame[9];
    uint16_t height = (uint16_t)(frame[12] << 8) + frame[11];
    uint16_t left = (uint16_t)(width * height);
    const uint8_t *src = frame + 13;
    uint16_t di = 0;
    for (;;) {
        uint8_t op = *src++;
        uint16_t n;
        if (op < 0x80) {
            dest[di++] = (uint8_t)(op + colourOffset);
            n = 1;
        } else if (op < 0xC0) {
            if (op == 0x80) {
                n = (uint16_t)(src[0] | src[1] << 8);
                src += 2;
            } else {
                n = op & 0x3F;
            }
            di += n;
        } else {
            if (op == 0xC0) {
                n = (uint16_t)(src[0] | src[1] << 8);
                src += 2;
            } else {
                n = op & 0x3F;
            }
            uint8_t v = (uint8_t)(*src++ + colourOffset);
            for (uint16_t i = 0; i < n; i++)
                dest[di++] = v;
        }
        left -= n;
        if (left == 0)
            break;
    }
}

// The rows of 3c64:0666 as source offset (from src), screen offset and
// length; the original is one unrolled sequence of rep movsw / movsb with
// these values.
static const struct {
    uint16_t src, dst, len;
} wedge_rows[132] = {
    {0x0C28, 0x4A08, 16},
    {0x0CC2, 0x4B42, 27},
    {0x0D5E, 0x4C7E, 35},
    {0x0DFA, 0x4DBA, 43},
    {0x0E98, 0x4EF8, 48},
    {0x0F35, 0x5035, 53},
    {0x0FD3, 0x5173, 57},
    {0x1071, 0x52B1, 61},
    {0x110F, 0x53EF, 65},
    {0x11AD, 0x552D, 69},
    {0x124C, 0x566C, 71},
    {0x12EA, 0x57AA, 75},
    {0x1389, 0x58E9, 77},
    {0x1428, 0x5A28, 80},
    {0x14C6, 0x5B66, 83},
    {0x1565, 0x5CA5, 85},
    {0x1604, 0x5DE4, 87},
    {0x16A3, 0x5F23, 89},
    {0x1742, 0x6062, 91},
    {0x17E1, 0x61A1, 93},
    {0x1880, 0x62E0, 95},
    {0x191F, 0x641F, 97},
    {0x19BE, 0x655E, 99},
    {0x1A5D, 0x669D, 101},
    {0x1AFD, 0x67DD, 102},
    {0x1B9C, 0x691C, 104},
    {0x1C3B, 0x6A5B, 106},
    {0x1CDA, 0x6B9A, 107},
    {0x1D79, 0x6CD9, 109},
    {0x1E19, 0x6E19, 109},
    {0x1EB8, 0x6F58, 111},
    {0x1F57, 0x7097, 113},
    {0x1FF7, 0x71D7, 113},
    {0x2097, 0x7317, 114},
    {0x2136, 0x7456, 115},
    {0x21D6, 0x7596, 116},
    {0x2275, 0x76D5, 117},
    {0x2314, 0x7814, 119},
    {0x23B4, 0x7954, 119},
    {0x2453, 0x7A93, 121},
    {0x24F3, 0x7BD3, 121},
    {0x2593, 0x7D13, 121},
    {0x2632, 0x7E52, 123},
    {0x26D2, 0x7F92, 123},
    {0x2771, 0x80D1, 125},
    {0x2811, 0x8211, 125},
    {0x28B1, 0x8351, 125},
    {0x2951, 0x8491, 125},
    {0x29F0, 0x85D0, 127},
    {0x2A90, 0x8710, 127},
    {0x2B30, 0x8850, 127},
    {0x2BD0, 0x8990, 127},
    {0x2C6F, 0x8ACF, 129},
    {0x2D0F, 0x8C0F, 129},
    {0x2DAF, 0x8D4F, 129},
    {0x2E4F, 0x8E8F, 129},
    {0x2EEF, 0x8FCF, 129},
    {0x2F8F, 0x910F, 129},
    {0x302F, 0x924F, 129},
    {0x30CF, 0x938F, 129},
    {0x316E, 0x94CE, 130},
    {0x320E, 0x960E, 131},
    {0x32AE, 0x974E, 131},
    {0x334E, 0x988E, 131},
    {0x33EE, 0x99CE, 131},
    {0x348E, 0x9B0E, 131},
    {0x352E, 0x9C4E, 131},
    {0x35CE, 0x9D8E, 131},
    {0x366E, 0x9ECE, 130},
    {0x370F, 0xA00F, 129},
    {0x37AF, 0xA14F, 129},
    {0x384F, 0xA28F, 129},
    {0x38EF, 0xA3CF, 129},
    {0x398F, 0xA50F, 129},
    {0x3A2F, 0xA64F, 129},
    {0x3ACF, 0xA78F, 129},
    {0x3B6F, 0xA8CF, 129},
    {0x3C10, 0xAA10, 127},
    {0x3CB0, 0xAB50, 127},
    {0x3D50, 0xAC90, 127},
    {0x3DF0, 0xADD0, 127},
    {0x3E91, 0xAF11, 126},
    {0x3F31, 0xB051, 125},
    {0x3FD1, 0xB191, 125},
    {0x4071, 0xB2D1, 125},
    {0x4112, 0xB412, 123},
    {0x41B2, 0xB552, 123},
    {0x4252, 0xB692, 123},
    {0x42F2, 0xB7D2, 123},
    {0x4393, 0xB913, 121},
    {0x4433, 0xBA53, 121},
    {0x44D4, 0xBB94, 119},
    {0x4574, 0xBCD4, 119},
    {0x4615, 0xBE15, 117},
    {0x46B5, 0xBF55, 117},
    {0x4756, 0xC096, 115},
    {0x47F6, 0xC1D6, 115},
    {0x4897, 0xC317, 113},
    {0x4937, 0xC457, 113},
    {0x49D8, 0xC598, 111},
    {0x4A78, 0xC6D8, 111},
    {0x4B19, 0xC819, 109},
    {0x4BBA, 0xC95A, 107},
    {0x4C5B, 0xCA9B, 105},
    {0x4CFB, 0xCBDB, 105},
    {0x4D9C, 0xCD1C, 103},
    {0x4E3D, 0xCE5D, 101},
    {0x4EDE, 0xCF9E, 99},
    {0x4F7F, 0xD0DF, 97},
    {0x5020, 0xD220, 95},
    {0x50C1, 0xD361, 93},
    {0x5162, 0xD4A2, 91},
    {0x5203, 0xD5E3, 89},
    {0x52A4, 0xD724, 87},
    {0x5345, 0xD865, 85},
    {0x53E6, 0xD9A6, 83},
    {0x5487, 0xDAE7, 81},
    {0x5529, 0xDC29, 77},
    {0x55CA, 0xDD6A, 75},
    {0x566B, 0xDEAB, 73},
    {0x570D, 0xDFED, 69},
    {0x57AE, 0xE12E, 67},
    {0x5850, 0xE270, 63},
    {0x58F2, 0xE3B2, 59},
    {0x5994, 0xE4F4, 55},
    {0x5A36, 0xE636, 51},
    {0x5AD9, 0xE779, 45},
    {0x5B7D, 0xE8BD, 38},
    {0x5C1F, 0xE9FF, 33},
    {0x5CC2, 0xEB42, 27},
    {0x5D67, 0xEC87, 18},
    {0x5E0C, 0xEDCC, 7},
};

// 3c64:0666
void Anim_BlitWedgeToScreen(const uint8_t *src)
{
    uint8_t *screen = gfx_screen();
    for (int r = 0; r < 132; r++)
        SDL_memcpy(screen + wedge_rows[r].dst, src + wedge_rows[r].src, wedge_rows[r].len);
}
