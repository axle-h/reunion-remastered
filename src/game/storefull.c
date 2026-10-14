#include "game/storefull.h"

#include <SDL3/SDL.h>

#include "game/dseg.h"
#include "game/groups.h"
#include "game/improvements.h"
#include "game/lang.h"
#include "game/message.h"
#include "game/planetinfo.h"

enum : int { BODIES_MAX = 32 };  // in a system's table

typedef struct BodyState {
    uint8_t full;  // bit ore - 1
    bool armed;
} BodyState;

static BodyState state[8][BODIES_MAX];

static ptrdiff_t index_of(int16_t system, int16_t planet, int16_t moon)
{
    return PlanetInfo_BodyAt(system, planet, moon) - ds.Galaxy_Bodies[system - 1];
}

static void warn(int16_t system, int16_t planet, int16_t moon)
{
    char name[64], t[128];
    PlanetInfo_GetBodyFullName(name, sizeof name, system, planet, moon);
    SDL_snprintf(t, sizeof t, "%s%s", Lang_Text("Storage is full on "), name);
    Message_AddMessageLogEntry(2, t, "message");
}

void StoreFull_Check(int16_t system, int16_t planet, int16_t moon, int16_t ore, int32_t stock,
                     int32_t cap)
{
    BodyState *s = &state[system - 1][index_of(system, planet, moon)];
    uint8_t bit = (uint8_t)(1u << (ore - 1));
    if (stock >= cap) {
        if (s->full & bit)
            return;
        s->full |= bit;
        if (s->armed) {
            s->armed = false;
            if (improvements.storage_full_message)
                warn(system, planet, moon);
        }
    } else if (s->full & bit) {
        s->full &= (uint8_t)~bit;
        s->armed = true;
    }
}

// The body's state from its stores: ores 2..6 if it has droids and the
// deposit, ore 1 if it has a Derrick at work.
static void seed(int16_t system, int16_t planet, int16_t moon, bool derrick)
{
    const TBody *b = PlanetInfo_BodyAt(system, planet, moon);
    BodyState *s = &state[system - 1][index_of(system, planet, moon)];
    bool mined = ds.Galaxy_SystemStatus[system - 1] == 1 && b->Owner == 1 && b->Droids != 0;
    int32_t cap = (int32_t)(uint16_t)Groups_PlanetStorage(system, planet, moon) * 1000;
    const int32_t *stores = system == 1 && planet == 5 && moon == 0 ? ds.Game_HomeStock : b->Stores;
    s->full = 0;
    for (int16_t ore = 1; ore <= 6; ore++) {
        bool produced = ore == 1 ? derrick : mined && b->Deposits[ore - 1] != 0;
        if (produced && stores[ore - 1] >= cap)
            s->full |= (uint8_t)(1u << (ore - 1));
    }
    s->armed = s->full == 0;
}

void StoreFull_Reset(void)
{
    bool derricks[8][BODIES_MAX] = {};
    for (int16_t i = 1; i <= ds.Game_BuildingCount; i++) {
        const TBuilding *bl = &ds.Game_Buildings[i - 1];
        if (bl->Kind == 5 && bl->BuildLeft == 0 && bl->Powered != 0)
            derricks[bl->System - 1][index_of(bl->System, bl->Planet, bl->Moon)] = true;
    }
    for (int16_t s = 1; s <= 8; s++) {
        for (int16_t p = 1; p <= ds.Galaxy_PlanetCount[s - 1]; p++) {
            seed(s, p, 0, derricks[s - 1][index_of(s, p, 0)]);
            for (int16_t m = 1; m <= ds.Galaxy_MoonLists[s - 1][p - 1].Count; m++)
                seed(s, p, m, derricks[s - 1][index_of(s, p, m)]);
        }
    }
}
