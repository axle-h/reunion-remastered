// message: a message box draws into the framebuffer and restores the
// background; logging, the edit field and the log redraw.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/dseg.h"
#include "game/le.h"
#include "game/main_program.h"
#include "game/gfx.h"
#include "game/message.h"
#include "game/system.h"
#include "game/textfile.h"
#include "platform/fs.h"
#include "platform/video.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static const char *queue;
static size_t qpos;
static int polls;
static uint8_t seen_fill, seen_outline;

static bool wait_retrace(void)
{
    return true;
}

// Pressed on the third poll, so the box has been drawn by then.
static bool key_pressed(void)
{
    if (queue[qpos] == '\0')
        return ++polls >= 3;
    return true;
}

static char read_key(void)
{
    if (queue[qpos] == '\0')
        return '\r';
    return queue[qpos++];
}

static bool key_pressed_snapshot(void)
{
    seen_fill = vid_screen[114][150];
    seen_outline = vid_screen[112][142];
    return key_pressed();
}

static void reset(void)
{
    dseg_clear();
    vid_clear();
    dseg_init();
    system_init();
    Game_InitGameData();
    Message_Init();
    ds.Gfx_LineStride = 320;
    ds.Gfx_LineStrideCopy = 320;
    ds.Mouse_CursorState = 1;
    ds.Mouse_BtnDown = 0;
    gfx_wait_retrace = wait_retrace;
    TextFile_KeyPressed = key_pressed;
    TextFile_ReadKey = read_key;
    queue = "";
    qpos = 0;
    polls = 0;
    for (int y = 0; y < 200; y++)
        for (int x = 0; x < 320; x++)
            vid_screen[y][x] = (uint8_t)(x * 7 + y * 3 + 40);
}

static bool screen_is_pattern(void)
{
    bool bad = false;
    // Everything but the status bar text (rows 37..44) must be untouched.
    for (int y = 0; y < 200; y++) {
        if (y >= 37 && y < 45)
            continue;
        for (int x = 0; x < 320; x++) {
            if (vid_screen[y][x] != (uint8_t)(x * 7 + y * 3 + 40)) {
                printf("pixel %d,%d differs: %d want %d\n", x, y, vid_screen[y][x], (uint8_t)(x * 7 + y * 3 + 40));
                bad = true;
            }
        }
    }
    return !bad;
}

static void box(void)
{
    reset();
    uint32_t used = system_heap_used();
    TextFile_KeyPressed = key_pressed_snapshot;
    Message_ShowMessageNoLog("Hello|World");
    CHECK(seen_fill == 2);      // box background
    CHECK(seen_outline != 2);   // outline or the pattern, not the fill
    CHECK(screen_is_pattern());  // background restored
    CHECK(system_heap_used() == used);
    CHECK(ds.Message_LogCount == 0);
}

static void logging(void)
{
    reset();
    Message_ShowMessage(" Hello||  World");
    // Empty lines are skipped and leading blanks removed; kind 99.
    CHECK(ds.Message_LogCount == 2);
    uint8_t *e1 = ds.Message_Log;
    CHECK(rd16le(e1) == 99 && e1[2] == 5 && SDL_memcmp(e1 + 3, "Hello", 5) == 0);
    uint8_t *e2 = e1 + 0x35;
    CHECK(rd16le(e2) == 99 && e2[2] == 5 && SDL_memcmp(e2 + 3, "World", 5) == 0);

    // The log keeps 15 entries, dropping the oldest.
    for (int i = 0; i < 20; i++)
        Message_AddMessageLogEntry(2, "abc", "");
    CHECK(ds.Message_LogCount == 15);
    CHECK(rd16le(ds.Message_Log) == 2);

    // On screen 0Bh the log is drawn.
    ds.Game_CurrentScreen = 0x0B;
    Message_AddMessageLogEntry(1, "xyz", "");
    ds.Game_CurrentScreen = 0;
}

static void editfield(void)
{
    reset();
    uint8_t name[13] = {0};
    queue = "Bob\r";
    Message_MessageBox("Name: &xxxxxxxx", "", false, name,
                       true, 9);
    CHECK(name[0] == 3 && SDL_memcmp(name + 1, "Bob", 3) == 0);
    CHECK(screen_is_pattern());
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    box();
    logging();
    editfield();
    return ok ? 0 : 1;
}
