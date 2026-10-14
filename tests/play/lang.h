// The CD's other languages in the playthrough. The policy is written
// against the English game's texts: its talk paths, the replies and the
// message boxes it waits for, and the captions, labels and names it looks
// for. The game reads its dialogues and messages by line number, so a line
// of a file in another language is the same line of the English file, and
// the readers (ui_talk.h, ui_message.h) turn what the game shows into
// English with that before the policy sees it. The game's own texts are
// paired with the English ones in game/lang.h.
//
// The English files are the fs root's, the language's those of its folder
// (an import's data/ and data/DEU; PLAY_LANGUAGE, run.h). The policy thread
// runs only while the game waits (play.h), so reading the English files
// with the language folder set aside meets no game read.
#pragma once

#include <stddef.h>

enum : int { LANG_LINE = 256 };  // longest line the lang_* functions handle, NUL included

// Notes the game's language, and checks that the English files can be had
// if it is another; false, with a message, if not. Until then the game is
// taken to be English.
bool lang_init(void);
// The game is in English: nothing to translate.
bool lang_english(void);
// The English of `text`, line `line` of TEXT\<file> (any case) as
// TextFile_ReadTextLine reads it: the English file's line.
void lang_line(const char *file, int line, const char *text, char *out, size_t size);
// The English of `text`, a line of TEXT\<file> in the game's language (or
// the start of one, 8 characters or more): the same line of the English
// file, whole; `text` if the file has no such line.
void lang_line_of(const char *file, const char *text, char *out, size_t size);
// The English of a message box's text as read off the screen (its lines
// joined by '|'): the English line of MESSAGE.TXT, KITALAL.TXT or
// PIRATE.TXT whose text is closest to it, if close enough, in the same
// shape (its '|' pieces without leading blanks); else the text with each
// of the game's own texts in it replaced by its English (game/lang.h),
// '|' and runs of blanks as one blank.
void lang_box(const char *text, char *out, size_t size);
// The n characters at `shown` (blanks at either end aside, any case) are
// `english` or one of the game's translations of it (game/lang.h).
bool lang_matches(const char *shown, size_t n, const char *english);
// lang_matches by the typed constants alone (button labels, names): the
// French game captions PLANET MAIN's hotspot "INFO VAISSEAU", as SHIP INFO,
// but labels its toolbar button "PLANETE PRINCIPAL".
bool lang_matches_constant(const char *shown, size_t n, const char *english);
// `read`, a text ui_read_text read off the screen (so made English), is
// `english` (any case) or reads as one of the game's translations of it
// does: "Raketenwerfer" reads as "Rocket launcher", but it is also the
// German "Missile launcher".
bool lang_reads_as(const char *read, const char *english);
// The k-th (0..) of the game's translations of `english` (blanks at the
// ends aside, any case), or nullptr: no more.
const char *lang_translation(const char *english, int k);
// A line of English made of the game's texts, as the game shows it: each
// of its texts of 5 characters or more, the longest first, translated.
const char *lang_translate_line(const char *english, char *out, size_t size);
// The English of a line read off the screen: as lang_untranslate, else
// with each of the game's texts in it of 5 characters or more, the longest
// first, as its English.
const char *lang_untranslate_line(const char *shown, char *out, size_t size);
// The English of `shown` (blanks at either end aside): the first of the
// game's texts that translates to it, without blanks at the ends; `shown`
// itself if none does.
const char *lang_untranslate(const char *shown, char *out, size_t size);
