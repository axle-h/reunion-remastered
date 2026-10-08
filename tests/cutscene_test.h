// Shared by the cut-scene smoke tests: headless set-up, a run under an
// autopilot script, frame dumps and the program's DS tables.
#pragma once

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#include "cutscene/cs.h"
#include "cutscene/cutscene.h"
#include "platform/autopilot.h"
#include "platform/fs.h"
#include "platform/input.h"
#include "platform/timing.h"

static bool ok = true;
#define CHECK(c)                                                    \
    do {                                                            \
        if (!(c)) {                                                 \
            printf("FAILED line %d: %s\n", __LINE__, #c);           \
            ok = false;                                             \
        }                                                           \
    } while (0)

// Exits 77 when the game data or the program's directory is missing.
static void cst_init(int argc, char *argv[], const char *dir)
{
    char *path = nullptr;
    if (argc < 2 || !fs_init(argv[1]) || !(path = fs_resolve(dir))) {
        printf("no game data, skipping\n");
        exit(77);
    }
    SDL_free(path);
    SDL_setenv_unsafe("SDL_AUDIO_DRIVER", "dummy", 1);
    if (!SDL_Init(SDL_INIT_EVENTS)) {
        printf("SDL_Init: %s\n", SDL_GetError());
        exit(1);
    }
}

// Plays the cut-scene under `script` (frames count from its start). Returns
// cutscene_play's result; *frames receives the retraces it took.
static bool cst_run(Cutscene which, const char *script, uint64_t *frames)
{
    input_reset();
    autopilot_set_script(script);
    uint64_t start = timing_frames();
    bool result = cutscene_play(which);
    *frames = timing_frames() - start;
    autopilot_set_script(nullptr);
    return result;
}

static const char *cst_dump_path(const char *name, int frame)
{
    static char path[256];
    SDL_snprintf(path, sizeof path, "%s_%d.bmp", name, frame);
    return path;
}

// Share (percent) of non-black pixels in a frame dumped by the autopilot;
// *width receives its width (320 or 640). -1 if there is no dump.
static int cst_lit_percent(const char *name, int frame, int *width)
{
    SDL_Surface *s = SDL_LoadBMP(cst_dump_path(name, frame));
    if (!s)
        return -1;
    SDL_Surface *rgb = SDL_ConvertSurface(s, SDL_PIXELFORMAT_XRGB8888);
    SDL_DestroySurface(s);
    if (!rgb)
        return -1;
    long lit = 0;
    for (int y = 0; y < rgb->h; y++) {
        const uint32_t *row = (const uint32_t *)((const uint8_t *)rgb->pixels + y * rgb->pitch);
        for (int x = 0; x < rgb->w; x++)
            lit += (row[x] & 0xffffff) != 0;
    }
    *width = rgb->w;
    int percent = (int)(lit * 100 / ((long)rgb->w * rgb->h));
    SDL_DestroySurface(rgb);
    SDL_RemovePath(cst_dump_path(name, frame));
    return percent;
}

// The frame-count table in the program's data segment (DS paragraph `ds`,
// relative to the load image) at `base` + 7n, against the port's copy.
static void cst_check_counts(const CsProgram *prog, const char *exe, int ds, int base)
{
    size_t size = 0;
    uint8_t *data = fs_load(exe, &size);
    CHECK(data && size > 0x40);
    if (!data)
        return;
    size_t header = (size_t)(data[8] | data[9] << 8) * 16;
    const CsProgram *saved = cs_prog;
    cs_prog = prog;
    for (int n = 1; n < prog->anims; n++) {
        size_t at = header + (size_t)ds * 16 + (size_t)(base + 7 * n);
        CHECK(at < size && cs_frame_count(n) == data[at]);
    }
    cs_prog = saved;
    SDL_free(data);
}
