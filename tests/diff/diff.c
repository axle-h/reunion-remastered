// Differential test harness: see diff.h and docs/difftest.md.
#include "diff/diff.h"

#include <SDL3/SDL.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unicorn/unicorn.h>

#include "diff/dseg_names.h"
#include "game/dseg.h"
#include "game/emm.h"
#include "game/fixes.h"
#include "game/gfx.h"
#include "game/main_program.h"
#include "game/mouse.h"
#include "game/system.h"
#include "platform/audio.h"
#include "platform/autopilot.h"
#include "platform/fs.h"
#include "platform/input.h"
#include "platform/timing.h"
#include "platform/video.h"

enum : uint32_t {
    CODE_LO = (uint32_t)MEM_LOAD_SEG * 16,  // the load image's code
    CODE_HI = (uint32_t)MEM_DS_SEG * 16,
    STACK_LO = (uint32_t)MEM_SS_SEG * 16,   // the original's stack
    STACK_HI = (uint32_t)MEM_HEAP_SEG * 16,
    HEAP_HI = (uint32_t)MEM_VIDEO_SEG * 16,
    RET_SEG = 0xF000,                        // far return trampoline F000:0000
    RET_LINEAR = RET_SEG * 16,
    NEAR_RET_IP = 0xFFFF,                    // near return: CS:FFFF
    TOP_SP = 0x1380,                         // below the startup SP (1388h)
};

static uc_engine *uc;
static uint8_t *code_image;  // relocated load image up to DS
static int failures, checks;

// ===========================================================================
// Small helpers
// ===========================================================================

static uint16_t ghidra_seg(uint16_t real) { return (uint16_t)(real - MEM_LOAD_SEG + 0x1000); }

static uint16_t rd(int reg)
{
    uint16_t v = 0;
    uc_reg_read(uc, reg, &v);
    return v;
}

static void wr(int reg, uint16_t v) { uc_reg_write(uc, reg, &v); }

static uint32_t rd_flags(void)
{
    uint32_t v = 0;
    uc_reg_read(uc, UC_X86_REG_EFLAGS, &v);
    return v;
}

static void set_flag(uint32_t mask, bool on)
{
    uint32_t f = rd_flags();
    f = on ? f | mask : f & ~mask;
    uc_reg_write(uc, UC_X86_REG_EFLAGS, &f);
}

enum : uint32_t { FLAG_CF = 1u << 0, FLAG_ZF = 1u << 6 };

// ===========================================================================
// Random numbers (splitmix64)
// ===========================================================================

static uint64_t mix64(uint64_t *s)
{
    uint64_t z = (*s += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

uint32_t diff_rand(DiffRng *r, uint32_t n)
{
    return n ? (uint32_t)(mix64(&r->s) % n) : 0;
}

int32_t diff_range(DiffRng *r, int32_t lo, int32_t hi)
{
    return lo + (int32_t)diff_rand(r, (uint32_t)(hi - lo + 1));
}

bool diff_chance(DiffRng *r, int percent)
{
    return (int)diff_rand(r, 100) < percent;
}

// ===========================================================================
// Prototypes from ghidra/symbols.txt
// ===========================================================================

enum { MAX_PARAMS = 16 };  // Gfx::LoadPic has 14

typedef struct Proto {
    char name[64];       // Unit::Name
    uint16_t seg, off;   // Ghidra numbering
    char decl[384];      // text after the address, without the comment
    bool near;           // comment says near/nested
} Proto;

static Proto *protos;
static int proto_count;

// @unit lines: code segment -> unit name.
enum { MAX_UNITS = 128 };
static struct {
    uint16_t seg;
    char name[32];
} units[MAX_UNITS];
static int unit_count;

// Parsed prototype: stack slot size of each parameter (2 or 4), result size.
typedef struct Call {
    int nparams;
    int slot[MAX_PARAMS];
    int result;    // 0, 1, 2 or 4 bytes
    int kept;      // bytes the callee leaves on the stack (string result pointer)
    char error[160];
} Call;

static bool has_word(const char *text, const char *word)
{
    size_t n = strlen(word);
    for (const char *t = strstr(text, word); t; t = strstr(t + 1, word))
        if ((t == text || !isalpha((unsigned char)t[-1])) && !isalpha((unsigned char)t[n]))
            return true;
    return false;
}

static void load_symbols(void)
{
    const char *path = REUNION_SOURCE_DIR "/ghidra/symbols.txt";
    size_t size;
    char *text = SDL_LoadFile(path, &size);
    if (!text) {
        printf("diff: can't read %s\n", path);
        exit(1);
    }
    int cap = 4096;
    protos = SDL_calloc((size_t)cap, sizeof *protos);
    for (char *line = strtok(text, "\n"); line; line = strtok(nullptr, "\n")) {
        unsigned seg, off;
        char name[64];
        int n = 0;
        if (sscanf(line, " @unit %x %31s", &seg, name) == 2 && unit_count < MAX_UNITS) {
            units[unit_count].seg = (uint16_t)seg;
            SDL_strlcpy(units[unit_count].name, name, sizeof units[unit_count].name);
            unit_count++;
            continue;
        }
        if (sscanf(line, "%4x:%4x %63[A-Za-z0-9_:]%n", &seg, &off, name, &n) != 3)
            continue;
        if (seg == 0x4A81 || proto_count == cap)
            continue;  // data
        Proto *p = &protos[proto_count++];
        p->seg = (uint16_t)seg;
        p->off = (uint16_t)off;
        const char *unit = "";
        for (int i = 0; i < unit_count; i++)
            if (units[i].seg == seg)
                unit = units[i].name;
        if (strstr(name, "::"))
            SDL_strlcpy(p->name, name, sizeof p->name);
        else
            SDL_snprintf(p->name, sizeof p->name, "%s::%s", unit, name);
        const char *rest = line + n;
        const char *comment = strstr(rest, "//");
        size_t len = comment ? (size_t)(comment - rest) : strlen(rest);
        if (len >= sizeof p->decl)
            len = sizeof p->decl - 1;
        memcpy(p->decl, rest, len);
        p->decl[len] = '\0';
        p->near = comment && (has_word(comment, "near") || has_word(comment, "nested"));
    }
    SDL_free(text);
}

// The last entry wins (merge_proposals.py appends corrections).
static const Proto *find_proto(const char *name)
{
    char want[64];
    unsigned seg, off;
    if (sscanf(name, "%4x:%4x", &seg, &off) == 2) {
        for (int i = proto_count - 1; i >= 0; i--)
            if (protos[i].seg == seg && protos[i].off == off)
                return &protos[i];
        return nullptr;
    }
    if (strstr(name, "::")) {
        SDL_strlcpy(want, name, sizeof want);
    } else {
        const char *u = strchr(name, '_');
        if (!u)
            return nullptr;
        SDL_snprintf(want, sizeof want, "%.*s::%s", (int)(u - name), name, u + 1);
    }
    for (int i = proto_count - 1; i >= 0; i--)
        if (SDL_strcasecmp(protos[i].name, want) == 0)
            return &protos[i];
    return nullptr;
}

static void trim(char *s)
{
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1]))
        *--e = '\0';
    char *b = s;
    while (isspace((unsigned char)*b))
        b++;
    memmove(s, b, strlen(b) + 1);
}

static int record_size(const char *name)
{
    for (int i = 0; i < diff_record_count; i++)
        if (SDL_strcasecmp(diff_records[i].name, name) == 0)
            return diff_records[i].size;
    return -1;
}

// Bytes of a Pascal type, -1 if unknown; *pstr for strings.
static int type_size(const char *t, bool *pstr)
{
    *pstr = false;
    static const struct { const char *n; int size; } scalars[] = {
        {"byte", 1}, {"shortint", 1}, {"char", 1}, {"boolean", 1}, {"word", 2}, {"integer", 2},
        {"longint", 4}, {"pointer", 4}, {"real", 6},
    };
    if (t[0] == '^')
        return 4;
    if (SDL_strncasecmp(t, "string", 6) == 0) {
        *pstr = true;
        return 256;
    }
    for (size_t i = 0; i < SDL_arraysize(scalars); i++)
        if (SDL_strcasecmp(t, scalars[i].n) == 0)
            return scalars[i].size;
    if (SDL_strncasecmp(t, "array", 5) == 0)
        return 1000;  // passed by reference whatever its size
    return record_size(t);
}

static bool parse_call(const Proto *p, Call *c)
{
    memset(c, 0, sizeof *c);
    char decl[384];
    SDL_strlcpy(decl, p->decl, sizeof decl);
    if (strchr(decl, '@')) {
        SDL_snprintf(c->error, sizeof c->error, "register arguments/results (@) are not supported");
        return false;
    }
    char *params = nullptr, *result = decl;
    char *open = strchr(decl, '(');
    if (open) {
        char *close = strrchr(decl, ')');
        if (!close) {
            SDL_snprintf(c->error, sizeof c->error, "unbalanced parentheses");
            return false;
        }
        *close = '\0';
        params = open + 1;
        result = close + 1;
    }
    trim(result);
    bool string_result = false;
    if (result[0] == ':') {
        char *t = result + 1;
        trim(t);
        bool pstr;
        int size = type_size(t, &pstr);
        if (pstr) {
            string_result = true;  // hidden first parameter, left on the stack
        } else if (size == 1 || size == 2 || size == 4) {
            c->result = size;
        } else {
            SDL_snprintf(c->error, sizeof c->error, "result type '%s' not supported", t);
            return false;
        }
    }
    if (string_result) {
        c->slot[c->nparams++] = 4;
        c->kept = 4;
    }
    if (params) {
        for (char *group = strtok(params, ";"); group; group = strtok(nullptr, ";")) {
            trim(group);
            if (!*group)
                continue;
            bool by_ref = false;
            if (SDL_strncasecmp(group, "var ", 4) == 0 || SDL_strncasecmp(group, "const ", 6) == 0) {
                by_ref = true;
                group = strchr(group, ' ') + 1;
            }
            char *colon = strchr(group, ':');
            int slot;
            if (!colon) {
                if (!by_ref) {
                    SDL_snprintf(c->error, sizeof c->error, "parameter without type: %s", group);
                    return false;
                }
                slot = 4;
                colon = group + strlen(group);
            } else {
                *colon = '\0';
                char *t = colon + 1;
                trim(t);
                bool pstr;
                int size = type_size(t, &pstr);
                if (size < 0) {
                    SDL_snprintf(c->error, sizeof c->error, "unknown parameter type '%s'", t);
                    return false;
                }
                if (size == 6 && !pstr && !by_ref) {
                    SDL_snprintf(c->error, sizeof c->error, "real parameters are not supported");
                    return false;
                }
                slot = by_ref || pstr || size > 4 ? 4 : size == 4 ? 4 : 2;
            }
            // One slot per name in "a, b: type".
            int names = 1;
            for (char *q = group; *q; q++)
                names += *q == ',';
            for (int i = 0; i < names; i++) {
                if (c->nparams == MAX_PARAMS) {
                    SDL_snprintf(c->error, sizeof c->error, "too many parameters");
                    return false;
                }
                c->slot[c->nparams++] = slot;
            }
        }
    }
    return true;
}

// ===========================================================================
// State
// ===========================================================================

struct DiffState {
    uint8_t *mem;
    uint8_t *emm;
    uint8_t palette[256][3];
};

DiffState *diff_save(void)
{
    DiffState *s = SDL_calloc(1, sizeof *s);
    s->mem = SDL_malloc(MEM_SIZE);
    s->emm = SDL_malloc(emm_state_size());
    memcpy(s->mem, mem, MEM_SIZE);
    emm_state_save(s->emm);
    memcpy(s->palette, vid_palette, sizeof s->palette);
    return s;
}

void diff_load(const DiffState *s)
{
    memcpy(mem, s->mem, MEM_SIZE);
    emm_state_load(s->emm);
    memcpy(vid_palette, s->palette, sizeof s->palette);
}

void diff_free(DiffState *s)
{
    if (s) {
        SDL_free(s->mem);
        SDL_free(s->emm);
        SDL_free(s);
    }
}

// ===========================================================================
// Booting the port
// ===========================================================================

// An autopilot script that only keeps retraces unpaced.
static const char IDLE_SCRIPT[] = "2000000000:quit";

static jmp_buf boot_jmp;
static int boot_halt;

[[noreturn]] static void boot_on_halt(uint8_t code)
{
    boot_halt = code;
    longjmp(boot_jmp, 1);
}

[[noreturn]] static void boot_on_quit(void)
{
    boot_halt = -1;
    longjmp(boot_jmp, 1);
}

bool diff_boot_new_game(int frames)
{
    char script[128];
    SDL_snprintf(script, sizeof script, "%d:quit", frames);
    return diff_boot(script);
}

bool diff_boot(const char *then)
{
    char script[1024];
    SDL_snprintf(script, sizeof script, "40:click:160,60;120:click:80,100;%s", then);
    audio_shutdown();
    input_reset();
    autopilot_set_script(script);
    boot_halt = -2;
    system_exit_handler = boot_on_halt;
    timing_set_quit_handler(boot_on_quit);
    if (setjmp(boot_jmp) == 0) {
        mem_init();
        if (!dseg_init())
            return false;
        system_init();
        Game_InitUnits();
        system_add_exit_proc(far_ptr(0x405F, 0x1A62), Gfx_ExitHandler);
        Game_Main();
    }
    system_exit_handler = nullptr;
    timing_set_quit_handler(nullptr);
    input_reset();
    // Keep the autopilot on (no real-time pacing), with nothing more to do.
    autopilot_set_script(IDLE_SCRIPT);
    if (boot_halt != -1) {
        printf("diff: boot ended with %d instead of the quit\n", boot_halt);
        return false;
    }
    diff_quiesce();
    return true;
}

void diff_quiesce(void)
{
    ds.Sound_Enabled = 0;
    ds.Game_SoundFx = 0;
    input_reset();
    input_set_mouse(ds.Mouse_X, ds.Mouse_Y, 0, 0);
    Mouse_Update();
}

// ===========================================================================
// Names for the reports
// ===========================================================================

typedef struct Region {
    char label[48];
    uint32_t lo, hi;
    int rec;
} Region;

enum { MAX_REGIONS = 32 };
static Region user_regions[MAX_REGIONS], auto_regions[8];
static int user_region_count, auto_region_count;

static int record_index(const char *name)
{
    if (!name)
        return -1;
    for (int i = 0; i < diff_record_count; i++)
        if (SDL_strcasecmp(diff_records[i].name, name) == 0)
            return i;
    printf("diff: unknown record '%s'\n", name);
    return -1;
}

static void add_region(Region *r, const char *label, FarPtr base, uint32_t size, const char *record)
{
    SDL_strlcpy(r->label, label, sizeof r->label);
    r->lo = far_linear(base);
    r->hi = r->lo + size;
    r->rec = record_index(record);
}

void diff_name_region(const char *label, FarPtr base, uint32_t size, const char *record)
{
    if (user_region_count < MAX_REGIONS)
        add_region(&user_regions[user_region_count++], label, base, size, record);
}

// The game data block (Ems_EmsMapGameData): Groups, then Buildings, then the
// charset; the message log.
static void auto_name_regions(void)
{
    auto_region_count = 0;
    if (ds.Game_Groups)
        add_region(&auto_regions[auto_region_count++], "Game_Groups^", ds.Game_Groups, 0x2840, "TGroup");
    if (ds.Game_Buildings)
        add_region(&auto_regions[auto_region_count++], "Game_Buildings^", ds.Game_Buildings,
                   0x5EF0 - 0x2840, "TBuilding");
    if (ds.Message_Log)
        add_region(&auto_regions[auto_region_count++], "Message_Log^", ds.Message_Log, 0x31B, nullptr);
}

static int put(char *buf, int size, int at, const char *fmt, ...)
{
    if (at >= size)
        return at;
    va_list ap;
    va_start(ap, fmt);
    int n = SDL_vsnprintf(buf + at, (size_t)(size - at), fmt, ap);
    va_end(ap);
    return n < 0 ? at : at + n;
}

static int put_index(char *buf, int size, int at, const DiffName *f, uint32_t idx)
{
    if (f->count <= 1)
        return at;
    uint32_t d1 = f->dims[1], d2 = f->dims[2];
    at = put(buf, size, at, "[%u]", (unsigned)(idx / (d1 * d2)));
    if (d1 > 1 || d2 > 1)
        at = put(buf, size, at, "[%u]", (unsigned)(idx / d2 % d1));
    if (d2 > 1)
        at = put(buf, size, at, "[%u]", (unsigned)(idx % d2));
    return at;
}

static int describe_record(char *buf, int size, int at, int rec, uint32_t off);

// Name of byte `off` inside field f (relative to its start).
static int describe_field(char *buf, int size, int at, const DiffName *f, uint32_t off)
{
    uint32_t idx = off / f->elem, rem = off % f->elem;
    at = put(buf, size, at, "%s", f->name);
    at = put_index(buf, size, at, f, idx);
    if (f->rec >= 0)
        return describe_record(buf, size, at, f->rec, rem);
    if (rem)
        at = put(buf, size, at, "+%u", (unsigned)rem);
    return at;
}

static int describe_record(char *buf, int size, int at, int rec, uint32_t off)
{
    const DiffRecord *r = &diff_records[rec];
    for (int i = 0; i < r->nfields; i++) {
        const DiffName *f = &r->fields[i];
        if (off >= f->ofs && off < f->ofs + f->elem * f->count) {
            at = put(buf, size, at, ".");
            return describe_field(buf, size, at, f, off - f->ofs);
        }
    }
    return put(buf, size, at, "+%u", (unsigned)off);
}

static bool describe_region(const Region *r, uint32_t lin, char *buf, int size, int at)
{
    if (lin < r->lo || lin >= r->hi)
        return false;
    uint32_t off = lin - r->lo;
    at = put(buf, size, at, " %s", r->label);
    if (r->rec >= 0) {
        uint32_t rs = diff_records[r->rec].size;
        at = put(buf, size, at, "[%u]", (unsigned)(off / rs));
        describe_record(buf, size, at, r->rec, off % rs);
    } else {
        put(buf, size, at, "+%X", (unsigned)off);
    }
    return true;
}

// true if the name is a field (of DS or of a named region).
static bool describe(uint32_t lin, char *buf, int size)
{
    buf[0] = '\0';
    uint32_t ds_lin = (uint32_t)MEM_DS_SEG * 16;
    if (lin >= ds_lin && lin < ds_lin + DSEG_SIZE) {
        uint16_t off = (uint16_t)(lin - ds_lin);
        int at = put(buf, size, 0, "DS:%04X", off);
        int lo = 0, hi = diff_ds_name_count - 1, found = -1;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            if (diff_ds_names[mid].ofs <= off) {
                found = mid;
                lo = mid + 1;
            } else {
                hi = mid - 1;
            }
        }
        if (found >= 0) {
            const DiffName *f = &diff_ds_names[found];
            if (off < f->ofs + f->elem * f->count) {
                at = put(buf, size, at, " ");
                describe_field(buf, size, at, f, off - f->ofs);
                return true;
            }
        }
        return false;
    }
    int at;
    if (lin >= CODE_LO && lin < CODE_HI) {
        // Code: Ghidra seg:off in the unit's segment.
        uint32_t g = lin - CODE_LO + 0x10000;
        int best = -1;
        for (int i = 0; i < unit_count; i++)
            if ((uint32_t)units[i].seg * 16 <= g && (best < 0 || units[i].seg > units[best].seg))
                best = i;
        if (best >= 0)
            at = put(buf, size, 0, "code %04x:%04x (%s)", units[best].seg,
                     (unsigned)(g - units[best].seg * 16u), units[best].name);
        else
            at = put(buf, size, 0, "code (Ghidra linear %05X)", (unsigned)g);
    } else if (lin >= HEAP_HI && lin < HEAP_HI + 64000) {
        at = put(buf, size, 0, "video x=%u y=%u", (unsigned)((lin - HEAP_HI) % 320),
                 (unsigned)((lin - HEAP_HI) / 320));
    } else {
        at = put(buf, size, 0, "%04X:%04X", (unsigned)(lin >> 4 & 0xF000), (unsigned)(lin & 0xFFFF));
        if (lin >= (uint32_t)EMM_FRAME_SEG * 16 && lin < (uint32_t)EMM_FRAME_SEG * 16 + 0x10000)
            at = put(buf, size, at, " EMS frame");
        else if (lin >= STACK_HI && lin < HEAP_HI)
            at = put(buf, size, at, " heap");
    }
    for (int i = 0; i < user_region_count; i++)
        if (describe_region(&user_regions[i], lin, buf, size, at))
            return true;
    for (int i = 0; i < auto_region_count; i++)
        if (describe_region(&auto_regions[i], lin, buf, size, at))
            return true;
    return false;
}

void diff_describe(uint32_t lin, char *buf, int size)
{
    describe(lin, buf, size);
}

// ===========================================================================
// The emulated machine
// ===========================================================================

typedef enum { OUT_RETURN, OUT_RUNERROR, OUT_HALT, OUT_ERROR } OutKind;

typedef struct Outcome {
    OutKind kind;
    uint16_t code;     // run-time error / exit code
    uint32_t result;   // DX:AX
    char msg[320];
} Outcome;

static Outcome emu;
static uint8_t dac[256][3];
static uint8_t dac_widx, dac_wcomp, dac_ridx, dac_rcomp, retrace;
static bool allowed_port[0x10000];
static FILE *files[32];

void diff_allow_port(uint16_t port)
{
    allowed_port[port] = true;
}

// Where the CPU is, in Ghidra numbering.
static void where(char *buf, size_t size, int back)
{
    uint16_t cs = rd(UC_X86_REG_CS), ip = rd(UC_X86_REG_IP);
    SDL_snprintf(buf, size, "%04x:%04x", ghidra_seg(cs), (uint16_t)(ip - back));
}

static bool in_int;  // emu_fail called from an interrupt handler

static void emu_fail(const char *fmt, ...)
{
    if (emu.kind == OUT_ERROR)
        return;
    emu.kind = OUT_ERROR;
    char at[16];
    where(at, sizeof at, in_int ? 2 : 0);  // the INT instruction, not the one after
    int n = SDL_snprintf(emu.msg, sizeof emu.msg, "at %s: ", at);
    va_list ap;
    va_start(ap, fmt);
    SDL_vsnprintf(emu.msg + n, sizeof emu.msg - (size_t)n, fmt, ap);
    va_end(ap);
    uc_emu_stop(uc);
}

static uint8_t *seg_ptr(int seg_reg, int off_reg)
{
    return far_host(far_ptr(rd(seg_reg), rd(off_reg)));
}

// --- INT 21h: the file functions the run-time library uses -------------------

static void int21(void)
{
    uint16_t ax = rd(UC_X86_REG_AX), bx = rd(UC_X86_REG_BX), cx = rd(UC_X86_REG_CX);
    uint16_t dx = rd(UC_X86_REG_DX);
    FILE *f = bx < SDL_arraysize(files) ? files[bx] : nullptr;
    switch (ax >> 8) {
    case 0x3D: {  // open
        char path[128];
        SDL_strlcpy(path, (const char *)seg_ptr(UC_X86_REG_DS, UC_X86_REG_DX), sizeof path);
        char *host = fs_resolve(path);
        FILE *nf = host ? fopen(host, "rb") : nullptr;
        SDL_free(host);
        int h = 5;
        while (h < (int)SDL_arraysize(files) && files[h])
            h++;
        if (!nf || h == (int)SDL_arraysize(files)) {
            if (nf)
                fclose(nf);
            wr(UC_X86_REG_AX, 2);
            set_flag(FLAG_CF, true);
            return;
        }
        files[h] = nf;
        wr(UC_X86_REG_AX, (uint16_t)h);
        set_flag(FLAG_CF, false);
        return;
    }
    case 0x3E:  // close
        if (f) {
            fclose(f);
            files[bx] = nullptr;
        }
        set_flag(FLAG_CF, false);
        return;
    case 0x3F:  // read
        if (!f) {
            wr(UC_X86_REG_AX, 6);
            set_flag(FLAG_CF, true);
            return;
        }
        wr(UC_X86_REG_AX, (uint16_t)fread(seg_ptr(UC_X86_REG_DS, UC_X86_REG_DX), 1, cx, f));
        set_flag(FLAG_CF, false);
        return;
    case 0x40:  // write: only to the standard handles, dropped
        if (bx > 2) {
            emu_fail("INT 21h write to handle %u", bx);
            return;
        }
        wr(UC_X86_REG_AX, cx);
        set_flag(FLAG_CF, false);
        return;
    case 0x42: {  // seek
        if (!f) {
            wr(UC_X86_REG_AX, 6);
            set_flag(FLAG_CF, true);
            return;
        }
        int32_t ofs = (int32_t)((uint32_t)cx << 16 | dx);
        int whence = (ax & 0xFF) == 0 ? SEEK_SET : (ax & 0xFF) == 1 ? SEEK_CUR : SEEK_END;
        fseek(f, ofs, whence);
        long pos = ftell(f);
        wr(UC_X86_REG_AX, (uint16_t)pos);
        wr(UC_X86_REG_DX, (uint16_t)(pos >> 16));
        set_flag(FLAG_CF, false);
        return;
    }
    case 0x44:  // IOCTL get device info: files are on disk, 0..4 are devices
        if ((ax & 0xFF) == 0) {
            wr(UC_X86_REG_DX, bx < 5 ? 0x80D3 : 0x0002);
            set_flag(FLAG_CF, false);
            return;
        }
        break;
    default:
        break;
    }
    emu_fail("unhandled INT 21h AX=%04X", ax);
}

// --- INT 16h / 33h: the port's input layer answers ----------------------------

static void int16(void)
{
    uint16_t ax = rd(UC_X86_REG_AX);
    switch (ax >> 8) {
    case 0x00:
    case 0x10: {
        int k = input_read_key();
        if (k < 0) {
            emu_fail("INT 16h waits for a key and none is queued");
            return;
        }
        if (k == 0) {  // extended: the scan code is the next entry (SDL_max would read it twice)
            int scan = input_read_key();
            wr(UC_X86_REG_AX, (uint16_t)((scan < 0 ? 0 : scan) << 8));
        } else
            wr(UC_X86_REG_AX, (uint16_t)((k == 13 ? 0x1C : 0) << 8 | k));
        return;
    }
    case 0x01:
    case 0x11:
        set_flag(FLAG_ZF, !input_key_pressed());
        return;
    case 0x02:
        wr(UC_X86_REG_AX, ax & 0xFF00);
        return;
    default:
        emu_fail("unhandled INT 16h AX=%04X", ax);
    }
}

static void int33(void)
{
    uint16_t ax = rd(UC_X86_REG_AX);
    InputMouse m = input_mouse();
    switch (ax) {
    case 0x00:  // reset: present, 2 buttons
        wr(UC_X86_REG_AX, 0xFFFF);
        wr(UC_X86_REG_BX, 2);
        return;
    case 0x01: case 0x02: case 0x04: case 0x07: case 0x08: case 0x0C: case 0x0F:
        return;  // cursor, position, range, callback, mickeys: the port ignores them too
    case 0x03:
        wr(UC_X86_REG_BX, (uint16_t)(m.left | m.right << 1));
        wr(UC_X86_REG_CX, (uint16_t)m.x);
        wr(UC_X86_REG_DX, (uint16_t)m.y);
        return;
    default:
        emu_fail("unhandled INT 33h AX=%04X", ax);
    }
}

// --- INT 10h: set mode, as the port models it (Gfx::SetVideoMode13/SetTextMode)

static void int10(void)
{
    uint16_t ax = rd(UC_X86_REG_AX);
    if (ax == 0x0013) {  // mode 13h clears the 64 KB at A000h
        memset(mem + (uint32_t)MEM_VIDEO_SEG * 16, 0, 0x10000);
        return;
    }
    if (ax == 0x0003)  // text mode: the port has no text screen
        return;
    emu_fail("unhandled INT 10h AX=%04X", ax);
}

static void int67(void)
{
    Registers r = {
        .AX = rd(UC_X86_REG_AX), .BX = rd(UC_X86_REG_BX), .CX = rd(UC_X86_REG_CX),
        .DX = rd(UC_X86_REG_DX), .BP = rd(UC_X86_REG_BP), .SI = rd(UC_X86_REG_SI),
        .DI = rd(UC_X86_REG_DI), .DS = rd(UC_X86_REG_DS), .ES = rd(UC_X86_REG_ES),
    };
    emm_int67(&r);
    wr(UC_X86_REG_AX, r.AX);
    wr(UC_X86_REG_BX, r.BX);
    wr(UC_X86_REG_CX, r.CX);
    wr(UC_X86_REG_DX, r.DX);
    wr(UC_X86_REG_SI, r.SI);
    wr(UC_X86_REG_DI, r.DI);
}

static void on_intr(uc_engine *u, uint32_t intno, void *user)
{
    (void)u;
    (void)user;
    in_int = intno != 0;
    switch (intno) {
    case 0x00:  // divide error: the RTL's handler reports run-time error 200
        if (emu.kind == OUT_RETURN) {
            emu.kind = OUT_RUNERROR;
            emu.code = 200;
        }
        uc_emu_stop(uc);
        break;
    case 0x10: int10(); break;
    case 0x16: int16(); break;
    case 0x21: int21(); break;
    case 0x33: int33(); break;
    case 0x67: int67(); break;
    default:
        emu_fail("unhandled INT %02Xh (AX=%04X)", intno, rd(UC_X86_REG_AX));
        break;
    }
    in_int = false;
}

// --- Port I/O: the VGA DAC and retrace -----------------------------------------

static uint8_t in8(uint16_t port)
{
    switch (port) {
    case 0x3DA:  // alternately in and out of vertical retrace
        retrace ^= 9;
        return retrace;
    case 0x3C9: {
        uint8_t v = dac[dac_ridx][dac_rcomp];
        if (++dac_rcomp == 3) {
            dac_rcomp = 0;
            dac_ridx++;
        }
        return v;
    }
    case 0x3C8:
        return dac_widx;
    default:
        if (!allowed_port[port])
            emu_fail("unhandled IN from port %03Xh", port);
        return 0;
    }
}

static void out8(uint16_t port, uint8_t v)
{
    switch (port) {
    case 0x3C8:
        dac_widx = v;
        dac_wcomp = 0;
        return;
    case 0x3C7:
        dac_ridx = v;
        dac_rcomp = 0;
        return;
    case 0x3C9:
        dac[dac_widx][dac_wcomp] = v & 63;
        if (++dac_wcomp == 3) {
            dac_wcomp = 0;
            dac_widx++;
        }
        return;
    case 0x20:  // PIC end of interrupt
        return;
    default:
        if (!allowed_port[port])
            emu_fail("unhandled OUT %02Xh to port %03Xh", v, port);
    }
}

static uint32_t on_in(uc_engine *u, uint32_t port, int size, void *user)
{
    (void)u;
    (void)user;
    uint32_t v = in8((uint16_t)port);
    if (size == 2)
        v |= (uint32_t)in8((uint16_t)(port + 1)) << 8;
    return v;
}

static void on_out(uc_engine *u, uint32_t port, int size, uint32_t value, void *user)
{
    (void)u;
    (void)user;
    out8((uint16_t)port, (uint8_t)value);
    if (size == 2)
        out8((uint16_t)(port + 1), (uint8_t)(value >> 8));
}

// --- Termination and runaway code ---------------------------------------------

static const uint32_t RUNERROR_LINEAR = (uint32_t)(0x48C7 - 0x1000 + MEM_LOAD_SEG) * 16 + 0x010F;
static const uint32_t HALT_LINEAR = (uint32_t)(0x48C7 - 0x1000 + MEM_LOAD_SEG) * 16 + 0x0116;

static void on_exit_code(uc_engine *u, uint64_t address, uint32_t size, void *user)
{
    (void)u;
    (void)size;
    (void)user;
    if (emu.kind != OUT_RETURN)
        return;
    emu.kind = address == RUNERROR_LINEAR ? OUT_RUNERROR : OUT_HALT;
    emu.code = rd(UC_X86_REG_AX);
    uc_emu_stop(uc);
}

static void on_wild_code(uc_engine *u, uint64_t address, uint32_t size, void *user)
{
    (void)u;
    (void)size;
    (void)user;
    emu_fail("executing outside the program at linear %05X", (unsigned)address);
}

static bool on_unmapped(uc_engine *u, uc_mem_type type, uint64_t address, int size, int64_t value,
                        void *user)
{
    (void)u;
    (void)type;
    (void)value;
    (void)user;
    emu_fail("access to unmapped address %X (%d bytes)", (unsigned)address, size);
    return false;
}

// uc_hook_add takes callbacks as void *; ISO C has no such conversion.
static void *hook_ptr(void (*fn)(void))
{
    void *p;
    static_assert(sizeof p == sizeof fn);
    memcpy(&p, &fn, sizeof p);
    return p;
}
#define HOOK(fn) hook_ptr((void (*)(void))(fn))

// REUNION_DIFF_COVERAGE: which bytes of the original's code ran.
static uint8_t *exec_bits;

static void on_block(uc_engine *u, uint64_t address, uint32_t size, void *user)
{
    (void)u;
    (void)user;
    for (uint64_t a = address; a < address + size && a < CODE_HI; a++)
        exec_bits[(a - CODE_LO) >> 3] |= (uint8_t)(1 << ((a - CODE_LO) & 7));
}

static bool executed(uint32_t lin)
{
    return exec_bits[(lin - CODE_LO) >> 3] >> ((lin - CODE_LO) & 7) & 1;
}

// The CPU as opened. Unicorn hands exceptions to the INTR hook without
// finishing their delivery, so the CPU keeps the divide error as its "previous
// exception": the next one becomes a double fault (INT 08h), the one after a
// triple fault (a hang). Each run starts from this context instead.
static uc_context *fresh_cpu;

static bool open_unicorn(void)
{
    if (uc_open(UC_ARCH_X86, UC_MODE_16, &uc) != UC_ERR_OK)
        return false;
    if (uc_context_alloc(uc, &fresh_cpu) != UC_ERR_OK || uc_context_save(uc, fresh_cpu) != UC_ERR_OK)
        return false;
    if (uc_mem_map_ptr(uc, 0, MEM_SIZE, UC_PROT_ALL, mem) != UC_ERR_OK)
        return false;
    uc_hook h;
    uc_hook_add(uc, &h, UC_HOOK_INTR, HOOK(on_intr), nullptr, 1, 0);
    uc_hook_add(uc, &h, UC_HOOK_INSN, HOOK(on_in), nullptr, 1, 0, UC_X86_INS_IN);
    uc_hook_add(uc, &h, UC_HOOK_INSN, HOOK(on_out), nullptr, 1, 0, UC_X86_INS_OUT);
    uc_hook_add(uc, &h, UC_HOOK_CODE, HOOK(on_exit_code), nullptr, RUNERROR_LINEAR, RUNERROR_LINEAR);
    uc_hook_add(uc, &h, UC_HOOK_CODE, HOOK(on_exit_code), nullptr, HALT_LINEAR, HALT_LINEAR);
    uc_hook_add(uc, &h, UC_HOOK_CODE, HOOK(on_wild_code), nullptr, 0, CODE_LO - 1);
    uc_hook_add(uc, &h, UC_HOOK_CODE, HOOK(on_wild_code), nullptr, CODE_HI, RET_LINEAR - 1);
    uc_hook_add(uc, &h, UC_HOOK_CODE, HOOK(on_wild_code), nullptr, RET_LINEAR + 1, MEM_SIZE - 1);
    uc_hook_add(uc, &h, UC_HOOK_MEM_UNMAPPED, HOOK(on_unmapped), nullptr, 1, 0);
    if (SDL_getenv("REUNION_DIFF_COVERAGE")) {
        exec_bits = SDL_calloc(1, (CODE_HI - CODE_LO + 7) / 8);
        uc_hook_add(uc, &h, UC_HOOK_BLOCK, HOOK(on_block), nullptr, CODE_LO, CODE_HI - 1);
    }
    return true;
}

// ===========================================================================
// Far code pointers: the port stores procedure variables in Ghidra numbering
// (far_ptr(0x4758, 0x012D)); the original needs the loaded segment.
// ===========================================================================

enum { MAX_CODE_PTRS = 64 };
static uint16_t code_ptr_fields[MAX_CODE_PTRS] = {
    offsetof(DSeg, Blit_CursorHook),       offsetof(DSeg, Game_FindPlanetForceProc),
    offsetof(DSeg, Ems_RestoreProc),       offsetof(DSeg, Game_GrWarProc),
    offsetof(DSeg, Game_GfxPathProc),      offsetof(DSeg, Groups_UpdateSatMenu),
    offsetof(DSeg, Groups_IntelHook),      offsetof(DSeg, Game_FindBuildingProc),
    offsetof(DSeg, System_ExitProc),
};
static int code_ptr_count = 9;

void diff_code_ptr(uint16_t ds_offset)
{
    for (int i = 0; i < code_ptr_count; i++)
        if (code_ptr_fields[i] == ds_offset)
            return;
    if (code_ptr_count < MAX_CODE_PTRS)
        code_ptr_fields[code_ptr_count++] = ds_offset;
}

static void convert_code_ptrs(bool to_loaded)
{
    for (int i = 0; i < code_ptr_count; i++) {
        uint16_t *seg = (uint16_t *)ds_at((uint16_t)(code_ptr_fields[i] + 2));
        if (to_loaded && *seg >= 0x1000 && *seg < 0x4A81)
            *seg = (uint16_t)(*seg - 0x1000 + MEM_LOAD_SEG);
        else if (!to_loaded && *seg >= MEM_LOAD_SEG && *seg < MEM_DS_SEG)
            *seg = ghidra_seg(*seg);
    }
}

// ===========================================================================
// Running both sides
// ===========================================================================

static double timeout_s = 10;
static uint16_t mouse_x, mouse_y;

enum { QUEUED_KEYS = 32 };

// diff_set_keys: the queue instead of the Enters (key_count < 0: the Enters).
static int key_queue[QUEUED_KEYS * 2], key_count = -1;

void diff_set_keys(const int *keys, int n)
{
    key_count = n < 0 ? -1 : SDL_min(n, (int)SDL_arraysize(key_queue));
    for (int i = 0; i < key_count; i++)
        key_queue[i] = keys[i];
}

// The same input for both sides: mouse still, no buttons, a queue of Enters
// (or the keys given to diff_set_keys).
static void prepare_input(void)
{
    input_reset();
    input_set_mouse(mouse_x, mouse_y, 0, 0);
    if (key_count >= 0) {
        for (int i = 0; i < key_count; i++)
            input_push_key(key_queue[i]);
        return;
    }
    for (int i = 0; i < QUEUED_KEYS; i++)
        input_push_key(13);
}

static int keys_left(void)
{
    int n = 0;
    while (input_read_key() >= 0)
        n++;
    return n;
}

static void push16(uint16_t *sp, uint16_t v)
{
    *sp = (uint16_t)(*sp - 2);
    uint8_t *p = mem + (uint32_t)MEM_SS_SEG * 16 + *sp;
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void run_original(const Proto *p, const Call *c, bool near, const int32_t *args)
{
    memset(&emu, 0, sizeof emu);
    memcpy(dac, vid_palette, sizeof dac);
    for (int i = 0; i < 256; i++)
        for (int j = 0; j < 3; j++)
            dac[i][j] &= 63;
    dac_widx = dac_wcomp = dac_ridx = dac_rcomp = retrace = 0;

    uc_context_restore(uc, fresh_cpu);
    uint16_t cs = (uint16_t)(p->seg - 0x1000 + MEM_LOAD_SEG);
    uint16_t sp = TOP_SP;
    for (int i = 0; i < c->nparams; i++) {
        if (c->slot[i] == 4)
            push16(&sp, (uint16_t)((uint32_t)args[i] >> 16));
        push16(&sp, (uint16_t)args[i]);
    }
    if (!near)
        push16(&sp, RET_SEG);
    push16(&sp, near ? NEAR_RET_IP : 0);

    static const int zero_regs[] = {UC_X86_REG_AX, UC_X86_REG_BX, UC_X86_REG_CX, UC_X86_REG_DX,
                                    UC_X86_REG_SI, UC_X86_REG_DI, UC_X86_REG_BP};
    for (size_t i = 0; i < SDL_arraysize(zero_regs); i++)
        wr(zero_regs[i], 0);
    wr(UC_X86_REG_SS, MEM_SS_SEG);
    wr(UC_X86_REG_SP, sp);
    wr(UC_X86_REG_DS, MEM_DS_SEG);
    wr(UC_X86_REG_ES, MEM_DS_SEG);
    wr(UC_X86_REG_CS, cs);
    uint32_t flags = 0x0202;
    uc_reg_write(uc, UC_X86_REG_EFLAGS, &flags);

    uint64_t stop = near ? (uint64_t)cs * 16 + NEAR_RET_IP : RET_LINEAR;
    uc_err err = uc_emu_start(uc, (uint64_t)cs * 16 + p->off, stop, (uint64_t)(timeout_s * 1e6), 0);
    for (size_t i = 0; i < SDL_arraysize(files); i++) {
        if (files[i]) {
            fclose(files[i]);
            files[i] = nullptr;
        }
    }
    if (emu.kind != OUT_RETURN)
        return;
    char at[16];
    where(at, sizeof at, 0);
    if (err != UC_ERR_OK) {
        emu.kind = OUT_ERROR;
        SDL_snprintf(emu.msg, sizeof emu.msg, "at %s: unicorn: %s", at, uc_strerror(err));
        return;
    }
    uint16_t ncs = rd(UC_X86_REG_CS), nip = rd(UC_X86_REG_IP);
    bool back = near ? ncs == cs && nip == NEAR_RET_IP : ncs == RET_SEG && nip == 0;
    if (!back) {
        emu.kind = OUT_ERROR;
        SDL_snprintf(emu.msg, sizeof emu.msg, "did not return within %.0f s (at %s)", timeout_s, at);
        return;
    }
    uint16_t want_sp = (uint16_t)(TOP_SP - c->kept);
    if (rd(UC_X86_REG_SP) != want_sp) {
        emu.kind = OUT_ERROR;
        SDL_snprintf(emu.msg, sizeof emu.msg,
                     "returned with SP %04X instead of %04X: wrong prototype or near/far",
                     rd(UC_X86_REG_SP), want_sp);
        return;
    }
    uint32_t ax = rd(UC_X86_REG_AX), dx = rd(UC_X86_REG_DX);
    emu.result = c->result == 1 ? (ax & 0xFF) : c->result == 2 ? ax : c->result == 4 ? (dx << 16 | ax) : 0;
}

static jmp_buf port_jmp;
static Outcome port;

[[noreturn]] static void port_on_runerror(uint16_t code)
{
    port.kind = OUT_RUNERROR;
    port.code = code;
    longjmp(port_jmp, 1);
}

[[noreturn]] static void port_on_halt(uint8_t code)
{
    if (port.kind != OUT_RUNERROR) {
        port.kind = OUT_HALT;
        port.code = code;
    }
    longjmp(port_jmp, 1);
}

// A port routine stuck waiting for input it never gets: the autopilot quits
// after PORT_RETRACES retraces.
enum { PORT_RETRACES = 100000 };

[[noreturn]] static void port_on_quit(void)
{
    port.kind = OUT_ERROR;
    SDL_snprintf(port.msg, sizeof port.msg, "still running after %d retraces (waiting for input?)",
                 PORT_RETRACES);
    longjmp(port_jmp, 1);
}

static void run_port(const DiffFn *fn, const Call *c, const int32_t *args)
{
    memset(&port, 0, sizeof port);
    char script[32];
    SDL_snprintf(script, sizeof script, "%d:quit", PORT_RETRACES);
    autopilot_set_script(script);
    timing_set_quit_handler(port_on_quit);
    system_runerror_handler = port_on_runerror;
    system_exit_handler = port_on_halt;
    if (setjmp(port_jmp) == 0) {
        uint32_t r = fn->port(args);
        port.result = c->result == 1 ? (r & 0xFF) : c->result == 2 ? (r & 0xFFFF) : c->result == 4 ? r : 0;
    }
    system_runerror_handler = nullptr;
    system_exit_handler = nullptr;
    timing_set_quit_handler(nullptr);
    autopilot_set_script(IDLE_SCRIPT);
}

// ===========================================================================
// Comparison
// ===========================================================================

static const char *outcome_text(const Outcome *o, char *buf, size_t size)
{
    switch (o->kind) {
    case OUT_RETURN: SDL_snprintf(buf, size, "returned %X", (unsigned)o->result); break;
    case OUT_RUNERROR: SDL_snprintf(buf, size, "run-time error %u", o->code); break;
    case OUT_HALT: SDL_snprintf(buf, size, "Halt(%u)", o->code); break;
    case OUT_ERROR: SDL_snprintf(buf, size, "harness error: %s", o->msg); break;
    }
    return buf;
}

static bool ignored(uint32_t lin, const DiffFn *fn)
{
    if (lin >= STACK_LO && lin < STACK_HI)
        return true;
    if (fn->ignore)
        for (const DiffRange *r = fn->ignore; r->hi; r++)
            if (lin >= r->lo && lin < r->hi)
                return true;
    return false;
}

enum { MAX_REPORTED = 16 };

// Differing runs printed per mismatch: REUNION_DIFF_RUNS if set, else 16.
static int max_reported(void)
{
    const char *s = SDL_getenv("REUNION_DIFF_RUNS");
    int n = s ? SDL_atoi(s) : 0;
    return n > 0 ? n : MAX_REPORTED;
}

// The name of the field holding `lin` (without "+n"), or "" outside DS and
// the named regions.
static void field_base(uint32_t lin, char *buf, int size)
{
    if (!describe(lin, buf, size)) {
        buf[0] = '\0';
        return;
    }
    char *plus = strrchr(buf, '+');
    char *space = strchr(buf, ' ');
    if (plus && space && plus > space)
        *plus = '\0';
    if (space)
        memmove(buf, space + 1, strlen(space + 1) + 1);  // drop the address
}

static void compare_mem(const DiffFn *fn, const uint8_t *orig)
{
    int runs = 0;
    uint32_t total = 0;
    for (uint32_t a = 0; a < MEM_SIZE; a++) {
        if (orig[a] == mem[a] || ignored(a, fn))
            continue;
        // A run: differing bytes (gaps of one byte bridged), cut where a
        // named field ends.
        char base[160];
        field_base(a, base, sizeof base);
        uint32_t end = a + 1;
        while (end < MEM_SIZE && !ignored(end, fn)
               && (orig[end] != mem[end] || (end + 1 < MEM_SIZE && orig[end + 1] != mem[end + 1]))) {
            if (base[0]) {
                char next[160];
                field_base(end, next, sizeof next);
                if (strcmp(base, next) != 0)
                    break;
            }
            end++;
        }
        while (end > a + 1 && orig[end - 1] == mem[end - 1])
            end--;
        total += end - a;
        if (runs < max_reported()) {
            char name[160];
            diff_describe(a, name, sizeof name);
            printf("    %05X %-48s orig", (unsigned)a, name);
            for (uint32_t i = a; i < end && i < a + 8; i++)
                printf(" %02X", orig[i]);
            printf("%s | port", end - a > 8 ? " .." : "");
            for (uint32_t i = a; i < end && i < a + 8; i++)
                printf(" %02X", mem[i]);
            printf("%s\n", end - a > 8 ? " .." : "");
        }
        runs++;
        a = end;
    }
    if (runs > max_reported())
        printf("    ... %d runs, %u bytes differ in all\n", runs, (unsigned)total);
}

static uint8_t *orig_mem, *orig_emm, *port_emm;
static bool shown;

static void mismatch(const Proto *p, const char *label)
{
    if (!shown)
        printf("MISMATCH %s %s\n", p->name, label);
    shown = true;
}

bool diff_run_original(const char *name, const int32_t *args)
{
    const Proto *p = find_proto(name);
    Call c;
    if (!p || !parse_call(p, &c)) {
        printf("diff_run_original %s: no usable prototype\n", name);
        return false;
    }
    memcpy(mem + CODE_LO, code_image, CODE_HI - CODE_LO);
    mouse_x = (uint16_t)ds.Mouse_X;
    mouse_y = (uint16_t)ds.Mouse_Y;
    prepare_input();
    convert_code_ptrs(true);
    run_original(p, &c, p->near, args);
    convert_code_ptrs(false);
    input_reset();
    if (emu.kind != OUT_RETURN) {
        char buf[400];
        printf("diff_run_original %s: %s\n", name, outcome_text(&emu, buf, sizeof buf));
    }
    return emu.kind == OUT_RETURN;
}

bool diff_open_output(void)
{
    uint8_t *empty = far_host(DIFF_SCRATCH);
    empty[0] = 0;
    FarPtr out = far_ptr(MEM_DS_SEG, offsetof(DSeg, System_Output));
    const int32_t assign[] = {(int32_t)out, (int32_t)DIFF_SCRATCH}, rewrite[] = {(int32_t)out};
    return diff_run_original("System::AssignText", assign) && diff_run_original("System::RewriteText", rewrite);
}

bool diff_call(const DiffFn *fn, const int32_t *args, const char *label)
{
    checks++;
    const Proto *p = find_proto(fn->name);
    Call c;
    if (!p) {
        printf("FAILED %s: not in symbols.txt\n", fn->name);
        failures++;
        return false;
    }
    Proto over;
    if (fn->decl) {  // the test's prototype instead of symbols.txt's
        over = *p;
        SDL_strlcpy(over.decl, fn->decl, sizeof over.decl);
        p = &over;
    }
    if (!parse_call(p, &c)) {
        printf("FAILED %s: prototype '%s': %s\n", p->name, p->decl, c.error);
        failures++;
        return false;
    }
    bool near = fn->near || p->near;

    memcpy(mem + CODE_LO, code_image, CODE_HI - CODE_LO);
    mouse_x = (uint16_t)ds.Mouse_X;
    mouse_y = (uint16_t)ds.Mouse_Y;
    DiffState *before = diff_save();
    auto_name_regions();

    // The original, on mem[] itself.
    prepare_input();
    convert_code_ptrs(true);
    run_original(p, &c, near, args);
    convert_code_ptrs(false);
    int orig_keys = keys_left();
    memcpy(orig_mem, mem, MEM_SIZE);
    emm_state_save(orig_emm);

    // The port, from the same state.
    diff_load(before);
    diff_free(before);
    prepare_input();
    run_port(fn, &c, args);
    int port_keys = keys_left();
    emm_state_save(port_emm);

    bool ok = true;
    char a[400], b[400];
    shown = false;
    if (emu.kind == OUT_ERROR || port.kind == OUT_ERROR) {
        mismatch(p, label);
        if (emu.kind == OUT_ERROR)
            printf("  original: %s\n", outcome_text(&emu, a, sizeof a));
        if (port.kind == OUT_ERROR)
            printf("  port: %s\n", outcome_text(&port, a, sizeof a));
        failures++;
        return false;
    }
    if (emu.kind != port.kind || emu.code != port.code || emu.result != port.result) {
        mismatch(p, label);
        printf("  original %s, port %s\n", outcome_text(&emu, a, sizeof a), outcome_text(&port, b, sizeof b));
        ok = false;
    }
    if (emu.kind != OUT_HALT) {  // after Halt the port has run the ExitProcs
        auto_name_regions();
        bool differ = false;
        for (uint32_t i = 0; i < MEM_SIZE && !differ; i++)
            differ = orig_mem[i] != mem[i] && !ignored(i, fn);
        if (differ) {
            mismatch(p, label);
            printf("  memory (address, name, original | port):\n");
            compare_mem(fn, orig_mem);
            ok = false;
        }
        if (orig_keys != port_keys) {
            mismatch(p, label);
            printf("  keys left in the queue: original %d, port %d\n", orig_keys, port_keys);
            ok = false;
        }
        int pal = 0;
        for (int i = 0; i < 256; i++) {
            if ((dac[i][0] ^ vid_palette[i][0]) & 63 || (dac[i][1] ^ vid_palette[i][1]) & 63
                || (dac[i][2] ^ vid_palette[i][2]) & 63) {
                mismatch(p, label);
                if (pal++ < 4)
                    printf("  DAC[%d]: original %d,%d,%d, port %d,%d,%d\n", i, dac[i][0], dac[i][1],
                           dac[i][2], vid_palette[i][0] & 63, vid_palette[i][1] & 63, vid_palette[i][2] & 63);
                ok = false;
            }
        }
        size_t n = emm_state_size();
        for (size_t i = 0; i < n; i++) {
            if (orig_emm[i] != port_emm[i]) {
                mismatch(p, label);
                printf("  EMS driver state differs from byte %zu (logical page %zu)\n", i,
                       i / EMM_PAGE_SIZE);
                ok = false;
                break;
            }
        }
    }
    if (!ok)
        failures++;
    return ok;
}

static uint64_t name_hash(const char *s)
{
    uint64_t h = 1469598103934665603ull;
    for (; *s; s++)
        h = (h ^ (uint8_t)*s) * 1099511628211ull;
    return h;
}

// The routine's bytes: from its entry to the next symbol of its segment (or
// the next unit). Includes the string constants Pascal puts between routines.
static void report_coverage(const DiffFn *fn)
{
    const Proto *p = find_proto(fn->name);
    if (!p)
        return;
    uint32_t end = 0x10000;
    for (int i = 0; i < proto_count; i++)
        if (protos[i].seg == p->seg && protos[i].off > p->off && protos[i].off < end)
            end = protos[i].off;
    for (int i = 0; i < unit_count; i++)
        if (units[i].seg > p->seg && (uint32_t)(units[i].seg - p->seg) * 16 < end)
            end = (uint32_t)(units[i].seg - p->seg) * 16;
    uint32_t base = (uint32_t)(p->seg - 0x1000 + MEM_LOAD_SEG) * 16;
    uint32_t ran = 0;
    for (uint32_t o = p->off; o < end; o++)
        ran += executed(base + o);
    printf("  coverage: %u of %u bytes of %04x:%04x-%04x ran; not run:", (unsigned)ran,
           (unsigned)(end - p->off), p->seg, p->off, (unsigned)end);
    int shown_ranges = 0;
    for (uint32_t o = p->off; o < end && shown_ranges < 24;) {
        if (executed(base + o)) {
            o++;
            continue;
        }
        uint32_t e = o;
        while (e < end && !executed(base + e))
            e++;
        if (e - o >= 3) {
            printf(" %04x-%04x", (unsigned)o, (unsigned)e);
            shown_ranges++;
        }
        o = e;
    }
    printf("\n");
    memset(exec_bits, 0, (CODE_HI - CODE_LO + 7) / 8);
}

int diff_run(const DiffFn *fn, DiffState *const *bases, int nbases, DiffSetup setup, int n)
{
    const char *only = SDL_getenv("REUNION_DIFF_CASE");
    const char *fns = SDL_getenv("REUNION_DIFF_ONLY");
    if (fns && !SDL_strstr(fn->name, fns))
        return 0;
    if (exec_bits)
        memset(exec_bits, 0, (CODE_HI - CODE_LO + 7) / 8);
    int bad = 0;
    for (int k = 0; k < n; k++) {
        if (only && SDL_atoi(only) != k)
            continue;
        diff_load(bases[k % nbases]);
        DiffRng rng = {name_hash(fn->name) ^ (uint64_t)k * 0x2545F4914F6CDD1Dull};
        int32_t args[MAX_PARAMS] = {0};
        key_count = -1;  // the Enters, unless setup calls diff_set_keys
        if (setup)
            setup(&rng, args);
        char label[48];
        SDL_snprintf(label, sizeof label, "case %d (base state %d)", k, k % nbases);
        if (!diff_call(fn, args, label))
            bad++;
    }
    printf("%-36s %d/%d cases match\n", fn->name, (only ? 1 : n) - bad, only ? 1 : n);
    if (exec_bits)
        report_coverage(fn);
    return bad;
}

// ===========================================================================
// Set-up
// ===========================================================================

int diff_cases(int dflt)
{
    const char *s = SDL_getenv("REUNION_DIFF_CASES");
    return s && SDL_atoi(s) > 0 ? SDL_atoi(s) : dflt;
}

static bool load_image(void)
{
    size_t size;
    uint8_t *exe = fs_load("GRWAR\\REUNION.PRG", &size);
    if (!exe || size < 0x1C || exe[0] != 'M' || exe[1] != 'Z') {
        SDL_free(exe);
        return false;
    }
    size_t hdr = (size_t)(exe[8] | exe[9] << 8) * 16;
    size_t code = CODE_HI - CODE_LO;
    if (hdr + code > size) {
        SDL_free(exe);
        return false;
    }
    code_image = SDL_malloc(code);
    memcpy(code_image, exe + hdr, code);
    // DOS adds the load segment to every word in the relocation table.
    size_t count = exe[6] | exe[7] << 8, table = exe[0x18] | exe[0x19] << 8;
    for (size_t i = 0; i < count && table + i * 4 + 4 <= size; i++) {
        const uint8_t *e = exe + table + i * 4;
        size_t at = (size_t)(e[2] | e[3] << 8) * 16 + (e[0] | e[1] << 8);
        if (at + 2 > code)
            continue;  // DS: dseg_init relocates those
        uint16_t v = (uint16_t)(code_image[at] | code_image[at + 1] << 8) + MEM_LOAD_SEG;
        code_image[at] = (uint8_t)v;
        code_image[at + 1] = (uint8_t)(v >> 8);
    }
    SDL_free(exe);
    return true;
}

bool diff_init(int argc, char *argv[])
{
    setvbuf(stdout, nullptr, _IOLBF, 0);
    Fixes_Original();  // compared with the original: none of the port's fixes
    if (argc < 2 || !fs_init(argv[1]) || !load_image())
        return false;
    mem_init();
    if (!dseg_init())
        return false;
    const char *t = SDL_getenv("REUNION_DIFF_TIMEOUT");
    if (t && SDL_atof(t) > 0)
        timeout_s = SDL_atof(t);
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_EVENTS)) {
        printf("SDL_Init: %s\n", SDL_GetError());
        return false;
    }
    load_symbols();
    if (!open_unicorn()) {
        printf("diff: can't open unicorn\n");
        exit(1);
    }
    orig_mem = SDL_malloc(MEM_SIZE);
    orig_emm = SDL_malloc(emm_state_size());
    port_emm = SDL_malloc(emm_state_size());
    return true;
}

int diff_finish(void)
{
    printf("%d checks, %d mismatches\n", checks, failures);
    fs_shutdown();
    return failures ? 1 : 0;
}
