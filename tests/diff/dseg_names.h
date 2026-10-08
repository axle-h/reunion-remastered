// Offset -> name tables of the DS mirror (generated into dseg_names.c by
// tools/gen_dseg.py), for naming differences in the differential tests.
#pragma once

#include <stdint.h>

// A DS field or record field: `count` elements of `elem` bytes at `ofs`,
// laid out as C arrays of dims[0] x dims[1] x dims[2]; rec indexes
// diff_records when the element is a record, else -1.
typedef struct DiffName {
    const char *name;
    uint16_t ofs;
    uint16_t elem;
    uint32_t count;
    int16_t rec;
    uint16_t dims[3];
} DiffName;

typedef struct DiffRecord {
    const char *name;
    uint16_t size;
    uint16_t nfields;
    const DiffName *fields;
} DiffRecord;

extern const DiffRecord diff_records[];
extern const int diff_record_count;
// Sorted by offset.
extern const DiffName diff_ds_names[];
extern const int diff_ds_name_count;
