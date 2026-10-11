// saveload: LoadInit from the game data, a 41,670-byte save with the layout of
// docs/formats/save.md, and a save/load round trip.
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "game/dseg.h"
#include "game/ems.h"
#include "game/saveload.h"
#include "game/system.h"
#include "platform/fs.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static uint8_t *read_file(const char *path, size_t *size)
{
    return SDL_LoadFile(path, size);
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    mem_init();
    if (!dseg_init()) {
        printf("no game data, skipping\n");
        return 77;
    }
    system_init();
    Ems_Init();
    ds.Message_Log = System_GetMem(0x31B);

    // Never touch the real save directory.
    char dir[128];
    SDL_snprintf(dir, sizeof dir, "/tmp/reunion_test_saveload_%d", (int)SDL_GetCurrentTime(nullptr) & 0xFFFF);
    SaveLoad_SetSaveDir(dir);

    SaveLoad_LoadInit();
    CHECK(ds.Game_BuildingCount == 23);
    CHECK(ds.Game_Buildings[0].Kind == 1);

    // Some recognisable state in the places the layout test looks at.
    ds.Game_Credits = 120000;
    ds.Game_Year = 2927; ds.Game_Month = 8; ds.Game_Day = 13; ds.Game_Hour = 23;
    ds.Game_StoryStage = 1;
    ds.Intel_LaunchTimer = 0x1234;
    ds.Advisor_TrainingPrice = (int32_t)0xCAFEF00D;
    ds.Message_Log[5] = 0x5A;
    ((uint8_t *)ds.Game_Groups)[100] = 0xA5;

    SaveLoad_ReadSlotNames();
    CHECK(ds.SaveLoad_SelectedSlot == 1);
    uint8_t *slot = ds.SaveLoad_SlotTable;
    CHECK(slot[0] == 0 && slot[1] == 20);   // "Empty---------------"
    memcpy(slot + 1, "\x06" "Hello!", 7);

    CHECK(SaveLoad_Transfer(true));
    char path[256];
    SDL_snprintf(path, sizeof path, "%s/spidysav.1", dir);
    size_t size = 0;
    uint8_t *f = read_file(path, &size);
    CHECK(f && size == SAVELOAD_FILE_SIZE);
    if (f && size == SAVELOAD_FILE_SIZE) {
        CHECK(f[0] == 6 && memcmp(f + 1, "Hello!", 6) == 0);
        CHECK(*(int32_t *)(f + 0x388C) == 120000);
        CHECK(*(uint16_t *)(f + 0x38A8) == 2927 && *(uint16_t *)(f + 0x38AA) == 8
              && *(uint16_t *)(f + 0x38AC) == 13 && *(uint16_t *)(f + 0x38AE) == 23);
        CHECK(*(int16_t *)(f + 0x3952) == 23);
        CHECK(f[0x3954] == ((uint8_t *)ds.Game_Buildings)[0]);
        CHECK(f[0x0038 + 5] == 0x5A);
        CHECK(f[0x79DE + 100] == 0xA5);
        CHECK(*(int16_t *)(f + 0xA269) == 1);
    }

    // Round trip: wreck the state, reload, compare with a snapshot.
    uint8_t *snap = SDL_malloc(sizeof(DSeg));
    uint8_t *bsnap = SDL_malloc(14000), *gsnap = SDL_malloc(0x2840), *msnap = SDL_malloc(0x31B);
    memcpy(snap, &ds, sizeof(DSeg));
    memcpy(bsnap, ds.Game_Buildings, 14000);
    memcpy(gsnap, ds.Game_Groups, 0x2840);
    memcpy(msnap, ds.Message_Log, 0x31B);

    ds.Game_Credits = 1;
    ds.Game_BuildingCount = 0;
    memset(ds.Game_Buildings, 0xEE, 14000);
    memset(ds.Game_Groups, 0xEE, 0x2840);
    memset(ds.Message_Log, 0xEE, 0x31B);
    // The story timers up to Research::Products, and Game's state from
    // SelInvention to PressHotspot.
    uint8_t *story = (uint8_t *)&ds.Intel_LaunchTimer, *game = (uint8_t *)&ds.Game_SelInvention;
    memset(story, 0xEE, (size_t)((uint8_t *)&ds.Research_Products - story));
    memset(game, 0xEE, (size_t)((uint8_t *)&ds.Game_PressHotspot - game));
    SaveLoad_ReadSlotNames();   // now sees the file
    slot = ds.SaveLoad_SlotTable;
    CHECK(slot[0] == 1 && slot[1] == 6 && memcmp(slot + 2, "Hello!", 6) == 0);
    CHECK(SaveLoad_Transfer(false));
    CHECK(ds.Game_Credits == 120000 && ds.Game_BuildingCount == 23);
    CHECK(memcmp(ds.Game_Buildings, bsnap, 14000) == 0);
    CHECK(memcmp(ds.Game_Groups, gsnap, 0x2840) == 0);
    CHECK(memcmp(ds.Message_Log, msnap, 0x31B) == 0);
    // Every saved block of the DS mirror is back (the slot table pointer differs).
    CHECK(ds.Intel_LaunchTimer == 0x1234 && ds.Advisor_TrainingPrice == (int32_t)0xCAFEF00D);
    // SelInvention .. Credits.
    size_t game_len = (size_t)((uint8_t *)(&ds.Game_Credits + 1) - game);
    CHECK(memcmp(game, snap + offsetof(DSeg, Game_SelInvention), game_len) == 0);

    // Saving again is stable.
    SaveLoad_SetSaveDir(dir);
    CHECK(SaveLoad_Transfer(true));
    size_t size2 = 0;
    uint8_t *f2 = read_file(path, &size2);
    CHECK(f2 && f && size2 == size && memcmp(f, f2, size) == 0);

    SDL_free(f); SDL_free(f2); SDL_free(snap); SDL_free(bsnap); SDL_free(gsnap); SDL_free(msnap);
    SDL_RemovePath(path);
    SDL_RemovePath(dir);
    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
