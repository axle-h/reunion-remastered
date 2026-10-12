// TEXT\ files: obfuscated line files and fixed-slot .RAW string tables
// (docs/text.md lists them).
#pragma once

#include <stddef.h>
#include <stdint.h>

// Longest decoded line (the game's string[255]) plus the terminating NUL.
#define TEXT_LINE_MAX 256

// Decodes line `line` (1-based) of a line file into out, NUL-terminated and
// truncated to 255 characters. A line past the end, or line 0, gives "".
void text_decode_line(const uint8_t *data, size_t size, int line, char out[TEXT_LINE_MAX]);

// Reads one line of a file such as "TEXT\\MESSAGE.TXT". False if it can't be loaded.
bool text_read_line(const char *dos_path, int line, char out[TEXT_LINE_MAX]);

enum text_raw_kind : uint8_t {
    TEXT_RAW_FACE,  // SZ_FACE.RAW: string[40], 4 slots per record, 12 records
    TEXT_RAW_TALAL, // SZ_TALAL.RAW: string[30], 7 slots per record, 35 records
    TEXT_RAW_FAJ,   // SZ_FAJ.RAW: string[35], 12 slots per record, 12 records
};

// Longest string in any slot (35) plus the NUL.
#define TEXT_RAW_MAX 36

struct text_raw {
    uint8_t *data;
    size_t size;
    int slot_size; // bytes per slot: length byte + string[n]
    int slots;     // slots per record
    int records;
};

bool text_raw_decode(const uint8_t *data, size_t size, enum text_raw_kind kind, struct text_raw *out);
bool text_raw_load(const char *dos_path, enum text_raw_kind kind, struct text_raw *out);
void text_raw_free(struct text_raw *raw);

// Copies string `slot` of `record` (both 0-based) into out. False if out of range.
bool text_raw_get(const struct text_raw *raw, int record, int slot, char out[TEXT_RAW_MAX]);
