#include "game/textfile.h"

#include <SDL3/SDL.h>

#include "assets/text.h"
#include "game/crt.h"
#include "game/dseg.h"
#include "game/gfx.h"
#include "game/mouse.h"
#include "game/system.h"
#include "platform/fs.h"

bool (*TextFile_KeyPressed)(void) = Crt_KeyPressed;
char (*TextFile_ReadKey)(void) = Crt_ReadKey;

// The editor's variables, unnamed in symbols.txt.
#define IN_ACTIVE (*(uint8_t *)ds_at(0x7C4A))    // editing in progress
#define IN_RESTORE (*(uint8_t *)ds_at(0x7C4B))   // Esc restores the backup (set by InputLine)
#define IN_BACKUP (*(uint32_t *)ds_at(0x7C4C))   // GetMem(3Ch) copy of the string
#define IN_STR (*(uint32_t *)ds_at(0x7C50))      // far pointer to the edited string
#define IN_BLINK (*(uint16_t *)ds_at(0x7C54))    // cursor blink counter 1..20
#define IN_CANCEL (*(uint8_t *)ds_at(0x7C56))    // Esc allowed
#define IN_X (*(int16_t *)ds_at(0x7C58))
#define IN_Y (*(int16_t *)ds_at(0x7C5A))
#define IN_WIDTH (*(int16_t *)ds_at(0x7C5C))
#define IN_CURX (*(int16_t *)ds_at(0x7C5E))      // cursor underline position
#define IN_CURY (*(int16_t *)ds_at(0x7C60))

// The edited string as a host pointer. The original stored the far pointer in
// DS:7C50; callers here may hold the string outside the emulated memory
// (tests), so the host pointer is kept too, and the far pointer only when the
// string lies inside mem[].
static uint8_t *input_str;
static FarPtr caller_ptr;  // TextFile_SetStringPtr

void TextFile_SetStringPtr(FarPtr p)
{
    caller_ptr = p;
}

// The far pointer the original's caller would have passed: DS-relative for a
// global, Game_Groups' segment for a group's name, normalised (as GetMem
// returns them) for any other heap block.
static uint32_t host_to_far(const void *p)
{
    const uint8_t *b = p;
    if (b < mem || b >= mem + MEM_SIZE)
        return 0;
    uint32_t lin = (uint32_t)(b - mem);
    uint32_t ds_lin = (uint32_t)MEM_DS_SEG * 16;
    if (lin >= ds_lin && lin < ds_lin + DSEG_SIZE)
        return far_ptr(MEM_DS_SEG, (uint16_t)(lin - ds_lin));
    // A group's name: Groups^[list][group] keeps Game_Groups' segment.
    uint32_t groups_lin = (uint32_t)(ds.Game_Groups >> 16) * 16 + (uint16_t)ds.Game_Groups;
    if (ds.Game_Groups != 0 && lin >= groups_lin && lin < groups_lin + 64 * sizeof(TGroup))
        return far_ptr((uint16_t)(ds.Game_Groups >> 16), (uint16_t)(lin - (ds.Game_Groups >> 16) * 16));
    return far_ptr((uint16_t)(lin >> 4), (uint16_t)(lin & 15));
}

static void underline(uint8_t colour)
{
    Mouse_HideCursor();
    Gfx_DrawLine(gfx_screen(), IN_CURX, IN_CURY, (int16_t)(IN_CURX + 5), IN_CURY, colour);
    Mouse_ShowCursor();
}

// The cursor sits after the last character, or on it when the field is full.
static void place_cursor(void)
{
    if (input_str[0] < IN_WIDTH)
        IN_CURX = (int16_t)(input_str[0] * 6 + IN_X);
    else
        IN_CURX = (int16_t)(input_str[0] * 6 + IN_X - 6);
    IN_CURY = (int16_t)(IN_Y + 8);
}

// 398f:0000
void TextFile_InputLine(uint8_t *s, int16_t x, int16_t y, int16_t width, bool canCancel)
{
    TextFile_BeginInput(s, x, y, width);
    IN_RESTORE = 1;
    IN_CANCEL = canCancel;
    while (IN_ACTIVE != 0) {
        TextFile_InputTick();
        Gfx_WaitVRetrace();
    }
    if (!canCancel) {
        // Dead code in practice: IN_ACTIVE is already 0 here.
        while (IN_RESTORE != 0 && IN_ACTIVE != 0) {
            TextFile_InputTick();
            Gfx_WaitVRetrace();
        }
    }
    TextFile_EndInput();
}

// 398f:0059
void TextFile_BeginInput(uint8_t *s, int16_t x, int16_t y, int16_t width)
{
    TextFile_EndInput();
    IN_BACKUP = System_GetMem(0x3C);
    IN_X = x;
    IN_Y = y;
    IN_WIDTH = width;
    input_str = s;
    IN_STR = caller_ptr != 0 && far_host(caller_ptr) == s ? caller_ptr : host_to_far(s);
    caller_ptr = 0;
    // BUG? StrStore(…, 255) into a 60-byte block: longer strings overrun it.
    System_StrStore((const PString *)s, far_host(IN_BACKUP), 0xFF);
    IN_BLINK = 0;
    IN_CANCEL = 0;
    Gfx_DrawTextFC(input_str, IN_X, IN_Y, IN_WIDTH);
    IN_ACTIVE = 1;
    IN_RESTORE = 0;
    place_cursor();
}

// 398f:011a
void TextFile_EndInput(void)
{
    if (IN_ACTIVE != 0) {
        underline(0xFC);
        Gfx_DrawTextFC(input_str, IN_X, IN_Y, IN_WIDTH);
        System_FreeMem(IN_BACKUP, 0x3C);
        IN_ACTIVE = 0;
    }
}

// 398f:0179
void TextFile_InputTick(void)
{
    IN_BLINK++;
    if (IN_BLINK > 20)
        IN_BLINK = 1;
    Gfx_WaitVRetrace();
    if (IN_BLINK == 1)
        underline(0xFD);
    if (IN_BLINK == 10)
        underline(0xFC);
    if (!TextFile_KeyPressed())
        return;

    uint8_t key = (uint8_t)TextFile_ReadKey();
    if (key == 0) {
        TextFile_ReadKey();  // extended key: the scan code is dropped
        return;
    }
    IN_BLINK = 0;
    underline(0xFC);
    if (key == 8 && input_str[0] != 0)
        input_str[0]--;  // Delete(s, Length(s), 1)
    if (key == 0x0D && input_str[0] != 0) {
        TextFile_EndInput();
        IN_ACTIVE = 0;
        IN_RESTORE = 0;
    }
    if (key == 0x1B && IN_CANCEL != 0) {
        if (IN_RESTORE != 0)
            System_StrStore(far_host(IN_BACKUP), (PString *)input_str, 0xFF);
        TextFile_EndInput();
        IN_ACTIVE = 0;
        IN_RESTORE = 1;
    }
    if (key == ' ' || (key > 0x60 && key < 0x7B) || (key > 0x40 && key < 0x5B)
        || (key > 0x2F && key < 0x3A)) {
        if (input_str[0] < IN_WIDTH) {
            if (input_str[0] < 255) {
                input_str[1 + input_str[0]] = (char)key;
                input_str[0]++;
            }
        } else {
            input_str[IN_WIDTH] = key;  // s[width]: overwrite the last character
        }
    }
    Gfx_DrawTextFC(input_str, IN_X, IN_Y, IN_WIDTH);
    place_cursor();
}

// 398f:03ae
void TextFile_ReadTextLine(const uint8_t *name, int16_t line, uint8_t *s)
{
    char path[300] = "text\\";
    SDL_memcpy(path + 5, name + 1, name[0]);
    path[5 + name[0]] = '\0';

    // Reset on a missing file is run-time error 2 at the following CheckIO.
    size_t size;
    uint8_t *data = fs_load(path, &size);
    if (!data)
        System_RunError(2);
    // Every line up to `line` is read into s (ReadStr, ReadLn) and only the
    // last is decoded in place, so s keeps the raw tail of longer earlier
    // lines past its length, as in the original. line <= 0: s is only decoded.
    size_t pos = 0;
    for (int16_t n = 1; n <= line; n++) {
        uint8_t len = 0;  // ReadStr: up to 255 characters before CR / ^Z
        while (pos < size && data[pos] != '\r' && data[pos] != 0x1A && len < 255)
            s[1 + len++] = data[pos++];
        s[0] = len;
        while (pos < size && data[pos] != '\r' && data[pos] != 0x1A)  // ReadLn
            pos++;
        if (pos < size && data[pos] == '\r') {
            pos++;
            if (pos < size && data[pos] == '\n')
                pos++;
        }
    }
    SDL_free(data);
    TextFile_DecodeLine(s);
}

// 398f:046f
void TextFile_ShowBuildingInfo(int16_t skip)
{
    // Lines 1.. of the file are numbered after skip, 6 per building block.
    for (int16_t i = 1; i <= 6; i++) {
        char out[TEXT_LINE_MAX];
        if (!text_read_line("text\\sz_felsz.txt", skip + i, out))
            System_RunError(2);
        uint8_t line[256];
        size_t n = SDL_strlen(out);
        line[0] = (uint8_t)n;
        SDL_memcpy(line + 1, out, n);
        if (i == 1)
            Gfx_DrawTextFC(line, 100, 0x86, 0x23);
        else
            Gfx_DrawTextF9(line, 100, (int16_t)((i - 1) * 9 + 0x8B), 0x23);
    }
}

// 398f:0552
void TextFile_DrawReply(const uint8_t *s, int16_t x, int16_t y, int16_t colour, bool f1, bool f2,
                        int16_t *code, int16_t *lines)
{
    uint8_t work[256];
    uint8_t rows[8][51];  // string[50], 1..7

    SDL_memcpy(work, s, (size_t)s[0] + 1);
    // The code is the first two characters (past the end they are whatever
    // the stack held; zero here, which Val rejects).
    uint8_t digits[3] = {2, s[0] >= 1 ? s[1] : 0, s[0] >= 2 ? s[2] : 0};
    int16_t err;
    *code = (int16_t)System_ValLong((const PString *)digits, &err);
    System_Delete((PString *)work, 1, 3);

    *lines = 0;
    for (int i = 1; i <= 7; i++)
        rows[i][0] = 0;
    while (work[0] != 0 && *lines < 7) {
        (*lines)++;
        uint8_t *row = rows[*lines];
        while (work[0] != 0 && work[1] != '|') {
            if (row[0] < 50)
                row[1 + row[0]++] = work[1];
            System_Delete((PString *)work, 1, 1);
        }
        if (work[0] != 0)
            System_Delete((PString *)work, 1, 1);
    }

    for (int16_t i = 1; i <= *lines; i++) {
        int16_t ly = (int16_t)(y + (i - 1) * 9);
        if (f2) {
            if (f1)
                Gfx_TypeTextFC(rows[i], x, ly, colour);
            else
                Gfx_TypeTextF9(rows[i], x, ly, colour);
        } else {
            if (f1)
                Gfx_DrawTextFC(rows[i], x, ly, colour);
            else
                Gfx_DrawTextF9(rows[i], x, ly, colour);
        }
    }
}

// 398f:07fe
int16_t TextFile_CountLines(const uint8_t *s)
{
    int16_t n = 0;
    for (uint16_t i = 1; i <= s[0]; i++) {
        if (s[i] == '|')
            n++;
    }
    return (int16_t)(n + 1);
}

// 398f:0864
void TextFile_DecodeLine(uint8_t *s)
{
    for (uint16_t i = 1; i <= s[0]; i++)
        s[i] = (uint8_t)((uint16_t)(0x5780 - i * 0x4F + s[i]) % 0xE0 - 0x20);
}
