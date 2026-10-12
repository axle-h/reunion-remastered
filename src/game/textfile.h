// Unit TextFile (398f): the single-line text editor, reading of the
// obfuscated TEXT\ line files and the multi-line reply/info text drawing.
//
// Strings are C strings, except the editor's: it edits game data (group,
// planet and save names) in place, so it takes a Pascal string (a length byte
// followed by the characters).
#pragma once

#include <stddef.h>
#include <stdint.h>


// Keyboard access of the editor (Crt.KeyPressed / Crt.ReadKey); tests
// replace them to queue keys.
extern bool (*TextFile_KeyPressed)(void);
extern char (*TextFile_ReadKey)(void);

// 398f:0000: blocking editor for s at (x, y), at most width characters.
void TextFile_InputLine(uint8_t *s, int16_t x, int16_t y, int16_t width, bool canCancel);
// 398f:0059 / 011a / 0179: the editor's start, end and one frame.
void TextFile_BeginInput(uint8_t *s, int16_t x, int16_t y, int16_t width);
void TextFile_EndInput(void);
void TextFile_InputTick(void);
// 398f:03ae: reads line `line` of text\<name> and decodes it into s (at
// most cap - 1 of the line's up to 255 characters).
void TextFile_ReadTextLine(const char *name, int16_t line, char *s, size_t cap);
// 398f:046f: skips `skip` lines of text\sz_felsz.txt, then draws the next 6
// (the first with DrawTextFC at (100, 86h), the rest DrawTextF9).
void TextFile_ShowBuildingInfo(int16_t skip);
// 398f:0552: s is "<2 digits><char>|..." text: the two digits are the code,
// then 7 lines (max) split at '|' of up to 50 characters drawn at x, y + 9 *
// line. f2 = typewriter, f1 = FC colour set (else F9).
void TextFile_DrawReply(const char *s, int16_t x, int16_t y, int16_t colour, bool f1, bool f2,
                        int16_t *code, int16_t *lines);
// Port: Val of the first two characters of s, the code the advisor's,
// pub's and aliens' texts start with; 0 if they aren't a number or s is
// shorter (past its end the original read what its buffer held before).
int16_t TextFile_LineCode(const char *s);
// 398f:07fe: number of '|' + 1.
int16_t TextFile_CountLines(const char *s);
// 398f:0864: s[i] := (s[i] + 5780h - i*4Fh) mod E0h - 20h for i = 1..len.
void TextFile_DecodeLine(char *s, size_t len);
