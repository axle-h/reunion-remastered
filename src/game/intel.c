// Unit Intel (29b9): see intel.h.
#include "game/intel.h"

#include <string.h>

#include "game/alientalk.h"
#include "game/fixes.h"
#include "game/groups.h"
#include "game/message.h"
#include "game/planetinfo.h"
#include "game/research.h"
#include "game/surface.h"
#include "game/system.h"

// Unnamed globals: story timers (word) with a "done" flag byte behind each.
#define U16(off) (*(uint16_t *)ds_at(off))
#define U8(off) (*(uint8_t *)ds_at(off))

static FarPtr far_add(FarPtr p, int32_t delta)
{
    // The offset wraps at 64 KB like the original's 16-bit arithmetic.
    return far_ptr((uint16_t)(p >> 16), (uint16_t)((uint16_t)p + delta));
}

// Galaxy::Bodies[system]^[idx]
static TBody *body_by_index(int16_t system, int16_t idx)
{
    return far_host(far_add(ds.Galaxy_Bodies[system - 1], idx * 0x41 - 0x41));
}

// Galaxy::MoonLists[system]^[planet]
static uint8_t *moon_list(int16_t system, int16_t planet)
{
    return far_host(far_add(ds.Galaxy_MoonLists[system - 1], planet * 9 - 9));
}

// The planet (moon = 0) or the moon's body record.
static TBody *body_at(int16_t system, int16_t planet, int16_t moon)
{
    if (moon == 0)
        return body_by_index(system, planet);
    return body_by_index(system, moon_list(system, planet)[moon]);
}

static const uint8_t *ps(const PString *p) { return &p->len; }

// 29b9:0000
void Intel_FirstContact(int16_t race, const uint8_t *msg)
{
    PString text;
    memcpy(&text, msg, (size_t)msg[0] + 1);
    if (race == 0 || Intel_Race(race)->Status >= 1)
        return;
    Message_ShowMessage(ps(&text));
    TRace *r = Intel_Race(race);
    r->Status = 4;
    if (race == 3 || race == 0xC)
        r->Status = 2;
    if (race == 7 || race == 8 || race == 9 || race == 10)
        r->Status = 2;
    if (race == 4 || race == 0xB)
        r->Status = 6;
    if (race == 2) {
        if (ds.Research_Products[7].State == 0 && ds.Game_IdeaTimer[7] == -1) {
            Intel_ScheduleIdea(8, 0x14, 0x14);
            Message_ShowMessageLine(5);
        }
        if (U8(0x5D50) == 0)
            U16(0x5D4E) = (uint16_t)(System_Random(0x46) + 0x28A);
        if (U8(0x5D56) == 0)
            U16(0x5D54) = (uint16_t)(U16(0x5D4E) + 0x2D0 + System_Random(0x32));
        if (U8(0x5D5A) == 0)
            U16(0x5D58) = (uint16_t)(U16(0x5D54) + 0x4C4 + System_Random(100));
        if (U8(0x5D5E) == 0)
            U16(0x5D5C) = (uint16_t)(U16(0x5D58) + 100 + System_Random(0x14));
    }
    if (race == 3 && ds.Research_Products[7].State == 0 && ds.Game_IdeaTimer[7] == -1)
        Message_ShowMessageLine(5);
    if (race == 4) {
        Message_ShowMessageLine(0xF);
        AlienTalk_StartAlienTalk(5);
        if (U8(0x5D78) == 0)
            U16(0x5D76) = (uint16_t)(System_Random(500) + 0x5DC);
    }
    if (race == 5 && U8(0x5D84) == 0)
        U16(0x5D82) = (uint16_t)(System_Random(0x14) + 0x14);
    if (race == 7 || race == 8 || race == 9 || race == 10) {
        if (U8(0x5D98) == 0 && (int16_t)U16(0x5D96) < 1) {
            Message_ShowMessageLine(0x1C);
            if (U8(0x5D98) == 0)
                U16(0x5D96) = (uint16_t)(System_Random(0x32) + 200);
        }
    }
    if (race == 0xB) {
        Message_ShowMessageLine(0x1D);
        AlienTalk_StartAlienTalk(10);
    }
    if (race == 0xC) {
        Message_ShowMessageLine(0x26);
        ds.Races_Races[10].Status = 2;
        Intel_ScheduleForceLaunch(0xC, 3, (int16_t)(System_Random(200) + 1000), 2);
    }
}

// 29b9:0217
void Intel_ClearBody(int16_t system, int16_t planet, int16_t moon, bool full)
{
    TBody *b = body_at(system, planet, moon);
    b->Owner = 0;
    b->_01 = 0;
    b->_04[0] = b->_04[1] = 0;
    b->Colony = 0;
    b->Miner = 0;
    if (full) {
        b->_0A = 0;
        b->_0B = 0;
        b->Population = 0;
        b->Resistance = 0;
        b->TechLevel = 0;
        b->Attitude = 0;
    }
    for (int k = 0; k < 8; k++)
        b->Stores[k] = 0;
}

// 29b9:0316
void Intel_DispatchForce(int16_t race, int16_t force, int16_t system, int16_t planet, int16_t moon,
                         uint8_t nextMission)
{
    Intel_SetForceTarget(race, force, system, planet, moon);
    TRaceForce *f = Intel_Force(race, force);
    f->Timer = (int16_t)(System_Random((uint16_t)(100 - Intel_Race(race)->Speed)) + 5);
    f->NextMission = nextMission;
}

// 29b9:0379
void Intel_DispatchForceNow(int16_t race, int16_t force, int16_t system, int16_t planet, int16_t moon,
                            uint8_t mission, int16_t unused)
{
    (void)unused;
    Intel_DispatchForce(race, force, system, planet, moon, mission);
    TRaceForce *f = Intel_Force(race, force);
    f->Mission = f->NextMission;
}

// 29b9:03b3
void Intel_ScheduleForceLaunch(int16_t race, int16_t force, int16_t delay, int16_t mission)
{
    ds.Intel_LaunchRace = race;
    ds.Intel_LaunchForce = force;
    ds.Intel_LaunchTimer = delay;
    ds.Intel_LaunchMission = mission;
}

// Forces of races with this Status that are present (Active 1, Mission > 1) at the battle: they
// land at home when the battle was for their home body, else retreat there.
static void forces_react(int8_t status, int16_t system, int16_t planet)
{
    for (int16_t race = 2; race <= 12; race++) {
        TRace *r = Intel_Race(race);
        if (r->Status != status || r->ForceCount <= 0)
            continue;
        for (int16_t f = 1; f <= r->ForceCount; f++) {
            TRaceForce *rf = Intel_Force(race, f);
            if (rf->Active == 1 && rf->Mission > 1 && rf->System == system && rf->Body == planet) {
                if (r->HomeSystem == system && r->HomeBody == planet)
                    Intel_ForceLands(race, f);
                else
                    Intel_SendForceHome(race, f);
            }
        }
    }
}

// 29b9:03d2: the player won a space battle at (system, planet).
void Intel_SpaceBattleWon(int16_t system, int16_t planet)
{
    forces_react(2, system, planet);
}

// 29b9:0487: the player lost: his stationary groups there are disbanded (Earth) or sent home.
void Intel_SpaceBattleLost(int16_t system, int16_t planet)
{
    int16_t i = 1;
    while (i <= ds.Game_GroupCount) {
        TGroup *g = Groups_At(0, i);
        if (g->Status < 3 && g->System == system && g->Planet == planet) {
            if (system == 1 && planet == 5) {
                Intel_DisbandGroup(0, i);
            } else {
                Intel_SendGroupHome(i);
                i++;
            }
        } else {
            i++;
        }
    }
    forces_react(6, system, planet);
}

// 29b9:059e
void Intel_ClearBodyFull(int16_t system, int16_t planet, int16_t moon)
{
    Intel_ClearBody(system, planet, moon, true);
}

// 29b9:05b5: clears the body and removes its buildings and planet force.
void Intel_PurgeBody(int16_t system, int16_t planet, int16_t moon)
{
    Intel_ClearBody(system, planet, moon, true);
    int16_t i = 1;
    while (i <= ds.Game_BuildingCount) {
        const TBuilding *b = (const TBuilding *)far_host(ds.Game_Buildings) + (i - 1);
        if (b->System == system && b->Planet == planet && b->Moon == moon)
            Surface_DeleteBuilding(i);
        else
            i++;
    }
    i = 1;
    while (i <= ds.Game_PlanetForceCount) {
        const TGroup *g = Groups_At(1, i);
        if (g->System == system && g->Planet == planet && g->Moon == moon)
            Intel_DisbandGroup(1, i);
        else
            i++;
    }
}

// 29b9:0666: leaves the planet screens; losing Earth (1,5,0) is game over (screen 23h).
void Intel_DestroyBody(int16_t system, int16_t planet, int16_t moon)
{
    int16_t s = ds.Game_CurrentScreen;
    if (s == 4 || s == 8 || s == 0xD || s == 0x10 || s == 0x11 || s == 0x14 || s == 0x16) {
        ds.Game_CurrentScreen = 1;
        ds.Game_ScreenSwitch = true;
    }
    Intel_PurgeBody(system, planet, moon);
    if (system == 1 && planet == 5 && moon == 0) {
        ds.Game_CurrentScreen = 0x23;
        ds.Game_ScreenSwitch = true;
    }
}

// Garrison stores of a new colony: base byte table at `base_off` + k, random range at `range_off` + k (k = 1..8).
static uint32_t stock(uint16_t base_off, uint16_t range_off, int k)
{
    return U8((uint16_t)(base_off + k)) + System_Random(U8((uint16_t)(range_off + k)));
}

// 29b9:06d4
void Intel_ClaimBodyForRace(int16_t owner, int16_t system, int16_t planet, int16_t moon)
{
    Intel_ClearBody(system, planet, moon, true);
    TBody *b = body_at(system, planet, moon);
    b->Owner = (uint8_t)owner;
    b->Colony = 1;
    b->Satellite = 0;
    b->_09 = 0;
    b->Population = System_Random(3000) + 2000;
    b->Resistance = 0x14;
    b->TechLevel = 3;
    b->Attitude = 0x1E;
    if (owner == 3) {
        for (int k = 1; k <= 8; k++)
            b->Stores[k - 1] = (int32_t)stock(0x88F, 0x89F, k);
        if (U8(0x5D6C) == 0) {
            // The first set is overwritten (its random numbers are still drawn).
            for (int k = 1; k <= 8; k++)
                b->Stores[k - 1] = (int32_t)stock(0x897, 0x89F, k);
        }
    }
    if (owner == 7) {
        for (int k = 1; k <= 8; k++)
            b->Stores[k - 1] = (int32_t)stock(0x8A7, 0x8AF, k);
    }
    if (owner == 8) {
        // BUG?: every pass writes Stores[1]; the other seven are left alone.
        for (int k = 1; k <= 8; k++)
            b->Stores[0] = (int32_t)stock(0x8B7, 0x8BF, k);
    }
    if (owner == 0xC) {
        // BUG?: same as for owner 8.
        for (int k = 1; k <= 8; k++)
            b->Stores[0] = (int32_t)stock(0x8C7, 0x8CF, k);
    }
}

// 29b9:08f5: the body becomes the player's colony with a planet force named "<body> forces".
void Intel_ClaimBodyForPlayer(int16_t system, int16_t planet, int16_t moon)
{
    Intel_ClearBody(system, planet, moon, true);
    TBody *b = body_at(system, planet, moon);
    b->Owner = 1;
    b->Colony = 1;
    b->_09 = 0;
    b->Population = System_Random(3000) + 2000;
    b->Resistance = 0x14;
    b->TechLevel = 3;
    b->Attitude = 0x1E;
    Surface_AutoPlaceBuilding(1, system, planet, moon);
    ds.Game_GroupList = 1;
    ds.Game_ShownGroupCount = (int16_t)(ds.Game_PlanetForceCount + 1);
    ds.Game_PlanetForceCount = ds.Game_ShownGroupCount;
    ds.Game_SelGroup = ds.Game_ShownGroupCount;
    ds.Game_SelGroup1 = ds.Game_ShownGroupCount;
    Groups_ClearGroup();
    PString name, tmp;
    PlanetInfo_GetBodyName(&tmp, system, planet, moon);
    System_StrStore(&tmp, &name, 255);
    TGroup *g = Groups_At(1, ds.Game_SelGroup);
    System_StrLoad(&tmp, &name);
    System_StrConcat(&tmp, PSTR(" forces"));
    System_StrStore(&tmp, (PString *)&g->Name, 17);
    g->Kind = 5;
    g->Status = 7;
    g->System = (uint8_t)system;
    g->Planet = (uint8_t)planet;
    g->Moon = (uint8_t)moon;
}

// 29b9:0a60
void Intel_ForceLands(int16_t race, int16_t force)
{
    TRaceForce *f = Intel_Force(race, force);
    TBody *b = body_at(f->System, f->Body, f->Moon);
    if ((int8_t)b->Owner == race) {
        // Units[1..4] add to Stores[1..4], Units[5..8] to Stores[5..8] (16-bit unsigned counts).
        for (int k = 0; k < 8; k++)
            b->Stores[k] = (int32_t)((uint32_t)b->Stores[k] + (uint16_t)f->Units[k]);
    }
    f->Active = 0;
}

// Slot (row t = 1..2, unit u = 1..4) of a group: ShipType and Weapons[1..4] as five words.
static int16_t *group_slot(TGroup *g, int t, int u)
{
    return (int16_t *)((uint8_t *)g + t * 40 + u * 10 - 21);
}

// 29b9:0b93
void Intel_DisbandGroup(int16_t list, int16_t group)
{
    if (list == 0) {
        TGroup *g = Groups_At(0, group);
        int16_t pf_index = Groups_FindPlanetForce(g->System, g->Planet, g->Moon);
        if (pf_index > 0) {
            TGroup *pf = Groups_At(1, pf_index);
            for (int t = 1; t <= 2; t++) {
                if (g->Kind == 1 || (g->Kind == 3 && t == 1)) {
                    for (int u = 1; u <= 4; u++) {
                        int16_t *src = group_slot(g, t, u);
                        int16_t *dst = group_slot(pf, t, u);
                        for (int w = 0; w < 5; w++)
                            dst[w] = (int16_t)(dst[w] + src[w]);
                    }
                }
            }
        }
    }
    // The original shifts up to the shown list's count, whichever list that
    // is. Port fix (game/fixes.h): this list's own count.
    int16_t count = !fixes.disband_list_count ? ds.Game_ShownGroupCount
                    : list == 0               ? ds.Game_GroupCount
                                              : ds.Game_PlanetForceCount;
    for (int16_t i = group; i <= count - 1; i++)
        System_MoveFwd(Groups_At(list, i + 1), Groups_At(list, i), sizeof(TGroup));
    if (list == 0)
        ds.Game_GroupCount--;
    else
        ds.Game_PlanetForceCount--;
    if (ds.Game_GroupList == list)
        ds.Game_ShownGroupCount--;
    ds.Game_SelGroup = 0;
    ds.Game_SelGroup0 = 0;
    ds.Game_SelGroup1 = 0;
}

// 29b9:0d5a
void Intel_SendForceHome(int16_t race, int16_t force)
{
    TRace *r = Intel_Race(race);
    Intel_SetForceTarget(race, force, r->HomeSystem, r->HomeBody, 0);
}

// 29b9:0d87: sends a fleet to Earth.
void Intel_SendGroupHome(int16_t group)
{
    Intel_SetGroupTarget(group, 1, 5, 0);
}

// 29b9:0dab: new destination for a race force; travel time 6-10 same body, 20-29 same system, 50-69 else.
void Intel_SetForceTarget(int16_t race, int16_t force, int16_t system, int16_t planet, int16_t moon)
{
    TRaceForce *f = Intel_Force(race, force);
    if (f->System == system && f->Body == planet && f->Moon == moon)
        return;
    if (f->System == system) {
        if (f->Body == planet)
            f->_03 = (uint16_t)(System_Random(5) + 6);
        else
            f->_03 = (uint16_t)(System_Random(10) + 0x14);
    } else {
        f->_03 = (uint16_t)(System_Random(0x14) + 0x32);
    }
    f->System = (uint8_t)system;
    f->Body = (uint8_t)planet;
    f->Moon = (uint8_t)moon;
    f->Active = 2;
    f->Timer = 0;
    f->NextMission = 0;
}

// 29b9:0e66: new destination for a player group: Leg1 / Jump / Leg2 and Status 4 (in system) or 6 (hyperspace).
void Intel_SetGroupTarget(int16_t group, int16_t system, int16_t planet, int16_t moon)
{
    TGroup *g = Groups_At(0, group);
    if (g->System == system && g->Planet == planet && g->Moon == moon)
        return;
    if (g->System == system) {
        if (g->Planet == planet) {
            g->Leg1 = 0;
            g->Jump = 0;
            g->Leg2 = (int16_t)(System_Random(0x14) + 0x50);
        } else {
            g->Leg1 = 0;
            g->Jump = 0;
            g->Leg2 = (int16_t)(System_Random(0x32) + 0x96);
        }
        g->Status = 4;
    } else {
        g->Leg1 = (int16_t)(System_Random(0x32) + 200);
        g->Jump = 1;
        g->Leg2 = (int16_t)(System_Random(0x32) + 200);
        g->Status = 6;
    }
    g->System = (uint8_t)system;
    g->Planet = (uint8_t)planet;
    g->Moon = (uint8_t)moon;
}

// 29b9:0f53
void Intel_ClearForce(int16_t race, int16_t force)
{
    memset(Intel_Force(race, force), 0, sizeof(TRaceForce));
}

// 29b9:0fe4: the race is destroyed: Status -1, all its bodies and forces cleared.
void Intel_EliminateRace(int16_t race)
{
    TRace *r = Intel_Race(race);
    r->_0F = 0;
    r->Speed = 0;
    r->HomeSystem = 0;
    r->HomeBody = 0;
    r->Status = -1;
    r->Credits = 0;
    memset(r->_20, 0xFF, sizeof r->_20);
    for (int16_t s = 1; s <= 8; s++) {
        int16_t n = ds.Galaxy_PlanetCount[s - 1];
        for (int16_t p = 1; p <= n; p++) {
            if ((int8_t)body_by_index(s, p)->Owner == race)
                Intel_ClearBody(s, p, 0, true);
        }
    }
    for (int16_t s = 1; s <= 8; s++) {
        int16_t n = ds.Galaxy_PlanetCount[s - 1];
        for (int16_t p = 1; p <= n; p++) {
            uint8_t moons = moon_list(s, p)[0];
            for (int16_t m = 1; m <= moons; m++) {
                if ((int8_t)body_at(s, p, m)->Owner == race)
                    Intel_ClearBody(s, p, m, true);
            }
        }
    }
    int16_t forces = r->ForceCount;
    for (int16_t f = 1; f <= forces; f++)
        Intel_ClearForce(race, f);
    r->ForceCount = 0;
}

// 29b9:1179: wipes system 4: buildings, bodies, race forces and the player's groups there.
void Intel_AnnihilateSystem4(void)
{
    // Port fix (game/fixes.h): leaves the screens that hold building or
    // group indices, as Intel_DestroyBody does.
    int16_t s = ds.Game_CurrentScreen;
    if (fixes.annihilate_leaves_screen
        && (s == 4 || s == 8 || s == 0xD || s == 0x10 || s == 0x11 || s == 0x14 || s == 0x16)) {
        ds.Game_CurrentScreen = 1;
        ds.Game_ScreenSwitch = true;
    }
    int16_t i = 1;
    while (i <= ds.Game_BuildingCount) {
        const TBuilding *b = (const TBuilding *)far_host(ds.Game_Buildings) + (i - 1);
        if (b->System == 4)
            Surface_DeleteBuilding(i);
        else
            i++;
    }
    int16_t planets = ds.Galaxy_PlanetCount[3];
    for (int16_t p = 1; p <= planets; p++)
        Intel_ClearBody(4, p, 0, true);
    planets = ds.Galaxy_PlanetCount[3];
    for (int16_t p = 1; p <= planets; p++) {
        uint8_t moons = moon_list(4, p)[0];
        for (int16_t m = 1; m <= moons; m++)
            Intel_ClearBody(4, p, m, true);
    }
    for (int16_t race = 2; race <= 12; race++) {
        int16_t n = Intel_Race(race)->ForceCount;
        for (int16_t f = 1; f <= n; f++) {
            if (Intel_Force(race, f)->System == 4)
                Intel_ClearForce(race, f);
        }
    }
    i = 1;
    while (i <= ds.Game_GroupCount) {
        TGroup *g = Groups_At(0, i);
        if (g->System == 4) {
            if (g->Status < 3) {
                Intel_DisbandGroup(0, i);
            } else {
                Intel_SendGroupHome(i);
                i++;
            }
        } else {
            i++;
        }
    }
    i = 1;
    while (i <= ds.Game_PlanetForceCount) {
        if (Groups_At(1, i)->System == 4)
            Intel_DisbandGroup(1, i);
        else
            i++;
    }
}

// 29b9:136e: unless the idea is already known or scheduled, it becomes available in base + Random(range) hours.
void Intel_ScheduleIdea(int16_t idea, int16_t delayBase, int16_t delayRandom)
{
    if (ds.Research_Products[idea - 1].State == 0 && ds.Game_IdeaTimer[idea - 1] == -1)
        ds.Game_IdeaTimer[idea - 1] = (int16_t)(delayBase + System_Random((uint16_t)delayRandom));
}

// Redraws what shows an idea's state: the research screen, the group panel.
static void idea_changed(int16_t idea, int16_t kind)
{
    if (ds.Game_CurrentScreen == 3) {
        Research_ResetSelectionFrame();
        ds.Game_SelInvention = idea;
        Research_DrawInventionInfo();
        Research_SetupResearchHotspots();
        Research_PlayIconTransitionOut(kind, idea);
    }
    if (ds.Game_CurrentScreen == 0x16) {
        Groups_SetupGroupsHotspots();
        Groups_DrawGroupPanel(true);
    }
}

// 29b9:13ab: state 0 -> done (5).
void Intel_GrantInvention(int16_t idea)
{
    TProduct *p = &ds.Research_Products[idea - 1];
    if (p->State == 0) {
        p->Progress = 0;
        p->State = 5;
        idea_changed(idea, 1);
    }
}

// 29b9:1409: state 0 -> can be analysed (3).
void Intel_OfferInventionForAnalysis(int16_t idea)
{
    TProduct *p = &ds.Research_Products[idea - 1];
    if (p->State == 0) {
        p->State = 3;
        idea_changed(idea, 3);
    }
}

// 29b9:1461
bool Intel_IsRaceActive(int16_t race)
{
    int8_t status = Intel_Race(race)->Status;
    return status == 2 || status == 4;
}

// 29b9:1501 (nested in ShowRaceIntel; `race` and `kind` come from its frame)
static void show_race_body_info(int16_t race, int16_t kind, int16_t system, int16_t planet, int16_t moon,
                                int16_t body)
{
    PString tmp, title, msg, num;
    PlanetInfo_GetBodyLongName(&tmp, system, planet, moon);
    System_StrStore(PSTR(""), &title, 50);
    System_StrStore(&tmp, &title, 50);
    const TBody *b = body_by_index(system, body);
    if ((int8_t)b->Owner != race || b->Colony == 0)
        return;
    const PString *name = (const PString *)&Intel_Race(race)->Name;
    if (kind == 1) {
        System_StrLoad(&tmp, PSTR("    Information on: "));
        System_StrConcat(&tmp, name);
        System_StrConcat(&tmp, PSTR("|"));
        System_StrStore(&tmp, &msg, 255);
        if (b->_01 != 0) {
            System_StrLoad(&tmp, &msg);
            System_StrConcat(&tmp, PSTR("  Their main planet is | "));
            System_StrConcat(&tmp, &title);
            System_StrConcat(&tmp, PSTR(""));
            System_StrStore(&tmp, &msg, 255);
        } else {
            System_StrLoad(&tmp, &msg);
            System_StrConcat(&tmp, PSTR("    Their colony is on| "));
            System_StrConcat(&tmp, &title);
            System_StrConcat(&tmp, PSTR(""));
            System_StrStore(&tmp, &msg, 255);
        }
        Message_ShowMessage(ps(&msg));
    }
    if (kind == 3) {
        // BUG?: the "Information on" line is built and then replaced by the next one.
        System_StrLoad(&tmp, PSTR("    Information on: "));
        System_StrConcat(&tmp, name);
        System_StrStore(&tmp, &msg, 255);
        System_StrLoad(&tmp, PSTR("  Their garrison troops are on| "));
        System_StrConcat(&tmp, &title);
        System_StrConcat(&tmp, PSTR(": "));
        System_StrStore(&tmp, &msg, 255);
        for (int i = 1; i <= 4; i++) {
            if (i & 1) {
                System_StrLoad(&tmp, &msg);
                System_StrConcat(&tmp, PSTR("|"));
                System_StrStore(&tmp, &msg, 255);
            }
            System_StrLong(b->Stores[i - 1], 3, &num, 255);
            System_StrLoad(&tmp, &msg);
            System_StrConcat(&tmp, &num);
            System_StrConcat(&tmp, PSTR("  "));
            System_StrConcat(&tmp, (const PString *)&ds.Names_ShipClasses[i - 1]);
            System_StrConcat(&tmp, PSTR(" "));
            System_StrStore(&tmp, &msg, 255);
        }
        for (int i = 1; i <= 4; i++) {
            if (i & 1) {
                System_StrLoad(&tmp, &msg);
                System_StrConcat(&tmp, PSTR("|"));
                System_StrStore(&tmp, &msg, 255);
            }
            System_StrLong(b->Stores[i + 3], 3, &num, 255);
            System_StrLoad(&tmp, &msg);
            System_StrConcat(&tmp, &num);
            System_StrConcat(&tmp, PSTR("  "));
            System_StrConcat(&tmp, (const PString *)&ds.Names_GroundUnits[i - 1]);
            System_StrConcat(&tmp, PSTR("  "));
            System_StrStore(&tmp, &msg, 255);
        }
        Message_ShowMessage(ps(&msg));
    }
}

// 29b9:185e (nested in ShowRaceIntel)
static void show_race_army_info(int16_t race)
{
    PString tmp, msg, num;
    const TRace *r = Intel_Race(race);
    const PString *name = (const PString *)&r->Name;
    int16_t forces = r->ForceCount;
    for (int16_t f = 1; f <= forces; f++) {
        const TRaceForce *rf = &r->Forces[f - 1];
        if (rf->Mission == 0 || rf->Active == 0)
            continue;
        System_StrLong(f, 0, &num, 255);
        System_StrLoad(&tmp, PSTR(" Information on: "));
        System_StrConcat(&tmp, name);
        System_StrConcat(&tmp, PSTR(" |"));
        System_StrStore(&tmp, &msg, 255);
        // BUG?: the first line is replaced by the next one.
        System_StrLoad(&tmp, PSTR("  The "));
        System_StrConcat(&tmp, &num);
        System_StrConcat(&tmp, PSTR(". army power is  "));
        System_StrStore(&tmp, &msg, 255);
        for (int k = 1; k <= 4; k++) {
            if (k & 1) {
                System_StrLoad(&tmp, &msg);
                System_StrConcat(&tmp, PSTR("|"));
                System_StrStore(&tmp, &msg, 255);
            }
            System_StrLong((uint16_t)rf->Units[k - 1], 3, &num, 255);
            System_StrLoad(&tmp, &msg);
            System_StrConcat(&tmp, &num);
            System_StrConcat(&tmp, PSTR("  "));
            System_StrConcat(&tmp, (const PString *)&ds.Names_ShipClasses[k - 1]);
            System_StrConcat(&tmp, PSTR(" "));
            System_StrStore(&tmp, &msg, 255);
        }
        for (int k = 1; k <= 4; k++) {
            if (k & 1) {
                System_StrLoad(&tmp, &msg);
                System_StrConcat(&tmp, PSTR("|"));
                System_StrStore(&tmp, &msg, 255);
            }
            System_StrLong((uint16_t)rf->Units[k + 3], 3, &num, 255);
            System_StrLoad(&tmp, &msg);
            System_StrConcat(&tmp, &num);
            System_StrConcat(&tmp, PSTR("  "));
            System_StrConcat(&tmp, (const PString *)&ds.Names_GroundUnits[k - 1]);
            System_StrConcat(&tmp, PSTR("  "));
            System_StrStore(&tmp, &msg, 255);
        }
        Message_ShowMessage(ps(&msg));
    }
}

// 29b9:1aed (nested in ShowRaceIntel)
static void show_race_weapon_info(int16_t race)
{
    PString tmp, msg;
    const TRace *r = Intel_Race(race);
    const uint8_t *flags = (const uint8_t *)r->KnownSystems; // [0..3] ship classes, [4..7] ground units
    System_StrLoad(&tmp, PSTR(" Information on: "));
    System_StrConcat(&tmp, (const PString *)&r->Name);
    System_StrStore(&tmp, &msg, 255);
    // BUG?: replaces the line above.
    System_StrStore(PSTR("    Their weapons are |"), &msg, 255);
    for (int k = 1; k <= 4; k++) {
        if (flags[k - 1] != 0) {
            System_StrLoad(&tmp, &msg);
            System_StrConcat(&tmp, PSTR("  "));
            System_StrConcat(&tmp, (const PString *)&ds.Names_ShipClasses[k - 1]);
            System_StrConcat(&tmp, PSTR(" "));
            System_StrStore(&tmp, &msg, 255);
        }
    }
    System_StrLoad(&tmp, &msg);
    System_StrConcat(&tmp, PSTR("|"));
    System_StrStore(&tmp, &msg, 255);
    for (int k = 1; k <= 4; k++) {
        if (flags[k + 3] != 0) {
            System_StrLoad(&tmp, &msg);
            System_StrConcat(&tmp, PSTR("  "));
            System_StrConcat(&tmp, (const PString *)&ds.Names_GroundUnits[k - 1]);
            System_StrConcat(&tmp, PSTR("  "));
            System_StrStore(&tmp, &msg, 255);
        }
    }
    Message_ShowMessage(ps(&msg));
}

// 29b9:1c49
void Intel_ShowRaceIntel(int16_t race, int16_t kind)
{
    if (kind == 1 || kind == 3) {
        for (int16_t s = 1; s <= 8; s++) {
            int16_t n = ds.Galaxy_PlanetCount[s - 1];
            for (int16_t p = 1; p <= n; p++) {
                const uint8_t *ml = moon_list(s, p);
                show_race_body_info(race, kind, s, p, 0, p);
                uint8_t moons = ml[0];
                for (int16_t m = 1; m <= moons; m++)
                    show_race_body_info(race, kind, s, p, m, ml[m]);
            }
        }
    }
    if (kind == 2)
        show_race_army_info(race);
    if (kind == 4)
        show_race_weapon_info(race);
}

// 29b9:1d23
void Intel_Init(void)
{
    // The group screen calls this through a far code pointer; code segments
    // use Ghidra's numbering here.
    ds.Groups_IntelHook = far_ptr(0x29B9, 0x0B93);
}
