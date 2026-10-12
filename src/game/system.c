#include "game/system.h"

#include <SDL3/SDL.h>
#include <stdlib.h>

#include "platform/autopilot.h"
#include "platform/video.h"

void (*system_runerror_handler)(uint16_t code) = nullptr;
void (*system_exit_handler)(uint8_t code) = nullptr;
void (*system_exit_hook)(void) = nullptr;

static void heap_reset(void);  // see Heap

// ===========================================================================
// Start-up and termination
// ===========================================================================

// 48c7:0000
void system_init(void)
{
    // The real-mode values of the selector variables.
    ds.System_Seg0040 = 0x0040;
    ds.System_SegA000 = 0xA000;
    ds.System_SegB000 = 0xB000;
    ds.System_SegB800 = 0xB800;
    // 48c7:00b1 DetectCPU: a 386 or better.
    ds.System_Test8086 = 2;

    heap_reset();
    system_exit_hook = nullptr;

    // Not done: the heap variables (the heap is the C allocator's),
    // saving/hooking interrupt vectors and opening Input/Output.
}

// 48c7:0116 from the instruction after the entry's CX/BX := 0: ExitCode,
// the exit hook instead of the ExitProc chain, then the exit. (ErrorAddr
// stays nil: the port reports errors itself.)
[[noreturn]] static void terminate(uint8_t code, const char *message)
{
    ds.System_ExitCode = code;
    void (*hook)(void) = system_exit_hook;
    system_exit_hook = nullptr;
    if (hook)
        hook();
    if (message) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
        if (vid_visible() && !autopilot_active())
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Reunion", message, vid_window());
    }
    // Not done: closing Input/Output and restoring the interrupt vectors.
    if (system_exit_handler)
        system_exit_handler(code);
    exit(code); // DOS returns AL
}

void system_fatal(uint8_t code, const char *message)
{
    terminate(code, message);
}

static const char *run_error_name(uint16_t code)
{
    switch (code) {
    case 2: return "file not found";
    case 100: return "disk read error";
    case 103: return "file not open";
    case 200: return "division by zero";
    case 204: return "invalid pointer operation";
    case 207: return "invalid floating point operation";
    default: return "error";
    }
}

// 48c7:010f
void system_run_error(uint16_t code, const char *where, const char *detail)
{
    if (system_runerror_handler)
        system_runerror_handler(code);
    char message[512];
    SDL_snprintf(message, sizeof message, "Runtime error %u (%s) in %s%s%s", (unsigned)code,
                 run_error_name(code), where, detail ? ": " : "", detail ? detail : "");
    terminate((uint8_t)code, message);
}

// 48c7:0116
void System_Halt(uint16_t code)
{
    terminate((uint8_t)code, nullptr);
}

// ===========================================================================
// Strings
// ===========================================================================

char *pstr_get(char *dst, size_t cap, const void *pstr)
{
    const PString *p = pstr;
    size_t n = SDL_min((size_t)p->len, cap - 1);
    SDL_memmove(dst, p->s, n);
    for (size_t i = 0; i < n; i++) {
        if (dst[i] == '\0')
            dst[i] = PSTR_NUL;
    }
    dst[n] = '\0';
    return dst;
}

void pstr_set(void *pstr, uint8_t maxLen, const char *src)
{
    PString *p = pstr;
    size_t n = SDL_min(SDL_strlen(src), (size_t)maxLen);
    SDL_memmove(p->s, src, n);
    for (size_t i = 0; i < n; i++) {
        if (p->s[i] == PSTR_NUL)
            p->s[i] = '\0';
    }
    p->len = (uint8_t)n;
}

// 48c7:1991 DigitsToLong: parses [+|-](digits | $hexdigits) at p[*pos]
// (0-based), *left characters remaining. Stops at the first non-digit, leaving *pos and
// *left there. Returns false on an error (no digits, overflow), with *pos
// at the offending character.
static bool digits_to_long(const uint8_t *p, uint16_t *pos, uint16_t *left, int32_t *out)
{
    uint32_t u = 0;
    bool neg = false;
    *out = 0;
    if (*left == 0)
        return false;
    if (p[*pos] == '+' || p[*pos] == '-') {
        neg = p[*pos] == '-';
        ++*pos;
        if (--*left == 0)
            return false;
    }
    if (p[*pos] == '$') {
        ++*pos;
        if (--*left == 0)
            return false;
        for (;;) {
            uint8_t c = p[*pos];
            if (c >= 'a')
                c -= 0x20;
            uint8_t d;
            if (c >= '0' && c <= '9')
                d = c - '0';
            else if (c >= 'A' && c <= 'F')
                d = c - 'A' + 10;
            else
                goto end_check; // shared with decimal (48c7:19e0)
            if (u >> 28)
                return false;
            u = u << 4 | d;
            ++*pos;
            if (--*left == 0) {
                // 48c7:1a2b: no sign check, so "$FFFFFFFF" is -1.
                *out = (int32_t)(neg ? 0u - u : u);
                return true;
            }
        }
    }
    for (;;) {
        uint8_t d = (uint8_t)(p[*pos] - '0');
        if (d >= 10)
            break;
        if (u & 0xF0000000u)
            return false;
        u = u * 10 + d;
        ++*pos;
        if (--*left == 0)
            break;
    }
end_check:
    if (u == 0)
        return true;
    if (neg)
        u = 0u - u;
    // The sign must come out as given: up to 2147483647 or -2147483648.
    if (((neg ? 0x80000000u : 0) ^ u) & 0x80000000u)
        return false;
    *out = (int32_t)u;
    return true;
}

// 48c7:1a83: leading blanks are skipped, trailing ones are an error.
int32_t System_ValLong(const char *s, int16_t *code)
{
    const uint8_t *p = (const uint8_t *)s;
    uint16_t left = (uint16_t)SDL_min(SDL_strlen(s), (size_t)255);
    uint16_t pos = 0;
    while (left && p[pos] == ' ') {
        pos++;
        left--;
    }
    int32_t value;
    if (!digits_to_long(p, &pos, &left, &value) || left != 0) {
        *code = (int16_t)(pos + 1);
        return 0;
    }
    *code = 0;
    return value;
}

// ===========================================================================
// Heap. The original's is BP7's free list between HeapOrg and HeapEnd; the
// port's blocks come from the C allocator, zeroed, and nothing depends on
// where they are. They are kept in a list, so that a new start (system_init)
// frees what the last run left, as DOS did with the program's memory: restart
// and quit leave deep screen code by longjmp, abandoning whatever the stack
// held. Each block's header keeps its size for system_heap_used.
// ===========================================================================

typedef struct HeapBlock {
    struct HeapBlock *prev, *next;
    size_t size;
    alignas(16) uint8_t data[];
} HeapBlock;

static HeapBlock heap_blocks = {&heap_blocks, &heap_blocks, 0};
static size_t heap_used;

static HeapBlock *heap_block(void *p)
{
    return (HeapBlock *)((uint8_t *)p - offsetof(HeapBlock, data));
}

static void heap_reset(void)
{
    while (heap_blocks.next != &heap_blocks)
        game_free(heap_blocks.next->data);
    heap_used = 0;
}

// 48c7:028a GetMem
void *game_alloc(size_t size)
{
    HeapBlock *b = SDL_calloc(1, sizeof(HeapBlock) + size);
    if (!b)
        system_fatal(203, "Out of memory");
    b->size = size;
    b->prev = heap_blocks.prev;
    b->next = &heap_blocks;
    b->prev->next = b;
    heap_blocks.prev = b;
    heap_used += size;
    return b->data;
}

// 48c7:029f FreeMem
void game_free(void *p)
{
    if (!p)
        System_RunError(204, "freeing nil");
    HeapBlock *b = heap_block(p);
    b->prev->next = b->next;
    b->next->prev = b->prev;
    heap_used -= b->size;
    SDL_free(b);
}

size_t system_heap_used(void)
{
    return heap_used;
}

// ===========================================================================
// Random numbers
// ===========================================================================

// 48c7:1910 RandNext
static uint32_t rand_next(void)
{
    uint32_t seed = (uint32_t)ds.System_RandSeed * 0x08088405u + 1;
    ds.System_RandSeed = (int32_t)seed;
    return seed;
}

// 48c7:18b3: the high word of seed * range.
uint16_t System_Random(uint16_t range)
{
    return (uint16_t)(((uint64_t)rand_next() * range) >> 32);
}

// 48c7:1948: RandSeed := CX + DX << 16 from INT 21h AH=2Ch (CH hour, CL
// minute, DH second, DL hundredths).
void System_Randomize(void)
{
    // Scripted runs (soak tests, fixtures) must replay exactly.
    uint32_t fixed;
    if (autopilot_seed(&fixed)) {
        ds.System_RandSeed = (int32_t)fixed;
        return;
    }
    SDL_Time now;
    SDL_DateTime t = {0};
    if (SDL_GetCurrentTime(&now))
        SDL_TimeToDateTime(now, &t, true);
    uint32_t cx = (uint32_t)(t.hour << 8 | t.minute);
    uint32_t dx = (uint32_t)(t.second << 8 | t.nanosecond / 10000000);
    ds.System_RandSeed = (int32_t)(dx << 16 | cx);
}
