#include "platform/gamedata.h"

#include <SDL3/SDL.h>
#include <miniz.h>

#include "platform/fs.h"

const char *gamedata_edition_name(Edition edition)
{
    switch (edition) {
    case EDITION_FLOPPY: return "floppy";
    case EDITION_CD: return "CD";
    default: return "unknown";
    }
}

Edition gamedata_edition_from_name(const char *name)
{
    for (Edition e = EDITION_FLOPPY; e <= EDITION_CD; e++)
        if (SDL_strcasecmp(name, gamedata_edition_name(e)) == 0)
            return e;
    return EDITION_UNKNOWN;
}

uint32_t gamedata_crc32(uint32_t crc, const void *data, size_t size)
{
    return (uint32_t)mz_crc32(crc, data, size);
}

Edition gamedata_edition(void)
{
    size_t size = 0;
    uint8_t *data = fs_load("GRAFIKA\\CHARSET1.PIC", &size);
    Edition edition = EDITION_FLOPPY;
    if (data) {
        uint32_t crc = gamedata_crc32(0, data, size);
        for (int i = 0; i < gamedata_file_count; i++) {
            const GameFile *f = &gamedata_files[i];
            if (f->edition && f->size == size && f->crc == crc)
                edition = f->edition;
        }
    }
    SDL_free(data);
    return edition;
}

// Compares DOS paths ignoring case and the kind of slash.
static int path_cmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        int x = *a == '/' ? '\\' : SDL_toupper((unsigned char)*a);
        int y = *b == '/' ? '\\' : SDL_toupper((unsigned char)*b);
        if (x != y || !x)
            return x - y;
    }
}

bool gamedata_listed(const char *dos_path)
{
    if (path_cmp(dos_path, "SAVE\\SETUP.SPD") == 0)
        return true;
    int lo = 0, hi = gamedata_file_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int c = path_cmp(dos_path, gamedata_files[mid].path);
        if (c == 0)
            return true;
        if (c < 0)
            hi = mid - 1;
        else
            lo = mid + 1;
    }
    return false;
}
