// The game's own texts in the CD's German, French and Italian games.
//
// The CD runs every language with the English REUNION.PRG, but each
// language folder's install archive holds that language's build of the
// floppy release's program, its texts translated (docs/versions.md). The
// port takes them from there: tools/cd_texts.py pairs each text of the
// English program with the other builds' (lang_texts.c, generated). Typed
// constants (names, button labels) are written into DS by dseg_init; code
// texts are looked up where the port uses them, by their English text, or
// where one English text has different translations, by the address of the
// English code that loads it.
#pragma once

#include <stdint.h>

#include "game/dseg.h"
#include "platform/gamedata.h"

typedef struct LangText {
    const char *english;
    const char *text[3];  // German, French, Italian (code page 437); nullptr: as English
} LangText;

typedef struct LangTextAt {
    uint32_t addr;        // seg:off of the English code that loads the text
    const char *english;
    const char *text[3];
} LangTextAt;

typedef struct LangSlot {
    uint8_t *slot;        // a PStrN of DS: its length byte
    uint8_t cap;          // N
    const char *text[3];
} LangSlot;

// lang_texts.c: sorted by English (strcmp); by address.
extern const LangText lang_texts[];
extern const int lang_text_count;
extern const LangTextAt lang_texts_at[];
extern const int lang_text_at_count;
extern const LangSlot lang_slots[];
extern const int lang_slot_count;

// The language of the game's texts, set at boot before dseg_init; English
// by default.
void Lang_Set(Language language);
Language Lang_Get(void);

// dseg_init: the language's typed constants. A text longer than its slot
// is cut there and kept whole for Lang_Slot.
void Lang_PatchDS(void);

// A code text of the English REUNION.PRG in the language, or `english`.
const char *Lang_Text(const char *english);
// The same for the text the English code at seg:off loads (an English text
// that translates differently in different places).
const char *Lang_TextAt(uint32_t addr, const char *english);
// A printf format of the port's for texts the English code at seg:off
// loads, with the pieces in the language's order (lang.c).
const char *Lang_Format(uint32_t addr, const char *english);
// A string slot of DS (a PStrN's length byte) as the language has it:
// itself, or the whole text where it is longer than the slot.
const PString *Lang_Slot(const void *slot);
