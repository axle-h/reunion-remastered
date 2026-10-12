// A copy of the DOS game to import (platform/import.h): a folder, a ZIP
// archive or an ISO 9660 image (plain ISO 9660 is enough for the CD: 8.3
// names, no Joliet or Rock Ridge). The game's root is the folder that holds
// GRAFIKA, at any depth: the shallowest one, and REUNION.ENG where there are
// several (the CD has a folder per language).
#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct Source Source;

typedef struct SourceFile {
    char *path;      // relative to the game's root, '\\' between parts, case as found
    uint64_t size;
    uint32_t crc;    // CRC-32, when crc_known (a ZIP's own)
    bool crc_known;
    uint64_t where;  // ZIP: entry index; ISO: byte offset
    char *host;      // folder: the host path
} SourceFile;

// nullptr with SDL_GetError() saying why: the path can't be read, is none
// of the three kinds, or holds no GRAFIKA folder.
Source *source_open(const char *path);
void source_close(Source *src);

// Where the root is inside the source ("REUNION.ENG", "" at the top).
const char *source_root(const Source *src);
int source_file_count(const Source *src);
const SourceFile *source_file(const Source *src, int i);
// A file under the root by DOS path, any case, either slash; nullptr if none.
const SourceFile *source_find(const Source *src, const char *dos_path);
// The whole file; free with SDL_free. nullptr on a read error.
uint8_t *source_read(Source *src, const SourceFile *file, size_t *size);
