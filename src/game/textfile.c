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

static void underline(uint8_t colour)
{
    Mouse_HideCursor();
    int16_t x = ds.TextFile_CursorX, y = ds.TextFile_CursorY;
    Gfx_DrawLine(gfx_screen(), x, y, (int16_t)(x + 5), y, colour);
    Mouse_ShowCursor();
}

static void draw_input(void)
{
    char s[256];
    Gfx_DrawTextFC(pstr_get(s, sizeof s, ds.TextFile_InputStr), ds.TextFile_InputX,
                   ds.TextFile_InputY, ds.TextFile_InputWidth);
}

// The cursor sits after the last character, or on it when the field is full.
static void place_cursor(void)
{
    uint8_t len = ds.TextFile_InputStr[0];
    if (len < ds.TextFile_InputWidth)
        ds.TextFile_CursorX = (int16_t)(len * 6 + ds.TextFile_InputX);
    else
        ds.TextFile_CursorX = (int16_t)(len * 6 + ds.TextFile_InputX - 6);
    ds.TextFile_CursorY = (int16_t)(ds.TextFile_InputY + 8);
}

// 398f:0000
void TextFile_InputLine(uint8_t *s, int16_t x, int16_t y, int16_t width, bool canCancel)
{
    TextFile_BeginInput(s, x, y, width);
    ds.TextFile_InputEscaped = 1;
    ds.TextFile_InputCanCancel = canCancel;
    while (ds.TextFile_InputActive != 0) {
        TextFile_InputTick();
        Gfx_WaitVRetrace();
    }
    if (!canCancel) {
        // Dead code in practice: InputActive is already 0 here.
        while (ds.TextFile_InputEscaped != 0 && ds.TextFile_InputActive != 0) {
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
    // The original stores the string (up to 256 bytes) into a 60-byte block;
    // the strings edited are at most 19 characters. The block here holds any.
    ds.TextFile_InputBackup = game_alloc(0x100);
    ds.TextFile_InputX = x;
    ds.TextFile_InputY = y;
    ds.TextFile_InputWidth = width;
    ds.TextFile_InputStr = s;
    SDL_memmove(ds.TextFile_InputBackup, s, (size_t)s[0] + 1);
    ds.TextFile_InputBlink = 0;
    ds.TextFile_InputCanCancel = 0;
    draw_input();
    ds.TextFile_InputActive = 1;
    ds.TextFile_InputEscaped = 0;
    place_cursor();
}

// 398f:011a
void TextFile_EndInput(void)
{
    if (ds.TextFile_InputActive != 0) {
        underline(0xFC);
        draw_input();
        game_free(ds.TextFile_InputBackup);
        ds.TextFile_InputActive = 0;
    }
}

// 398f:0179
void TextFile_InputTick(void)
{
    ds.TextFile_InputBlink++;
    if (ds.TextFile_InputBlink > 20)
        ds.TextFile_InputBlink = 1;
    Gfx_WaitVRetrace();
    if (ds.TextFile_InputBlink == 1)
        underline(0xFD);
    if (ds.TextFile_InputBlink == 10)
        underline(0xFC);
    if (!TextFile_KeyPressed())
        return;

    uint8_t key = (uint8_t)TextFile_ReadKey();
    if (key == 0) {
        TextFile_ReadKey();  // extended key: the scan code is dropped
        return;
    }
    ds.TextFile_InputBlink = 0;
    underline(0xFC);
    uint8_t *s = ds.TextFile_InputStr;
    if (key == 8 && s[0] != 0)
        s[0]--;  // Delete(s, Length(s), 1)
    if (key == 0x0D && s[0] != 0) {
        TextFile_EndInput();
        ds.TextFile_InputActive = 0;
        ds.TextFile_InputEscaped = 0;
    }
    if (key == 0x1B && ds.TextFile_InputCanCancel != 0) {
        if (ds.TextFile_InputEscaped != 0)
            SDL_memmove(s, ds.TextFile_InputBackup, (size_t)ds.TextFile_InputBackup[0] + 1);
        TextFile_EndInput();
        ds.TextFile_InputActive = 0;
        ds.TextFile_InputEscaped = 1;
    }
    if (key == ' ' || (key > 0x60 && key < 0x7B) || (key > 0x40 && key < 0x5B)
        || (key > 0x2F && key < 0x3A)) {
        if (s[0] < ds.TextFile_InputWidth) {
            if (s[0] < 255) {
                s[1 + s[0]] = (char)key;
                s[0]++;
            }
        } else {
            s[ds.TextFile_InputWidth] = key;  // s[width]: overwrite the last character
        }
    }
    draw_input();
    place_cursor();
}

// 398f:03ae. The original reads every line up to `line` into s (ReadStr,
// ReadLn) and decodes only the last in place, so s keeps the raw tail of
// longer earlier lines past its length; nothing reads it. Every line of the
// game's files fits the strings it is read into.
void TextFile_ReadTextLine(const char *name, int16_t line, char *s, size_t cap)
{
    char path[300];
    SDL_snprintf(path, sizeof path, "text\\%s", name);

    // Reset on a missing file is run-time error 2 at the following CheckIO.
    size_t size;
    uint8_t *data = fs_load(path, &size);
    if (!data)
        System_RunError(2, path);
    size_t pos = 0, len = SDL_strlen(s);  // line <= 0: s is only decoded
    for (int16_t n = 1; n <= line; n++) {
        len = 0;  // ReadStr: up to 255 characters before CR / ^Z
        while (pos < size && data[pos] != '\r' && data[pos] != 0x1A && len < 255) {
            if (len < cap - 1)
                s[len] = (char)data[pos];
            len++;
            pos++;
        }
        while (pos < size && data[pos] != '\r' && data[pos] != 0x1A)  // ReadLn
            pos++;
        if (pos < size && data[pos] == '\r') {
            pos++;
            if (pos < size && data[pos] == '\n')
                pos++;
        }
    }
    SDL_free(data);
    len = SDL_min(len, cap - 1);
    TextFile_DecodeLine(s, len);
    s[len] = '\0';
}

// 398f:046f
void TextFile_ShowBuildingInfo(int16_t skip)
{
    // Lines 1.. of the file are numbered after skip, 6 per building block.
    for (int16_t i = 1; i <= 6; i++) {
        char line[TEXT_LINE_MAX];
        if (!text_read_line("text\\sz_felsz.txt", skip + i, line))
            System_RunError(2, "text\\sz_felsz.txt");
        if (i == 1)
            Gfx_DrawTextFC(line, 100, 0x86, 0x23);
        else
            Gfx_DrawTextF9(line, 100, (int16_t)((i - 1) * 9 + 0x8B), 0x23);
    }
}

// 398f:0552
void TextFile_DrawReply(const char *s, int16_t x, int16_t y, int16_t colour, bool f1, bool f2,
                        int16_t *code, int16_t *lines)
{
    char rows[8][51] = {};  // string[50], 1..7
    size_t len = SDL_min(SDL_strlen(s), (size_t)255);

    *code = TextFile_LineCode(s);
    // Delete(s, 1, 3), then lines up to '|', each cut to 50 characters.
    const char *p = s + SDL_min(len, (size_t)3), *end = s + len;
    *lines = 0;
    while (p < end && *lines < 7) {
        (*lines)++;
        char *row = rows[*lines];
        size_t n = 0;
        for (; p < end && *p != '|'; p++) {
            if (n < 50)
                row[n++] = *p;
        }
        if (p < end)
            p++;
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

int16_t TextFile_LineCode(const char *s)
{
    if (s[0] == '\0' || s[1] == '\0')
        return 0;
    char digits[3] = {s[0], s[1], '\0'};
    int16_t err;
    return (int16_t)System_ValLong(digits, &err);
}

// 398f:07fe
int16_t TextFile_CountLines(const char *s)
{
    int16_t n = 0;
    for (size_t i = 0; s[i] != '\0' && i < 255; i++) {
        if (s[i] == '|')
            n++;
    }
    return (int16_t)(n + 1);
}

// 398f:0864
void TextFile_DecodeLine(char *s, size_t len)
{
    for (uint16_t i = 1; i <= len; i++)
        s[i - 1] = (char)((uint16_t)(0x5780 - i * 0x4F + (uint8_t)s[i - 1]) % 0xE0 - 0x20);
}
