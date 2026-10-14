#include "game/autosave.h"

#include <SDL3/SDL.h>

#include "game/dseg.h"
#include "game/gfx.h"
#include "game/improvements.h"
#include "game/lang.h"
#include "game/saveload.h"

enum : int { WEEK = 7 };  // midnights

static struct {
    void *data;  // the copy
    size_t size;
    uint32_t id, written;  // the copy's number; the last one written
    int midnights;         // since the file was last written
    bool quick_save;       // asked for
} autosave;

static void drop_copy(void)
{
    SDL_free(autosave.data);
    autosave.data = nullptr;
}

// The game as it is now, named for the slot.
static void *save_now(size_t *size)
{
    char text[32];
    SDL_snprintf(text, sizeof text, "%s %d-%d-%d", Lang_Text("Autosave"), ds.Game_Year,
                 ds.Game_Month, ds.Game_Day);
    uint8_t name[0x14] = {};
    size_t len = SDL_min(SDL_strlen(text), sizeof name - 1);
    name[0] = (uint8_t)len;
    SDL_memcpy(name + 1, text, len);
    return SaveLoad_SaveToMemory(name, size);
}

static void take_copy(void)
{
    drop_copy();
    autosave.data = save_now(&autosave.size);
    autosave.id++;
}

static bool write_file(const void *data, size_t size)
{
    if (!SaveLoad_WriteAutosave(data, size)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Autosave: %s", SDL_GetError());
        return false;
    }
    autosave.midnights = 0;
    return true;
}

static void write_copy(void)
{
    if (autosave.data && autosave.written != autosave.id) {
        write_file(autosave.data, autosave.size);
        autosave.written = autosave.id;
    }
}

void Autosave_Reset(void)
{
    autosave.midnights = 0;
    autosave.quick_save = false;
    if (improvements.autosave)
        take_copy();
    else
        drop_copy();
}

void Autosave_HourStart(void)
{
    if (!improvements.autosave) {
        drop_copy();
        return;
    }
    take_copy();
    if (ds.Game_Hour == 23 && ++autosave.midnights >= WEEK)
        write_copy();
}

void Autosave_ScreenEntered(int16_t screen)
{
    if (improvements.autosave && (screen == 19 || screen == 29 || screen == 32 || screen == 34))
        write_copy();
}

void Autosave_QuickSaveKey(bool waiting)
{
    if (!improvements.autosave)
        return;
    if (!waiting)
        Gfx_DrawStatusText(Lang_Text("Can't save now"));
    else
        autosave.quick_save = true;
}

void Autosave_Frame(void)
{
    if (!autosave.quick_save)
        return;
    autosave.quick_save = false;
    size_t size;
    void *data = ds.Game_ClockRunning ? save_now(&size) : nullptr;
    bool saved = data && write_file(data, size);
    SDL_free(data);
    Gfx_DrawStatusText(Lang_Text(saved ? "Game saved" : "Can't save now"));
}
