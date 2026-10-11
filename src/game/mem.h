// The original program's real-mode address space.
//
// Ported code keeps the game's own far pointers (segment:offset values stored
// in its data), so the port emulates the 1 MB DOS address space as one byte
// array and places everything at the address the original used: the data
// segment, video memory at A000:0000 and (later) the heap. Far pointers then
// work unchanged, and memory dumps can be compared with the original running
// in DOSBox-X.
#pragma once

#include <stdint.h>

enum : uint32_t {
    // 1 MB plus the 64 KB that FFFF:xxxx can reach.
    MEM_SIZE = 0x110000,
};

enum : uint16_t {
    // Segment of the program's PSP. The load image starts 10h paragraphs
    // later. -DREUNION_PSP_SEG=... moves everything but video memory and the
    // EMS frame (game/emm.h has -DREUNION_EMM_FRAME_SEG): the golden state
    // hashes (tests/test_golden.c) must not change, which finds pointers that
    // game/state.c still counts as state.
#ifdef REUNION_PSP_SEG
    MEM_PSP_SEG = REUNION_PSP_SEG,
#else
    MEM_PSP_SEG = 0x0192,
#endif
    MEM_LOAD_SEG = MEM_PSP_SEG + 0x10,

    // Data segment (Ghidra 4A81h = file 3A81h).
    MEM_DS_SEG = MEM_LOAD_SEG + 0x3A81,

    // Stack segment from the MZ header (file 44D6h, SP = 1388h); the
    // System unit puts the heap right after the stack.
    MEM_SS_SEG = MEM_LOAD_SEG + 0x44D6,
    MEM_HEAP_SEG = MEM_SS_SEG + (0x1388 + 0x13) / 16,

    MEM_VIDEO_SEG = 0xA000,
};

extern uint8_t mem[MEM_SIZE];

// A far pointer as the game stores it: segment in the high word.
typedef uint32_t FarPtr;

static inline FarPtr far_ptr(uint16_t seg, uint16_t off)
{
    return (FarPtr)seg << 16 | off;
}

static inline uint32_t far_linear(FarPtr p)
{
    return (p >> 16) * 16 + (p & 0xFFFF);
}

// Host pointer for a far pointer (no bounds check beyond the 1 MB + 64 KB).
static inline void *far_host(FarPtr p)
{
    return mem + far_linear(p);
}

// Host pointer for the code's segment numbering in Ghidra (e.g. 4A81h:95BEh),
// for reading data the decompile refers to.
static inline void *ghidra_host(uint16_t seg, uint16_t off)
{
    return mem + (uint32_t)(seg - 0x1000 + MEM_LOAD_SEG) * 16 + off;
}

// Host pointer for an offset in the data segment, for globals that have no
// name in DSeg yet: e.g. *(uint16_t *)ds_at(0x8A80).
static inline void *ds_at(uint16_t off)
{
    return mem + MEM_DS_SEG * 16 + off;
}

// Clears the address space, points the video layer at A000:0000 and
// installs an EMS driver (game/emm.h) with its page frame at E000:0000.
void mem_init(void);
