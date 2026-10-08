// text: obfuscated line files and .RAW tables, checked against tools/text.py.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "assets/text.h"
#include "platform/fs.h"

static const struct {
    const char *path;
    int line;
    uint32_t crc;
    const char *text; // checked instead of crc when set
} lines[] = {
    {"TEXT\\MESSAGE.TXT", 3, 2752404943, nullptr},
    {"TEXT\\MESSAGE.TXT", 48, 3428014599, nullptr}, // longest lines, with '|'
    {"TEXT\\PIRATE.TXT", 20, 3940487374, nullptr},
    {"TEXT\\VALASZ2.LOC", 10, 3799030525, nullptr},
    {"TEXT\\VALASZ1.SP", 2, 3132805546, nullptr},
    {"TEXT\\KERDES1.SP", 11, 3386214075, nullptr},
    {"TEXT\\AJANLAS.SP", 1, 279370035, nullptr},
    {"TEXT\\VALASZ1.AT", 1, 0, "01 -main"},
    {"TEXT\\MESSAGE.TXT", 49, 0, ""},  // past the end
    {"TEXT\\MESSAGE.TXT", 0, 0, ""},
    {"TEXT\\KERDES1.AT", 1, 0, ""},
    {"TEXT\\SZ_FELSZ.TXT", 156, 0, ""},
};

static bool raw_is(const char *path, enum text_raw_kind kind, int records, int record, int slot,
                   const char *want)
{
    struct text_raw raw;
    char s[TEXT_RAW_MAX];
    bool ok = text_raw_load(path, kind, &raw) && raw.records == records &&
              text_raw_get(&raw, record, slot, s) && SDL_strcmp(s, want) == 0 &&
              !text_raw_get(&raw, records, 0, s) && !text_raw_get(&raw, 0, raw.slots, s);
    if (!ok)
        printf("FAILED %s\n", path);
    text_raw_free(&raw);
    return ok;
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }

    bool ok = true;
    for (size_t i = 0; i < SDL_arraysize(lines); i++) {
        char s[TEXT_LINE_MAX];
        bool good = text_read_line(lines[i].path, lines[i].line, s);
        good = good && (lines[i].text ? SDL_strcmp(s, lines[i].text) == 0
                                      : SDL_crc32(0, s, SDL_strlen(s)) == lines[i].crc);
        if (!good)
            printf("FAILED %s line %d\n", lines[i].path, lines[i].line);
        ok = ok && good;
    }

    char s[TEXT_LINE_MAX];
    ok = ok && !text_read_line("TEXT\\NOSUCH.TXT", 1, s);

    ok = ok && raw_is("TEXT\\SZ_FACE.RAW", TEXT_RAW_FACE, 12, 0, 1, "I am still learning to fly");
    ok = ok && raw_is("TEXT\\SZ_FACE.RAW", TEXT_RAW_FACE, 12, 11, 3, "Salary: 250000 Cr");
    ok = ok && raw_is("TEXT\\SZ_TALAL.RAW", TEXT_RAW_TALAL, 35, 0, 0, "        Nuclear Plant");
    ok = ok && raw_is("TEXT\\SZ_FAJ.RAW", TEXT_RAW_FAJ, 12, 0, 0,
                      "             HUMANS                ");

    // Wrong slot layout for the file size is rejected.
    struct text_raw raw;
    ok = ok && !text_raw_load("TEXT\\SZ_FACE.RAW", TEXT_RAW_TALAL, &raw);

    fs_shutdown();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
