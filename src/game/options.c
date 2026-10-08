#include "game/options.h"

#include <string.h>

#include "game/dseg.h"
#include "game/gfx.h"
#include "game/system.h"

Options options;

// The values the frozen stocks are held at.
static int32_t money_frozen, ore_frozen[6];

enum : uint8_t { FLAG_MONEY = 1, FLAG_ORE = 2, VERSION = 1 };

// Ui tables indexed by command id: label string[17] at DS:488A, icon at
// DS:4E06, button sound string[8] at DS:4E56.
static void define_command(int id, const PString *label, uint8_t icon)
{
    System_StrStore(label, (PString *)ds_at((uint16_t)(0x488A + id * 18)), 17);
    *(uint8_t *)ds_at((uint16_t)(0x4E06 + id)) = icon;
    System_StrStore(PSTR("x"), (PString *)ds_at((uint16_t)(0x4E56 + id * 9)), 8);
}

void Options_Init(void)
{
    options = (Options){0};
    define_command(OPTIONS_COMMAND, PSTR("OPTIONS          "), OPTIONS_ICON);
    define_command(OPTIONS_BACK, PSTR("BACK TO DISK OPS "), 54);  // RETREAT's arrow

    // Screen record 0Ch (DS:50FB + 25 * screen): count at +0Ch, ids after it.
    // The original has BACK, LOAD, SAVE, GAME CREDITS, EXIT TO DOS.
    uint8_t *rec = (uint8_t *)ds_at(0x50FB + 0x0C * 25);
    if (rec[0x0C] < 6) {
        rec[0x0D + rec[0x0C]] = OPTIONS_COMMAND;
        rec[0x0C]++;
    }
}

void Options_SetFreezeMoney(bool on)
{
    options.freeze_money = on;
    money_frozen = ds.Game_Credits;
}

void Options_SetFreezeOre(bool on)
{
    options.freeze_ore = on;
    memcpy(ore_frozen, ds.Game_HomeStock, sizeof ore_frozen);
}

void Options_AddMoney(void)
{
    ds.Game_Credits += OPTIONS_ADD_MONEY;
    money_frozen += OPTIONS_ADD_MONEY;
    Gfx_DrawCredits();
}

void Options_AddOre(void)
{
    for (int i = 0; i < 6; i++) {
        ds.Game_HomeStock[i] += OPTIONS_ADD_ORE;
        ore_frozen[i] += OPTIONS_ADD_ORE;
    }
}

void Options_ApplyCheats(void)
{
    // The main loop's hourly update draws the credits on any screen too.
    if (options.freeze_money && ds.Game_Credits != money_frozen) {
        ds.Game_Credits = money_frozen;
        Gfx_DrawCredits();
    }
    if (options.freeze_ore)
        memcpy(ds.Game_HomeStock, ore_frozen, sizeof ore_frozen);
}

size_t Options_SaveState(uint8_t out[OPTIONS_SAVE_SIZE])
{
    if (!options.freeze_money && !options.freeze_ore)
        return 0;
    memcpy(out, "RNCH", 4);
    out[4] = VERSION;
    out[5] = (uint8_t)((options.freeze_money ? FLAG_MONEY : 0) | (options.freeze_ore ? FLAG_ORE : 0));
    return OPTIONS_SAVE_SIZE;
}

void Options_LoadState(const uint8_t *in, size_t size)
{
    bool valid = size >= OPTIONS_SAVE_SIZE && memcmp(in, "RNCH", 4) == 0 && in[4] == VERSION;
    // The loaded stocks are what stays frozen.
    Options_SetFreezeMoney(valid && (in[5] & FLAG_MONEY));
    Options_SetFreezeOre(valid && (in[5] & FLAG_ORE));
}
