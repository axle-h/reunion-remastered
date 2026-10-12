#include "game/options.h"

#include <string.h>

#include "game/dseg.h"
#include "game/fixes.h"
#include "game/gfx.h"
#include "platform/config.h"

// The values the frozen stocks are held at.
static int32_t money_frozen, ore_frozen[6];
static bool rebase, running, fixes_changed, cheats_changed;
static int add_money, add_ore;

void Options_Init(void)
{
    rebase = true;
    running = false;
    fixes_changed = cheats_changed = false;
    add_money = add_ore = 0;
}

void Options_Request(OptionsRequest request)
{
    switch (request) {
    case OPTIONS_CHEATS: cheats_changed = true; break;
    case OPTIONS_ADD_MONEY_ONCE: add_money++; break;
    case OPTIONS_ADD_ORE_ONCE: add_ore++; break;
    case OPTIONS_FIXES: fixes_changed = true; break;
    }
}

void Options_Rebase(void)
{
    rebase = true;
}

bool Options_GameRunning(void)
{
    return running;
}

void Options_Frame(void)
{
    running = true;
    if (fixes_changed)
        Fixes_Init();
    fixes_changed = false;

    // Switching a freeze on (or a new game under it) holds today's stock.
    if (rebase || cheats_changed) {
        money_frozen = ds.Game_Credits;
        memcpy(ore_frozen, ds.Game_HomeStock, sizeof ore_frozen);
    }
    rebase = cheats_changed = false;
    for (; add_money > 0; add_money--) {
        ds.Game_Credits += OPTIONS_ADD_MONEY;
        money_frozen += OPTIONS_ADD_MONEY;
        Gfx_DrawCredits();
    }
    for (; add_ore > 0; add_ore--) {
        for (int i = 0; i < 6; i++) {
            ds.Game_HomeStock[i] += OPTIONS_ADD_ORE;
            ore_frozen[i] += OPTIONS_ADD_ORE;
        }
    }

    // A freeze only stops the stock falling: income, or ore mined or
    // unloaded at New Earth, raises the value held. The main loop's hourly
    // update draws the credits on any screen too.
    if (config.freeze_money && ds.Game_Credits > money_frozen)
        money_frozen = ds.Game_Credits;
    if (config.freeze_money && ds.Game_Credits != money_frozen) {
        ds.Game_Credits = money_frozen;
        Gfx_DrawCredits();
    }
    if (config.freeze_ore)
        for (int i = 0; i < 6; i++) {
            if (ds.Game_HomeStock[i] > ore_frozen[i])
                ore_frozen[i] = ds.Game_HomeStock[i];
            ds.Game_HomeStock[i] = ore_frozen[i];
        }
}
