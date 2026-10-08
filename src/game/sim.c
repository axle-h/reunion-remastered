// Unit Sim (1b8a): the hourly simulation. See sim.h for the call order.
//
// Ported from the disassembly (the decompile drops code in places: e.g. it
// misses ProcessAllBodyEvents adding the day's taxes to Game_Credits).
// Globals without a name in DSeg are reached with ds_at() and named in a
// comment; most of them sit in the story-timer block DS:5D48..5DAB, which
// overlaps the folded base of Research_Products (Products[0] would start at
// 5D77), so products are addressed as DS:5D77 + 35h*p for p = 0 as well.
#include "game/sim.h"

#include "game/blit.h"
#include "game/cargo.h"
#include "game/colonize.h"
#include "game/droids.h"
#include "game/fixes.h"
#include "game/dseg.h"
#include "game/gfx.h"
#include "game/gfxpath.h"
#include "game/groups.h"
#include "game/mem.h"
#include "game/message.h"
#include "game/minmax.h"
#include "game/planetinfo.h"
#include "game/research.h"
#include "game/satellite.h"
#include "game/shipinfo.h"
#include "game/shipmove.h"
#include "game/shop.h"
#include "game/sound.h"
#include "game/system.h"
#include "game/textfile.h"

// Routines of units that have no header yet (stubs in stubs.c).
void Protect_CheckToken(void);                                         // 24eb:00a5
void Protect2_ArmDateQuestion(void);                                   // 2693:0000
void Protect2_AskDateQuestion(void);                                   // 2693:0085
void Protect2_EnforceDateCheck(void);                                  // 2693:0342
void Protect2_VerifyDateAnswer(void);                                  // 2693:04aa
void Protect2_HaltIfDateCheckFailed(void);                             // 2693:0538
void Credits_EnforceManualCheck(void);                                 // 264a:034a
void Command_ShowHologram(int16_t vec);                                // 3abd:04b4
void AlienTalk_StartAlienScene(int16_t scene);                         // 2e4b:0000
void AlienTalk_StartAlienTalk(int16_t race);                           // 2e4b:03a5
void Intel_FirstContact(int16_t race, const uint8_t *msg);             // 29b9:0000
void Intel_DispatchForce(int16_t race, int16_t force, int16_t system, int16_t planet,
                         int16_t moon, uint8_t nextMission);           // 29b9:0316
void Intel_DispatchForceNow(int16_t race, int16_t force, int16_t system, int16_t planet,
                            int16_t moon, uint8_t mission, int16_t unused); // 29b9:0379
void Intel_ScheduleForceLaunch(int16_t race, int16_t force, int16_t delay,
                               int16_t mission);                       // 29b9:03b3
void Intel_DestroyBody(int16_t system, int16_t planet, int16_t moon);  // 29b9:0666
void Intel_ClaimBodyForRace(int16_t owner, int16_t system, int16_t planet,
                            int16_t moon);                             // 29b9:06d4
void Intel_SetForceTarget(int16_t race, int16_t force, int16_t system, int16_t planet,
                          int16_t moon);                               // 29b9:0dab
void Intel_EliminateRace(int16_t race);                                // 29b9:0fe4
void Intel_AnnihilateSystem4(void);                                    // 29b9:1179
void Intel_ScheduleIdea(int16_t idea, int16_t b, int16_t r);           // 29b9:136e
void Intel_GrantInvention(int16_t idea);                               // 29b9:13ab
void Intel_OfferInventionForAnalysis(int16_t idea);                    // 29b9:1409
void Intel_ShowRaceIntel(int16_t race, int16_t kind);                  // 29b9:1c49
void Surface_RegisterBuildingAnims(int16_t system, int16_t planet, int16_t moon); // 2ef2:062f
void Surface_SetupSurfaceHotspots(void);                               // 2ef2:0791
void Surface_DrawBuildingInfo(void);                                   // 2ef2:0a9d
void Surface_DrawBuildingPreview(int16_t kind);                        // 2ef2:19f7
void Surface_DrawMapView(void);                                        // 2ef2:1da3
void Surface_AutoPlaceBuilding(int16_t k, int16_t s, int16_t p, int16_t m); // 2ef2:29b1
void Surface_DeleteBuilding(int16_t index);                            // 2ef2:2ac5
int16_t Surface_CountBuildings(int16_t kind, int16_t system, int16_t planet, int16_t moon,
                               bool activeOnly);                       // 2ef2:2b1a
void Surface_CountBuildingsByKind(int16_t system, int16_t planet, int16_t moon); // 2ef2:2b9e
void Surface_RebuildRadar(void);                                       // 2ef2:2c52

// ---------------------------------------------------------------------------
// Data access, with the original's address arithmetic (out-of-range indices
// land where they did in DOS).
// ---------------------------------------------------------------------------

static inline uint8_t *b8(uint16_t off) { return (uint8_t *)ds_at(off); }
static inline int8_t *i8(uint16_t off) { return (int8_t *)ds_at(off); }
static inline int16_t *w16(uint16_t off) { return (int16_t *)ds_at(off); }
static inline int32_t *l32(uint16_t off) { return (int32_t *)ds_at(off); }

static inline const uint8_t *ps(const PString *s) { return &s->len; }

// Research::Products[p] (DS:5D77 + 35h*p).
static inline TProduct *product(int16_t p) { return (TProduct *)ds_at((uint16_t)(0x5D77 + p * 0x35)); }
// Races::Races[r] (DS:6A06 + E4h*r); Status = +1Bh.
static inline TRace *race(int16_t r) { return (TRace *)ds_at((uint16_t)(0x6A06 + r * 0xE4)); }
// Galaxy::SystemStatus[s] (DS:4815 + s).
static inline int8_t *system_status(int16_t s) { return i8((uint16_t)(0x4815 + s)); }
// Galaxy::PlanetStatus[s, p] (DS:47CD + 8s + p).
static inline int8_t *planet_status(int16_t s, int16_t p) { return i8((uint16_t)(0x47CD + s * 8 + p)); }
// DS:481C + 2s: completed observatories (building kind 6) in system s,
// counted by UpdateBuildings. Unnamed.
static inline int16_t *observatories(int16_t s) { return w16((uint16_t)(0x481C + s * 2)); }
// Names::Systems[s] (DS:5B65 + 9s).
static inline const PString *system_name(int16_t s) { return (const PString *)ds_at((uint16_t)(0x5B65 + s * 9)); }
// Game::CurPlanet[CurSystem] / CurMoon[CurSystem].
static inline int16_t *cur_planet(void) { return w16((uint16_t)(0x7AE4 + ds.Game_CurSystem * 2)); }
static inline int16_t *cur_moon(void) { return w16((uint16_t)(0x7AF4 + ds.Game_CurSystem * 2)); }
// Game::CurMoon[s] for any system.
static inline int16_t cur_moon_of(int16_t s) { return *w16((uint16_t)(0x7AF4 + s * 2)); }
// Game::ColonySetups[k] (DS:A20C + 10h*k).
static inline TColonySetup *colony_setup(int16_t k) { return (TColonySetup *)ds_at((uint16_t)(0xA20C + k * 16)); }
// Game::HomeStock[i] (DS:95C2 + 4i).
static inline int32_t *home_stock(int16_t i) { return l32((uint16_t)(0x95C2 + i * 4)); }
// Game::Buildings^[i].
static inline TBuilding *building(int16_t i) { return (TBuilding *)far_host(ds.Game_Buildings) + (i - 1); }
// The longint table filled by Surface::CountBuildingsByKind (DS:791C).
static inline int32_t kind_count(uint16_t off) { return *l32(off); }

// Galaxy::Bodies[s]^[idx]: 65-byte records; the offset wraps at 64 KB.
static TBody *body(int16_t s, int16_t idx)
{
    FarPtr p = *(FarPtr *)ds_at((uint16_t)(0x4752 + s * 4));
    return far_host(far_ptr((uint16_t)(p >> 16), (uint16_t)((uint16_t)p + idx * 0x41 - 0x41)));
}

// Galaxy::MoonLists[s]^[planet] (count + 8 body indices).
static uint8_t *moon_list(int16_t s, int16_t planet)
{
    FarPtr p = *(FarPtr *)ds_at((uint16_t)(0x4732 + s * 4));
    return far_host(far_ptr((uint16_t)(p >> 16), (uint16_t)((uint16_t)p + planet * 9 - 9)));
}

// The body at (system, planet, moon): the planet itself for moon 0.
static TBody *body_at(int16_t s, int16_t planet, int16_t moon)
{
    if (moon == 0)
        return body(s, planet);
    return body(s, moon_list(s, planet)[moon]);
}

static inline int32_t lmul(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }

// System.LongDivMod's quotient: run-time error 200 on division by zero.
static int32_t ldiv32(int32_t a, int32_t b)
{
    if (b == 0)
        System_RunError(200);
    return (int32_t)((int64_t)a / b);
}

// a + b + c into a 255-character temporary, then ShowMessage.
static void show3(const PString *a, const PString *b, const PString *c)
{
    PString t;
    System_StrLoad(&t, a);
    System_StrConcat(&t, b);
    System_StrConcat(&t, c);
    Message_ShowMessage(ps(&t));
}

// Jade: Amnesty (system 1) planet 7, the Jaanosians' home world.
enum : int16_t { JADE_PLANET = 7 };

// The game date is before y-m-d 00:00.
static bool date_before(int y, int m, int d)
{
    if (ds.Game_Year != y)
        return ds.Game_Year < y;
    if (ds.Game_Month != m)
        return ds.Game_Month < m;
    return ds.Game_Day < d;
}

static bool at_current_body(int16_t s, int16_t planet, int16_t moon)
{
    return s == ds.Game_CurSystem && planet == *cur_planet() && moon == *cur_moon();
}

// ---------------------------------------------------------------------------

// 1b8a:0000
void Sim_AdvanceHour(void)
{
    ds.Game_Hour++;
    if (ds.Game_Hour > 23) {
        ds.Game_Day++;
        ds.Game_Hour = 0;
    }
    if (ds.Game_Day > 30) {
        ds.Game_Month++;
        ds.Game_Day = 1;
    }
    Protect_CheckToken();
    if (ds.Game_Month > 12) {
        ds.Game_Year++;
        ds.Game_Month = 1;
    }
    Gfx_DrawGameDate();
}

// 1b8a:006e
void Sim_ProcessPubTimers(void)
{
    for (int16_t k = 1; k <= 10; k++) {
        TPubPerson *person = &ds.Pub_People[k - 1];
        if (!((person->State == 0 || person->State == 2) && person->Timer > 0))
            continue;
        if (--person->Timer != 0)
            continue;

        if (k == 2) {
            // An intel deal: Deal = kind*20 + race.
            int16_t r = person->Deal % 20;
            int16_t kind = person->Deal / 20;
            Message_ShowMessageLine(0x27);
            Intel_ShowRaceIntel(r, kind);
            // Races::Hostile[r] (DS:6517 + r)
            if (kind == 1 && *b8((uint16_t)(0x6517 + r)) != 2)
                *b8((uint16_t)(0x6517 + r)) = 1;
            if (kind == 3)
                *b8((uint16_t)(0x6517 + r)) = 2;
        }
        if (k == 5) {
            if (person->Deal == 1)
                ds.Races_Races[5].Status = 6;
            if (person->Deal == 2)
                ds.Races_Races[6].Status = 6;
            Message_ShowMessageLine(0x28);
            if (person->Deal == 1)
                Message_ShowMessageLine(0x2B);
            if (person->Deal == 2) {
                Message_ShowMessageLine(0x2C);
                Intel_GrantInvention(0x1D);
                Intel_OfferInventionForAnalysis(0x1A);
                ds.Pub_People[2].State = 2;
            }
        }
        if (k == 7) {
            *b8(0x5D9A) = 1;  // story flag
            ds.Pub_People[6].State = 1;
            TBody *earth = &ds.Galaxy_SunBodies[2];
            earth->Stores[0] -= 20;
            earth->Stores[2] -= 1;
            earth->Stores[3] -= 2;
            earth->Stores[5] -= 10;
            earth->Stores[6] -= 10;
            earth->Stores[7] -= 10;
            for (int j = 0; j < 8; j++) {
                if (earth->Stores[j] < 0)
                    earth->Stores[j] = 0;
            }
            Message_ShowMessageLine(0x29);
        }

        // Deal > 10: deal Deal-10 at half the amounts.
        int16_t divisor = 1;
        int16_t d = person->Deal;
        if (d > 10)
            divisor = 2;
        if (d > 10)
            d -= 10;
        if (k == 9) {
            TPubDeal *deal = (TPubDeal *)ds_at((uint16_t)(0x40C + d * 0x18));
            if (System_Random(100) < deal->Chance) {
                Message_ShowMessageLine(0x2F);
                for (int16_t j = 1; j <= 4; j++) {
                    uint8_t item = deal->Items[j - 1];
                    if (item == 0)
                        continue;
                    int32_t amount = ldiv32(deal->Amounts[j - 1], divisor);
                    PString num, t;
                    System_StrLong(amount, 0, &num, 0xFF);
                    if (item < 10) {
                        *home_stock(item) += amount;
                        System_StrLoad(&t, PSTR("You got "));
                        System_StrConcat(&t, &num);
                        System_StrConcat(&t, PSTR(" t of "));
                        System_StrConcat(&t, (const PString *)ds_at((uint16_t)(0x5A73 + item * 9)));
                    } else {
                        product((int16_t)(item - 10))->Stock += (int16_t)amount;
                        System_StrLoad(&t, PSTR("You got "));
                        System_StrConcat(&t, &num);
                        System_StrConcat(&t, PSTR(" pieces of "));
                        System_StrConcat(&t, (const PString *)&product((int16_t)(item - 10))->Name);
                        System_StrConcat(&t, PSTR("s"));
                    }
                    Message_ShowMessage(ps(&t));
                }
            } else {
                Message_ShowMessageLine(0x2A);
            }
            deal->Active = 0;
        }
        person->Deal = 0;
        if (ds.Game_CurrentScreen == 0x17) {
            ds.Game_CurrentScreen = -0x17;
            ds.Game_ScreenSwitch = 1;
        }
    }

    for (int16_t k = 1; k <= 10; k++) {
        TPubDeal *deal = &ds.Pub_Deals[k - 1];
        if ((int16_t)deal->Year == (int32_t)ds.Game_Year && deal->Month == ds.Game_Month
            && deal->Day == ds.Game_Day && deal->Hour == ds.Game_Hour) {
            if (System_Random(3) < 4) {  // always
                uint8_t line[256];
                TextFile_ReadTextLine(ps(PSTR("pirate.txt")), k, line);
                Message_ShowMessage(line);
            } else {
                deal->Active = 0;
            }
        }
    }
    Protect2_HaltIfDateCheckFailed();
}

// 1b8a:04d8
void Sim_ProgressResearch(void)
{
    for (int16_t k = 1; k <= 35; k++) {
        TProduct *p = product(k);
        // DS:5D52: story timer that blocks research while it runs (Main);
        // DS:5D9E: staff member away at the university (4 = developer).
        if (*w16(0x5D52) != 0 || *w16(0x5D9E) == 4 || !(p->State == 2 || p->State == 4))
            continue;

        int16_t shortfall = 0;
        for (int j = 0; j < 4; j++) {
            if (ds.Game_DevSkill[j] < p->Skills[j])
                shortfall += (int16_t)(p->Skills[j] - ds.Game_DevSkill[j]);
        }
        // The developer cannot get past this point without the skills.
        int16_t limit = (int16_t)(10000 - 10000 / (shortfall + 1));
        if (p->Progress > limit)
            p->Progress -= (int16_t)ldiv32(lmul(ds.Game_StaffExp[3], 10000), (int16_t)p->DevTime);

        if (p->Progress <= 0) {
            p->Progress = 0;
            p->State = 5;
            ds.Game_CurResearch = 0;
            show3(PSTR("The "), (const PString *)&p->Name, PSTR(" is ready for production"));
            if (ds.Game_CurrentScreen == 3) {
                Research_ResetSelectionFrame();
                ds.Game_SelInvention = k;
                Research_DrawInventionInfo();
                // The original pushes (k, 1); research.h takes them as (kind, inv).
                Research_PlayIconTransitionOut(1, k);
            }
            if (k == 5)
                Intel_ScheduleIdea(6, 0x1E, 10);
            if (k == 5)
                Message_ShowMessageLine(3);
            if (k == 0xE)
                Intel_ScheduleIdea(0xF, 0x32, 0x46);
            if (k == 0xE)
                Message_ShowMessageLine(0xC);
            if (k == 0xD)
                Intel_ScheduleIdea(0xF, 200, 0x1E);
            if (k == 0x17)
                Intel_ScheduleIdea(0x18, 0x32, 0x46);
            if (k == 0x17)
                Message_ShowMessageLine(0x16);
            if (k == 4)
                ds.Game_GroupsEnabled = 1;
            if (k == 6)
                ds.Game_GroupsEnabled = 1;
            if (k == 10)
                ds.Game_GroupKindAllowed[0] = 1;
            if (k == 6)
                ds.Game_GroupKindAllowed[1] = 1;
            if (k == 4)
                ds.Game_GroupKindAllowed[3] = 1;
            if (ds.Game_CurrentScreen == 1)
                Command_ShowHologram(k);
        }
        if (ds.Game_CurrentScreen == 3)
            Research_DrawInventionInfo();
        if (ds.Game_CurrentScreen == 3)
            Research_SetupResearchHotspots();
        if (ds.Game_CurrentScreen == 0x16)
            Groups_SetupGroupsHotspots();
        if (ds.Game_CurrentScreen == 0x16)
            Groups_DrawGroupPanel(true);
    }
    Credits_EnforceManualCheck();
}

// 1b8a:0705
void Sim_ProgressProduction(void)
{
    bool completed = false;
    for (int16_t k = 1; k <= 35; k++) {
        TProduct *p = product(k);
        // Shop_ListMode+1 (DS:79FB): quantity edit active on the shop screen.
        if ((ds.Game_CurrentScreen == 5 && *b8(0x79FB) != 0) || p->_1D == 0)
            continue;
        p->_21 -= ds.Game_ProductionRate;
        // DS:7AB5 + Shop_Sel: the product selected in the shop's list.
        if ((int16_t)p->_21 <= 0) {
            completed = true;
            p->_21 = 0;
            p->Stock++;
            p->_1D--;
            if ((int16_t)p->_1D > 0)
                p->_21 = p->_1F;
            if (ds.Game_CurrentScreen == 5 && *b8((uint16_t)(0x7AB5 + ds.Shop_Sel)) == k
                && (int16_t)p->_1D > 0)
                Shop_DrawQueueNumbers();
        }
        if (ds.Game_CurrentScreen == 5 && *b8((uint16_t)(0x7AB5 + ds.Shop_Sel)) == k) {
            if (p->_1D == 0)
                Shop_DrawNotProducible();
            else
                Shop_DrawQueueTime();
        }
    }
    if (completed) {
        if (ds.Game_CurrentScreen == 4)
            Droids_DrawDroidsScreen();
        if (ds.Game_CurrentScreen == 0x16)
            Groups_DrawGroupPanel(false);
        if (ds.Game_CurrentScreen == 8 || ds.Game_CurrentScreen == 0x14) {
            bool any = false;
            Satellite_AddBodyActionItems(ds.Game_CurSystem, *cur_planet(), *cur_moon(), &any);
            if (any)
                GfxPath_ShowToolbar();
        }
        if (ds.Game_CurrentScreen == 0x11) {
            TGroup *g = Groups_Selected();
            if (g->Status == 1 || g->Status == 2) {
                bool any = false;
                Satellite_AddBodyActionItems(g->System, g->Planet, g->Moon, &any);
                if (any)
                    GfxPath_ShowToolbar();
            }
        }
    }
    if (ds.Game_CurrentScreen == 0xD)
        Cargo_DrawCargoAmounts();
}

// The locals of ProcessAllBodyEvents that its nested procedure uses.
typedef struct BodyEventsFrame {
    int32_t income;     // bp-0Ah: the day's taxes, added to Credits at the end
    uint8_t discovered; // bp-0Bh: a new planet was found today (one per day)
} BodyEventsFrame;

// 1b8a:0cba: nested in ProcessAllBodyEvents; bodyIdx = index in Galaxy::Bodies[system].
static void Sim_ProcessBodyEvents(BodyEventsFrame *parent, int16_t system, int16_t planet,
                                  int16_t moon, int16_t bodyIdx)
{
    PString name;   // string[50]
    PString line;   // bp-116h
    PString t;
    PlanetInfo_GetBodyLongName(&t, system, planet, moon);
    System_StrStore(&t, &name, 0x32);
    const PString *end = PSTR(" ");

    TBody *b = body(system, bodyIdx);
    if (b->Owner == 1 && b->Colony != 0) {
        Surface_CountBuildingsByKind(system, planet, moon);
        // Target attitude; the people's attitude moves towards it.
        int16_t target = (int16_t)((7 - (int8_t)b->TechLevel) * 10);
        int32_t living = lmul(kind_count(0x799C), 7000) + 10000;
        int32_t food = lmul(kind_count(0x79D4), 20000) + lmul(kind_count(0x79C0), 8000) + 20000;
        int32_t medicine = kind_count(0x79E0);
        int32_t hospital = lmul(kind_count(0x79BC), 20000) + 20000;
        if (b->_14 > 0 && b->_14 < 100)
            b->_14--;
        int16_t sick = b->_14 == 0 ? 2 : 1;
        if (b->_14 == 0 && medicine == 0) {
            if (System_Random(5) == 0)
                show3(PSTR("Because of disease your people need a medicine plant|  on "), &name, end);
            target -= 10;
            if (target > 25)
                target = 25;
        }
        if (lmul(sick, b->Population) > hospital) {
            if (System_Random(5) == 0)
                show3(PSTR(" Due to sanitary conditions you need a hospital |  on "), &name, end);
            target -= 10;
            b->Population -= 200;
            if (target > 25)
                target = 25;
        }
        if (b->Population > 30000)
            target -= 10;
        if (b->Population > 60000)
            target -= 10;
        if (b->Population > 100000)
            target -= 20;
        if (b->Population > 150000)
            target -= 20;
        int16_t k8 = *w16(0x79A0), stadium = *w16(0x79A4), k10 = *w16(0x79A8);
        int16_t k14 = *w16(0x79B8), university = *w16(0x79B4);
        if (k8 > 0)
            target += 10;
        if (stadium > 0)
            target += 10;
        if (k10 > 0)
            target += 10;
        if (k14 > 0)
            target += 10;
        if (b->Population > 50000 && stadium == 0) {
            if (System_Random(5) == 0)
                show3(PSTR("  Your people ask for a stadium | on "), &name, end);
            target -= 20;
        }
        if (b->Population > 70000 && university == 0) {
            if (System_Random(5) == 0)
                show3(PSTR("  Your people demand a University | on "), &name, end);
            target -= 20;
        }
        if (planet < 3 && kind_count(0x79CC) == 0) {
            if (System_Random(5) == 0) {
                uint16_t r = System_Random((uint16_t)ldiv32(b->Population, 4));
                b->Population -= (uint16_t)(r + 500);
                Intel_ScheduleIdea(0x1E, 0x32, 0x19);
                show3(PSTR(" Your people are dying because of radiation |  on "), &name, end);
            }
            target -= 40;
        }
        if (b->_14 == 100 && product(28)->State < 5 && ds.Game_CurrentScreen != 0x14
            && System_Random(10) == 0) {
            int16_t hit = -1;
            int16_t count = ds.Game_BuildingCount;
            for (int16_t i = 1; i <= count; i++) {
                TBuilding *bl = building(i);
                if (bl->System == system && bl->Planet == planet && bl->Moon == moon
                    && bl->Health == 0 && bl->Kind != 1 && System_Random(10) == 0)
                    hit = i;
            }
            if (hit == -1) {
                show3(PSTR("  A meteorite hit your colony on | "), &name,
                      PSTR(" |        but causes no harm."));
            } else {
                Surface_DeleteBuilding(hit);
                int16_t n = (int16_t)(Surface_CountBuildings(0x19, system, planet, moon, false)
                                      + Surface_CountBuildings(4, system, planet, moon, false));
                // BUG? decrements the droids of CurBody, not of this body.
                TBody *cur = far_host(ds.Game_CurBody);
                if (cur->_0A > n)
                    cur->_0A--;
                uint16_t r = System_Random((uint16_t)ldiv32(b->Population, 20));
                b->Population -= r;
                Intel_ScheduleIdea(0x1C, 0xFA, 100);
                b->Attitude -= 10;
                show3(PSTR("  One of your buildings has exploded |      due to meteorite impact| on "),
                      &name, end);
            }
            target -= 30;
        }
        if (b->Population > food) {
            if (System_Random(3) == 0)
                show3(PSTR(" Your people are starving |  on "), &name, end);
            target -= 15;
            if (target > 25)
                target = 25;
            b->Attitude -= 2;
        }
        if (b->Population > living) {
            if (System_Random(3) == 0)
                show3(PSTR(" There isn't enough living space|  on "), &name, end);
            target -= 10;
            if (target > 25)
                target = 25;
            b->Attitude -= 2;
        }
        if (target < 0)
            target = 0;

        int32_t maxPop = ldiv32(lmul(b->Attitude + 200, living), 230);
        if (b->Population > food)
            maxPop = MinMax_Min(maxPop, food + 5000);
        maxPop = MinMax_Min(maxPop, ldiv32(hospital, sick) + 5000);
        maxPop = MinMax_Min(maxPop, 200000);
        if (b->Attitude < 30)
            maxPop = ldiv32(maxPop, 2);
        if (b->Attitude < 20)
            maxPop = MinMax_Min(maxPop, ldiv32(b->Population, 2));
        if (b->Attitude < 10)
            maxPop = 500;
        if (university == 0)
            maxPop = MinMax_Min(maxPop, 80000);

        // Taxes: Round(Population * TechLevel * Resistance * (Attitude + 40) / 200000).
        static const Real48 k200000 = {{0x92, 0x00, 0x00, 0x00, 0x50, 0x43}};
        Real48 tech = System_LongToReal((int8_t)b->TechLevel);
        Real48 att = System_LongToReal(b->Attitude + 40);
        Real48 res = System_LongToReal(b->Resistance);
        Real48 x = System_RealMul(System_LongToReal(b->Population), tech);
        x = System_RealMul(x, res);
        x = System_RealMul(x, att);
        parent->income += System_Round(System_RealDiv(x, k200000));

        if (System_Random((uint16_t)(b->Resistance * b->Resistance)) < 100)
            b->Resistance++;
        if (b->Resistance > 60)
            b->Resistance = 60;

        // Logistic growth towards maxPop (or decline above it).
        int32_t pop = b->Population;
        int32_t growth;
        if (maxPop > pop)
            growth = ldiv32(lmul(maxPop - pop, ldiv32(lmul(pop, 1000), maxPop)), maxPop);
        else
            growth = ldiv32(lmul(maxPop - pop, 1000), pop);
        b->Population += growth;

        b->Attitude += (int8_t)((target - b->Attitude) / 10);
        if (b->Attitude < target)
            b->Attitude++;
        if (b->Attitude > target)
            b->Attitude -= 3;
        if (b->Attitude < 0)
            b->Attitude = 0;
        if (b->Attitude > 69)
            b->Attitude = 69;

        // Stores[7] doubles as the revolt counter.
        if (b->Attitude < 10) {
            b->Stores[6]++;
            if (b->Stores[6] > 5)
                b->Stores[6] = 5;
            if (b->Stores[6] == 5 && System_Random(3) < 2)
                b->Stores[6] = 4;
            if (b->Stores[6] == 5) {
                show3(PSTR(" Your people have revolted  |  on "), &name, end);
                Intel_DestroyBody(system, planet, moon);
            } else {
                show3(PSTR(" Your advisors report that| there are rebellious forces |  on "), &name, end);
            }
        } else {
            b->Stores[6] = 0;
        }
        if (b->Population < 1000) {
            show3(PSTR(" Your colony has been destroyed  |  on "), &name, end);
            Intel_DestroyBody(system, planet, moon);
        }
        if (b->Colony != 0 && b->Owner == 1)
            PlanetInfo_BalanceBuildingPower(system, planet, moon);
    }

    // Information level: satellites, droids and colonies find things out.
    b = body(system, bodyIdx);
    if ((int8_t)b->Satellite < 0)
        b->Satellite += 10;
    int16_t gain = 0;
    if ((int8_t)b->Satellite > 0)
        gain = (int16_t)(System_Random(5) + 2);
    if (b->_0A != 0 && b->Owner == 1)
        gain = (int16_t)(System_Random(5) + 3);
    if (b->Colony != 0 && b->Owner == 1)
        gain = (int16_t)(System_Random(5) + 4);
    if ((int8_t)b->Owner >= 2 && race((int8_t)b->Owner)->Status == 0 && b->InfoLevel >= 31)
        gain = 0;
    b->InfoLevel += (int8_t)gain;
    if (b->InfoLevel > 60)
        b->InfoLevel = 60;

    if (b->InfoLevel - gain < 10 && b->InfoLevel >= 10 && (int8_t)b->Owner < 2) {
        System_StrLoad(&t, end);
        System_StrConcat(&t, &name);
        System_StrConcat(&t, b->_03 != 0 ? PSTR(" |    is ready for mining ")
                                         : PSTR(" |    is not suitable for mining "));
        System_StrStore(&t, &line, 0xFF);
        Message_ShowMessage(ps(&line));
        if (product(5)->State == 0 && ds.Game_IdeaTimer[4] == -1 && b->_03 != 0) {
            Intel_ScheduleIdea(5, 0x14, 0x28);
            Message_ShowMessageLine(2);
        }
    }
    if (ds.Game_CurrentScreen == 8 && b->InfoLevel - gain < 6 && b->InfoLevel >= 6
        && PlanetInfo_IsCurrentBody(system, planet, moon))
        PlanetInfo_LoadPlanetPlanetPic((int8_t)b->SurfaceType, true);
    if (b->InfoLevel - gain < 30 && b->InfoLevel >= 30) {
        if ((int8_t)b->Owner <= 1) {
            // Galaxy::SurfaceColonisable[Surface::Tileset[SurfaceType]]
            uint16_t tileset = (uint16_t)*w16((uint16_t)(0x1A38 + (int8_t)b->SurfaceType * 2));
            if (b->Surveyed != 0 && *b8((uint16_t)(0x1A4F + tileset)) != 0)
                show3(end, &name, PSTR(" |    is a suitable place for living"));
            else
                show3(end, &name, PSTR(" |    is not suitable for living"));
        } else {
            show3(PSTR(" Your satellite reports alien activity on | "), &name, end);
            if (PlanetInfo_IsCurrentBody(system, planet, moon) && ds.Game_CurrentScreen == 8)
                PlanetInfo_LoadPlanetFajPic(0xD, true);
        }
    }
    if (b->InfoLevel - gain < 40 && b->InfoLevel >= 40 && (int8_t)b->Owner > 1) {
        System_StrLoad(&t, PSTR(" Your satellite found "));
        System_StrConcat(&t, (const PString *)&race((int8_t)b->Owner)->Name);
        System_StrConcat(&t, PSTR(" colony at | "));
        System_StrConcat(&t, &name);
        System_StrConcat(&t, end);
        Message_ShowMessage(ps(&t));
        if (PlanetInfo_IsCurrentBody(system, planet, moon) && ds.Game_CurrentScreen == 8)
            PlanetInfo_LoadPlanetFajPic((int8_t)b->Owner, true);
    }

    // Miner counts down the hours until a bought colony gets built.
    if (b->Colony == 0 && b->Miner > 0 && ds.Game_CurrentScreen != 0x1C && --b->Miner <= 0) {
        int16_t count = ds.Game_ColonySetupCount;
        for (int16_t k = 1; k <= count; k++) {
            TColonySetup *s = colony_setup(k);
            if (s->System == system && s->Planet == planet && s->Moon == moon) {
                s->Active = 1;
                int16_t at = (int16_t)(System_Random(10) + 0x32);
                for (int j = 0; j < 6; j++) {
                    if (s->Timers[j] != 0) {
                        s->Timers[j] = at;
                        at = (int16_t)(at + 5 + System_Random(5));
                    }
                }
                Surface_AutoPlaceBuilding(1, system, planet, moon);
            }
        }
        System_StrLoad(&t, PSTR("  The building of a colony started  | on "));
        System_StrConcat(&t, &name);
        System_StrConcat(&t, end);
        System_StrStore(&t, &line, 0xFF);
        Message_ShowMessage(ps(&line));
        b->Miner = 0;
        b->Colony = 1;
        b->Population += (uint16_t)(System_Random(3000) + 2000);
        b->Resistance = 20;
        b->TechLevel = 3;
        b->Attitude = 30;
        b->Satellite = 0;
        if (at_current_body(system, planet, moon)) {
            if (ds.Game_CurrentScreen == 8) {
                GfxPath_AddToolbarIcon(0x26);
                GfxPath_AddToolbarIcon(0x27);
                GfxPath_ShowToolbar();
            }
            if (ds.Game_CurrentScreen == 0x14) {
                ds.Surface_CanBuild = 1;
                Surface_DrawBuildingPreview(ds.Surface_SelKind);
                if (ds.Surface_Mode == 1 || ds.Surface_Mode == 2)
                    Surface_SetupSurfaceHotspots();
                GfxPath_AddToolbarIcon(0x42);
                GfxPath_ShowToolbar();
            }
            if (ds.Game_CurrentScreen == 7 || ds.Game_CurrentScreen == 0x12)
                PlanetInfo_SetupSystemViewHotspots();
            if (ds.Game_CurrentScreen == 0x16) {
                Groups_SetupGroupsHotspots();
                Groups_DrawGroupPanel(true);
            }
        }
    }

    bool any = false;
    if ((ds.Game_CurrentScreen == 8 || ds.Game_CurrentScreen == 0x14)
        && at_current_body(system, planet, moon)) {
        Satellite_AddBodyActionItems(system, planet, moon, &any);
        if (any)
            GfxPath_ShowToolbar();
    }
    if (ds.Game_CurrentScreen == 0x11) {
        TGroup *g = Groups_Selected();
        if ((g->Status == 1 || g->Status == 2) && g->System == system && g->Planet == planet
            && g->Moon == moon) {
            Satellite_AddBodyActionItems(system, planet, moon, &any);
            if (any)
                GfxPath_ShowToolbar();
        }
    }

    // Undiscovered planets (status -2/-3/-4): observatories, fleets in the
    // system and the Psy radar find them, at most one a day.
    // Port fix (game/fixes.h): Jade is found by JADE_FOUND_BY at the latest.
    bool jade_due = fixes.find_jade && system == 1 && planet == JADE_PLANET && moon == 0
                    && !date_before(JADE_FOUND_BY_YEAR, JADE_FOUND_BY_MONTH, JADE_FOUND_BY_DAY);
    if ((*observatories(system) > 0 || jade_due) && moon == 0
        && (*planet_status(system, planet) == -2 || *planet_status(system, planet) == -3)
        && *system_status(system) == 1 && (jade_due || System_Random(0x32) == 1)
        && !parent->discovered) {
        *planet_status(system, planet) = 0;
        if (ds.Game_CurrentScreen == 7 || ds.Game_CurrentScreen == 0x12)
            PlanetInfo_SetupSystemViewHotspots();
        show3(PSTR(" Your observatory found a new planet |          in "), system_name(system),
              PSTR(" system"));
        Message_PromptPlanetName(system, planet);
        parent->discovered = 1;
    }
    if (moon == 0 && *planet_status(system, planet) == -2 && *system_status(system) == 1
        && System_Random(5) == 1 && !parent->discovered) {
        any = false;
        int16_t count = ds.Game_GroupCount;
        for (int16_t i = 1; i <= count; i++) {
            TGroup *g = Groups_At(0, i);
            if (g->System == system && g->Status > 2) {
                any = true;
                System_StrStore(g->Kind == 4 ? PSTR("satellite carrier") : PSTR("pilots"), &line, 0xFF);
            }
        }
        if (any) {
            *planet_status(system, planet) = 0;
            if (ds.Game_CurrentScreen == 7 || ds.Game_CurrentScreen == 0x12)
                PlanetInfo_SetupSystemViewHotspots();
            System_StrLoad(&t, PSTR(" Your "));
            System_StrConcat(&t, &line);
            System_StrConcat(&t, PSTR(" found a new planet |        in "));
            System_StrConcat(&t, system_name(system));
            System_StrConcat(&t, PSTR(" system "));
            Message_ShowMessage(ps(&t));
            Message_PromptPlanetName(system, planet);
            parent->discovered = 1;
        }
    }
    if (product(34)->State == 5 && moon == 0 && *planet_status(system, planet) == -4
        && *system_status(system) == 1 && System_Random(10) == 1 && !parent->discovered) {
        *planet_status(system, planet) = 0;
        if (ds.Game_CurrentScreen == 7 || ds.Game_CurrentScreen == 0x12)
            PlanetInfo_SetupSystemViewHotspots();
        show3(PSTR(" Your Psy-radar radar found a new planet |          in "), system_name(system),
              PSTR(" system"));
        Message_PromptPlanetName(system, planet);
        parent->discovered = 1;
    }
}

// 1b8a:206e
void Sim_ProcessAllBodyEvents(void)
{
    BodyEventsFrame frame = {.income = 0, .discovered = 0};
    for (int16_t s = 1; s <= 8; s++) {
        if (*system_status(s) != 1)
            continue;
        int16_t planets = *w16((uint16_t)(0x4774 + s * 2));  // Galaxy::PlanetCount[s]
        for (int16_t p = 1; p <= planets; p++) {
            const uint8_t *moons = moon_list(s, p);
            Sim_ProcessBodyEvents(&frame, s, p, 0, p);
            uint16_t count = moons[0];
            for (uint16_t m = 1; m <= count; m++)
                Sim_ProcessBodyEvents(&frame, s, p, (int16_t)m, moon_list(s, p)[m]);
        }
    }
    ds.Game_Credits += frame.income;
    // PlanetInfo::AlienMode, read as a word.
    if (ds.Game_CurrentScreen == 8 && *w16(0x7B06) == 0)
        PlanetInfo_DrawPlanetInfoText();
    if (ds.Game_CurrentScreen == 8 && *w16(0x7B06) == 1)
        PlanetInfo_DrawStoresList();
}

// 1b8a:2178
void Sim_UpdateBuildings(void)
{
    for (int16_t s = 1; s <= 8; s++)
        *observatories(s) = 0;
    *w16(0x482E) = 0;  // completed buildings of kind 13
    int16_t count = ds.Game_BuildingCount;
    for (int16_t i = 1; i <= count; i++) {
        TBuilding *bl = building(i);
        if (bl->Health == 0) {
            if (bl->Kind == 0xD)
                (*w16(0x482E))++;
            if (bl->Kind == 6)
                (*observatories(bl->System))++;
        }
        // Health > 0: still being built; the builder's work brings it down.
        if (bl->Health > 0) {
            TBody *bd = cur_moon_of(bl->System) == 0 ? body(bl->System, bl->Planet)
                                                     : body_at(bl->System, bl->Planet, bl->Moon);
            int16_t w4 = (int16_t)(bd->_04[0] | bd->_04[1] << 8);
            int32_t work = (int32_t)(int16_t)(w4 + 4000) + (uint16_t)(ds.Game_StaffExp[1] * 100);
            work = ldiv32(work, 1500);
            if (ds.Game_StaffHired[1] == 1)
                work = ldiv32(work, 2);
            if (ds.Game_StaffHired[1] == 3)
                work = lmul(work, 2);
            uint8_t old = bl->Health;
            // Surface::BuildingTypes[Kind]._2C
            if (System_Random(*b8((uint16_t)(0x657F + bl->Kind * 0x3F))) < 10) {
                if ((int32_t)bl->Health > work)
                    bl->Health -= (uint8_t)work;
                else
                    bl->Health = 0;
            }
            if (bl->Health == 0 || (bl->Health < 40 && old >= 40) || (bl->Health < 80 && old >= 80)) {
                PlanetInfo_BalanceBuildingPower(bl->System, bl->Planet, bl->Moon);
                if (ds.Game_CurrentScreen == 0x14 && at_current_body(bl->System, bl->Planet, bl->Moon)) {
                    Surface_RegisterBuildingAnims(ds.Game_CurSystem, *cur_planet(), *cur_moon());
                    if (ds.Surface_Mode == 1 || ds.Surface_Mode == 2)
                        Surface_DrawMapView();
                    if (ds.Surface_Mode == 4)
                        Surface_DrawBuildingInfo();
                    Surface_RebuildRadar();
                }
            }
        }
        if (bl->Health == 0)
            Protect2_AskDateQuestion();
        // Kind 5 with Flag set mines ore 1 into the body's store.
        if (bl->Health == 0 && bl->Flag != 0 && bl->Kind == 5) {
            // BUG? picks planet or moon by the current system's CurMoon.
            TBody *bd = *cur_moon() == 0 ? body(bl->System, bl->Planet)
                                         : body_at(bl->System, bl->Planet, bl->Moon);
            int16_t force = Groups_FindPlanetForce(bl->System, bl->Planet, bl->Moon);
            int32_t cap = lmul((uint16_t)Groups_At(1, force)->Storage, 1000);
            if (bl->System == 1 && bl->Planet == 5 && bl->Moon == 0) {
                if (ds.Game_HomeStock[0] < cap && System_Random(100) < 0x23)
                    ds.Game_HomeStock[0] += bd->Deposits[0] / 10;
            } else if (bd->Stores[0] < cap && System_Random(100) < 0x23) {
                bd->Stores[0] += bd->Deposits[0] / 10;
            }
        }
    }
}

// 1b8a:25b5
void Sim_UpdateColonySetups(void)
{
    int16_t k = 1;
    Protect2_EnforceDateCheck();
    // BUG? k also advances after a removal, so the entry moved into slot k
    // waits until the next hour.
    for (; k <= ds.Game_ColonySetupCount; k++) {
        TColonySetup *s = colony_setup(k);
        if (s->Active == 0)
            continue;
        bool done = true;
        for (int16_t j = 1; j <= 6; j++) {
            if (s->Timers[j - 1] == 0)
                continue;
            if (--s->Timers[j - 1] == 0) {
                // Colony::StarterBuildings[j].Building
                Surface_AutoPlaceBuilding(*w16((uint16_t)(0x651E + j * 10)), s->System, s->Planet,
                                          s->Moon);
            }
            done = done && s->Timers[j - 1] == 0;
        }
        if (done) {
            int16_t last = (int16_t)(ds.Game_ColonySetupCount - 1);
            for (int16_t m = k; m <= last; m++)
                System_MoveFwd(colony_setup((int16_t)(m + 1)), colony_setup(m), 0x10);
            ds.Game_ColonySetupCount--;
        }
    }
}

// 1b8a:26cf: nested in MineAllBodies (uses none of its locals).
static void Sim_MineBody(int16_t system, int16_t planet, int16_t moon, int16_t bodyIdx)
{
    // BUG? the body comes from the current system's table, not `system`'s.
    TBody *b = body(ds.Game_CurSystem, bodyIdx);
    if (!(b->Owner == 1 && (b->Colony != 0 || b->_0A != 0)))
        return;
    int16_t force = Groups_FindPlanetForce(system, planet, moon);
    int32_t cap = lmul((uint16_t)Groups_At(1, force)->Storage, 1000);
    // Ores 2..6; without a colony only the droids (_0A) mine, 30% of the time.
    if (system == 1 && planet == 5 && moon == 0) {
        for (int16_t j = 2; j <= 6; j++) {
            if (*home_stock(j) < cap && (b->Colony != 0 || System_Random(100) < 0x1E))
                *home_stock(j) += (int16_t)(uint16_t)(b->Deposits[j - 1] * b->_0A) / 10;
        }
    } else {
        for (int16_t j = 2; j <= 6; j++) {
            if (b->Stores[j - 1] < cap && (b->Colony != 0 || System_Random(100) < 0x1E))
                b->Stores[j - 1] += (int16_t)(uint16_t)(b->Deposits[j - 1] * b->_0A) / 10;
        }
    }
}

// 1b8a:2837
void Sim_MineAllBodies(void)
{
    for (int16_t s = 1; s <= 8; s++) {
        if (*system_status(s) != 1)
            continue;
        int16_t planets = *w16((uint16_t)(0x4774 + s * 2));
        for (int16_t p = 1; p <= planets; p++) {
            const uint8_t *moons = moon_list(s, p);
            Sim_MineBody(s, p, 0, p);
            uint16_t count = moons[0];
            for (uint16_t m = 1; m <= count; m++)
                Sim_MineBody(s, p, (int16_t)m, moon_list(s, p)[m]);
        }
    }
    Protect2_VerifyDateAnswer();
    if (ds.Game_CurrentScreen == 4)
        Droids_DrawDroidsScreen();
}

// Group i of list 0 is the selected one (list 0 shown).
static bool is_selected_fleet(int16_t i)
{
    return i >= 0 && ds.Game_SelGroup == i && ds.Game_GroupList == 0;
}

// " Your pilots met with ..." + system + " |" + spaces + "They call themselves '" + race + "'"
static void first_contact(int16_t r, const PString *intro, const PString *where,
                          const PString *callThem)
{
    PString t;
    System_StrLoad(&t, intro);
    System_StrConcat(&t, where);
    System_StrConcat(&t, PSTR(" |"));
    System_StrConcat(&t, callThem);
    System_StrConcat(&t, (const PString *)&race(r)->Name);
    System_StrConcat(&t, PSTR("'"));
    Intel_FirstContact(r, ps(&t));
}

// 1b8a:2a0e
void Sim_ProcessTravellingGroups(void)
{
    bool arrived = false;
    for (int16_t i = 1; i <= ds.Game_GroupCount; i++) {
        TGroup *g = Groups_At(0, i);
        if (!(g->Status == 4 || g->Status == 5 || g->Status == 6))
            continue;
        // Status 4..6: Leg1 hours to the jump point, Jump hours in hyperspace,
        // Leg2 hours to the target; the pilot's experience speeds up the legs.
        if (g->Jump == 1 && g->Leg1 == 0) {
            if (*b8(0x5D48) == 0) {  // story flag: first hyperspace jump seen
                Message_ShowMessageLine(0x22);
                AlienTalk_StartAlienScene(2);
                *b8(0x5D48) = 1;
            } else if (ds.Game_CurrentScreen == 0x11 && i >= 0 && ds.Game_SelGroup == i) {
                Gfx_FadeToWhite(0x70, 0x84, Gfx_PalettePtr(), 0x14);
                Blit_FillBytes(ds.ShipMove_Backdrop, 0x8980, 0x7F);
                ShipMove_AnimateStarfield();
                Gfx_FadeFromWhite(0x70, 0x84, Gfx_PalettePtr(), 0x14);
            }
        }
        if (ds.Game_CurrentScreen == 0x11 && i >= 0 && ds.Game_SelGroup == i && g->Leg1 > 0
            && (int32_t)g->Leg1 <= (int32_t)(uint16_t)(ds.Game_StaffExp[0] + 1))
            Sound_PlayButtonSfx(ps(PSTR("hiper")));
        if (g->Leg1 > 0)
            g->Leg1 -= (int16_t)(ds.Game_StaffExp[0] + 1);
        else if (g->Jump > 0)
            g->Jump--;
        else if (g->Leg2 > 0)
            g->Leg2 -= (int16_t)(ds.Game_StaffExp[0] + 1);
        if (g->Leg2 < 0)
            g->Leg2 = 0;
        if (g->Jump < 0)
            g->Jump = 0;
        if (g->Leg1 < 0)
            g->Leg1 = 0;
        if (g->Leg2 == 0)
            g->Status = 2;
        else if (g->Jump == 0)
            g->Status = 4;
        else if (g->Leg1 == 0)
            g->Status = 5;

        // DS:5D71: story flag armed by ProcessStoryTimers (invention 12 done).
        if (*b8(0x5D71) != 0 && g->Status == 2 && ds.Game_CurSystem == 2
            && *cur_planet() == 1 && *cur_moon() == 0) {
            Message_ShowMessageLine(0x12);
            AlienTalk_StartAlienTalk(6);
            Intel_OfferInventionForAnalysis(0x13);
            *b8(0x5D71) = 0;
        }

        PString t, who, s2;
        if (g->Leg2 == 0) {
            System_StrLoad(&t, (const PString *)&g->Name);
            System_StrConcat(&t, PSTR(" arrived to "));
            System_StrConcat(&t, system_name(g->System));
            System_StrConcat(&t, PSTR(" "));
            PlanetInfo_GetBodyFullName(&s2, g->System, g->Planet, g->Moon);
            System_StrConcat(&t, &s2);
            System_StrStore(&t, &who, 0xFF);
            Message_AddMessageLogEntry(2, ps(&who), ps(PSTR("message")));
            g->Status = 2;
            arrived = true;
        }
        if (g->Leg2 == 0) {
            if (g->Planet == 0) {
                if (*system_status(g->System) == 0) {
                    System_StrLoad(&t, PSTR(" Your ships explored "));
                    System_StrConcat(&t, system_name(g->System));
                    System_StrConcat(&t, PSTR(" system"));
                    System_StrConcat(&t, PSTR(" |    They found new planets"));
                    Message_ShowMessage(ps(&t));
                    if (g->System == 5)
                        Protect2_ArmDateQuestion();
                    *system_status(g->System) = 1;
                    for (int16_t p = 1; p <= 8; p++) {
                        if (*planet_status(g->System, p) == -1) {
                            Message_PromptPlanetName(g->System, p);
                            *planet_status(g->System, p) = 0;
                            g->Planet = (uint8_t)p;
                            g->Moon = 0;
                        }
                    }
                    if (g->System == 4) {
                        if (*b8(0x5D94) == 0)     // story flag
                            *w16(0x5D92) = 0x32;  // story timer
                        Intel_ScheduleIdea(0x1B, 0x96, 0x46);
                    }
                } else {
                    for (int16_t p = 1; p <= 8; p++) {
                        if (*planet_status(g->System, p) >= 0) {
                            g->Planet = (uint8_t)p;
                            g->Moon = 0;
                        }
                    }
                }
            }
            if (*planet_status(g->System, g->Planet) == 0) {
                *planet_status(g->System, g->Planet) = 1;
                System_StrStore(g->Kind == 4 ? PSTR("satellite carrier") : PSTR("pilots"), &who, 0xFF);
                System_StrLoad(&t, PSTR("Your "));
                System_StrConcat(&t, &who);
                System_StrConcat(&t, PSTR(" explored "));
                PlanetInfo_GetBodyName(&s2, g->System, g->Planet, 0);
                System_StrConcat(&t, &s2);
                System_StrConcat(&t, PSTR("'s moons"));
                Message_ShowMessage(ps(&t));
            }
        }

        TBody *bd = body_at(g->System, g->Planet, g->Moon);
        if (g->Leg2 == 0) {
            if (Groups_At(0, i)->Kind == 1 && g->System == 8 && g->Planet == 3 && g->Moon == 0)
                ds.Pub_People[9].State = 2;
            if (g->Planet != 0 && Groups_At(0, i)->Kind != 4 && (int8_t)bd->Owner >= 2
                && race((int8_t)bd->Owner)->Status == 0)
                first_contact((int8_t)bd->Owner, PSTR(" Your pilots met with a new alien race in "),
                              system_name(g->System), PSTR("         They call themselves '"));
            if (g->Planet != 0 && Groups_At(0, i)->Kind != 4) {
                for (int16_t r = 2; r <= 12; r++) {
                    TRace *rc = race(r);
                    if (rc->Status != 0)
                        continue;
                    int16_t forces = rc->ForceCount;
                    for (int16_t f = 1; f <= forces; f++) {
                        TRaceForce *fc = (TRaceForce *)((uint8_t *)rc + 0xC + f * 0x1B);
                        if (fc->Active == 1 && fc->System == g->System && fc->Body == g->Planet
                            && fc->Moon == g->Moon)
                            first_contact(r, PSTR(" Your pilots met with new alien race in "),
                                          system_name(g->System),
                                          PSTR("         They call themselves '"));
                    }
                }
            }
            // DS:5D95: story flag set with 5D94 (system 4 found).
            if (*b8(0x5D95) != 0 && g->System == 4 && bd->Colony != 0
                && (bd->Owner == 1 || bd->Owner == 6)) {
                *b8(0x5D95) = 0;
                AlienTalk_StartAlienTalk(9);
                Message_ShowMessageLine(0x2D);
                if (*b8(0x5D90) == 0)
                    *w16(0x5D8E) = (int16_t)(System_Random(10) + 0x230);
            }
            if (is_selected_fleet(i)) {
                if (ds.Game_CurrentScreen == 0x10) {
                    Satellite_UpdateShipInfoMenu();
                    ShipInfo_DrawGroupInfo();
                }
                if (ds.Game_CurrentScreen == 0x11) {
                    ds.Game_CurSystem = g->System;
                    *cur_planet() = g->Planet;
                    *cur_moon() = g->Moon;
                    PlanetInfo_SelectCurBody();
                    ShipMove_SetupGroupScreenHotspots();
                    ShipMove_RedrawBackdrop();
                }
                if (ds.Game_CurrentScreen == 0x16) {
                    Groups_SetupGroupsHotspots();
                    Satellite_UpdateGroupsMenu();
                    Groups_DrawGroupPanel(true);
                }
            }
            if (ds.Game_CurrentScreen == 0x11)
                Satellite_SetupGroupScreenMenu();
            if ((ds.Game_CurrentScreen == 8 || ds.Game_CurrentScreen == 0x14)
                && at_current_body(g->System, g->Planet, g->Moon)) {
                bool any = false;
                Satellite_AddBodyActionItems(ds.Game_CurSystem, *cur_planet(), *cur_moon(), &any);
                if (any)
                    GfxPath_ShowToolbar();
            }
            if (ds.Game_CurrentScreen == 0x12 || ds.Game_CurrentScreen == 7)
                PlanetInfo_SetupSystemViewHotspots();
            if (ds.Game_CurrentScreen == 7 && *cur_planet() != 0) {
                // System-view state (PlanetInfo): DS:7C3A, 7C3B flags, DS:7C36
                // selected entry, DS:7BC4 + 6k entry kinds (>3: race kind-3).
                if (*b8(0x7C3A) == 0 && *w16(0x7C36) > 0) {
                    int16_t kind = *w16((uint16_t)(0x7BC4 + *w16(0x7C36) * 6));
                    if (kind > 3 && race((int16_t)(kind - 3))->Status != 6) {
                        *b8(0x7C3B) = 0;
                        for (int16_t e = 1; e <= 0x12; e++) {
                            if (*w16((uint16_t)(0x7BC4 + e * 6)) == 1)
                                *b8(0x7C3B) = 1;
                        }
                    }
                }
                if (*b8(0x7C3A) == 0 && *b8(0x7C3B) != 0) {
                    GfxPath_SetToolbar(7);
                    GfxPath_AddToolbarIcon(0x21);
                    GfxPath_AddToolbarIcon(0x37);
                    GfxPath_ShowToolbar();
                }
            }
            if (ds.Game_CurrentScreen == 4)
                Droids_DrawDroidsScreen();
        }
        if (ds.Game_CurrentScreen == 0x10 && ds.Game_ScreenSwitch == 0 && is_selected_fleet(i))
            ShipInfo_DrawGroupFlightInfo();
    }
    if (ds.Game_CurrentScreen == 0x10 && arrived && ds.Game_ScreenSwitch == 0)
        ShipInfo_DrawAllGroupIcons();
}

// 1b8a:3546
void Sim_CheckEnemyAttacks(void)
{
    PString t, s2;
    PString where;  // string[50]
    for (int16_t r = 2; r <= 12; r++) {
        TRace *rc = race(r);
        // Forces with Active = 2 are on their way; _03 counts down the hours.
        int16_t forces = rc->ForceCount;
        for (int16_t f = 1; f <= forces; f++) {
            TRaceForce *fc = (TRaceForce *)((uint8_t *)rc + 0xC + f * 0x1B);
            if (fc->Active != 2)
                continue;
            PlanetInfo_GetBodyLongName(&t, fc->System, fc->Body, fc->Moon);
            System_StrStore(&t, &where, 0x32);
            fc->_03--;
            if ((int16_t)fc->_03 < 0)
                fc->_03 = 0;
            if (fc->_03 == 0)
                fc->Active = 1;
            if (fc->_03 != 0)
                continue;
            bool hostile = false;
            int16_t n = ds.Game_PlanetForceCount;
            for (int16_t k = 1; k <= n; k++) {
                TGroup *pf = Groups_At(1, k);
                if (fc->System == pf->System && fc->Body == pf->Planet && fc->Moon == pf->Moon) {
                    if (rc->Status == 0)
                        first_contact(r, PSTR("   New alien race's ships arrived |     at your colony in "),
                                      system_name(pf->System), PSTR(" They call themselves '"));
                    else if (rc->Status == 2)
                        hostile = true;
                }
            }
            n = ds.Game_GroupCount;
            for (int16_t k = 1; k <= n; k++) {
                TGroup *g = Groups_At(0, k);
                if (g->Kind == 4)
                    continue;
                if (fc->System == g->System && fc->Body == g->Planet && fc->Moon == g->Moon
                    && g->Status <= 2) {
                    if (rc->Status == 0)
                        first_contact(r, PSTR(" Your pilots met with new alien race in |   "), &where,
                                      PSTR("     They call themselves '"));
                    else if (rc->Status == 2)
                        hostile = true;
                }
            }
            if (hostile)
                show3(PSTR("            ATTENTION!         |     Enemy fleet approaching   | "), &where,
                      PSTR(" "));
        }

        Colonize_CheckKey1();
        // Forces at their target (Active = 1) with a mission: Timer counts
        // down to the attack.
        forces = rc->ForceCount;
        for (int16_t f = 1; f <= forces; f++) {
            TRaceForce *fc = (TRaceForce *)((uint8_t *)rc + 0xC + f * 0x1B);
            if (!(fc->Active == 1 && fc->NextMission != 0 && --fc->Timer == 0))
                continue;
            PlanetInfo_GetBodyLongName(&t, fc->System, fc->Body, fc->Moon);
            System_StrStore(&t, &where, 0x32);
            TBody *bd = body_at(fc->System, fc->Body, fc->Moon);
            // Battle setup (SpWar/GrWar): DS:7798/779A/779C target, DS:778A
            // ground battle, DS:7788 colony attacked, DS:778B, DS:778C race.
            *w16(0x7798) = fc->System;
            *w16(0x779A) = fc->Body;
            *w16(0x779C) = fc->Moon;
            *b8(0x778A) = fc->NextMission >= 3;
            if (fc->NextMission == 1)
                *b8(0x7788) = 0;
            if (fc->NextMission == 2)
                *b8(0x7788) = 1;
            if (fc->NextMission == 3)
                *b8(0x7788) = 0;
            if (fc->NextMission == 4)
                *b8(0x7788) = 1;
            if (*b8(0x7788) != 0 && bd->Owner != 1)
                *b8(0x7788) = 0;
            bool defended = false;
            int16_t n = ds.Game_GroupCount;
            for (int16_t k = 1; k <= n; k++) {
                TGroup *g = Groups_At(0, k);
                if ((g->Kind == 1 || g->Kind == 3) && fc->System == g->System && fc->Body == g->Planet)
                    defended = true;
            }
            if (!defended) {
                if (*b8(0x7788) != 0) {
                    System_StrLoad(&t, PSTR("| Your colony is under attack by "));
                    System_StrConcat(&t, (const PString *)&rc->Name);
                    System_StrConcat(&t, PSTR(" |  at "));
                    System_StrConcat(&t, &where);
                    System_StrConcat(&t, PSTR(" |"));
                    Message_ShowMessage(ps(&t));
                    ds.Game_CurrentScreen = 0x1D;
                    ds.Game_ScreenSwitch = 1;
                }
            } else {
                if (*b8(0x7788) == 0) {
                    System_StrLoad(&t, PSTR(" Your ships around "));
                    PlanetInfo_GetBodyLongName(&s2, fc->System, fc->Body, 0);
                    System_StrConcat(&t, &s2);
                    System_StrConcat(&t, PSTR(" |"));
                    System_StrConcat(&t, PSTR("        is under attack by "));
                    System_StrConcat(&t, (const PString *)&rc->Name);
                    System_StrConcat(&t, PSTR(" "));
                    Message_ShowMessage(ps(&t));
                } else {
                    System_StrLoad(&t, PSTR("| Your colony is under attack by "));
                    System_StrConcat(&t, (const PString *)&rc->Name);
                    System_StrConcat(&t, PSTR(" |  at "));
                    System_StrConcat(&t, &where);
                    System_StrConcat(&t, PSTR(" |"));
                    Message_ShowMessage(ps(&t));
                }
                ds.Game_CurrentScreen = 0x1D;
                ds.Game_ScreenSwitch = 1;
            }
            *b8(0x778B) = 0;
            fc->NextMission = 0;
            *w16(0x778C) = r;
        }
        if (ds.Game_CurrentScreen == 7 || ds.Game_CurrentScreen == 0x12)
            PlanetInfo_SetupSystemViewHotspots();
    }
}

// Staff::Ability[type, hired] (DS:57E0 + 3*type + hired): experience cap.
static uint8_t ability_cap(int16_t type)
{
    return *b8((uint16_t)(0x57E0 + type * 3 + ds.Game_StaffHired[type - 1]));
}

// 1b8a:3b83
void Sim_GrowStaffExperience(void)
{
    for (int16_t k = 1; k <= 4; k++) {
        if (ds.Game_StaffHired[k - 1] != 0 && System_Random(100) == 0
            && ability_cap(k) > ds.Game_StaffExp[k - 1])
            ds.Game_StaffExp[k - 1]++;
    }
    // DS:5D9E: staff type at the university, DS:5D9C: hours left, DS:5DA0:
    // the course (skill gains at DS:125 + 4*course + field).
    int16_t t = *w16(0x5D9E);
    if (t <= 0)
        return;
    if (*w16(0x5D9C) > 0)
        (*w16(0x5D9C))--;
    if (*w16(0x5D9C) != 0)
        return;
    uint16_t *exp = (uint16_t *)ds_at((uint16_t)(0x95A2 + t * 2));  // Game::StaffExp[t]
    *exp += (uint16_t)(System_Random(4) + 0x69);
    uint8_t cap = *b8((uint16_t)(0x57E0 + t * 3 + *w16((uint16_t)(0x95B4 + t * 2))));
    if (cap < *exp)
        *exp = cap;
    if (t == 4) {
        for (int16_t j = 1; j <= 4; j++) {
            ds.Game_DevSkill[j - 1] += *b8((uint16_t)(0x125 + *w16(0x5DA0) * 4 + j));
            // Staff::DevSkills[StoryStage, StaffHired[4], j]
            uint8_t skillCap = *b8((uint16_t)(0x57EB + ds.Game_StoryStage * 12
                                              + ds.Game_StaffHired[3] * 4 + j));
            if (skillCap < ds.Game_DevSkill[j - 1])
                ds.Game_DevSkill[j - 1] = skillCap;
        }
    }
    *w16(0x5D9E) = 0;
    if (ds.Game_CurrentScreen == 1) {
        ds.Game_CurrentScreen = -1;
        ds.Game_ScreenSwitch = 1;
    }
    Message_ShowMessage(ps(PSTR("| Your advisor has returned from the university |")));
    if (ds.Game_CurrentScreen == 3) {
        Research_DrawInventionInfo();
        Research_DrawDeveloperSkills();
    }
}

// A story timer: word `timer` counts down while `done` (a byte) is clear;
// true when it reaches 0 this hour (the caller sets `done`).
static bool tick(uint16_t timer)
{
    if (*w16(timer) > 0 && --*w16(timer) == 0)
        return true;
    return false;
}

// 1b8a:3d07. The story block DS:5D4A..5D98 holds pairs of a word timer and a
// byte "fired" flag (5D4A/5D4C, 5D4E/5D50, 5D54/5D56, ...).
void Sim_ProcessStoryTimers(void)
{
    if (ds.Game_StaffExp[3] != 0 && *w16(0x5D52) == 0 && (*w16(0x5D9C) == 0 || *w16(0x5D9E) != 4)) {
        for (int16_t k = 1; k <= 35; k++) {
            int16_t *timer = &ds.Game_IdeaTimer[k - 1];
            if (*timer <= 0)
                continue;
            (*timer)--;
            if (k == 0xF && product(9)->State != 5 && *timer == 0)
                *timer = 10;
            if (k == 0x22 && *timer == 0x32)
                Message_ShowMessageLine(0x21);
            if (*timer == 0) {
                Message_ShowKitalalLine(k);
                product(k)->State = 1;
                if (ds.Game_CurrentScreen == 3) {
                    ds.Game_SelInvention = k;
                    Research_DrawInventionInfo();
                    Research_SetupResearchHotspots();
                    // The original pushes (k, 3); research.h takes them as (kind, inv).
                    Research_PlayIconTransitionOut(3, k);
                }
            }
        }
    }
    if (*b8(0x5D4C) == 0 && tick(0x5D4A)) {
        *b8(0x5D4C) = 1;
        Intel_ScheduleIdea(4, 10, 10);
        AlienTalk_StartAlienScene(9);
    }
    if (ds.Game_StaffHired[3] != 0 && *b8(0x5D50) == 0 && *w16(0x5D4E) > 0
        && (*w16(0x5D9C) == 0 || *w16(0x5D9E) != 4) && ds.Races_Races[0].Status > 2
        && --*w16(0x5D4E) == 0) {
        if (product(8)->State == 5)
            AlienTalk_StartAlienTalk(2);
        else
            Message_ShowMessageLine(6);
    }
    if (tick(0x5D52)) {
        Message_ShowMessageLine(8);
        if (ds.Game_CurrentScreen == 1) {
            *b8(0x7E29) = 1;  // Command
            ds.Game_PrevScreen = 1;
            ds.Game_CurrentScreen = -1;
            ds.Game_ScreenSwitch = 1;
        }
        Intel_GrantInvention(9);
    }
    if (*b8(0x5D56) == 0 && *w16(0x5D54) > 0 && ds.Races_Races[0].Status > 2) {
        (*w16(0x5D54))--;
        if (*w16(0x5D54) == 0) {
            if (product(8)->State == 5) {
                Message_ShowMessageLine(9);
                AlienTalk_StartAlienTalk(3);
            } else {
                Message_ShowMessageLine(6);
            }
        }
        if (*w16(0x5D54) == 0)
            *b8(0x5D56) = 1;
    }
    if (*b8(0x5D5A) == 0 && *w16(0x5D58) > 0
        && (ds.Races_Races[0].Status == 0 || ds.Races_Races[0].Status > 2) && --*w16(0x5D58) == 0) {
        if (product(8)->State == 5) {
            Message_ShowMessageLine(10);
            AlienTalk_StartAlienTalk(4);
        } else {
            Message_ShowMessageLine(6);
        }
        *b8(0x5D5A) = 1;
        if (*b8(0x5D5E) == 0)
            *w16(0x5D5C) = (int16_t)(System_Random(0x32) + 200);
        Intel_SetForceTarget(3, 1, 1, 7, 0);
    }
    if (*b8(0x5D5E) == 0 && tick(0x5D5C)) {
        if (ds.Races_Races[0].Status > 0)
            Message_ShowMessageLine(0xB);
        if (ds.Races_Races[0].Status > 0)
            AlienTalk_StartAlienScene(1);
        Intel_EliminateRace(2);
        Intel_ClaimBodyForRace(3, 1, 7, 0);
        if (*b8(0x5D66) == 0)
            *w16(0x5D64) = (int16_t)(System_Random(0x32) + 600);
        if (*b8(0x5D62) == 0)
            *w16(0x5D60) = (int16_t)(System_Random(0x32) + 300);
        if (*b8(0x5D6C) == 0)
            *w16(0x5D6A) = (int16_t)(System_Random(200) + 0x708);
        *b8(0x5D5E) = 1;
        ds.Game_StoryStage = 2;
    }
    if (*b8(0x5D66) == 0 && tick(0x5D64)) {
        Intel_DispatchForce(3, 1, 1, 5, 0, 2);
        *b8(0x5D66) = 1;
    }
    if (*b8(0x5D62) == 0 && *w16(0x5D60) > 0 && product(14)->State > 0 && ds.Game_StaffExp[3] != 0
        && *w16(0x5D52) == 0 && *w16(0x5D9E) != 4 && --*w16(0x5D60) == 0) {
        Message_ShowMessageLine(0x1E);
        *b8(0x5D62) = 1;
    }
    if (*b8(0x5D6C) == 0 && tick(0x5D6A)) {
        Intel_DispatchForce(3, 2, 1, 5, 0, 2);
        *b8(0x5D6C) = 1;
        *b8(0x5D6D) = 1;
        if (*b8(0x5D74) == 0)
            *w16(0x5D72) = (int16_t)(System_Random(0x32) + 0x44C);
        if (*b8(0x5D70) == 0)
            *w16(0x5D6E) = (int16_t)(System_Random(0x14) + 100);
    }
    if (*b8(0x5D70) == 0 && *w16(0x5D6E) > 0 && product(12)->State == 5 && --*w16(0x5D6E) == 0) {
        Message_ShowMessageLine(0x11);
        *b8(0x5D70) = 1;
        *b8(0x5D71) = 1;
    }
    if (*b8(0x5D74) == 0 && tick(0x5D72)) {
        Intel_DispatchForce(3, 3, 1, 5, 0, 2);
        *b8(0x5D74) = 1;
        *b8(0x5D75) = 1;
        if (*b8(0x5D88) == 0)
            *w16(0x5D86) = (int16_t)(System_Random(0x32) + 0x32);
        Intel_ScheduleForceLaunch(3, 6, (int16_t)(System_Random(500) + 8000), 2);
    }
    if (*b8(0x5D78) == 0 && tick(0x5D76)) {
        *b8(0x5D78) = 1;
        *b8(0x5D3E) = 1;
        if (*b8(0x5D7C) == 0)
            *w16(0x5D7A) = (int16_t)(System_Random(0x32) + 4000);
        Intel_SetForceTarget(4, 2, 2, 5, 0);
    }
    if (*b8(0x5D7C) == 0 && tick(0x5D7A)) {
        *b8(0x5D7C) = 1;
        *b8(0x5D3E) = 0;
        if (*b8(0x5D80) == 0)
            *w16(0x5D7E) = (int16_t)(System_Random(0x32) + 300);
        ds.Races_Races[2].Status = 2;
        Intel_SetForceTarget(3, 4, 2, 5, 0);
        Message_ShowMessageLine(0x18);
        Intel_EliminateRace(5);
    }
    if (*b8(0x5D80) == 0 && tick(0x5D7E)) {
        *b8(0x5D80) = 1;
        Intel_DispatchForce(3, 4, 1, 5, 0, 2);
        Intel_DispatchForce(4, 2, 1, 5, 0, 2);
    }
    if (*b8(0x5D84) == 0 && *w16(0x5D82) > 0 && ds.Races_Races[3].Status > 2 && --*w16(0x5D82) == 0) {
        *b8(0x5D84) = 1;
        AlienTalk_StartAlienTalk(7);
    }
    if (*b8(0x5D88) == 0 && tick(0x5D86)) {
        *b8(0x5D88) = 1;
        Message_ShowMessageLine(0x14);
    }
    if (ds.Galaxy_SystemStatus[2] < 0 && tick(0x5D8A)) {
        ds.Galaxy_SystemStatus[2] = 0;
        Message_ShowMessageLine(0x25);
    }
    if (*b8(0x5D94) == 0 && *w16(0x5D92) > 0 && *observatories(4) > 0 && --*w16(0x5D92) == 0) {
        *b8(0x5D94) = 1;
        *b8(0x5D95) = 1;
        Message_ShowMessageLine(0x19);
        if (*b8(0x5D90) == 0)
            *w16(0x5D8E) = (int16_t)(System_Random(100) + 1000);
    }
    if (*b8(0x5D90) == 0 && tick(0x5D8E)) {
        *b8(0x5D90) = 1;
        *w16(0x5D92) = 0;
        Message_ShowMessageLine(0x1B);
        AlienTalk_StartAlienScene(7);
        Intel_AnnihilateSystem4();
    }
    if (*b8(0x5D98) == 0 && tick(0x5D96)) {
        Intel_DispatchForce(7, 1, 1, 5, 0, 2);
        *b8(0x5D98) = 1;
        Intel_ScheduleForceLaunch(8, 1, (int16_t)(System_Random(500) + 1000), 2);
    }
    if (tick(0x5D40)) {  // Intel_LaunchTimer
        if (race(ds.Intel_LaunchRace)->Status == 2)
            Intel_DispatchForceNow(ds.Intel_LaunchRace, ds.Intel_LaunchForce, 1, 5, 0, 2,
                                   ds.Intel_LaunchMission);
        ds.Intel_LaunchTimer = 0;
        if (ds.Intel_LaunchRace == 3 && ds.Intel_LaunchForce == 6)
            Intel_ScheduleForceLaunch(3, 7, (int16_t)(System_Random(100) + 0x1194), 2);
        if (ds.Intel_LaunchRace == 7 && ds.Intel_LaunchForce == 6)
            Intel_ScheduleForceLaunch(7, 7, (int16_t)(System_Random(500) + 3000), 2);
        if (ds.Intel_LaunchRace == 7 && ds.Intel_LaunchForce == 5)
            Intel_ScheduleForceLaunch(7, 6, (int16_t)(System_Random(500) + 0x5DC), 2);
        if (ds.Intel_LaunchRace == 8 && ds.Intel_LaunchForce == 1)
            Intel_ScheduleForceLaunch(7, 5, (int16_t)(System_Random(500) + 1000), 2);
        if (ds.Intel_LaunchRace == 0xC && ds.Intel_LaunchForce == 4)
            Intel_ScheduleForceLaunch(0xC, 5, (int16_t)(System_Random(500) + 2000), 2);
        if (ds.Intel_LaunchRace == 0xC && ds.Intel_LaunchForce == 3)
            Intel_ScheduleForceLaunch(0xC, 4, (int16_t)(System_Random(500) + 2000), 2);
    }
}
