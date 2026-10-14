#include "play/lang.h"

#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/dseg.h"
#include "game/lang.h"
#include "game/textfile.h"
#include "platform/fs.h"
#include "platform/gamedata.h"
#include "play/ui_message.h"

enum : int {
    LINE_MAX = 256,
    FILES_MAX = 64,
    BOX_MAX = 512,
    // A box is a message's line if 85% of the shorter's characters are a
    // common subsequence of both (reading through the cursor drops some).
    BOX_PERCENT = 85,
};

// A text file's lines in the game's language and in English.
typedef struct {
    char name[24];
    int count;
    char (*lang)[LINE_MAX];
    char (*english)[LINE_MAX];
} LangFile;

// The game's own texts (game/lang.h) in English and in the game's
// language, without blanks at the ends: code texts, then typed constants.
typedef struct {
    const char *english, *text;
    size_t english_len, text_len;
    bool somewhere;  // translated so only by some code (Lang_TextAt)
    bool english_lead, english_trail, text_lead, text_trail;  // blanks cut at the ends
} Pair;

static Pair *pairs;
static int pair_count;
static int constants_from;  // the first typed constant's pair

static LangFile files[FILES_MAX];
static int file_count;
static bool translate;  // lang_init found another language

bool lang_english(void)
{
    return !translate;
}

bool lang_init(void)
{
    translate = gamedata_language() != LANGUAGE_ENGLISH;
    if (!translate)
        return true;
    char *keep = fs_language_dir() ? SDL_strdup(fs_language_dir()) : nullptr;
    fs_set_language_dir(nullptr);
    bool english_root = gamedata_language() == LANGUAGE_ENGLISH;
    fs_set_language_dir(keep);
    SDL_free(keep);
    if (!english_root || !fs_language_dir())
        printf("lang: the %s game needs the English one in the data's root "
               "(an import's data/ with PLAY_LANGUAGE)\n", gamedata_language_name(gamedata_language()));
    return english_root && fs_language_dir();
}

// The lines of TEXT\<name> (count: CR LFs + 1), read with the fs as it is.
static int read_lines(const char *name, char (**out)[LINE_MAX])
{
    char path[48];
    SDL_snprintf(path, sizeof path, "text\\%s", name);
    size_t size = 0;
    uint8_t *data = fs_load(path, &size);
    int count = 1;
    for (size_t i = 0; data && i + 1 < size; i++)
        count += data[i] == '\r' && data[i + 1] == '\n';
    SDL_free(data);
    *out = SDL_calloc((size_t)count, LINE_MAX);
    for (int i = 0; *out && i < count; i++)
        TextFile_ReadTextLine(name, (int16_t)(i + 1), (*out)[i], LINE_MAX);
    return *out ? count : 0;
}

static LangFile *file(const char *name)
{
    for (int i = 0; i < file_count; i++)
        if (SDL_strcasecmp(files[i].name, name) == 0)
            return &files[i];
    if (file_count == FILES_MAX)
        return nullptr;
    LangFile *f = &files[file_count++];
    SDL_strlcpy(f->name, name, sizeof f->name);
    int n = read_lines(name, &f->lang);
    char *keep = fs_language_dir() ? SDL_strdup(fs_language_dir()) : nullptr;
    fs_set_language_dir(nullptr);
    int m = read_lines(name, &f->english);
    fs_set_language_dir(keep);
    SDL_free(keep);
    f->count = SDL_min(n, m);
    return f;
}

void lang_line(const char *name, int line, const char *text, char *out, size_t size)
{
    const LangFile *f = lang_english() ? nullptr : file(name);
    SDL_strlcpy(out, f && line >= 1 && line <= f->count ? f->english[line - 1] : text, size);
}

void lang_line_of(const char *name, const char *text, char *out, size_t size)
{
    const LangFile *f = lang_english() ? nullptr : file(name);
    const char *found = nullptr;
    size_t n = SDL_strlen(text);
    for (int i = 0; f && i < f->count && !found; i++)
        if (SDL_strcmp(f->lang[i], text) == 0)
            found = f->english[i];
    for (int i = 0; f && n >= 8 && i < f->count && !found; i++)
        if (SDL_strncmp(f->lang[i], text, n) == 0)
            found = f->english[i];
    SDL_strlcpy(out, found ? found : text, size);
}

static const char *trim(const char *s, size_t *n)
{
    while (*n > 0 && s[*n - 1] == ' ')
        (*n)--;
    while (*n > 0 && *s == ' ') {
        s++;
        (*n)--;
    }
    return s;
}

// The n characters at s as the screen reads them: the accents the font
// lacks drawn as a letter without ("Dro\213de" as "Droide").
static char *as_read(const char *s, size_t n, char *out)
{
    for (size_t i = 0; i < n; i++) {
        char c = (uint8_t)s[i] >= 0x80 ? ui_read_as(s[i]) : '?';
        out[i] = c != '?' ? c : s[i];
    }
    out[n] = 0;
    return out;
}

// A pair of the same text is kept only if `same`.
static void add_pair(const char *english, size_t en, const char *text, size_t tn, bool somewhere,
                     bool same)
{
    bool el = en > 0 && english[0] == ' ', et = en > 0 && english[en - 1] == ' ';
    bool tl = tn > 0 && text[0] == ' ', tt = tn > 0 && text[tn - 1] == ' ';
    english = trim(english, &en);
    text = trim(text, &tn);
    if (tn == 0 || (!same && en == tn && SDL_memcmp(english, text, en) == 0))
        return;
    char *shown = SDL_malloc(tn + 1);
    if (!shown)
        return;
    text = as_read(text, tn, shown);
    Pair *grown = SDL_realloc(pairs, (size_t)(pair_count + 1) * sizeof *grown);
    if (!grown) {
        SDL_free(shown);
        return;
    }
    pairs = grown;
    pairs[pair_count++] = (Pair){english, text, en, tn, somewhere, el, et, tl, tt};
}

// Message pieces whose translation stands for more of the English message
// than the English piece: the neighbours the other builds leave out
// (game/lang.c) are in it.
static const struct {
    uint32_t addr;
    const char *english;
} wider[] = {
    {0x1b8a1f0d, " Your satellite carrier"},
    {0x1b8a1f22, " Your pilots"},
    {0x1b8a2d10, " system |    They found new planets"},
    {0x1b8a2e5d, "Your satellite carrier explored "},
    {0x1b8a2e72, "Your pilots explored "},
};

static void know_pairs(void)
{
    if (pairs || !translate)
        return;
    Language l = Lang_Get();
    if (l == LANGUAGE_ENGLISH)
        return;
    for (int i = 0; i < lang_text_count; i++)
        if (lang_texts[i].text[l - 1])
            add_pair(lang_texts[i].english, SDL_strlen(lang_texts[i].english),
                     lang_texts[i].text[l - 1], SDL_strlen(lang_texts[i].text[l - 1]), false, false);
    for (int i = 0; i < lang_text_at_count; i++) {
        const char *english = lang_texts_at[i].english;
        for (size_t k = 0; k < SDL_arraysize(wider); k++)
            if (wider[k].addr == lang_texts_at[i].addr)
                english = wider[k].english;
        if (lang_texts_at[i].text[l - 1])
            add_pair(english, SDL_strlen(english), lang_texts_at[i].text[l - 1],
                     SDL_strlen(lang_texts_at[i].text[l - 1]), true, false);
    }
    // The races' names first, even where they are the same: where another
    // name has the same translation (French "Morgruls" is also Names_Races'
    // "Morgrul", "Undorlings" its "Undorling"), the messages mean the race
    // ("Your forces have eliminated 'The Morgruls'").
    constants_from = pair_count;
    const uint8_t *races = (const uint8_t *)&ds.Races_Races;
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < lang_slot_count; i++) {
            const LangSlot *sl = &lang_slots[i];
            bool race = sl->slot >= races && sl->slot < races + sizeof ds.Races_Races;
            if (!sl->text[l - 1] || race != (pass == 0))
                continue;
            const uint8_t *en = (const uint8_t *)&ds_initial + (sl->slot - (uint8_t *)&ds);
            add_pair((const char *)en + 1, en[0], sl->text[l - 1], SDL_strlen(sl->text[l - 1]), false,
                     race);
        }
    }
}

static bool matches_from(int from, const char *shown, size_t n, const char *english)
{
    char read[LANG_LINE];
    n = SDL_min(n, sizeof read - 1);
    shown = trim(as_read(shown, n, read), &n);
    size_t en = SDL_strlen(english);
    english = trim(english, &en);
    if (n == en && SDL_strncasecmp(shown, english, n) == 0)
        return true;
    know_pairs();
    for (int i = from; i < pair_count; i++)
        if (pairs[i].english_len == en && pairs[i].text_len == n
            && SDL_strncasecmp(pairs[i].english, english, en) == 0
            && SDL_strncasecmp(pairs[i].text, shown, n) == 0)
            return true;
    return false;
}

bool lang_matches(const char *shown, size_t n, const char *english)
{
    return matches_from(0, shown, n, english);
}

bool lang_matches_constant(const char *shown, size_t n, const char *english)
{
    know_pairs();
    return matches_from(constants_from, shown, n, english);
}

const char *lang_translation(const char *english, int k)
{
    size_t en = SDL_strlen(english);
    english = trim(english, &en);
    know_pairs();
    static char t[BOX_MAX];
    for (int i = 0; i < pair_count; i++) {
        if (pairs[i].english_len == en && SDL_strncasecmp(pairs[i].english, english, en) == 0
            && k-- == 0) {
            SDL_snprintf(t, sizeof t, "%.*s", (int)pairs[i].text_len, pairs[i].text);
            return t;
        }
    }
    return nullptr;
}

bool lang_reads_as(const char *read, const char *english)
{
    if (SDL_strcasecmp(read, english) == 0)
        return true;
    char t[LANG_LINE], e[LANG_LINE];
    for (int k = 0; lang_translation(english, k); k++) {
        SDL_strlcpy(t, lang_translation(english, k), sizeof t);
        if (SDL_strcasecmp(lang_untranslate_line(t, e, sizeof e), read) == 0)
            return true;
    }
    return false;
}

// Where `piece` (n characters) is in `line` as a text of its own, outside
// the texts already replaced (done, by character of line): anywhere if it
// has 5 characters or more, else (3 or 4) with no letter or digit next to
// it ("Fl." in "Kall Fl. 2", not "Auf" in "Aufbau").
static char *find_piece(char *line, const bool *done, const char *piece, size_t n)
{
    for (char *at = SDL_strstr(line, piece); at; at = SDL_strstr(at + 1, piece)) {
        bool free = true;
        for (size_t i = 0; i < n && free; i++)
            free = !done[at - line + (ptrdiff_t)i];
        if (!free)
            continue;
        if (n >= 5)
            return at;
        bool before = at > line && SDL_isalnum((unsigned char)at[-1]);
        bool after = SDL_isalnum((unsigned char)at[n]);
        if (n >= 3 && !before && !after)
            return at;
    }
    return nullptr;
}

// `to` in place of the n characters at `at` of `line` (size bytes), marked
// done: a replacement is not replaced again ("Morgruls" by "The Morgruls",
// whose "Morgruls" is also the French of "Morgrul"). False if it does not
// fit.
static bool splice(char *line, bool *done, size_t size, char *at, size_t n, const char *to)
{
    size_t off = (size_t)(at - line), tl = SDL_strlen(to), rest = SDL_strlen(at + n);
    if (off + tl + rest + 1 > size)
        return false;
    SDL_memmove(at + tl, at + n, rest + 1);
    SDL_memmove(done + off + tl, done + off + n, rest + 1);
    SDL_memcpy(at, to, tl);
    for (size_t i = 0; i < tl; i++)
        done[off + i] = true;
    return true;
}

// `line` with each text of the pairs in it (find_piece), the
// longest first, replaced by its English (or translation; not by the
// translations only some code makes, which would land in other texts).
static void replace_pieces(char line[LANG_LINE], bool to_english)
{
    know_pairs();
    bool used[1024] = {}, done[LANG_LINE] = {};
    for (;;) {
        int best = -1;
        size_t best_len = 2;
        for (int i = 0; i < pair_count && i < 1024; i++) {
            const char *from = to_english ? pairs[i].text : pairs[i].english;
            size_t n = to_english ? pairs[i].text_len : pairs[i].english_len;
            if (used[i] || (pairs[i].somewhere && !to_english) || n <= best_len)
                continue;
            char piece[LANG_LINE];
            SDL_snprintf(piece, sizeof piece, "%.*s", (int)n, from);
            if (find_piece(line, done, piece, n)) {
                best = i;
                best_len = n;
            }
        }
        if (best < 0)
            return;
        used[best] = true;
        const Pair *b = &pairs[best];
        const char *from = to_english ? b->text : b->english;
        char piece[LANG_LINE], to[LANG_LINE];
        SDL_snprintf(piece, sizeof piece, "%.*s", (int)best_len, from);
        char *at = find_piece(line, done, piece, best_len);
        // The blanks the target text has at its ends, where the line has none.
        bool lead = (to_english ? b->english_lead : b->text_lead) && at > line && at[-1] != ' ';
        bool trail = (to_english ? b->english_trail : b->text_trail) && at[best_len] != ' '
                     && at[best_len] != '\0';
        SDL_snprintf(to, sizeof to, "%s%.*s%s", lead ? " " : "",
                     (int)(to_english ? b->english_len : b->text_len), to_english ? b->english : b->text,
                     trail ? " " : "");
        if (!splice(line, done, LANG_LINE, at, best_len, to))
            return;
    }
}

const char *lang_translate_line(const char *english, char *out, size_t size)
{
    char line[LANG_LINE];
    SDL_strlcpy(line, english, sizeof line);
    if (translate)
        replace_pieces(line, false);
    SDL_strlcpy(out, line, size);
    return out;
}

const char *lang_untranslate(const char *shown, char *out, size_t size)
{
    char read[LANG_LINE];
    size_t n = SDL_min(SDL_strlen(shown), sizeof read - 1);
    const char *t = trim(as_read(shown, n, read), &n);
    know_pairs();
    // The same case first ("Weltraumhafen" is a building, "WELTRAUMHAFEN" a button).
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < pair_count; i++) {
            if (pairs[i].text_len == n
                && (pass ? SDL_strncasecmp(pairs[i].text, t, n) : SDL_strncmp(pairs[i].text, t, n)) == 0) {
                SDL_snprintf(out, size, "%.*s", (int)pairs[i].english_len, pairs[i].english);
                return out;
            }
        }
    }
    SDL_strlcpy(out, read, size);
    return out;
}

// Lines read off the screen and their English (lang_untranslate_line).
enum : int { LINE_CACHE = 4096 };
static struct {
    char shown[LINE_MAX], english[LINE_MAX];
    bool used;
} line_cache[LINE_CACHE];

static uint32_t hash(const char *s)
{
    uint32_t h = 2166136261u;
    for (; *s; s++)
        h = (h ^ (uint8_t)*s) * 16777619u;
    return h;
}

const char *lang_untranslate_line(const char *shown, char *out, size_t size)
{
    if (!translate) {
        SDL_strlcpy(out, shown, size);
        return out;
    }
    char read[LANG_LINE];
    shown = as_read(shown, SDL_min(SDL_strlen(shown), sizeof read - 1), read);
    uint32_t h = hash(shown) % LINE_CACHE;
    if (line_cache[h].used && SDL_strcmp(line_cache[h].shown, shown) == 0) {
        SDL_strlcpy(out, line_cache[h].english, size);
        return out;
    }
    char line[LANG_LINE];
    lang_untranslate(shown, line, sizeof line);
    if (SDL_strcmp(line, shown) == 0)
        replace_pieces(line, true);
    SDL_strlcpy(line_cache[h].shown, shown, sizeof line_cache[h].shown);
    SDL_strlcpy(line_cache[h].english, line, sizeof line_cache[h].english);
    line_cache[h].used = true;
    SDL_strlcpy(out, line, size);
    return out;
}

// `s` with '|' as a blank and runs of blanks as one, without blanks at the
// ends.
static void flatten(const char *s, size_t n, char *out, size_t size)
{
    size_t k = 0;
    bool blank = true;
    for (size_t i = 0; i < n && k + 1 < size; i++) {
        char c = s[i] == '|' ? ' ' : s[i];
        if (c == ' ' && blank)
            continue;
        out[k++] = c;
        blank = c == ' ';
    }
    while (k > 0 && out[k - 1] == ' ')
        k--;
    out[k] = '\0';
}

// A message box shows the text being edited in place of an "&xxx" field:
// a text is matched up to it.
static void cut_field(char *s)
{
    char *amp = SDL_strchr(s, '&');
    if (amp) {
        while (amp > s && amp[-1] == ' ')
            amp--;
        *amp = '\0';
    }
}

// A box made of the game's own texts and names: each text in it of 5
// characters or more, the longest first, as its English.
static void box_pieces(const char *text, char *out, size_t size)
{
    char flat[BOX_MAX * 2], piece[BOX_MAX], english[BOX_MAX];
    flatten(text, SDL_strlen(text), flat, sizeof flat);
    know_pairs();
    // Longest first: repeat picking the longest piece found.
    bool used[1024] = {}, done[sizeof flat] = {};
    for (;;) {
        int best = -1;
        size_t best_len = 4;
        for (int i = 0; i < pair_count && i < 1024; i++) {
            if (used[i])
                continue;
            flatten(pairs[i].text, pairs[i].text_len, piece, sizeof piece);
            cut_field(piece);
            size_t len = SDL_strlen(piece);
            if (len > best_len && find_piece(flat, done, piece, len)) {
                best = i;
                best_len = len;
            }
        }
        if (best < 0)
            break;
        used[best] = true;
        flatten(pairs[best].text, pairs[best].text_len, piece, sizeof piece);
        cut_field(piece);
        SDL_snprintf(english, sizeof english, "%s%.*s%s", pairs[best].english_lead ? " " : "",
                     (int)pairs[best].english_len, pairs[best].english,
                     pairs[best].english_trail ? " " : "");
        cut_field(english);
        char *at = find_piece(flat, done, piece, SDL_strlen(piece));
        if (!splice(flat, done, sizeof flat, at, SDL_strlen(piece), english))
            break;
    }
    flatten(flat, SDL_strlen(flat), english, sizeof english);
    SDL_strlcpy(out, english, size);
}

// `s` as the box reader gives it, without '|' and blanks: each character
// as read (ui_read_as).
static size_t squeeze(const char *s, char *out, size_t size)
{
    size_t n = 0;
    for (; *s && n + 1 < size; s++)
        if (*s != '|' && *s != ' ')
            out[n++] = ui_read_as(*s);
    out[n] = '\0';
    return n;
}

// The longest common subsequence of a and b.
static int common(const char *a, size_t na, const char *b, size_t nb)
{
    static int row[2][BOX_MAX + 1];
    SDL_memset(row, 0, sizeof row);
    for (size_t i = 1; i <= na; i++) {
        int *cur = row[i % 2], *prev = row[(i + 1) % 2];
        for (size_t k = 1; k <= nb; k++)
            cur[k] = a[i - 1] == b[k - 1] ? prev[k - 1] + 1 : SDL_max(prev[k], cur[k - 1]);
    }
    return row[na % 2][nb];
}

void lang_box(const char *text, char *out, size_t size)
{
    static char last_text[BOX_MAX], last_out[BOX_MAX];
    if (lang_english()) {
        SDL_strlcpy(out, text, size);
        return;
    }
    if (SDL_strcmp(text, last_text) == 0) {
        SDL_strlcpy(out, last_out, size);
        return;
    }
    static const char *const NAMES[] = {"message.txt", "kitalal.txt", "pirate.txt"};
    char want[BOX_MAX], line[BOX_MAX];
    size_t nw = squeeze(text, want, sizeof want);
    const char *best = nullptr;
    int best_score = 0;
    for (size_t k = 0; k < SDL_arraysize(NAMES) && nw > 0; k++) {
        const LangFile *f = file(NAMES[k]);
        for (int i = 0; f && i < f->count; i++) {
            size_t nl = squeeze(f->lang[i], line, sizeof line);
            if (nl == 0)
                continue;
            int score = common(want, nw, line, nl) * 100 / (int)SDL_max(nw, nl);
            if (score > best_score) {
                best_score = score;
                best = f->english[i];
            }
        }
    }
    if (best && best_score >= BOX_PERCENT) {
        // The English box: each '|' piece without its leading blanks.
        size_t n = 0;
        last_out[0] = '\0';
        for (const char *p = best; *p;) {
            while (*p == ' ')
                p++;
            const char *end = SDL_strchr(p, '|');
            size_t len = end ? (size_t)(end - p) : SDL_strlen(p);
            while (len > 0 && p[len - 1] == ' ')
                len--;
            if (len > 0 && n + len + 2 < sizeof last_out) {
                if (n)
                    last_out[n++] = '|';
                SDL_memcpy(last_out + n, p, len);
                n += len;
                last_out[n] = '\0';
            }
            p = end ? end + 1 : p + SDL_strlen(p);
        }
    } else {
        box_pieces(text, last_out, sizeof last_out);
    }
    SDL_strlcpy(last_text, text, sizeof last_text);
    SDL_strlcpy(out, last_out, size);
}
