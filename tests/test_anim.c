// anim: the Anim unit's frame decoder on a hand-made frame, the shaped
// screen copy, and (with game data) whole animations loaded and drawn the
// game's way, compared with the assets/ani.h decoder by CRC.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "assets/ani.h"
#include "game/anim.h"
#include "game/dseg.h"
#include "game/system.h"
#include "platform/fs.h"
#include "platform/video.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

// The 320x200 screen at A000:0000 as one linear array.
static uint8_t *scr(void)
{
    return &vid_screen[0][0];
}

static void decoder(void)
{
    dseg_clear();
    vid_clear();
    // 9-byte magic, width 4, height 2, then: literal 5, skip 2, run 2 x 7,
    // skip (word) 1, run (word) 2 x 9.
    static const uint8_t block[] = {'S', 'p', 'i', 'd', 'y', 'A', 'n', 'i', 'm', 4, 0, 2, 0,
                                    0x05, 0x82, 0xC2, 0x07, 0x80, 0x01, 0x00, 0xC0, 0x02,
                                    0x00, 0x09,
                                    0x01};  // never read: the counter is 0 by then
    static uint8_t dst[16];
    SDL_memset(dst, 0xEE, 16);
    Anim_DecodeAnimFrame(block, dst, 0x40);
    static const uint8_t want[10] = {0x45, 0xEE, 0xEE, 0x47, 0x47, 0xEE, 0x49, 0x49, 0xEE, 0xEE};
    CHECK(SDL_memcmp(dst, want, sizeof want) == 0);
}

static void wedge(void)
{
    dseg_clear();
    vid_clear();
    static uint8_t s[0x8000];
    for (int i = 0; i < 0x8000; i++)
        s[i] = (uint8_t)(i % 251 + 1);
    Anim_BlitWedgeToScreen(s);
    int written = 0;
    for (int i = 0; i < 64000; i++)
        written += scr()[i] != 0;
    CHECK(written == 13409);
    // First row: 16 pixels at (72, 59) from src + C28h; the source is 160
    // wide, so the last row (7 pixels at (76, 190)) comes from row 150.
    CHECK(SDL_memcmp(scr() + 59 * 320 + 72, s + 0xC28, 16) == 0);
    CHECK(scr()[59 * 320 + 71] == 0 && scr()[59 * 320 + 88] == 0);
    CHECK(SDL_memcmp(scr() + 190 * 320 + 76, s + 150 * 160 + 76, 7) == 0);
    CHECK(scr()[58 * 320 + 80] == 0 && scr()[191 * 320 + 80] == 0);
}

// --- with game data ----------------------------------------------------------------

static void setup(void)
{
    dseg_clear();
    vid_clear();
    dseg_init();
    system_init();
    // The record Anim::Record points to (InitRoomAnim allocates it).
    ds.Anim_Record = System_GetMem(sizeof(TAnimRecord));
    SDL_memset(ds.Anim_Record, 0, sizeof(TAnimRecord));
}

// MAIN8 (320x200, delta frames) straight onto the screen, as the cut-scene
// player does, against ani_draw_frame on a host copy.
static void main_anim(void)
{
    setup();
    uint32_t before = system_heap_used();
    uint8_t frames = ds.Cc_Doors[8 - 1].Frames;
    Ani ani;
    CHECK(ani_load("ANIM\\MAIN8.ANI", &ani));
    CHECK(ani.width == 320 && ani.height == 200 && frames >= 2 && frames <= ani.count);
    Anim_LoadMainAnim(8);
    static uint8_t ref[64000];
    SDL_memset(ref, 0, sizeof ref);
    bool same = true;
    for (int f = 1; f <= frames; f++) {
        bool drawn = false;
        Anim_DrawAnimFrame((uint16_t)f, scr(), &drawn);
        CHECK(drawn);
        ani_draw_frame(&ani, f - 1, ref, 320, 0x40);
        same = same && SDL_crc32(0, scr(), 64000) == SDL_crc32(0, ref, 64000);
    }
    CHECK(same);
    // The record holds the sizes (stream + 13) and the loaded flag.
    const TAnimRecord *r = ds.Anim_Record;
    CHECK(r->Loaded == 1);
    CHECK((size_t)r->Sizes[0] == ani.frames[0].size + 13);
    // A second load while one is loaded does nothing.
    uint32_t loaded = system_heap_used();
    Anim_LoadGrAnim(1);
    CHECK(system_heap_used() == loaded);
    Anim_FreeMainAnim(8);
    CHECK(r->Loaded == 0);
    CHECK(system_heap_used() == before);
    CHECK(ds.Game_MinMemAvail <= SYSTEM_MEM_PLENTY);
    // Nothing loaded: ok stays true and nothing is drawn.
    uint32_t crc = SDL_crc32(0, scr(), 64000);
    bool drawn = false;
    Anim_DrawAnimFrame(2, scr(), &drawn);
    CHECK(drawn && SDL_crc32(0, scr(), 64000) == crc);
    ani_free(&ani);
}

// SA1 drawn keyed (frame 1, then frame N) into a width-stride buffer.
static void war_anim(void)
{
    setup();
    const TSaAnim *t = &ds.SpWar_SpriteRects[1 - 1];
    Ani ani;
    CHECK(ani_load("WAR\\WAR\\SA1.ANI", &ani));
    CHECK(t->Frames >= 2 && t->Frames <= ani.count);
    CHECK(t->Width == ani.width && t->Height == ani.height);
    Anim_LoadWarAnim(1);
    static uint8_t buf[0x10000]; // the decoder's 16-bit index stays inside
    size_t n = (size_t)ani.width * (size_t)ani.height;
    uint8_t *ref = SDL_malloc(n);
    bool same = true;
    for (int f = 1; f <= t->Frames; f++) {
        Anim_DrawAnimKeyed((uint16_t)f, buf);
        ani_draw_frame(&ani, 0, ref, (size_t)ani.width, 0x40);
        ani_draw_frame(&ani, f - 1, ref, (size_t)ani.width, 0x40);
        same = same && SDL_crc32(0, buf, n) == SDL_crc32(0, ref, n);
    }
    CHECK(same);
    Anim_FreeWarAnim(1);
    SDL_free(ref);
    ani_free(&ani);
}

int main(int argc, char *argv[])
{
    decoder();
    wedge();
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping the file tests\n");
        return ok ? 77 : 1;
    }
    main_anim();
    war_anim();
    return ok ? 0 : 1;
}
