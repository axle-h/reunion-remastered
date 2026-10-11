// The data segment's fields by name (generated into dsfields.c by
// tools/gen_dseg.py from ghidra/symbols.txt and ghidra/types.pas), for code
// that walks the game's state field by field (game/state.c).
#pragma once

#include <stdint.h>

enum : uint8_t {
    DSF_POINTER = 1,   // holds pointers (pointer or ^T, or an array of them)
    DSF_LABEL = 2,     // a name without a type: one byte, its extent unknown
    DSF_UNNAMED = 4,   // bytes between named fields
    DSF_HOST = 8,      // DSF_POINTER as host pointers (sizeof(void *) each)
};

// A DS field or record field: `count` elements of `elem` bytes (in the
// original) at `ofs` in the original's DS (or in the record), at `at` in
// the port's DSeg (or record), laid out as C arrays of dims[0] x dims[1] x dims[2]; rec indexes
// ds_records when the element is a record, else -1.
typedef struct DsField {
    const char *name;
    uint16_t ofs;
    uint16_t at;
    uint16_t elem;
    uint32_t count;
    int16_t rec;
    uint8_t flags;
    uint16_t dims[3];
} DsField;

typedef struct DsRecord {
    const char *name;
    uint16_t size;        // in the original
    uint16_t port_size;   // in the port (differs with host pointers)
    uint16_t nfields;
    const DsField *fields;
} DsRecord;

extern const DsRecord ds_records[];
extern const int ds_record_count;
// In offset order, covering all of DS: every byte of the original's is in
// exactly one entry.
extern const DsField ds_fields[];
extern const int ds_field_count;
