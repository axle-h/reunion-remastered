#include "game/state.h"

#include <SDL3/SDL.h>

#include "game/dseg.h"
#include "game/dsfields.h"
#include "platform/autopilot.h"
#include "platform/video.h"

// DS fields left out besides the pointers (DSF_POINTER), because they only
// describe the memory layout or the port's emulation of it. "Field.Sub"
// names a field of every element of a record field.
static const char *const layout_fields[] = {
    // Segments: the PSP, video memory and the blitters' destination (screen
    // or an off-screen block).
    "System_PrefixSeg", "System_Seg0040", "System_SegA000", "System_SegB000", "System_SegB800",
    "Gfx_ScreenSeg",
    // The overlay heap's segments (empty, at the start of the heap).
    "System_OvrHeapOrg", "System_OvrHeapPtr", "System_OvrHeapEnd",
    // The Borland heap's own bookkeeping: the size GetMem is working on, and
    // the lowest MemAvail seen (diagnostic only).
    "System_HeapAllocSize", "Game_MinMemAvail",
    // Where the EMS blocks live (the original's state 2, in EMS with a
    // handle, frame and pages, or state 1 on the heap), and the last EMM
    // status. The blocks' contents are in the stream.
    "Ems_Status", "Ems_GameData.State", "Ems_GameData.Handle", "Ems_GameData.FrameSeg",
    "Ems_GameData.PageCount", "GfxPath_ToolbarBlock.State", "GfxPath_ToolbarBlock.Handle",
    "GfxPath_ToolbarBlock.FrameSeg", "GfxPath_ToolbarBlock.PageCount",
    // The copy protection's variables: the port keeps none of its state,
    // only the random numbers it drew (game/protect.c).
    "Protect_PageIndex", "Protect_Key1", "Protect_Key2", "Protect2_QuestionIndex",
    "Protect2_AnswerHash", "Protect2_HashEcho", "Protect2_AnswerKey", "Protect2_KeyEcho",
    "Protect2_Lives", "Protect2_ScribbleDone",
};

static bool layout_field(const char *name, const char *sub)
{
    for (size_t i = 0; i < SDL_arraysize(layout_fields); i++) {
        const char *f = layout_fields[i];
        size_t n = SDL_strlen(name);
        if (sub ? SDL_strncmp(f, name, n) == 0 && f[n] == '.' && SDL_strcmp(f + n + 1, sub) == 0
                : SDL_strcmp(f, name) == 0)
            return true;
    }
    return false;
}

static void reserve(StateBuf *b, size_t more)
{
    if (b->len + more <= b->cap)
        return;
    size_t cap = b->cap ? b->cap : 1 << 18;
    while (cap < b->len + more)
        cap *= 2;
    b->data = SDL_realloc(b->data, cap);
    b->cap = cap;
}

static void add_part(StateBuf *b, StatePart p)
{
    if (b->nparts == b->partcap) {
        b->partcap = b->partcap ? b->partcap * 2 : 1024;
        b->parts = SDL_realloc(b->parts, b->partcap * sizeof *b->parts);
    }
    b->parts[b->nparts++] = p;
}

static void put(StateBuf *b, const char *name, const void *p, uint32_t size, uint16_t elem,
                int16_t rec)
{
    add_part(b, (StatePart){b->len, size, name, elem, rec});
    reserve(b, size);
    SDL_memcpy(b->data + b->len, p, size);
    b->len += size;
}

static void put_u8(StateBuf *b, const char *name, uint8_t v)
{
    put(b, name, &v, 1, 1, -1);
}

// The DS part of the stream is the same list of byte ranges every time, so
// it is worked out once: the parts (with their stream offsets) and the DS
// runs to copy, adjacent ranges merged.
static struct {
    StatePart *parts;
    size_t nparts, partcap, len;
    struct {
        uint16_t ofs;
        uint32_t size;
    } *runs;
    size_t nruns, runcap;
} plan;

static void plan_range(const char *name, uint16_t ofs, uint32_t size, uint16_t elem, int16_t rec)
{
    if (plan.nparts == plan.partcap) {
        plan.partcap = plan.partcap ? plan.partcap * 2 : 1024;
        plan.parts = SDL_realloc(plan.parts, plan.partcap * sizeof *plan.parts);
    }
    plan.parts[plan.nparts++] = (StatePart){plan.len, size, name, elem, rec};
    plan.len += size;
    if (plan.nruns && plan.runs[plan.nruns - 1].ofs + plan.runs[plan.nruns - 1].size == ofs) {
        plan.runs[plan.nruns - 1].size += size;
        return;
    }
    if (plan.nruns == plan.runcap) {
        plan.runcap = plan.runcap ? plan.runcap * 2 : 256;
        plan.runs = SDL_realloc(plan.runs, plan.runcap * sizeof *plan.runs);
    }
    plan.runs[plan.nruns].ofs = ofs;
    plan.runs[plan.nruns++].size = size;
}

// A record field whose record holds pointers goes element by element, field
// by field.
static bool record_has_layout(int16_t rec, const char *name)
{
    if (rec < 0)
        return false;
    const DsRecord *r = &ds_records[rec];
    for (int i = 0; i < r->nfields; i++)
        if ((r->fields[i].flags & DSF_POINTER) || layout_field(name, r->fields[i].name))
            return true;
    return false;
}

static void make_plan(void)
{
    for (int i = 0; i < ds_field_count; i++) {
        const DsField *f = &ds_fields[i];
        if ((f->flags & DSF_POINTER) || layout_field(f->name, nullptr))
            continue;
        if (!record_has_layout(f->rec, f->name)) {
            plan_range(f->name, f->at, f->elem * f->count, f->elem, f->rec);
            continue;
        }
        const DsRecord *r = &ds_records[f->rec];
        for (uint32_t e = 0; e < f->count; e++)
            for (int k = 0; k < r->nfields; k++) {
                const DsField *sf = &r->fields[k];
                if ((sf->flags & DSF_POINTER) || layout_field(f->name, sf->name))
                    continue;
                plan_range(f->name, (uint16_t)(f->at + e * r->port_size + sf->at), sf->elem * sf->count,
                              1, -1);
            }
    }
}

static void put_ds(StateBuf *b)
{
    if (!plan.len)
        make_plan();
    reserve(b, plan.len);
    const uint8_t *base = (const uint8_t *)&ds;
    uint8_t *out = b->data + b->len;
    for (size_t i = 0; i < plan.nruns; i++) {
        SDL_memcpy(out, base + plan.runs[i].ofs, plan.runs[i].size);
        out += plan.runs[i].size;
    }
    b->len += plan.len;
    if (b->partcap < plan.nparts) {
        b->partcap = plan.nparts * 2;
        b->parts = SDL_realloc(b->parts, b->partcap * sizeof *b->parts);
    }
    SDL_memcpy(b->parts, plan.parts, plan.nparts * sizeof *plan.parts);
    b->nparts = plan.nparts;
}

// The contents of an EMS block (game/ems.h), a heap copy (state 1).
static void put_ems_block(StateBuf *b, const char *name, const TEmsBlock *blk)
{
    put_u8(b, name, blk->State != 0);
    if (blk->State != 0)
        put(b, name, blk->Data, blk->Size, 1, -1);
}

// The message log's entries 1..Message_LogCount, {kind: integer; text:
// string[50]} each, with the bytes past the text's length as zeros. The
// game reads nothing else: the rest of the block, and of each string, is
// whatever the heap or an earlier message left there.
static void put_log(StateBuf *b)
{
    enum { ENTRY = 0x35 };
    uint8_t log[15 * ENTRY] = {};
    int16_t n = SDL_clamp(ds.Message_LogCount, 0, 15);
    put_u8(b, "Message_Log^", ds.Message_Log != 0);
    if (!ds.Message_Log)
        return;
    const uint8_t *src = ds.Message_Log;
    for (int16_t i = 0; i < n; i++) {
        const uint8_t *e = src + i * ENTRY;
        SDL_memcpy(log + i * ENTRY, e, 3 + SDL_min(e[2], 50));
    }
    put(b, "Message_Log^", log, (uint32_t)n * ENTRY, ENTRY, -1);
}

void state_serialize(StateBuf *b)
{
    b->len = 0;
    b->nparts = 0;
    put_ds(b);
    put_ems_block(b, "Ems_GameData^", &ds.Ems_GameData);
    put_ems_block(b, "GfxPath_ToolbarBlock^", &ds.GfxPath_ToolbarBlock);
    put_log(b);
    put_u8(b, "vid_mode", (uint8_t)vid_mode());
    if (vid_mode() == VID_MODE_13H) {
        put(b, "screen", vid_screen, VID_WIDTH * VID_HEIGHT, 1, -1);
    } else {
        put(b, "screen12", vid12_screen, sizeof vid12_screen, 1, -1);
        put(b, "vid_attr", vid_attr, sizeof vid_attr, 1, -1);
    }
    put(b, "vid_palette", vid_palette, sizeof vid_palette, 3, -1);
}

void state_buf_free(StateBuf *b)
{
    SDL_free(b->data);
    SDL_free(b->parts);
    *b = (StateBuf){};
}

uint64_t state_buf_hash(const StateBuf *b)
{
    return autopilot_hash(b->data, b->len);
}

uint64_t state_hash(void)
{
    static StateBuf b;
    state_serialize(&b);
    return state_buf_hash(&b);
}

size_t state_first_difference(const StateBuf *a, const StateBuf *b)
{
    size_t n = SDL_min(a->len, b->len);
    if (n == a->len && n == b->len && SDL_memcmp(a->data, b->data, n) == 0)
        return SIZE_MAX;
    size_t at = 0;
    while (at < n && a->data[at] == b->data[at])
        at++;
    return at;
}

void state_describe(const StateBuf *b, size_t at, char *out, size_t size)
{
    const StatePart *p = nullptr;
    for (size_t i = 0; i < b->nparts; i++)
        if (at >= b->parts[i].start && at < b->parts[i].start + b->parts[i].size)
            p = &b->parts[i];
    if (!p) {
        SDL_snprintf(out, size, "end of the stream (+%zu)", at - b->len);
        return;
    }
    size_t k = at - p->start;
    if (SDL_strcmp(p->name, "screen") == 0) {
        SDL_snprintf(out, size, "screen x=%zu y=%zu", k % VID_WIDTH, k / VID_WIDTH);
        return;
    }
    if (SDL_strcmp(p->name, "screen12") == 0) {
        SDL_snprintf(out, size, "mode 12h screen x=%zu y=%zu", k % VID12_WIDTH, k / VID12_WIDTH);
        return;
    }
    bool in_ds = p->name[SDL_strlen(p->name) - 1] != '^' && SDL_strncmp(p->name, "vid", 3) != 0;
    const char *where = in_ds ? "DS " : "";
    if (p->rec >= 0) {
        const DsRecord *r = &ds_records[p->rec];
        size_t e = k / p->elem, ofs = k % p->elem;
        for (int i = 0; i < r->nfields; i++) {
            const DsField *f = &r->fields[i];
            if (ofs >= f->ofs && ofs < f->ofs + (size_t)f->elem * f->count) {
                SDL_snprintf(out, size, "%s%s[%zu].%s+%zu", where, p->name, e, f->name, ofs - f->ofs);
                return;
            }
        }
    }
    if (p->elem > 1 && p->size > p->elem)
        SDL_snprintf(out, size, "%s%s[%zu]+%zu", where, p->name, k / p->elem, k % p->elem);
    else
        SDL_snprintf(out, size, "%s%s+%zu", where, p->name, k);
}

// The file: the stream's length (8 bytes, little-endian) and bytes, then one
// line per part: "start size elem rec name".
bool state_save(const StateBuf *b, const char *path)
{
    SDL_IOStream *f = SDL_IOFromFile(path, "wb");
    if (!f)
        return false;
    bool ok = SDL_WriteU64LE(f, b->len) && SDL_WriteIO(f, b->data, b->len) == b->len;
    for (size_t i = 0; ok && i < b->nparts; i++)
        ok = SDL_IOprintf(f, "%zu %u %u %d %s\n", b->parts[i].start, b->parts[i].size, b->parts[i].elem,
                          b->parts[i].rec, b->parts[i].name) > 0;
    return SDL_CloseIO(f) && ok;
}

bool state_load(StateBuf *b, const char *path)
{
    size_t size;
    char *file = SDL_LoadFile(path, &size);
    uint64_t len = 0;
    if (file && size >= 8)
        SDL_memcpy(&len, file, 8);
    if (!file || len > size - 8) {
        SDL_free(file);
        return false;
    }
    state_buf_free(b);
    reserve(b, len);
    SDL_memcpy(b->data, file + 8, len);
    b->len = len;
    // Part names are kept for good: loaded dumps are few.
    // SDL_LoadFile ends the data with a zero byte.
    char *text = file + 8 + len, *save = nullptr;
    for (char *line = SDL_strtok_r(text, "\n", &save); line; line = SDL_strtok_r(nullptr, "\n", &save)) {
        size_t start;
        unsigned psize, elem;
        int rec, at = 0;
        if (SDL_sscanf(line, "%zu %u %u %d %n", &start, &psize, &elem, &rec, &at) >= 4 && at > 0)
            add_part(b, (StatePart){start, psize, SDL_strdup(line + at), (uint16_t)elem, (int16_t)rec});
    }
    SDL_free(file);
    return true;
}
