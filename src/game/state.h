// The game's state as a byte stream that does not depend on where anything
// lives in memory: the data segment field by field (game/dsfields.h),
// without pointer values and other bytes that only describe the memory
// layout, then the blocks the game keeps behind pointers, the screen and the
// palette. Two runs that behave the same give the same stream however their
// memory is laid out, so the stream (or its hash) can check that a change
// to the memory model keeps the behaviour (tests/golden/, the playthrough's
// write barrier and hashes).
#pragma once

#include <stddef.h>
#include <stdint.h>

// One named part of the stream, for state_describe.
typedef struct StatePart {
    size_t start;
    uint32_t size;
    const char *name;
    uint16_t elem;           // element size (1 for plain bytes)
    int16_t rec;             // ds_records index of the element, or -1
} StatePart;

typedef struct StateBuf {
    uint8_t *data;
    size_t len, cap;
    StatePart *parts;
    size_t nparts, partcap;
} StateBuf;

// Replaces b's contents with the current state. b starts zeroed; reuse it
// to avoid allocations.
void state_serialize(StateBuf *b);
void state_buf_free(StateBuf *b);
// Hash of the stream.
uint64_t state_hash(void);
uint64_t state_buf_hash(const StateBuf *b);
// Names the byte at offset `at` of b's stream, e.g. "DS Game_Year+1",
// "DS Pub_People[2].Timer", "Message_Log^+0x12", "screen x=10 y=20".
void state_describe(const StateBuf *b, size_t at, char *out, size_t size);
// Offset of the first byte where a and b differ, or SIZE_MAX if they are
// the same.
size_t state_first_difference(const StateBuf *a, const StateBuf *b);
// A stream and its parts in a file (debugging: dumps from two runs, then
// `test_golden compare A B` names where they differ).
bool state_save(const StateBuf *b, const char *path);
bool state_load(StateBuf *b, const char *path);
