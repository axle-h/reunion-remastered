// Unit Sim (1b8a): the hourly simulation. See sim.h for the call order.
//
// Ported from the disassembly (the decompile drops code in places: e.g. it
// misses ProcessAllBodyEvents adding the day's taxes to Game_Credits).
// Most of the story state is in the timer block DS:5D3E..5DA1 (Intel:: and
// Advisor:: fields): word timers, each with a "done" byte behind it.
#include "game/sim.h"

#include <SDL3/SDL.h>
#include <string.h>

#include "game/blit.h"
#include "game/cargo.h"
#include "game/colonize.h"
#include "game/colonystats.h"
#include "game/droids.h"
#include "game/fixes.h"
#include "game/dseg.h"
#include "game/gfx.h"
#include "game/gfxpath.h"
#include "game/groups.h"
#include "game/intel.h"
#include "game/message.h"
#include "game/minmax.h"
#include "game/planetinfo.h"
#include "game/protect.h"
#include "game/research.h"
#include "game/satellite.h"
#include "game/shipinfo.h"
#include "game/shipmove.h"
#include "game/shop.h"
#include "game/sound.h"
#include "game/system.h"
#include "game/textfile.h"

// Routines missing from their units' headers.
void Command_ShowHologram(int16_t vec);                                // 3abd:04b4
void AlienTalk_StartAlienScene(int16_t scene);                         // 2e4b:0000
void AlienTalk_StartAlienTalk(int16_t race);                           // 2e4b:03a5
void Intel_FirstContact(int16_t race, const char *msg);             // 29b9:0000
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
// Data access (the original's 1-based arrays).
// ---------------------------------------------------------------------------

static inline TProduct *product(int16_t p) { return &ds.Research_Products[p - 1]; }
static inline TRace *race(int16_t r) { return Intel_Race(r); }
static inline int8_t *system_status(int16_t s) { return &ds.Galaxy_SystemStatus[s - 1]; }
static inline int8_t *planet_status(int16_t s, int16_t p)
{
    return &ds.Galaxy_PlanetStatus[s - 1][p - 1];
}
// Completed observatories in system s, counted by UpdateBuildings.
static inline int16_t *observatories(int16_t s) { return &ds.Sim_Observatories[s - 1]; }
// Galaxy::Names of system s, into buf.
static const char *system_name(char buf[256], int16_t s)
{
    return pstr_get(buf, 256, &ds.Names_Systems[s - 1]);
}
// Game::CurPlanet[CurSystem] / CurMoon[CurSystem].
static inline int16_t *cur_planet(void) { return &ds.Game_CurPlanet[ds.Game_CurSystem - 1]; }
static inline int16_t *cur_moon(void) { return &ds.Game_CurMoon[ds.Game_CurSystem - 1]; }
// Game::CurMoon[s] for any system.
static inline int16_t cur_moon_of(int16_t s) { return ds.Game_CurMoon[s - 1]; }
static inline TColonySetup *colony_setup(int16_t k) { return &ds.Game_ColonySetups[k - 1]; }
// Game::Buildings^[i].
static inline TBuilding *building(int16_t i)
{
    return &ds.Game_Buildings[i - 1];
}
// Surface::KindCounts[1, kind], filled by Surface::CountBuildingsByKind.
static inline int32_t kind_count(int16_t kind) { return ds.Surface_KindCounts[1][kind - 1]; }

// Galaxy::Bodies[s]^[idx]: 65-byte records.
static TBody *body(int16_t s, int16_t idx) { return ds.Galaxy_Bodies[s - 1] + (idx - 1); }

// Galaxy::MoonLists[s]^[planet] (count + 8 body indices).
static uint8_t *moon_list(int16_t s, int16_t planet)
{
    return (uint8_t *)(ds.Galaxy_MoonLists[s - 1] + (planet - 1));
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
        System_RunError(200, nullptr);
    return (int32_t)((int64_t)a / b);
}

// a + b + c into a 255-character temporary, then ShowMessage.
static void show3(const char *a, const char *b, const char *c)
{
    char t[256];
    SDL_snprintf(t, sizeof t, "%s%s%s", a, b, c);
    Message_ShowMessage(t);
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
    // The original calls Protect::CheckToken (24eb:00a5) here, whose prompt
    // is dead code in the shipped EXE.
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
            if (kind == 1 && ds.Races_Hostile[r - 1] != 2)
                ds.Races_Hostile[r - 1] = 1;
            if (kind == 3)
                ds.Races_Hostile[r - 1] = 2;
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
            ds.Intel_Pub7Done = 1;
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
            // Pub::Deals[d]; d = 0 (no active deal) reaches the bytes before the
            // array, as in the original.
            TPubDeal *deal =
                (TPubDeal *)((uint8_t *)ds.Pub_Deals + (d - 1) * (int)sizeof(TPubDeal));
            if (System_Random(100) < deal->Chance) {
                Message_ShowMessageLine(0x2F);
                for (int16_t j = 1; j <= 4; j++) {
                    uint8_t item = deal->Items[j - 1];
                    if (item == 0)
                        continue;
                    int32_t amount = ldiv32(deal->Amounts[j - 1], divisor);
                    char name[256], t[256];
                    if (item < 10) {
                        ds.Game_HomeStock[item - 1] += amount;
                        pstr_get(name, sizeof name, &ds.Names_Ores[item - 1]);
                        SDL_snprintf(t, sizeof t, "You got %d t of %s", (int)amount, name);
                    } else {
                        product((int16_t)(item - 10))->Stock += (int16_t)amount;
                        pstr_get(name, sizeof name, &product((int16_t)(item - 10))->Name);
                        SDL_snprintf(t, sizeof t, "You got %d pieces of %ss", (int)amount, name);
                    }
                    Message_ShowMessage(t);
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
                char line[256] = "";
                TextFile_ReadTextLine("pirate.txt", k, line, sizeof line);
                Message_ShowMessage(line);
            } else {
                deal->Active = 0;
            }
        }
    }
    // (2693:0538: the original halts here after three wrong dates.)
}

// 1b8a:04d8
void Sim_ProgressResearch(void)
{
    for (int16_t k = 1; k <= 35; k++) {
        TProduct *p = product(k);
        // Research stops while the developer is with race 2 or at the university.
        if (ds.Intel_DevAwayTimer != 0 || ds.Advisor_TrainingKind == 4
            || !(p->State == 2 || p->State == 4))
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
            char name[256];
            show3("The ", pstr_get(name, sizeof name, &p->Name), " is ready for production");
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
    Protect_ManualCheckHour();
}

// 1b8a:0705
void Sim_ProgressProduction(void)
{
    bool completed = false;
    for (int16_t k = 1; k <= 35; k++) {
        TProduct *p = product(k);
        if ((ds.Game_CurrentScreen == 5 && ds.Shop_QtyEdit != 0) || p->_1D == 0)
            continue;
        p->_21 -= ds.Game_ProductionRate;
        // Shop::List[Shop::Sel]: the product selected in the shop's list.
        if ((int16_t)p->_21 <= 0) {
            completed = true;
            p->_21 = 0;
            p->Stock++;
            p->_1D--;
            if ((int16_t)p->_1D > 0)
                p->_21 = p->_1F;
            if (ds.Game_CurrentScreen == 5 && ds.Shop_List[ds.Shop_Sel - 1] == k
                && (int16_t)p->_1D > 0)
                Shop_DrawQueueNumbers();
        }
        if (ds.Game_CurrentScreen == 5 && ds.Shop_List[ds.Shop_Sel - 1] == k) {
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
    char name[51];  // string[50]
    char t[256];
    PlanetInfo_GetBodyLongName(name, sizeof name, system, planet, moon);
    const char *end = " ";

    TBody *b = body(system, bodyIdx);
    if (b->Owner == 1 && b->Colony != 0) {
        Surface_CountBuildingsByKind(system, planet, moon);
        // Target attitude; the people's attitude moves towards it.
        int16_t target = (int16_t)((7 - (int8_t)b->TechLevel) * 10);
        int32_t living = lmul(kind_count(7), 7000) + 10000;
        int32_t food = lmul(kind_count(21), 20000) + lmul(kind_count(16), 8000) + 20000;
        int32_t medicine = kind_count(24);
        int32_t hospital = lmul(kind_count(15), 20000) + 20000;
        if (b->_14 > 0 && b->_14 < 100)
            b->_14--;
        int16_t sick = b->_14 == 0 ? 2 : 1;
        if (b->_14 == 0 && medicine == 0) {
            if (System_Random(5) == 0)
                show3("Because of disease your people need a medicine plant|  on ", name, end);
            target -= 10;
            if (target > 25)
                target = 25;
        }
        if (lmul(sick, b->Population) > hospital) {
            if (System_Random(5) == 0)
                show3(" Due to sanitary conditions you need a hospital |  on ", name, end);
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
        // The low words of the counts, as the original reads them.
        int16_t k8 = (int16_t)kind_count(8), stadium = (int16_t)kind_count(9);
        int16_t k10 = (int16_t)kind_count(10), k14 = (int16_t)kind_count(14);
        int16_t university = (int16_t)kind_count(13);
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
                show3("  Your people ask for a stadium | on ", name, end);
            target -= 20;
        }
        if (b->Population > 70000 && university == 0) {
            if (System_Random(5) == 0)
                show3("  Your people demand a University | on ", name, end);
            target -= 20;
        }
        if (planet < 3 && kind_count(19) == 0) {
            if (System_Random(5) == 0) {
                uint16_t r = System_Random((uint16_t)ldiv32(b->Population, 4));
                b->Population -= (uint16_t)(r + 500);
                Intel_ScheduleIdea(0x1E, 0x32, 0x19);
                show3(" Your people are dying because of radiation |  on ", name, end);
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
                show3("  A meteorite hit your colony on | ", name, " |        but causes no harm.");
            } else {
                Surface_DeleteBuilding(hit);
                int16_t n = (int16_t)(Surface_CountBuildings(0x19, system, planet, moon, false)
                                      + Surface_CountBuildings(4, system, planet, moon, false));
                // The original decrements the droids of CurBody, not of this
                // body. Port fix (game/fixes.h): this body's.
                TBody *cur = fixes.meteorite_droids ? b : ds.Game_CurBody;
                if (cur->_0A > n)
                    cur->_0A--;
                uint16_t r = System_Random((uint16_t)ldiv32(b->Population, 20));
                b->Population -= r;
                Intel_ScheduleIdea(0x1C, 0xFA, 100);
                b->Attitude -= 10;
                show3("  One of your buildings has exploded |      due to meteorite impact| on ",
                      name, end);
            }
            target -= 30;
        }
        if (b->Population > food) {
            if (System_Random(3) == 0)
                show3(" Your people are starving |  on ", name, end);
            target -= 15;
            if (target > 25)
                target = 25;
            b->Attitude -= 2;
        }
        if (b->Population > living) {
            if (System_Random(3) == 0)
                show3(" There isn't enough living space|  on ", name, end);
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
        // The original multiplies and divides Reals (Real48). Every operand is
        // an integer (TechLevel 0..7 read as a shortint, Resistance 0..60,
        // Attitude + 40 in -88..167, population held near maxPop <= 200000),
        // and while the product stays below 2^40 (|Population| < 15.6
        // million) the Real chain is exact up to the division, whose
        // rounding never crosses a half: this is the same result (checked
        // 2026-10-12 on every operand combination at edge populations, every
        // division boundary below 2^40 and 20 million random cases).
        int64_t taxes = (int64_t)b->Population * (int8_t)b->TechLevel * b->Resistance *
                        (b->Attitude + 40);
        int64_t rounded = (taxes < 0 ? -taxes : taxes) + 100000;
        parent->income += (int32_t)(taxes < 0 ? -(rounded / 200000) : rounded / 200000);

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
                show3(" Your people have revolted  |  on ", name, end);
                Intel_DestroyBody(system, planet, moon);
            } else {
                show3(" Your advisors report that| there are rebellious forces |  on ", name, end);
            }
        } else {
            b->Stores[6] = 0;
        }
        if (b->Population < 1000) {
            show3(" Your colony has been destroyed  |  on ", name, end);
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
        show3(end, name,
              b->_03 != 0 ? " |    is ready for mining " : " |    is not suitable for mining ");
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
            uint16_t tileset = (uint16_t)ds.Surface_Tileset[(int8_t)b->SurfaceType - 1];
            if (b->Surveyed != 0 && ds.Galaxy_SurfaceColonisable[tileset - 1] != 0)
                show3(end, name, " |    is a suitable place for living");
            else
                show3(end, name, " |    is not suitable for living");
        } else {
            show3(" Your satellite reports alien activity on | ", name, end);
            if (PlanetInfo_IsCurrentBody(system, planet, moon) && ds.Game_CurrentScreen == 8)
                PlanetInfo_LoadPlanetFajPic(0xD, true);
        }
    }
    if (b->InfoLevel - gain < 40 && b->InfoLevel >= 40 && (int8_t)b->Owner > 1) {
        char who[256];
        pstr_get(who, sizeof who, &race((int8_t)b->Owner)->Name);
        SDL_snprintf(t, sizeof t, " Your satellite found %s colony at | %s%s", who, name, end);
        Message_ShowMessage(t);
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
        show3("  The building of a colony started  | on ", name, end);
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
        char sys[256];
        show3(" Your observatory found a new planet |          in ", system_name(sys, system),
              " system");
        Message_PromptPlanetName(system, planet);
        parent->discovered = 1;
    }
    if (moon == 0 && *planet_status(system, planet) == -2 && *system_status(system) == 1
        && System_Random(5) == 1 && !parent->discovered) {
        any = false;
        const char *who = "";
        int16_t count = ds.Game_GroupCount;
        for (int16_t i = 1; i <= count; i++) {
            TGroup *g = Groups_At(0, i);
            if (g->System == system && g->Status > 2) {
                any = true;
                who = g->Kind == 4 ? "satellite carrier" : "pilots";
            }
        }
        if (any) {
            *planet_status(system, planet) = 0;
            if (ds.Game_CurrentScreen == 7 || ds.Game_CurrentScreen == 0x12)
                PlanetInfo_SetupSystemViewHotspots();
            char sys[256];
            SDL_snprintf(t, sizeof t, " Your %s found a new planet |        in %s system ", who,
                         system_name(sys, system));
            Message_ShowMessage(t);
            Message_PromptPlanetName(system, planet);
            parent->discovered = 1;
        }
    }
    if (product(34)->State == 5 && moon == 0 && *planet_status(system, planet) == -4
        && *system_status(system) == 1 && System_Random(10) == 1 && !parent->discovered) {
        *planet_status(system, planet) = 0;
        if (ds.Game_CurrentScreen == 7 || ds.Game_CurrentScreen == 0x12)
            PlanetInfo_SetupSystemViewHotspots();
        char sys[256];
        show3(" Your Psy-radar radar found a new planet |          in ", system_name(sys, system),
              " system");
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
        int16_t planets = ds.Galaxy_PlanetCount[s - 1];
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
    if (ds.Game_CurrentScreen == 8 && ds.PlanetInfo_AlienMode == 0)
        PlanetInfo_DrawPlanetInfoText();
    if (ds.Game_CurrentScreen == 8 && ds.PlanetInfo_AlienMode == 1)
        PlanetInfo_DrawStoresList();
    // Port addition (game/colonystats.h).
    if (ds.Game_CurrentScreen == 8 && ds.PlanetInfo_AlienMode == COLONY_STATS_MODE)
        ColonyStats_Draw();
}

// 1b8a:2178
void Sim_UpdateBuildings(void)
{
    for (int16_t s = 1; s <= 8; s++)
        *observatories(s) = 0;
    ds.Sim_UniversityCount = 0;
    int16_t count = ds.Game_BuildingCount;
    for (int16_t i = 1; i <= count; i++) {
        TBuilding *bl = building(i);
        if (bl->Health == 0) {
            if (bl->Kind == 0xD)
                ds.Sim_UniversityCount++;
            if (bl->Kind == 6)
                (*observatories(bl->System))++;
        }
        // Health > 0: still being built; the builder's work brings it down.
        if (bl->Health > 0) {
            // The original picks planet or moon by the system's CurMoon. Port
            // fix (game/fixes.h): the building's own body.
            TBody *bd = fixes.build_on_moons || cur_moon_of(bl->System) != 0
                            ? body_at(bl->System, bl->Planet, bl->Moon)
                            : body(bl->System, bl->Planet);
            int16_t w4 = (int16_t)(bd->_04[0] | bd->_04[1] << 8);
            int32_t work = (int32_t)(int16_t)(w4 + 4000) + (uint16_t)(ds.Game_StaffExp[1] * 100);
            work = ldiv32(work, 1500);
            if (ds.Game_StaffHired[1] == 1)
                work = ldiv32(work, 2);
            if (ds.Game_StaffHired[1] == 3)
                work = lmul(work, 2);
            uint8_t old = bl->Health;
            if (System_Random(ds.Surface_BuildingTypes[bl->Kind - 1]._2C) < 10) {
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
            Protect_AskDateQuestion();
        // Kind 5 with Flag set mines ore 1 into the body's store.
        if (bl->Health == 0 && bl->Flag != 0 && bl->Kind == 5) {
            // The original picks planet or moon by the current system's
            // CurMoon. Port fix (game/fixes.h): the building's own body.
            TBody *bd = fixes.derricks_on_moons || *cur_moon() != 0
                            ? body_at(bl->System, bl->Planet, bl->Moon)
                            : body(bl->System, bl->Planet);
            int32_t cap = lmul((uint16_t)Groups_PlanetStorage(bl->System, bl->Planet, bl->Moon), 1000);
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
    Protect_DateColonyStep();
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
                Surface_AutoPlaceBuilding(ds.Colony_StarterBuildings[j - 1].Building, s->System,
                                          s->Planet, s->Moon);
            }
            done = done && s->Timers[j - 1] == 0;
        }
        if (done) {
            int16_t last = (int16_t)(ds.Game_ColonySetupCount - 1);
            for (int16_t m = k; m <= last; m++)
                memcpy(colony_setup(m), colony_setup((int16_t)(m + 1)), 0x10);
            ds.Game_ColonySetupCount--;
        }
    }
}

// 1b8a:26cf: nested in MineAllBodies (uses none of its locals).
static void Sim_MineBody(int16_t system, int16_t planet, int16_t moon, int16_t bodyIdx)
{
    // The original reads the body from the current system's table, not
    // `system`'s. Port fix (game/fixes.h): `system`'s own table.
    TBody *b = body(fixes.mine_all_systems ? system : ds.Game_CurSystem, bodyIdx);
    if (!(b->Owner == 1 && (b->Colony != 0 || b->_0A != 0)))
        return;
    int32_t cap = lmul((uint16_t)Groups_PlanetStorage(system, planet, moon), 1000);
    // Ores 2..6; without a colony only the droids (_0A) mine, 30% of the time.
    if (system == 1 && planet == 5 && moon == 0) {
        for (int16_t j = 2; j <= 6; j++) {
            if (ds.Game_HomeStock[j - 1] < cap && (b->Colony != 0 || System_Random(100) < 0x1E))
                ds.Game_HomeStock[j - 1] += (int16_t)(uint16_t)(b->Deposits[j - 1] * b->_0A) / 10;
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
        int16_t planets = ds.Galaxy_PlanetCount[s - 1];
        for (int16_t p = 1; p <= planets; p++) {
            const uint8_t *moons = moon_list(s, p);
            Sim_MineBody(s, p, 0, p);
            uint16_t count = moons[0];
            for (uint16_t m = 1; m <= count; m++)
                Sim_MineBody(s, p, (int16_t)m, moon_list(s, p)[m]);
        }
    }
    Protect_DateDailyCheck();
    if (ds.Game_CurrentScreen == 4)
        Droids_DrawDroidsScreen();
}

// Group i of list 0 is the selected one (list 0 shown).
static bool is_selected_fleet(int16_t i)
{
    return i >= 0 && ds.Game_SelGroup == i && ds.Game_GroupList == 0;
}

// " Your pilots met with ..." + system + " |" + spaces + "They call themselves '" + race + "'"
static void first_contact(int16_t r, const char *intro, const char *where, const char *callThem)
{
    char name[256], t[256];
    pstr_get(name, sizeof name, &race(r)->Name);
    SDL_snprintf(t, sizeof t, "%s%s |%s%s'", intro, where, callThem, name);
    Intel_FirstContact(r, t);
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
            if (ds.Intel_FirstJumpSeen == 0) {
                Message_ShowMessageLine(0x22);
                AlienTalk_StartAlienScene(2);
                ds.Intel_FirstJumpSeen = 1;
            } else if (ds.Game_CurrentScreen == 0x11 && i >= 0 && ds.Game_SelGroup == i) {
                Gfx_FadeToWhite(0x70, 0x84, Gfx_PalettePtr(), 0x14);
                Blit_FillBytes(ds.ShipMove_Backdrop, 0x8980, 0x7F);
                ShipMove_AnimateStarfield();
                Gfx_FadeFromWhite(0x70, 0x84, Gfx_PalettePtr(), 0x14);
            }
        }
        if (ds.Game_CurrentScreen == 0x11 && i >= 0 && ds.Game_SelGroup == i && g->Leg1 > 0
            && (int32_t)g->Leg1 <= (int32_t)(uint16_t)(ds.Game_StaffExp[0] + 1))
            Sound_PlayButtonSfx("hiper");
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

        // Intel::Phoenix1Armed: set by ProcessStoryTimers (invention 12 done).
        // The original tests the selected body, not where the group arrived.
        // Port fix (game/fixes.h): the group's.
        bool at_phoenix1 = fixes.drifting_ship
                               ? g->System == 2 && g->Planet == 1 && g->Moon == 0
                               : ds.Game_CurSystem == 2 && *cur_planet() == 1 && *cur_moon() == 0;
        if (ds.Intel_Phoenix1Armed != 0 && g->Status == 2 && at_phoenix1) {
            Message_ShowMessageLine(0x12);
            AlienTalk_StartAlienTalk(6);
            Intel_OfferInventionForAnalysis(0x13);
            ds.Intel_Phoenix1Armed = 0;
        }

        char t[256], name[256], sys[256], body[256];
        if (g->Leg2 == 0) {
            // A group in a system, not at a planet, arrives at planet 0: its
            // name is the bytes before the system's names (pstr_get's
            // PSTR_NUL).
            pstr_get(name, sizeof name, &g->Name);
            PlanetInfo_GetBodyFullName(body, sizeof body, g->System, g->Planet, g->Moon);
            SDL_snprintf(t, sizeof t, "%s arrived to %s %s", name, system_name(sys, g->System),
                         body);
            Message_AddMessageLogEntry(2, t, "message");
            g->Status = 2;
            arrived = true;
        }
        if (g->Leg2 == 0) {
            if (g->Planet == 0) {
                if (*system_status(g->System) == 0) {
                    SDL_snprintf(t, sizeof t,
                                 " Your ships explored %s system |    They found new planets",
                                 system_name(sys, g->System));
                    Message_ShowMessage(t);
                    if (g->System == 5)
                        Protect_ArmDateQuestion();
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
                        if (ds.Intel_System4Warned == 0)
                            ds.Intel_System4WarnTimer = 0x32;
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
                PlanetInfo_GetBodyName(body, sizeof body, g->System, g->Planet, 0);
                SDL_snprintf(t, sizeof t, "Your %s explored %s's moons",
                             g->Kind == 4 ? "satellite carrier" : "pilots", body);
                Message_ShowMessage(t);
            }
        }

        TBody *bd = body_at(g->System, g->Planet, g->Moon);
        if (g->Leg2 == 0) {
            if (Groups_At(0, i)->Kind == 1 && g->System == 8 && g->Planet == 3 && g->Moon == 0)
                ds.Pub_People[9].State = 2;
            if (g->Planet != 0 && Groups_At(0, i)->Kind != 4 && (int8_t)bd->Owner >= 2
                && race((int8_t)bd->Owner)->Status == 0)
                first_contact((int8_t)bd->Owner, " Your pilots met with a new alien race in ",
                              system_name(sys, g->System), "         They call themselves '");
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
                            first_contact(r, " Your pilots met with new alien race in ",
                                          system_name(sys, g->System),
                                          "         They call themselves '");
                    }
                }
            }
            // Intel::System4TalkArmed: set with System4Done (system 4 found).
            if (ds.Intel_System4TalkArmed != 0 && g->System == 4 && bd->Colony != 0
                && (bd->Owner == 1 || bd->Owner == 6)) {
                ds.Intel_System4TalkArmed = 0;
                AlienTalk_StartAlienTalk(9);
                Message_ShowMessageLine(0x2D);
                if (ds.Intel_System4Destroyed == 0)
                    ds.Intel_System4DestroyTimer = (int16_t)(System_Random(10) + 0x230);
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
                // The map screen's toolbar state (Launch): the selected system-view
                // entry, whose Kind > 3 is the force of race Kind - 3.
                if (ds.Launch_AttackIconShown == 0 && (int16_t)ds.Launch_SelectedEntry > 0) {
                    int16_t kind = ds.Launch_SystemViewTable[ds.Launch_SelectedEntry - 1].Kind;
                    if (kind > 3 && race((int16_t)(kind - 3))->Status != 6) {
                        ds.Launch_AttackIcon = 0;
                        for (int16_t e = 1; e <= 0x12; e++) {
                            if (ds.Launch_SystemViewTable[e - 1].Kind == 1)
                                ds.Launch_AttackIcon = 1;
                        }
                    }
                }
                if (ds.Launch_AttackIconShown == 0 && ds.Launch_AttackIcon != 0) {
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
    char t[256], body[256], name[256], sys[256];
    char where[51];  // string[50]
    for (int16_t r = 2; r <= 12; r++) {
        TRace *rc = race(r);
        // Forces with Active = 2 are on their way; _03 counts down the hours.
        int16_t forces = rc->ForceCount;
        for (int16_t f = 1; f <= forces; f++) {
            TRaceForce *fc = (TRaceForce *)((uint8_t *)rc + 0xC + f * 0x1B);
            if (fc->Active != 2)
                continue;
            PlanetInfo_GetBodyLongName(where, sizeof where, fc->System, fc->Body, fc->Moon);
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
                        first_contact(r,
                                      "   New alien race's ships arrived |     at your colony in ",
                                      system_name(sys, pf->System), " They call themselves '");
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
                        first_contact(r, " Your pilots met with new alien race in |   ", where,
                                      "     They call themselves '");
                    else if (rc->Status == 2)
                        hostile = true;
                }
            }
            if (hostile)
                show3("            ATTENTION!         |     Enemy fleet approaching   | ", where,
                      " ");
        }

        // (2841:0893: the original halts here after wrong page answers.)

        // Forces at their target (Active = 1) with a mission: Timer counts
        // down to the attack.
        forces = rc->ForceCount;
        for (int16_t f = 1; f <= forces; f++) {
            TRaceForce *fc = (TRaceForce *)((uint8_t *)rc + 0xC + f * 0x1B);
            if (!(fc->Active == 1 && fc->NextMission != 0 && --fc->Timer == 0))
                continue;
            PlanetInfo_GetBodyLongName(where, sizeof where, fc->System, fc->Body, fc->Moon);
            TBody *bd = body_at(fc->System, fc->Body, fc->Moon);
            // Battle setup (SpWar/GrWar).
            ds.SpWar_BattleSystem = fc->System;
            ds.SpWar_BattlePlanet = fc->Body;
            ds.SpWar_BattleMoon = fc->Moon;
            ds.SpWar_RaceMissionHigh = fc->NextMission >= 3;
            if (fc->NextMission == 1)
                ds.SpWar_GroundBattle = 0;
            if (fc->NextMission == 2)
                ds.SpWar_GroundBattle = 1;
            if (fc->NextMission == 3)
                ds.SpWar_GroundBattle = 0;
            if (fc->NextMission == 4)
                ds.SpWar_GroundBattle = 1;
            if (ds.SpWar_GroundBattle != 0 && bd->Owner != 1)
                ds.SpWar_GroundBattle = 0;
            bool defended = false;
            int16_t n = ds.Game_GroupCount;
            for (int16_t k = 1; k <= n; k++) {
                TGroup *g = Groups_At(0, k);
                if ((g->Kind == 1 || g->Kind == 3) && fc->System == g->System && fc->Body == g->Planet)
                    defended = true;
            }
            if (!defended) {
                if (ds.SpWar_GroundBattle != 0) {
                    SDL_snprintf(t, sizeof t, "| Your colony is under attack by %s |  at %s |",
                                 pstr_get(name, sizeof name, &rc->Name), where);
                    Message_ShowMessage(t);
                    ds.Game_CurrentScreen = 0x1D;
                    ds.Game_ScreenSwitch = 1;
                }
            } else {
                if (ds.SpWar_GroundBattle == 0) {
                    PlanetInfo_GetBodyLongName(body, sizeof body, fc->System, fc->Body, 0);
                    SDL_snprintf(t, sizeof t,
                                 " Your ships around %s |        is under attack by %s ", body,
                                 pstr_get(name, sizeof name, &rc->Name));
                    Message_ShowMessage(t);
                } else {
                    SDL_snprintf(t, sizeof t, "| Your colony is under attack by %s |  at %s |",
                                 pstr_get(name, sizeof name, &rc->Name), where);
                    Message_ShowMessage(t);
                }
                ds.Game_CurrentScreen = 0x1D;
                ds.Game_ScreenSwitch = 1;
            }
            ds.Sim_PlayerAttacks = 0;
            fc->NextMission = 0;
            ds.SpWar_EnemyRace = r;
        }
        if (ds.Game_CurrentScreen == 7 || ds.Game_CurrentScreen == 0x12)
            PlanetInfo_SetupSystemViewHotspots();
    }
}

// Staff::Ability[type, hired]: experience cap. Indexed flat like the original,
// so hired = 0 reads the previous entry.
static uint8_t ability_cap(int16_t type)
{
    return (&ds.Staff_Ability[0][0])[(type - 1) * 3 + ds.Game_StaffHired[type - 1] - 1];
}

// 1b8a:3b83
void Sim_GrowStaffExperience(void)
{
    for (int16_t k = 1; k <= 4; k++) {
        if (ds.Game_StaffHired[k - 1] != 0 && System_Random(100) == 0
            && ability_cap(k) > ds.Game_StaffExp[k - 1])
            ds.Game_StaffExp[k - 1]++;
    }
    // A staff member at the university: back when the hours are up, with
    // experience (and for the developer, the course's skills).
    int16_t t = ds.Advisor_TrainingKind;
    if (t <= 0)
        return;
    if (ds.Advisor_TrainingTimer > 0)
        ds.Advisor_TrainingTimer--;
    if (ds.Advisor_TrainingTimer != 0)
        return;
    uint16_t *exp = &ds.Game_StaffExp[t - 1];
    *exp += (uint16_t)(System_Random(4) + 0x69);
    uint8_t cap = ability_cap(t);
    if (cap < *exp)
        *exp = cap;
    if (t == 4) {
        for (int16_t j = 1; j <= 4; j++) {
            int16_t course = ds.Advisor_TrainingCourse;
            ds.Game_DevSkill[j - 1] += ds.Advisor_CourseSkills[course - 1][j - 1];
            // Staff::DevSkills[StoryStage, StaffHired[4], j], indexed flat
            const uint8_t *caps = &ds.Staff_DevSkills[0][0][0];
            uint8_t skillCap =
                caps[ds.Game_StoryStage * 12 + (ds.Game_StaffHired[3] - 1) * 4 + j - 1];
            if (skillCap < ds.Game_DevSkill[j - 1])
                ds.Game_DevSkill[j - 1] = skillCap;
        }
    }
    ds.Advisor_TrainingKind = 0;
    if (ds.Game_CurrentScreen == 1) {
        ds.Game_CurrentScreen = -1;
        ds.Game_ScreenSwitch = 1;
    }
    Message_ShowMessage("| Your advisor has returned from the university |");
    if (ds.Game_CurrentScreen == 3) {
        Research_DrawInventionInfo();
        Research_DrawDeveloperSkills();
    }
}

// A story timer counts down while its `done` byte is clear; true when it
// reaches 0 this hour (the caller sets `done`).
static bool tick(int16_t *timer)
{
    if (*timer > 0 && --*timer == 0)
        return true;
    return false;
}

// 1b8a:3d07: the idea timers and the story timers (each with its "done" flag).
void Sim_ProcessStoryTimers(void)
{
    if (ds.Game_StaffExp[3] != 0 && ds.Intel_DevAwayTimer == 0
        && (ds.Advisor_TrainingTimer == 0 || ds.Advisor_TrainingKind != 4)) {
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
    if (ds.Intel_SatTheftDone == 0 && tick(&ds.Intel_SatTheftTimer)) {
        ds.Intel_SatTheftDone = 1;
        Intel_ScheduleIdea(4, 10, 10);
        AlienTalk_StartAlienScene(9);
    }
    if (ds.Game_StaffHired[3] != 0 && ds.Intel_Race2Done1 == 0 && ds.Intel_Race2Timer1 > 0
        && (ds.Advisor_TrainingTimer == 0 || ds.Advisor_TrainingKind != 4)
        && ds.Races_Races[0].Status > 2 && --ds.Intel_Race2Timer1 == 0) {
        if (product(8)->State == 5)
            AlienTalk_StartAlienTalk(2);
        else
            Message_ShowMessageLine(6);
    }
    if (tick(&ds.Intel_DevAwayTimer)) {
        Message_ShowMessageLine(8);
        if (ds.Game_CurrentScreen == 1) {
            ds.Command_ShowHologramOnEntry = 1;
            ds.Game_PrevScreen = 1;
            ds.Game_CurrentScreen = -1;
            ds.Game_ScreenSwitch = 1;
        }
        Intel_GrantInvention(9);
    }
    if (ds.Intel_Race2Done2 == 0 && ds.Intel_Race2Timer2 > 0 && ds.Races_Races[0].Status > 2) {
        ds.Intel_Race2Timer2--;
        if (ds.Intel_Race2Timer2 == 0) {
            if (product(8)->State == 5) {
                Message_ShowMessageLine(9);
                AlienTalk_StartAlienTalk(3);
            } else {
                Message_ShowMessageLine(6);
            }
        }
        if (ds.Intel_Race2Timer2 == 0)
            ds.Intel_Race2Done2 = 1;
    }
    if (ds.Intel_Race2Done3 == 0 && ds.Intel_Race2Timer3 > 0
        && (ds.Races_Races[0].Status == 0 || ds.Races_Races[0].Status > 2)
        && --ds.Intel_Race2Timer3 == 0) {
        if (product(8)->State == 5) {
            Message_ShowMessageLine(10);
            AlienTalk_StartAlienTalk(4);
        } else {
            Message_ShowMessageLine(6);
        }
        ds.Intel_Race2Done3 = 1;
        if (ds.Intel_Race2Done4 == 0)
            ds.Intel_Race2Timer4 = (int16_t)(System_Random(0x32) + 200);
        Intel_SetForceTarget(3, 1, 1, 7, 0);
    }
    if (ds.Intel_Race2Done4 == 0 && tick(&ds.Intel_Race2Timer4)) {
        if (ds.Races_Races[0].Status > 0)
            Message_ShowMessageLine(0xB);
        if (ds.Races_Races[0].Status > 0)
            AlienTalk_StartAlienScene(1);
        Intel_EliminateRace(2);
        Intel_ClaimBodyForRace(3, 1, 7, 0);
        if (ds.Intel_MorgrulAttack1Done == 0)
            ds.Intel_MorgrulAttack1Timer = (int16_t)(System_Random(0x32) + 600);
        if (ds.Intel_Invention14Revealed == 0)
            ds.Intel_Invention14Timer = (int16_t)(System_Random(0x32) + 300);
        if (ds.Intel_MorgrulAttack2Done == 0)
            ds.Intel_MorgrulAttack2Timer = (int16_t)(System_Random(200) + 0x708);
        ds.Intel_Race2Done4 = 1;
        ds.Game_StoryStage = 2;
    }
    if (ds.Intel_MorgrulAttack1Done == 0 && tick(&ds.Intel_MorgrulAttack1Timer)) {
        Intel_DispatchForce(3, 1, 1, 5, 0, 2);
        ds.Intel_MorgrulAttack1Done = 1;
    }
    if (ds.Intel_Invention14Revealed == 0 && ds.Intel_Invention14Timer > 0 && product(14)->State > 0
        && ds.Game_StaffExp[3] != 0 && ds.Intel_DevAwayTimer == 0 && ds.Advisor_TrainingKind != 4
        && --ds.Intel_Invention14Timer == 0) {
        Message_ShowMessageLine(0x1E);
        ds.Intel_Invention14Revealed = 1;
    }
    if (ds.Intel_MorgrulAttack2Done == 0 && tick(&ds.Intel_MorgrulAttack2Timer)) {
        Intel_DispatchForce(3, 2, 1, 5, 0, 2);
        ds.Intel_MorgrulAttack2Done = 1;
        ds.Intel_MorgrulAttack2Open = 1;
        if (ds.Intel_MorgrulAttack3Done == 0)
            ds.Intel_MorgrulAttack3Timer = (int16_t)(System_Random(0x32) + 0x44C);
        if (ds.Intel_Phoenix1Done == 0)
            ds.Intel_Phoenix1Timer = (int16_t)(System_Random(0x14) + 100);
    }
    if (ds.Intel_Phoenix1Done == 0 && ds.Intel_Phoenix1Timer > 0 && product(12)->State == 5
        && --ds.Intel_Phoenix1Timer == 0) {
        Message_ShowMessageLine(0x11);
        ds.Intel_Phoenix1Done = 1;
        ds.Intel_Phoenix1Armed = 1;
    }
    if (ds.Intel_MorgrulAttack3Done == 0 && tick(&ds.Intel_MorgrulAttack3Timer)) {
        Intel_DispatchForce(3, 3, 1, 5, 0, 2);
        ds.Intel_MorgrulAttack3Done = 1;
        ds.Intel_MorgrulAttack3Open = 1;
        if (ds.Intel_Msg14Done == 0)
            ds.Intel_Msg14Timer = (int16_t)(System_Random(0x32) + 0x32);
        Intel_ScheduleForceLaunch(3, 6, (int16_t)(System_Random(500) + 8000), 2);
    }
    if (ds.Intel_Race4Done == 0 && tick(&ds.Intel_Race4Timer)) {
        ds.Intel_Race4Done = 1;
        ds.Intel_KallForceSent = 1;
        if (ds.Intel_KallTurnDone == 0)
            ds.Intel_KallTurnTimer = (int16_t)(System_Random(0x32) + 4000);
        Intel_SetForceTarget(4, 2, 2, 5, 0);
    }
    if (ds.Intel_KallTurnDone == 0 && tick(&ds.Intel_KallTurnTimer)) {
        ds.Intel_KallTurnDone = 1;
        ds.Intel_KallForceSent = 0;
        if (ds.Intel_JointAttackDone == 0)
            ds.Intel_JointAttackTimer = (int16_t)(System_Random(0x32) + 300);
        ds.Races_Races[2].Status = 2;
        Intel_SetForceTarget(3, 4, 2, 5, 0);
        Message_ShowMessageLine(0x18);
        Intel_EliminateRace(5);
    }
    if (ds.Intel_JointAttackDone == 0 && tick(&ds.Intel_JointAttackTimer)) {
        ds.Intel_JointAttackDone = 1;
        Intel_DispatchForce(3, 4, 1, 5, 0, 2);
        Intel_DispatchForce(4, 2, 1, 5, 0, 2);
    }
    if (ds.Intel_Race5Done == 0 && ds.Intel_Race5Timer > 0 && ds.Races_Races[3].Status > 2
        && --ds.Intel_Race5Timer == 0) {
        ds.Intel_Race5Done = 1;
        AlienTalk_StartAlienTalk(7);
    }
    if (ds.Intel_Msg14Done == 0 && tick(&ds.Intel_Msg14Timer)) {
        ds.Intel_Msg14Done = 1;
        Message_ShowMessageLine(0x14);
    }
    if (ds.Galaxy_SystemStatus[2] < 0 && tick(&ds.Intel_System3Timer)) {
        ds.Galaxy_SystemStatus[2] = 0;
        Message_ShowMessageLine(0x25);
    }
    if (ds.Intel_System4Warned == 0 && ds.Intel_System4WarnTimer > 0 && *observatories(4) > 0
        && --ds.Intel_System4WarnTimer == 0) {
        ds.Intel_System4Warned = 1;
        ds.Intel_System4TalkArmed = 1;
        Message_ShowMessageLine(0x19);
        if (ds.Intel_System4Destroyed == 0)
            ds.Intel_System4DestroyTimer = (int16_t)(System_Random(100) + 1000);
    }
    if (ds.Intel_System4Destroyed == 0 && tick(&ds.Intel_System4DestroyTimer)) {
        ds.Intel_System4Destroyed = 1;
        ds.Intel_System4WarnTimer = 0;
        Message_ShowMessageLine(0x1B);
        AlienTalk_StartAlienScene(7);
        Intel_AnnihilateSystem4();
    }
    if (ds.Intel_Race7to10Done == 0 && tick(&ds.Intel_Race7to10Timer)) {
        Intel_DispatchForce(7, 1, 1, 5, 0, 2);
        ds.Intel_Race7to10Done = 1;
        Intel_ScheduleForceLaunch(8, 1, (int16_t)(System_Random(500) + 1000), 2);
    }
    if (tick(&ds.Intel_LaunchTimer)) {
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
