#include "game/system.h"

#include <SDL3/SDL.h>
#include <stdlib.h>

#include "platform/autopilot.h"

void (*system_runerror_handler)(uint16_t code) = nullptr;
void (*system_exit_handler)(uint8_t code) = nullptr;

// Segment the original's System code runs in (Ghidra 48c7h).
enum : uint16_t { SYSTEM_SEG = 0x48C7 - 0x1000 + MEM_LOAD_SEG };

// ===========================================================================
// Start-up and termination
// ===========================================================================

// 48c7:0000
void system_init(void)
{
    ds.System_PrefixSeg = MEM_PSP_SEG;
    // The real-mode values of the selector variables.
    ds.System_Seg0040 = 0x0040;
    ds.System_SegA000 = 0xA000;
    ds.System_SegB000 = 0xB000;
    ds.System_SegB800 = 0xB800;
    // 48c7:00b1 DetectCPU: a 386 or better.
    ds.System_Test8086 = 2;

    // The heap starts after the stack (and the overlay buffer, empty here);
    // HeapEnd is the top of memory from the PSP (heap_alloc may move it).
    // ClearBss left the offsets 0 and FreeList = HeapPtr means "no free
    // blocks".
    ds.System_OvrHeapOrg = MEM_HEAP_SEG;
    ds.System_OvrHeapPtr = MEM_HEAP_SEG;
    uint16_t org = MEM_HEAP_SEG + ds.System_OvrHeapSize;
    ds.System_OvrHeapEnd = org;
    ds.System_HeapOrg = far_ptr(org, 0);
    ds.System_HeapPtr = far_ptr(org, 0);
    ds.System_FreeList = far_ptr(org, 0);
    ds.System_FreeZero = 0;
    ds.System_HeapEnd = far_ptr(MEM_VIDEO_SEG, 0);
    // 48c7:00d6, the default heap error function (returns 0: run-time error).
    ds.System_HeapError = far_ptr(SYSTEM_SEG, 0x00D6);

    // Not done: saving/hooking interrupt vectors and opening Input/Output.
}

// C functions for the far procedure values the game stores in ExitProc.
static struct {
    FarPtr proc;
    void (*fn)(void);
} exit_procs[8];

void system_add_exit_proc(FarPtr proc, void (*fn)(void))
{
    for (size_t i = 0; i < SDL_arraysize(exit_procs); i++) {
        if (!exit_procs[i].fn || exit_procs[i].proc == proc) {
            exit_procs[i].proc = proc;
            exit_procs[i].fn = fn;
            return;
        }
    }
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Too many exit procedures");
}

// 48c7:0116 from the instruction after the entry's CX/BX := 0: runs the
// ExitProc chain (each procedure may install the next), then reports the
// error and exits with ExitCode.
[[noreturn]] static void terminate(uint16_t code, FarPtr error_addr)
{
    ds.System_ExitCode = (int16_t)code;
    ds.System_ErrorAddr = error_addr;
    while (ds.System_ExitProc) {
        FarPtr proc = ds.System_ExitProc;
        ds.System_ExitProc = 0;
        ds.System_InOutRes = 0;
        void (*fn)(void) = nullptr;
        for (size_t i = 0; i < SDL_arraysize(exit_procs) && !fn; i++)
            if (exit_procs[i].fn && exit_procs[i].proc == proc)
                fn = exit_procs[i].fn;
        if (!fn) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ExitProc %04X:%04X has no port",
                         (unsigned)(proc >> 16), (unsigned)(proc & 0xFFFF));
            break;
        }
        fn();
    }
    // Not done: closing Input/Output and restoring the interrupt vectors.
    if (ds.System_ErrorAddr)
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Runtime error %u at %04X:%04X.",
                     (unsigned)ds.System_ExitCode, (unsigned)(ds.System_ErrorAddr >> 16),
                     (unsigned)(ds.System_ErrorAddr & 0xFFFF));
    if (system_exit_handler)
        system_exit_handler((uint8_t)ds.System_ExitCode);
    exit((uint8_t)ds.System_ExitCode); // DOS returns AL
}

// 48c7:010f
void System_RunError(uint16_t code)
{
    if (system_runerror_handler)
        system_runerror_handler(code);
    // The original reports the caller's address (relative to the program);
    // the port has none, so any non-nil value that marks an error.
    terminate(code, far_ptr(0xFFFF, 0x000F));
}

// 48c7:0116
void System_Halt(uint16_t code)
{
    terminate(code, 0);
}

// ===========================================================================
// Strings
// ===========================================================================

// 48c7:0c7c
void System_StrLoad(PString *tmp, const PString *src)
{
    SDL_memmove(tmp, src, (size_t)src->len + 1);
}

// 48c7:0c96: only the low byte of maxLen counts.
void System_StrStore(const PString *src, PString *dst, uint16_t maxLen)
{
    uint8_t len = src->len;
    if (len > (uint8_t)maxLen)
        len = (uint8_t)maxLen;
    SDL_memmove(dst->s, src->s, len);
    dst->len = len;
}

// 48c7:0cba: index < 1 counts as 1, count < 0 as 0.
void System_Copy(PString *result, const PString *s, int16_t index, int16_t count)
{
    uint16_t avail = s->len;
    if (index <= 0)
        index = 1;
    uint16_t n = 0;
    if (avail >= (uint16_t)index) {
        n = avail - (uint16_t)index + 1;
        if (count < 0)
            count = 0;
        if (n > (uint16_t)count)
            n = (uint16_t)count;
    }
    if (n)
        SDL_memmove(result->s, s->s + index - 1, n);
    result->len = (uint8_t)n;
}

// 48c7:0cfb
void System_StrConcat(PString *tmp, const PString *src)
{
    uint8_t old = tmp->len;
    uint8_t n = src->len;
    if (old + n > 255)
        n = (uint8_t)~old;
    SDL_memmove(tmp->s + old, src->s, n);
    tmp->len = old + n;
}

// 48c7:0d27
int16_t System_Pos(const PString *sub, const PString *s)
{
    if (sub->len == 0 || s->len < sub->len)
        return 0;
    for (int i = 0; i <= s->len - sub->len; i++)
        if (SDL_memcmp(s->s + i, sub->s, sub->len) == 0)
            return (int16_t)(i + 1);
    return 0;
}

// 48c7:0d6d
int System_StrCompare(const PString *a, const PString *b)
{
    int n = a->len < b->len ? a->len : b->len;
    for (int i = 0; i < n; i++)
        if (a->s[i] != b->s[i])
            return (uint8_t)a->s[i] < (uint8_t)b->s[i] ? -1 : 1;
    return a->len < b->len ? -1 : a->len > b->len;
}

// 48c7:0d98
void System_CharToStr(PString *result, char c)
{
    result->len = 1;
    result->s[0] = c;
}

// 48c7:0dc5: s := Copy(s, 1, index - 1) + src + Copy(s, index, 255), with
// each concatenation truncated at 255 and the result at maxLen.
void System_Insert(const PString *src, PString *s, uint16_t maxLen, int16_t index)
{
    PString head, tail;
    if (index < 1)
        index = 1;
    System_Copy(&head, s, 1, (int16_t)(index - 1));
    System_StrConcat(&head, src);
    System_Copy(&tail, s, index, 255);
    System_StrConcat(&head, &tail);
    System_StrStore(&head, s, maxLen);
}

// 48c7:0e24: s := Copy(s, 1, index - 1) + Copy(s, index + count, 255).
void System_Delete(PString *s, int16_t index, int16_t count)
{
    PString head, tail;
    if (count <= 0 || index > 255)
        return;
    if (count > 255)
        count = 255;
    if (index < 1)
        index = 1;
    System_Copy(&head, s, 1, (int16_t)(index - 1));
    System_Copy(&tail, s, (int16_t)(index + count), 255);
    System_StrConcat(&head, &tail);
    System_StrStore(&head, s, 255);
}

// 48c7:1955 LongToDigits: writes the decimal digits (and '-') of value so
// they end at `end`; returns how many.
static int long_to_digits(int32_t value, char *end)
{
    uint32_t u = value < 0 ? 0u - (uint32_t)value : (uint32_t)value;
    char *p = end;
    do { // 48c7:1971 ULongToDigits
        *--p = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    if (value < 0)
        *--p = '-';
    return (int)(end - p);
}

// 48c7:1a38: the number right-aligned in `width` spaces; width and the
// digits are cut to maxLen (keeping the leading characters).
void System_StrLong(int32_t value, int16_t width, PString *s, uint16_t maxLen)
{
    char buf[12];
    int16_t n = (int16_t)long_to_digits(value, buf + sizeof buf);
    const char *digits = buf + sizeof buf - n;
    int16_t max = (int16_t)maxLen;
    int16_t len = width;
    if (len > max)
        len = max;
    if (n > max)
        n = max;
    if (len < n)
        len = n;
    s->len = (uint8_t)len;
    SDL_memset(s->s, ' ', (size_t)(len - n));
    SDL_memcpy(s->s + (len - n), digits, (size_t)n);
}

// 48c7:1991 DigitsToLong: parses [+|-](digits | $hexdigits) at p[*pos],
// *left characters remaining. Stops at the first non-digit, leaving *pos and
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
int32_t System_ValLong(const PString *s, int16_t *code)
{
    const uint8_t *p = (const uint8_t *)s;
    uint16_t left = s->len;
    uint16_t pos = 1;
    while (left && p[pos] == ' ') {
        pos++;
        left--;
    }
    int32_t value;
    if (!digits_to_long(p, &pos, &left, &value) || left != 0) {
        *code = (int16_t)pos;
        return 0;
    }
    *code = 0;
    return value;
}

// 48c7:1b78
char System_UpCase(char c)
{
    return c >= 'a' && c <= 'z' ? (char)(c - 0x20) : c;
}

// ===========================================================================
// Memory
// ===========================================================================

// 48c7:0b4f
void System_MoveFwd(const void *src, void *dst, uint16_t count)
{
    const uint8_t *s = src;
    uint8_t *d = dst;
    while (count--)
        *d++ = *s++;
}

// 48c7:1b40
void System_Move(const void *src, void *dst, uint16_t count)
{
    SDL_memmove(dst, src, count);
}

// 48c7:1b64
void System_FillChar(void *dst, uint16_t count, uint8_t value)
{
    SDL_memset(dst, value, count);
}

// ===========================================================================
// Heap
//
// A free block holds {next: pointer; size: pointer} where size is a
// normalised byte count (paragraphs in the segment word, 0 or 8 in the
// offset word). The list is sorted by address and ends with a link equal to
// HeapPtr; FreeList itself acts as the head "block" (with FreeZero as its
// size, so nothing ever merges into it).
// ===========================================================================

enum : uint16_t { FREELIST_OFF = offsetof(DSeg, System_FreeList) };

static uint16_t rd16(uint16_t seg, uint16_t off)
{
    const uint8_t *p = far_host(far_ptr(seg, off));
    return (uint16_t)(p[0] | p[1] << 8);
}

static void wr16(uint16_t seg, uint16_t off, uint16_t v)
{
    uint8_t *p = far_host(far_ptr(seg, off));
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static uint16_t seg_of(FarPtr p) { return (uint16_t)(p >> 16); }
static uint16_t off_of(FarPtr p) { return (uint16_t)p; }

// seg:off + 16-bit offset sum, normalised the original's way: only AL is
// checked for a paragraph carry (the sums stay below 32 for heap blocks).
static FarPtr heap_norm(uint16_t seg, uint16_t off)
{
    if ((off & 0xFF) >= 0x10)
        seg++;
    return far_ptr(seg, (uint16_t)(off & 0xFF0F));
}

// 48c7:04dc HeapSizeToParas: size rounded up to 8, as paragraphs + 0/8.
static void heap_size(uint16_t size, uint16_t *paras, uint16_t *off)
{
    uint32_t s = (uint32_t)size + 7;
    *paras = (uint16_t)(s >> 4);
    *off = (uint16_t)(s & 8);
}

// The default HeapError (48c7:00d6) returns 0: "fail with a run-time
// error". The game never installs another one.
static uint16_t heap_error(uint16_t size)
{
    (void)size;
    return 0;
}

// 48c7:0353 HeapAlloc: first fit; the block is cut from the low end of a
// free block, or taken from HeapPtr. Returns false when out of memory.
static bool heap_alloc(uint16_t size, FarPtr *out)
{
    *out = 0;
    if (size == 0)
        return true;
    ds.System_HeapAllocSize = size;
    for (;;) {
        uint16_t paras, off;
        heap_size(ds.System_HeapAllocSize, &paras, &off);

        uint16_t prev_seg = MEM_DS_SEG, prev_off = FREELIST_OFF;
        for (;;) {
            uint16_t blk_off = rd16(prev_seg, prev_off);
            uint16_t blk_seg = rd16(prev_seg, prev_off + 2);
            if (blk_seg == seg_of(ds.System_HeapPtr)) {
                // End of the list: grow the heap. The new top is computed
                // from the list's last link, which equals HeapPtr.
                FarPtr top = heap_norm((uint16_t)(blk_seg + paras), (uint16_t)(blk_off + off));
                uint16_t end_seg = seg_of(ds.System_HeapEnd);
                if (seg_of(top) > end_seg ||
                    (seg_of(top) == end_seg && off_of(top) > off_of(ds.System_HeapEnd)))
                    break;
                ds.System_HeapPtr = top;
                wr16(prev_seg, prev_off, off_of(top));
                wr16(prev_seg, prev_off + 2, seg_of(top));
                heap_error(0); // the original notifies HeapError with size 0
                *out = far_ptr(blk_seg, blk_off);
                return true;
            }
            uint16_t bsize_off = rd16(blk_seg, blk_off + 4);
            uint16_t bsize_seg = rd16(blk_seg, blk_off + 6);
            if (paras > bsize_seg || (paras == bsize_seg && off > bsize_off)) {
                prev_seg = blk_seg;
                prev_off = blk_off;
                continue;
            }
            uint16_t next_off = rd16(blk_seg, blk_off);
            uint16_t next_seg = rd16(blk_seg, blk_off + 2);
            if (paras == bsize_seg && off == bsize_off) {
                wr16(prev_seg, prev_off, next_off);
                wr16(prev_seg, prev_off + 2, next_seg);
            } else {
                // The rest of the block stays free, in its place in the list.
                uint32_t left = ((uint32_t)bsize_seg << 16 | bsize_off) - ((uint32_t)paras << 16 | off);
                FarPtr rest = heap_norm((uint16_t)(blk_seg + paras), (uint16_t)(blk_off + off));
                wr16(seg_of(rest), off_of(rest), next_off);
                wr16(seg_of(rest), off_of(rest) + 2, next_seg);
                wr16(seg_of(rest), off_of(rest) + 4, (uint16_t)(left & 0xF));
                wr16(seg_of(rest), off_of(rest) + 6, (uint16_t)(left >> 16));
                wr16(prev_seg, prev_off, off_of(rest));
                wr16(prev_seg, prev_off + 2, seg_of(rest));
            }
            *out = far_ptr(blk_seg, blk_off);
            return true;
        }

        // Port addition. The game moved its two big blocks into EMS, which
        // the port doesn't have (game/ems.h), so the heap can run out where
        // the original's didn't. When the heap reaches video memory it goes
        // on above it, to F000:0000 (so that segment + block size can't
        // wrap): what is left below A000:0000 becomes a free block, and
        // video memory a block that is never freed. Until then the heap is
        // the original's. (It goes when its blocks move out of mem[];
        // docs/native-memory-plan.md.)
        if (seg_of(ds.System_HeapEnd) == MEM_VIDEO_SEG) {
            FarPtr top = ds.System_HeapPtr, above = far_ptr(MEM_VIDEO_SEG + 0x1000, 0);
            uint32_t gap = MEM_VIDEO_SEG * 16u - far_linear(top);  // a multiple of 8
            ds.System_HeapEnd = far_ptr(0xF000, 0);
            ds.System_HeapPtr = above;
            // The list's last link (to the old HeapPtr) goes to the gap, or
            // above video memory if there is none.
            uint16_t ls = MEM_DS_SEG, lo = FREELIST_OFF;
            while (rd16(ls, lo + 2) != seg_of(top)) {
                uint16_t ns = rd16(ls, lo + 2), no = rd16(ls, lo);
                ls = ns;
                lo = no;
            }
            FarPtr next = gap ? top : above;
            wr16(ls, lo, off_of(next));
            wr16(ls, lo + 2, seg_of(next));
            if (gap) {
                wr16(seg_of(top), off_of(top), off_of(above));
                wr16(seg_of(top), off_of(top) + 2, seg_of(above));
                wr16(seg_of(top), off_of(top) + 4, (uint16_t)(gap & 0xF));
                wr16(seg_of(top), off_of(top) + 6, (uint16_t)(gap >> 4));
            }
            continue;
        }

        // 48c7:040a: 0 = run-time error, 1 = return nil, else retry.
        uint16_t r = heap_error(ds.System_HeapAllocSize) & 0xFF;
        if (r == 0)
            return false;
        if (r == 1)
            return true;
    }
}

// 48c7:047d HeapMerge: joins block a with the following free block b if
// they touch (or lowers HeapPtr to a if b is HeapPtr); else links a -> b.
static void heap_merge(uint16_t a_seg, uint16_t a_off, uint16_t b_seg, uint16_t b_off)
{
    uint16_t end_off = (uint16_t)(a_off + rd16(a_seg, a_off + 4));
    uint16_t end_seg = (uint16_t)(a_seg + rd16(a_seg, a_off + 6));
    FarPtr end = heap_norm(end_seg, end_off);
    if (end != far_ptr(b_seg, b_off)) {
        wr16(a_seg, a_off, b_off);
        wr16(a_seg, a_off + 2, b_seg);
        return;
    }
    if (end == ds.System_HeapPtr) {
        ds.System_HeapPtr = far_ptr(a_seg, a_off);
        return;
    }
    wr16(a_seg, a_off, rd16(b_seg, b_off));
    wr16(a_seg, a_off + 2, rd16(b_seg, b_off + 2));
    uint16_t size_off = (uint16_t)(rd16(b_seg, b_off + 4) + rd16(a_seg, a_off + 4));
    uint16_t size_seg = (uint16_t)(rd16(b_seg, b_off + 6) + rd16(a_seg, a_off + 6));
    FarPtr size = heap_norm(size_seg, size_off);
    wr16(a_seg, a_off + 4, off_of(size));
    wr16(a_seg, a_off + 6, seg_of(size));
}

// 48c7:041e HeapFree: inserts the block into the address-sorted free list
// and merges it with its neighbours. Returns false for an invalid pointer
// (offset not 0/8, outside HeapOrg..HeapPtr, or already free).
static bool heap_free(FarPtr p, uint16_t size)
{
    if (size == 0)
        return true;
    uint16_t seg = seg_of(p), off = off_of(p);
    if (off & 0xFFF7)
        return false;
    if (seg < seg_of(ds.System_HeapOrg) ||
        (seg == seg_of(ds.System_HeapOrg) && off < off_of(ds.System_HeapOrg)))
        return false;
    if (seg > seg_of(ds.System_HeapPtr) ||
        (seg == seg_of(ds.System_HeapPtr) && off >= off_of(ds.System_HeapPtr)))
        return false;

    uint16_t paras, size_off;
    heap_size(size, &paras, &size_off);
    wr16(seg, off + 4, size_off);
    wr16(seg, off + 6, paras);

    uint16_t prev_seg = MEM_DS_SEG, prev_off = FREELIST_OFF;
    for (;;) {
        uint16_t next_off = rd16(prev_seg, prev_off);
        uint16_t next_seg = rd16(prev_seg, prev_off + 2);
        if (seg > next_seg || (seg == next_seg && off > next_off)) {
            prev_seg = next_seg;
            prev_off = next_off;
            continue;
        }
        if (seg == next_seg && off == next_off)
            return false;
        heap_merge(seg, off, next_seg, next_off);
        heap_merge(prev_seg, prev_off, seg, off);
        return true;
    }
}

// 48c7:0323 HeapWalk: walks the free list starting with the space above
// HeapPtr; `max` picks MaxAvail's callback (48c7:031c) over MemAvail's
// (48c7:02fc). Sizes are paragraphs:offset pairs until the end.
static int32_t heap_walk(bool max)
{
    uint32_t end = ds.System_HeapEnd, ptr = ds.System_HeapPtr;
    uint16_t ax = (uint16_t)((uint16_t)end - (uint16_t)ptr);
    uint16_t dx = (uint16_t)((end >> 16) - (ptr >> 16) - ((uint16_t)end < (uint16_t)ptr));
    ax &= 0xF;
    FarPtr cur = ds.System_FreeList;
    while (seg_of(cur) != seg_of(ds.System_HeapPtr)) {
        uint16_t b_off = rd16(seg_of(cur), off_of(cur) + 4);
        uint16_t b_seg = rd16(seg_of(cur), off_of(cur) + 6);
        if (max) {
            if (dx < b_seg || (dx == b_seg && ax < b_off)) {
                ax = b_off;
                dx = b_seg;
            }
        } else {
            if ((uint32_t)ax + b_off > 0xFFFF)
                dx += 0x1000;
            ax += b_off;
            dx += b_seg;
        }
        cur = far_ptr(rd16(seg_of(cur), off_of(cur) + 2), rd16(seg_of(cur), off_of(cur)));
    }
    return (int32_t)((uint32_t)dx * 16 + ax);
}

// 48c7:028a
FarPtr System_GetMem(uint16_t size)
{
    FarPtr p;
    if (!heap_alloc(size, &p))
        System_RunError(203);
    return p;
}

// 48c7:029f
void System_FreeMem(FarPtr p, uint16_t size)
{
    if (!heap_free(p, size))
        System_RunError(204);
}

// 48c7:02e7. The game asks before it loads something big (the start-up
// check, the satellite animation, picture buffers) and skips or stops when
// there isn't enough. The port always has enough: all of conventional
// memory free, which no check asks for.
int32_t System_MemAvail(void)
{
    return SYSTEM_MEM_PLENTY;
}

// 48c7:0303
int32_t System_MaxAvail(void)
{
    return SYSTEM_MEM_PLENTY;
}

int32_t system_heap_free(void)
{
    return heap_walk(false);
}

int32_t system_heap_max_block(void)
{
    return heap_walk(true);
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

// ===========================================================================
// 6-byte Real
//
// The original works on registers: a = DX:BX:AX, b = DI:SI:CX, with AL the
// exponent, AH the lowest mantissa byte and the top bit of DH the sign. The
// cores below keep that form (Rr) and return the carry flag the original
// returns (true = overflow). Mantissas are handled as integers: m40 = the
// 40 bits with the hidden bit set (DX|8000h : BX : AH), m48 = m40 << 8 with
// AL as a guard byte.
// ===========================================================================

typedef struct Rr {
    uint16_t ax, bx, dx;
} Rr;

static const Rr RR_ZERO = {0, 0, 0};
static const uint64_t M48 = (1ull << 48) - 1;
static const uint64_t M40 = (1ull << 40) - 1;

static Rr rr_from(Real48 r)
{
    return (Rr){(uint16_t)(r.b[0] | r.b[1] << 8), (uint16_t)(r.b[2] | r.b[3] << 8),
                (uint16_t)(r.b[4] | r.b[5] << 8)};
}

static Real48 rr_real(Rr r)
{
    return (Real48){{(uint8_t)r.ax, (uint8_t)(r.ax >> 8), (uint8_t)r.bx, (uint8_t)(r.bx >> 8),
                     (uint8_t)r.dx, (uint8_t)(r.dx >> 8)}};
}

static uint8_t rr_exp(Rr r) { return (uint8_t)r.ax; }

static uint64_t rr_m48(Rr r)
{
    return (uint64_t)(r.dx | 0x8000) << 32 | (uint64_t)r.bx << 16 | (r.ax & 0xFF00);
}

// Mantissa m48 (top bit set) back into registers with exponent and sign.
static Rr rr_pack(uint64_t m, uint8_t exp, uint16_t sign)
{
    return (Rr){(uint16_t)((m & 0xFF00) | exp), (uint16_t)(m >> 16),
                (uint16_t)(((m >> 32) & 0x7FFF) | sign)};
}

// 48c7:10e3 RealAddCore (48c7:10df RealSubCore flips b's sign first). The
// smaller operand is shifted right with truncation into the guard byte;
// the result is rounded half up on that byte.
static bool real_add_core(Rr *a, Rr b)
{
    if (rr_exp(b) == 0)
        return false;
    if (rr_exp(*a) == 0) {
        *a = b;
        return false;
    }
    Rr x = *a, y = b; // x: smaller exponent
    if (rr_exp(x) > rr_exp(y)) {
        x = b;
        y = *a;
    }
    uint8_t diff = rr_exp(y) - rr_exp(x);
    if (diff >= 0x29) {
        *a = y;
        return false;
    }
    uint16_t sign = x.dx & 0x8000;
    uint8_t exp = rr_exp(y);
    uint64_t mx = rr_m48(x) >> diff, my = rr_m48(y), m;
    if (((x.dx ^ y.dx) & 0x8000) == 0) {
        m = mx + my;
        if (m >> 48) {
            m >>= 1;
            if (++exp == 0)
                return true;
        }
    } else {
        if (mx >= my) {
            m = mx - my;
        } else {
            m = my - mx;
            sign ^= 0x8000;
        }
        if (m == 0) {
            *a = RR_ZERO;
            return false;
        }
        while (!(m >> 47)) {
            m = (m << 1) & M48;
            if (--exp == 0) {
                *a = RR_ZERO;
                return false;
            }
        }
    }
    // 48c7:1150
    m += 0x80;
    if (m >> 48) {
        m = 1ull << 47;
        if (++exp == 0)
            return true;
    }
    *a = rr_pack(m, exp, sign);
    return false;
}

static bool real_sub_core(Rr *a, Rr b)
{
    b.dx ^= 0x8000;
    return real_add_core(a, b);
}

// 48c7:1287: shared tail of Mul and Div. cx holds the exponent biased so
// that bit 14 flags underflow, bit 8 (after the increment) overflow and
// bit 15 the inverted sign, which XORs into the mantissa's hidden bit.
static bool real_round_tail(Rr *a, uint64_t m, uint16_t cx)
{
    m += 0x80;
    if (m >> 48) {
        m = 1ull << 47;
        cx++;
    }
    if (cx & 0x4000) {
        *a = RR_ZERO;
        return false;
    }
    cx++;
    a->ax = (uint16_t)((m & 0xFF00) | (cx & 0xFF));
    a->bx = (uint16_t)(m >> 16);
    a->dx = (uint16_t)((m >> 32) ^ (cx & 0xFF00));
    return (cx >> 8) & 1;
}

// 48c7:11a6 RealMulCore: the top 48 bits of the 80-bit mantissa product
// (exact, then truncated), normalised and rounded.
static bool real_mul_core(Rr *a, Rr b)
{
    if (rr_exp(*a) == 0 || rr_exp(b) == 0) {
        *a = RR_ZERO;
        return false;
    }
    uint16_t cx = (uint16_t)(((a->dx ^ b.dx) & 0x8000) + rr_exp(*a) + rr_exp(b));
    uint64_t x = rr_m48(*a) >> 8, y = rr_m48(b) >> 8;
    uint64_t x1 = x >> 32, x0 = x & 0xFFFFFFFF, y1 = y >> 32, y0 = y & 0xFFFFFFFF;
    uint64_t m = (x1 * y1 << 32) + x1 * y0 + x0 * y1 + (x0 * y0 >> 32);
    if (!(m >> 47)) {
        m = (m << 1) & M48;
        cx--;
    }
    return real_round_tail(a, m, (uint16_t)(cx - 0x8081));
}

// 48c7:12a9 RealDivCore: restoring division producing 16 + 16 + 10
// quotient bits (inverted, then complemented). b must not be 0.
static bool real_div_core(Rr *a, Rr b)
{
    if (rr_exp(*a) == 0) {
        *a = RR_ZERO;
        return false;
    }
    uint16_t cx = (uint16_t)(((a->dx ^ b.dx) & 0x8000) + rr_exp(*a) - rr_exp(b));
    uint64_t r = rr_m48(*a) >> 8, d = rr_m48(b) >> 8;
    uint16_t q = 1, words[2] = {0, 0};
    int left = 2;
    bool shifted_out = false;
    for (;;) {
        bool bit; // inverted quotient bit
        if (shifted_out) {
            r = (r - d) & M40;
            bit = false;
        } else if (r >= d) {
            r -= d;
            bit = false;
        } else {
            bit = true;
        }
        bool full = q & 0x8000;
        q = (uint16_t)(q << 1 | bit);
        if (full) {
            if (--left < 0)
                break;
            words[1 - left] = q;
            q = left ? 1 : 0x40;
        }
        shifted_out = (r >> 39) & 1;
        r = (r << 1) & M40;
    }
    uint16_t dx = (uint16_t)~words[0], bx = (uint16_t)~words[1], ax = (uint16_t)~(q << 6);
    uint64_t m = (uint64_t)dx << 32 | (uint64_t)bx << 16 | ax;
    if (!(dx & 0x8000)) {
        m = (m << 1) & M48;
        cx--;
    }
    return real_round_tail(a, m, (uint16_t)(cx + 0x8080));
}

// 48c7:1320 RealCmpCore / 1337 RealCmpMagCore. Two zeros are equal
// whatever their mantissa bytes; otherwise the bytes are compared.
static int real_cmp_core(Rr a, Rr b)
{
    if ((a.dx ^ b.dx) & 0x8000)
        return (a.dx & 0x8000) ? -1 : 1;
    int m;
    if (rr_exp(a) != rr_exp(b))
        m = rr_exp(a) < rr_exp(b) ? -1 : 1;
    else if (rr_exp(a) == 0)
        m = 0;
    else if (a.dx != b.dx)
        m = a.dx < b.dx ? -1 : 1;
    else if (a.bx != b.bx)
        m = a.bx < b.bx ? -1 : 1;
    else if ((a.ax >> 8) != (b.ax >> 8))
        m = (a.ax >> 8) < (b.ax >> 8) ? -1 : 1;
    else
        m = 0;
    return (a.dx & 0x8000) ? -m : m;
}

// 48c7:134a LongToRealCore (exact).
static Rr real_from_long(int32_t v)
{
    if (v == 0)
        return RR_ZERO;
    uint32_t u = v < 0 ? 0u - (uint32_t)v : (uint32_t)v;
    uint8_t exp = 0xA0;
    while (!(u & 0x80000000u)) {
        u <<= 1;
        exp--;
    }
    Rr r = {exp, (uint16_t)u, (uint16_t)(u >> 16)};
    if (v > 0)
        r.dx &= 0x7FFF;
    return r;
}

// 48c7:1386 RealToLongCore: truncates, or with `round` adds the first
// dropped bit (half away from zero). Returns true if out of range.
static bool real_to_long(Rr a, bool round, int32_t *out)
{
    *out = 0;
    uint8_t exp = rr_exp(a);
    if (exp > 0xA0)
        return true;
    uint8_t shift = 0xA0 - exp;
    if (shift > 0x20)
        return false;
    uint8_t sign = (uint8_t)(a.dx >> 8);
    uint64_t w = (uint64_t)(a.dx | 0x8000) << 24 | (uint64_t)a.bx << 8 | a.ax >> 8;
    w >>= shift; // 32 integer bits + 8 guard bits
    uint64_t n = w >> 8;
    if (round)
        n += (w >> 7) & 1;
    if (n >> 32)
        return true;
    uint32_t u = (uint32_t)n;
    if (u == 0)
        return false;
    if (sign & 0x80)
        u = 0u - u;
    *out = (int32_t)u;
    return ((sign ^ (u >> 24)) & 0x80) != 0;
}

// 48c7:1468: clears the fraction bits (|a| < 2^39 only).
static Rr real_int(Rr a)
{
    uint8_t exp = rr_exp(a);
    if (exp >= 0xA8)
        return a;
    if (exp <= 0x80)
        return RR_ZERO;
    unsigned keep = exp - 0x80; // top bits of the 40 (the first is the sign)
    uint64_t m = (uint64_t)a.dx << 24 | (uint64_t)a.bx << 8 | a.ax >> 8;
    m &= M40 & ~((1ull << (40 - keep)) - 1);
    return (Rr){(uint16_t)((m & 0xFF) << 8 | exp), (uint16_t)(m >> 8), (uint16_t)(m >> 24)};
}

// 48c7:14b9
static Rr real_frac(Rr a)
{
    Rr r = a;
    real_sub_core(&r, real_int(a));
    return r;
}

// 48c7:1864 RealPoly: 1 + y*(c[n-1] + y*(... + y*c[0])); c in the
// original's code segment. No overflow checks.
static Rr real_poly(Rr y, const Rr *c, int n)
{
    Rr a = c[0];
    real_mul_core(&a, y);
    for (int i = 1; i < n; i++) {
        real_add_core(&a, c[i]);
        real_mul_core(&a, y);
    }
    real_add_core(&a, (Rr){0x81, 0, 0});
    return a;
}

// 48c7:184b RealPolyHelper: x * poly(x*x).
static Rr real_poly_odd(Rr x, const Rr *c, int n)
{
    Rr a = x;
    real_mul_core(&a, x);
    a = real_poly(a, c, n);
    real_mul_core(&a, x);
    return a;
}

static Rr rr_bytes(const uint8_t b[6])
{
    return rr_from((Real48){{b[0], b[1], b[2], b[3], b[4], b[5]}});
}

// Sine series coefficients at 48c7:15a0 (x^15/15! ... -x^3/3!).
static const uint8_t SIN_COEF[7][6] = {
    {0x58, 0x9D, 0x39, 0x9F, 0x3F, 0xD7}, {0x60, 0x43, 0x9D, 0x30, 0x92, 0x30},
    {0x67, 0xAA, 0x3F, 0x28, 0x32, 0xD7}, {0x6E, 0xB6, 0x2A, 0x1D, 0xEF, 0x38},
    {0x74, 0x0D, 0xD0, 0x00, 0x0D, 0xD0}, {0x7A, 0x88, 0x88, 0x88, 0x88, 0x08},
    {0x7E, 0xAB, 0xAA, 0xAA, 0xAA, 0xAA},
};

// ---- Public wrappers -------------------------------------------------------

// 48c7:13f4
Real48 System_RealAdd(Real48 a, Real48 b)
{
    Rr r = rr_from(a);
    if (real_add_core(&r, rr_from(b)))
        System_RunError(205);
    return rr_real(r);
}

// 48c7:13fa
Real48 System_RealSub(Real48 a, Real48 b)
{
    Rr r = rr_from(a);
    if (real_sub_core(&r, rr_from(b)))
        System_RunError(205);
    return rr_real(r);
}

// 48c7:1400
Real48 System_RealSqr(Real48 a)
{
    return System_RealMul(a, a);
}

// 48c7:1406
Real48 System_RealMul(Real48 a, Real48 b)
{
    Rr r = rr_from(a);
    if (real_mul_core(&r, rr_from(b)))
        System_RunError(205);
    return rr_real(r);
}

// 48c7:140c
Real48 System_RealDiv(Real48 a, Real48 b)
{
    Rr r = rr_from(a);
    if (rr_exp(rr_from(b)) == 0)
        System_RunError(200);
    if (real_div_core(&r, rr_from(b)))
        System_RunError(205);
    return rr_real(r);
}

// 48c7:1416
int System_RealCmp(Real48 a, Real48 b)
{
    return real_cmp_core(rr_from(a), rr_from(b));
}

// 48c7:141a
Real48 System_LongToReal(int32_t value)
{
    return rr_real(real_from_long(value));
}

// 48c7:141e
int32_t System_Trunc(Real48 a)
{
    int32_t v;
    if (real_to_long(rr_from(a), false, &v))
        System_RunError(207);
    return v;
}

// 48c7:1426
int32_t System_Round(Real48 a)
{
    int32_t v;
    if (real_to_long(rr_from(a), true, &v))
        System_RunError(207);
    return v;
}

// 48c7:1468
Real48 System_Int(Real48 a)
{
    return rr_real(real_int(rr_from(a)));
}

// 48c7:14b9
Real48 System_Frac(Real48 a)
{
    return rr_real(real_frac(rr_from(a)));
}

// 48c7:14cd: Newton's iteration y := (x/y + y)/2 from a first guess with
// half the exponent, until a step is below 2^-20 of the guess.
Real48 System_Sqrt(Real48 a)
{
    Rr x = rr_from(a);
    if (rr_exp(x) == 0)
        return a;
    if (x.dx & 0x8000)
        System_RunError(207);
    // (exp + 80h) sar 1 + 80h, in bytes
    uint8_t half = (uint8_t)(rr_exp(x) + 0x80);
    uint8_t exp = (uint8_t)(((half >> 1) | (half & 0x80)) + 0x80);
    uint8_t limit = exp - 0x14;
    Rr y = {(uint16_t)((x.ax & 0xFF00) | exp), x.bx, x.dx};
    for (;;) {
        Rr t = x;
        real_div_core(&t, y);
        real_add_core(&t, y);
        t.ax = (uint16_t)((t.ax & 0xFF00) | (uint8_t)(t.ax - 1)); // halve
        Rr step = t;
        real_sub_core(&step, y);
        y = t;
        if (rr_exp(step) < limit)
            return rr_real(y);
    }
}

// 48c7:152c: sin(pi/2 - x), reduced to 0..pi/2 and evaluated as a series.
Real48 System_Cos(Real48 a)
{
    Rr x = rr_from(a);
    real_add_core(&x, (Rr){0x2181, 0xDAA2, 0xC90F}); // - pi/2
    if (rr_exp(x) != 0)
        x.dx ^= 0x8000;

    // Sine from here (48c7:153f).
    if (rr_exp(x) < 0x6C)
        return rr_real(x);
    Rr b = {0x2183, 0xDAA2, 0x490F}; // 2*pi
    Rr mag = x;
    mag.dx &= 0x7FFF;
    if (real_cmp_core(mag, b) >= 0) {
        real_div_core(&x, b);
        x = real_frac(x);
        real_mul_core(&x, b);
    }
    if (x.dx & 0x8000)
        real_add_core(&x, b);
    b.ax--; // pi
    bool below_pi = real_cmp_core(x, b) < 0;
    if (!below_pi)
        real_sub_core(&x, b);
    b.ax--; // pi/2
    if (real_cmp_core(x, b) >= 0) {
        b.ax++;
        x.dx |= 0x8000;
        real_add_core(&x, b); // pi - x
    }
    if (rr_exp(x) >= 0x6C) {
        Rr c[7];
        for (int i = 0; i < 7; i++)
            c[i] = rr_bytes(SIN_COEF[i]);
        x = real_poly_odd(x, c, 7);
    }
    if (!below_pi && rr_exp(x) != 0)
        x.dx ^= 0x8000;
    return rr_real(x);
}

// ---- Conversions (not in the original) -------------------------------------

double real48_to_double(Real48 r)
{
    Rr x = rr_from(r);
    if (rr_exp(x) == 0)
        return 0.0;
    double m = (double)(rr_m48(x) >> 8); // 40 bits, exact
    double v = SDL_scalbn(m, rr_exp(x) - 129 - 39);
    return (x.dx & 0x8000) ? -v : v;
}

Real48 real48_from_double(double d)
{
    uint64_t bits;
    SDL_memcpy(&bits, &d, sizeof bits);
    uint16_t sign = (bits >> 63) ? 0x8000 : 0;
    int e = (int)((bits >> 52) & 0x7FF);
    uint64_t f = bits & ((1ull << 52) - 1);
    if (e == 0)
        return (Real48){{0}}; // zero or subnormal: far below the Real range
    // 52 fraction bits -> 39, ties to even.
    uint64_t m = (1ull << 52 | f) >> 13;
    uint64_t rest = f & 0x1FFF;
    if (rest > 0x1000 || (rest == 0x1000 && (m & 1)))
        m++;
    int exp = e - 1023 + 129;
    if (m >> 40) {
        m >>= 1;
        exp++;
    }
    if (exp <= 0)
        return (Real48){{0}};
    if (exp > 255 || e == 0x7FF) { // too big, Inf or NaN
        exp = 255;
        m = M40;
    }
    return rr_real(rr_pack(m << 8, (uint8_t)exp, sign));
}
