// The port's side of tools/dosbox/side_by_side.py.
//
//   dosside port DATA_DIR SCRIPT OUT_DIR HOURS
//       plays SCRIPT (autopilot syntax, '@file' allowed) with every fix and
//       improvement off and saves the state stream at the first retrace
//       after each of the next HOURS game hours as OUT_DIR/port-N.state
//   dosside dos MEM.BIN DS_SEG OUT.state [PORT.state]
//       turns a dump of the DOS game's memory into a state stream: MEM.BIN
//       holds linear 0..DFFFF (mode 13h video memory at A0000), then a 64K
//       view of each EMS block's pages, the game data's first. The palette
//       (not in memory) is taken from PORT.state if given
//   dosside compare A.state B.state
//       lists the parts that differ, leaving out IGNORED; exit status 1 if any
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#include "game/dseg.h"
#include "game/dsfields.h"
#include "game/fixes.h"
#include "game/state.h"
#include "platform/config.h"
#include "platform/fs.h"
#include "platform/input.h"
#include "platform/video.h"
#include "play/play.h"

// --- port ---------------------------------------------------------------------

static const char *out_dir;
static int hours_left, hour_no;
static int last_hour = -1;

static void watch(uint64_t frame)
{
    (void)frame;
    if (hours_left <= 0)
        return;
    // The new game's date (23:00) is set, not simulated: hours count from
    // the first change after it, as the original's Sim_AdvanceHour runs.
    int h = ds.Game_Year ? ds.Game_Hour + 24 * (ds.Game_Day + 31 * ds.Game_Month) : -1;
    if (last_hour >= 0 && h != last_hour) {
        static StateBuf b;
        state_serialize(&b);
        char path[1024];
        SDL_snprintf(path, sizeof path, "%s/port-%d.state", out_dir, ++hour_no);
        state_save(&b, path);
        printf("port: hour %d, %d-%d-%d %d:00, FrameCount %d, RandSeed %08x\n", hour_no, ds.Game_Year,
               ds.Game_Month, ds.Game_Day, ds.Game_Hour, ds.Game_FrameCount, (uint32_t)ds.System_RandSeed);
        if (--hours_left == 0)
            input_request_quit();
    }
    last_hour = h;
}

static int port(const char *data, const char *script, const char *dir, int hours)
{
    if (!fs_init(data)) {
        printf("no game data in %s\n", data);
        return 1;
    }
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_EVENTS))
        return 1;
    for (int i = 0; i < Fixes_Count; i++)
        config_set_fix(Fixes_List[i].key, false);
    config.colony_stats = false;
    config.detoxin_rate = false;
    out_dir = dir;
    hours_left = hours;
    play_replay_watch(script, watch);
    SDL_Quit();
    return hours_left == 0 ? 0 : 1;
}

// --- dos ----------------------------------------------------------------------

static uint8_t *mem;
static size_t mem_len;
static uint32_t ds_base;
static uint32_t ems_view = 0xE0000;  // the next EMS block's view

static const uint8_t *far_ptr(uint32_t at, uint32_t size)
{
    uint16_t off = (uint16_t)(mem[at] | mem[at + 1] << 8);
    uint16_t seg = (uint16_t)(mem[at + 2] | mem[at + 3] << 8);
    uint32_t lin = ((uint32_t)seg << 4) + off;
    if (lin + size > mem_len) {
        printf("pointer at DS:%04X (%04X:%04X) outside the dump\n", at - ds_base, seg, off);
        exit(1);
    }
    return mem + lin;
}

// A DS field or record field from the original's bytes at `src` into the
// port's at `dst`, element by element where the record holds pointers
// (state.c leaves the pointers out, so they stay zero).
static void copy_field(uint8_t *dst, const uint8_t *src, const DsField *f)
{
    if (f->flags & DSF_POINTER)
        return;
    if (f->rec < 0) {
        SDL_memcpy(dst + f->at, src + f->ofs, (size_t)f->elem * f->count);
        return;
    }
    const DsRecord *r = &ds_records[f->rec];
    for (uint32_t e = 0; e < f->count; e++)
        for (int k = 0; k < r->nfields; k++)
            copy_field(dst + f->at + e * r->port_size, src + f->ofs + e * r->size, &r->fields[k]);
}

// An EMS block as state.c reads it: State, then Data[Size] if State != 0.
// TEmsBlock in the original: State, Data (far), Size, Handle, FrameSeg,
// PageCount; State 1 is a heap block, 2 one in EMS.
static void ems_block(TEmsBlock *blk, uint16_t ofs)
{
    const uint8_t *rec = mem + ds_base + ofs;
    uint16_t size = (uint16_t)(rec[5] | rec[6] << 8);
    blk->Data = nullptr;
    if (blk->State == 1) {
        blk->Data = (uint8_t *)far_ptr(ds_base + ofs + 1, size);
    } else if (blk->State == 2) {
        if (ems_view + size > mem_len) {
            printf("no EMS view for DS:%04X\n", ofs);
            exit(1);
        }
        blk->Data = mem + ems_view;
        ems_view += 0x10000;
    }
}

static int dos(const char *mem_path, const char *ds_seg, const char *out_path, const char *port_state)
{
    mem = SDL_LoadFile(mem_path, &mem_len);
    if (!mem || mem_len < 0xE0000) {
        printf("can't read %s (or under E0000h bytes)\n", mem_path);
        return 1;
    }
    ds_base = (uint32_t)SDL_strtoul(ds_seg, nullptr, 16) << 4;
    SDL_memset(&ds, 0, sizeof ds);
    for (int i = 0; i < ds_field_count; i++)
        copy_field((uint8_t *)&ds, mem + ds_base, &ds_fields[i]);
    ems_block(&ds.Ems_GameData, 0x8A4E);
    ems_block(&ds.GfxPath_ToolbarBlock, 0x7DFA);
    // Message_Log: a far pointer, null or the log's block.
    const uint8_t *p = mem + ds_base + 0x7ADE;
    if (p[0] | p[1] | p[2] | p[3])
        ds.Message_Log = (uint8_t *)far_ptr(ds_base + 0x7ADE, 15 * 0x35);
    vid_set_mode(VID_MODE_13H);
    SDL_memcpy(vid_screen, mem + 0xA0000, VID_WIDTH * VID_HEIGHT);
    if (port_state) {
        StateBuf ps = {};
        if (!state_load(&ps, port_state))
            return 1;
        for (size_t i = 0; i < ps.nparts; i++)
            if (SDL_strcmp(ps.parts[i].name, "vid_palette") == 0)
                SDL_memcpy(vid_palette, ps.data + ps.parts[i].start, sizeof vid_palette);
    }
    StateBuf b = {};
    state_serialize(&b);
    if (!state_save(&b, out_path))
        return 1;
    printf("%s: %d-%d-%d %d:00, FrameCount %d, RandSeed %08x\n", out_path, ds.Game_Year, ds.Game_Month,
           ds.Game_Day, ds.Game_Hour, ds.Game_FrameCount, (uint32_t)ds.System_RandSeed);
    return 0;
}

// --- compare ------------------------------------------------------------------

// What the port doesn't keep as the original did, by state_describe name.
static const char *const IGNORED[] = {
    "DS System_",          // the Borland run time
    "DS Sound_SbPort",     // the Sound Blaster's settings from SETUP.SPD:
    "DS Sound_SbIrq",      // the port's sound is SDL's
    "DS Sound_SbDma",
    "DS SoundDrv_",
    "DS Gus_",
    "DS Mouse_Regs",       // the INT 33h call's registers
    "DS _pad_",            // bytes no routine of the port names
    // A C for loop leaves its variable one past the last value, Pascal's at
    // it; every read of Main_I follows an assignment.
    "DS Main_I",
};

static int compare(const char *a_path, const char *b_path)
{
    StateBuf a = {}, b = {};
    if (!state_load(&a, a_path) || !state_load(&b, b_path))
        return 1;
    if (a.len != b.len) {
        printf("  lengths differ: %zu | %zu\n", a.len, b.len);
        return 1;
    }
    // Every part that differs, with its first differing byte and count.
    int parts = 0, ignored = 0;
    for (size_t i = 0; i < a.nparts; i++) {
        const StatePart *pa = &a.parts[i];
        size_t first = SIZE_MAX, count = 0;
        for (size_t k = 0; k < pa->size; k++)
            if (a.data[pa->start + k] != b.data[pa->start + k]) {
                if (first == SIZE_MAX)
                    first = k;
                count++;
            }
        if (!count)
            continue;
        char name[128];
        state_describe(&a, pa->start + first, name, sizeof name);
        bool skip = false;
        for (size_t k = 0; k < SDL_arraysize(IGNORED); k++)
            skip |= SDL_strncmp(name, IGNORED[k], SDL_strlen(IGNORED[k])) == 0;
        if (skip) {
            ignored++;
            continue;
        }
        printf("  %-44s %5zu bytes differ, first %02X | %02X\n", name, count, a.data[pa->start + first],
               b.data[pa->start + first]);
        parts++;
    }
    printf("  %s (%d parts differ, %d of them ignored)\n", parts ? "DIFFERENT" : "same", parts + ignored, ignored);
    return parts != 0;
}

int main(int argc, char *argv[])
{
    if (argc == 6 && SDL_strcmp(argv[1], "port") == 0)
        return port(argv[2], argv[3], argv[4], atoi(argv[5]));
    if ((argc == 5 || argc == 6) && SDL_strcmp(argv[1], "dos") == 0)
        return dos(argv[2], argv[3], argv[4], argc == 6 ? argv[5] : nullptr);
    if (argc == 4 && SDL_strcmp(argv[1], "compare") == 0)
        return compare(argv[2], argv[3]);
    printf("usage: see tools/dosbox/dosside.c\n");
    return 2;
}
