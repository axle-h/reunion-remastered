// routes: the trade routes improvement (game/routes.h) without the screens.
// A route of two stops runs several loops, docked 5 hours at each stop and
// at New Earth; a full group goes home early; New Earth full keeps the rest
// aboard; each stop condition ends a route with one log entry, routine
// traffic adds none; a disband shifts the routes with the groups; the switch
// off freezes them. Saves: the port's format while the switch is on or a
// route is held, the DOS one otherwise, one file a slot, and a save made
// while docked loads back with its hours.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/cargo.h"
#include "game/dseg.h"
#include "game/gfx.h"
#include "game/groups.h"
#include "game/improvements.h"
#include "game/intel.h"
#include "game/main_program.h"
#include "game/message.h"
#include "game/planetinfo.h"
#include "game/routes.h"
#include "game/saveload.h"
#include "game/sim.h"
#include "game/system.h"
#include "game/textfile.h"
#include "platform/fs.h"
#include "platform/video.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static bool wait_retrace(void) { return true; }
static bool key_pressed(void) { return true; }
static char read_key(void) { return '\r'; }

enum : int16_t { A_MOON = 1, B_MOON = 2 };  // New Earth's first two moons are the stops

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
    gfx_wait_retrace = wait_retrace;
    TextFile_KeyPressed = key_pressed;
    TextFile_ReadKey = read_key;
    SaveLoad_LoadInit();
    Groups_InitPlanetForce();  // New Earth's planet force
    Groups_At(1, 1)->Storage = 10;  // 10,000 t of each ore
    Routes_Reset();
    improvements.trade_routes = true;
    ds.Game_CurrentScreen = 1;
    ds.Game_StaffExp[0] = 20;  // a pilot: 21 legs an hour
    ds.Game_StaffHired[0] = 1;
    ds.Message_LogCount = 0;
}

static TBody *stop_body(int16_t moon)
{
    return PlanetInfo_BodyAt(1, 5, moon);
}

// A Miner station body of the player's with `ore` t of ore `k` (0-based).
static void own_stop(int16_t moon, int k, int32_t ore)
{
    TBody *b = stop_body(moon);
    b->Owner = 1;
    b->Droids = 1;
    b->SurfaceType = 1;
    for (int i = 0; i < 8; i++)
        b->Stores[i] = 0;
    b->Stores[k] = ore;
}

// Fleet group i, a trade group of `ships` Sloops docked at New Earth.
static TGroup *trade_group(int16_t i, int16_t ships)
{
    if (ds.Game_GroupCount < i)
        ds.Game_GroupCount = i;
    TGroup *g = Groups_At(0, i);
    SDL_memset(g, 0, sizeof *g);
    g->Kind = 2;
    pstr_set(&g->Name, 17, "Hauler");
    g->System = 1;
    g->Planet = 5;
    g->Status = 1;
    g->Units[0][0].Count = ships;
    return g;
}

static void two_stops(int16_t group)
{
    CHECK(Routes_StopRefusal(group, 1, 5, A_MOON) == nullptr);
    Routes_AddStop(group, 0, 1, 5, A_MOON);
    CHECK(Routes_StopRefusal(group, 1, 5, B_MOON) == nullptr);
    Routes_AddStop(group, 1, 1, 5, B_MOON);
}

static void hour(void)
{
    Sim_AdvanceHour();
    Sim_ProcessTravellingGroups();
    Routes_Hour();
}

// Where group g spends an hour docked: 0 New Earth, the moon of a stop, -1
// not docked.
static int docked_at(const TGroup *g)
{
    return g->Status == 1 ? g->Moon : -1;
}

// Runs the hours until `n` dock periods have ended, recording each period's
// place and length.
typedef struct {
    int place[16], hours[16], count;
} Docks;

static void run_docks(const TGroup *g, Docks *d, int n, int max_hours, void (*at_home)(void))
{
    int current = -1;
    for (int h = 0; h < max_hours && d->count < n; h++) {
        hour();
        int at = docked_at(g);
        if (at != current && current >= 0)
            d->count++;
        if (at >= 0) {
            if (at != current) {
                d->place[d->count] = at;
                d->hours[d->count] = 0;
                if (at == 0 && at_home)
                    at_home();
            }
            d->hours[d->count]++;
        }
        current = at;
    }
}

static void refill(void)
{
    stop_body(A_MOON)->Stores[0] += 300;
    stop_body(B_MOON)->Stores[1] += 200;
}

static void test_loops(void)
{
    reset();
    own_stop(A_MOON, 0, 300);
    own_stop(B_MOON, 1, 200);
    TGroup *g = trade_group(1, 2);
    two_stops(1);
    CHECK(Routes_Of(1)->StopCount == 2);
    int32_t home0 = ds.Game_HomeStock[0], home1 = ds.Game_HomeStock[1];
    CHECK(Routes_Start(1) == nullptr && Routes_Running(1));
    Docks d = {};
    run_docks(g, &d, 9, 500, refill);
    CHECK(d.count == 9);
    for (int i = 0; i < d.count; i++) {
        CHECK(d.place[i] == (i % 3 == 0 ? A_MOON : i % 3 == 1 ? B_MOON : 0));
        CHECK(d.hours[i] == ROUTE_DOCK_HOURS);
    }
    // Three loads of each brought home (refilled at each unloading).
    CHECK(ds.Game_HomeStock[0] == home0 + 3 * 300);
    CHECK(ds.Game_HomeStock[1] == home1 + 3 * 200);
    CHECK(g->Cargo[0] == 0 && g->Cargo[1] == 0);
    CHECK(ds.Message_LogCount == 0);  // no arrivals logged
}

static void test_full_goes_home(void)
{
    reset();
    TGroup *g = trade_group(1, 1);
    int32_t cap = Cargo_ShipCapacity(g);
    own_stop(A_MOON, 0, cap + 100);
    own_stop(B_MOON, 1, 200);
    two_stops(1);
    CHECK(Routes_Start(1) == nullptr);
    Docks d = {};
    run_docks(g, &d, 2, 300, nullptr);
    CHECK(d.count == 2 && d.place[0] == A_MOON && d.place[1] == 0);
    CHECK(stop_body(A_MOON)->Stores[0] == 100 && stop_body(B_MOON)->Stores[1] == 200);
    CHECK(ds.Message_LogCount == 0);
}

static void test_new_earth_full(void)
{
    reset();
    TGroup *g = trade_group(1, 2);
    own_stop(A_MOON, 0, 300);
    Routes_AddStop(1, 0, 1, 5, A_MOON);
    int32_t cap = Groups_PlanetStorage(1, 5, 0) * 1000;
    ds.Game_HomeStock[0] = cap - 50;
    CHECK(Routes_Start(1) == nullptr);
    Docks d = {};
    run_docks(g, &d, 2, 300, nullptr);
    CHECK(d.count == 2 && d.place[1] == 0);
    CHECK(ds.Game_HomeStock[0] == cap && g->Cargo[0] == 250);
    // On to the stop again, the rest aboard.
    CHECK(Routes_Running(1) && Routes_Of(1)->Next == 1);
}

// Starts group 1's route and lets it fly an hour.
static void running_route(void)
{
    reset();
    own_stop(A_MOON, 0, 300);
    own_stop(B_MOON, 1, 200);
    trade_group(1, 2);
    two_stops(1);
    CHECK(Routes_Start(1) == nullptr);
    hour();
    CHECK(Routes_Running(1) && ds.Message_LogCount == 0);
}

static bool ended(int16_t group, RouteEnd why, int stop)
{
    const Route *r = Routes_Of(group);
    return r->State == ROUTE_ENDED && r->Why == why && r->WhyStop == stop && !Routes_Running(group);
}

static void test_stop_conditions(void)
{
    char text[128];

    running_route();
    Routes_Order(1);
    CHECK(ended(1, ROUTE_END_ORDER, 0) && ds.Message_LogCount == 1);
    Routes_StateText(1, text, sizeof text);
    CHECK(SDL_strcmp(text, "Route ended: another order") == 0);
    Routes_Order(1);  // once
    CHECK(ds.Message_LogCount == 1);

    running_route();
    stop_body(B_MOON)->Owner = 3;
    hour();
    CHECK(ended(1, ROUTE_END_LOST, 2) && ds.Message_LogCount == 1);

    running_route();
    ds.Game_StaffExp[0] = 0;
    hour();
    CHECK(ended(1, ROUTE_END_REACH, 1) && ds.Message_LogCount == 1);

    // Out of the system: no Galleon, then a pilot who can't go that far.
    reset();
    trade_group(1, 2);
    CHECK(Routes_StopRefusal(1, 2, 1, 0) != nullptr);

    running_route();
    trade_group(2, 1);  // another group, no route
    Groups_At(0, 1)->Status = 2;
    Groups_At(0, 1)->Moon = 0;
    Intel_SpaceBattleLost(1, 5);  // at New Earth: disbanded
    CHECK(ds.Game_GroupCount == 0 && ds.Message_LogCount == 1);
    CHECK(!Routes_Any());

    // A disband: the later routes move up with their groups.
    running_route();
    trade_group(2, 1);
    trade_group(3, 1);
    Routes_AddStop(3, 0, 1, 5, B_MOON);
    pstr_set(&Groups_At(0, 3)->Name, 17, "Third");
    ds.Game_ShownGroupCount = ds.Game_GroupCount;
    Intel_DisbandGroup(0, 1);
    CHECK(ds.Message_LogCount == 1);  // the running one
    CHECK(Routes_Of(1)->StopCount == 0 && Routes_Of(2)->StopCount == 1);
    CHECK(Routes_Of(2)->Stops[0].Moon == B_MOON && Groups_At(0, 2)->Name.s[0] == 'T');
    CHECK(Routes_Of(3)->StopCount == 0);

    // The switch off: kept, neither flown nor ended.
    running_route();
    improvements.trade_routes = false;
    Route before = *Routes_Of(1);
    TGroup gb = *Groups_At(0, 1);
    Routes_Hour();
    Routes_Order(1);
    CHECK(SDL_memcmp(&before, Routes_Of(1), sizeof before) == 0);
    CHECK(SDL_memcmp(&gb, Groups_At(0, 1), sizeof gb) == 0);
    CHECK(ds.Message_LogCount == 0);
    improvements.trade_routes = true;
}

static void test_editing(void)
{
    reset();
    own_stop(A_MOON, 0, 0);
    own_stop(B_MOON, 0, 0);
    trade_group(1, 1);
    CHECK(Routes_StopRefusal(1, 1, 5, 0) != nullptr);  // New Earth, the drop-off
    stop_body(B_MOON)->Owner = 0;
    CHECK(Routes_StopRefusal(1, 1, 5, B_MOON) != nullptr);  // not the player's
    stop_body(B_MOON)->Owner = 1;
    CHECK(Routes_AddStop(1, 0, 1, 5, B_MOON) == 1);
    CHECK(Routes_StopRefusal(1, 1, 5, B_MOON) != nullptr);  // on the route
    CHECK(Routes_AddStop(1, 0, 1, 5, A_MOON) == 1);  // before it
    Route *r = Routes_Of(1);
    CHECK(r->Stops[0].Moon == A_MOON && r->Stops[1].Moon == B_MOON);
    Routes_MoveStop(1, 1, 1);
    CHECK(r->Stops[0].Moon == B_MOON && r->Stops[1].Moon == A_MOON);
    Routes_RemoveStop(1, 1);
    CHECK(r->StopCount == 1 && r->Stops[0].Moon == A_MOON);
    for (int k = 0; k < ROUTE_MAX_STOPS - 1; k++)
        r->Stops[r->StopCount++] = (RouteStop){2, (uint8_t)(k + 1), 0};
    CHECK(Routes_StopRefusal(1, 1, 5, B_MOON) != nullptr);  // full
}

// --- Saves ---------------------------------------------------------------------------

static char dir[128];

static bool exists(const char *name)
{
    char path[256];
    SDL_snprintf(path, sizeof path, "%s/%s", dir, name);
    return SDL_GetPathInfo(path, nullptr);
}

static bool save_slot(int16_t slot)
{
    SaveLoad_ReadSlotNames();
    ds.SaveLoad_SelectedSlot = slot;
    SDL_memcpy(ds.SaveLoad_SlotTable + (slot - 1) * 0x16 + 1, "\x04" "Test", 5);
    bool saved = SaveLoad_Transfer(true);
    game_free(ds.SaveLoad_SlotTable);
    return saved;
}

static bool load_slot(int16_t slot)
{
    SaveLoad_ReadSlotNames();
    ds.SaveLoad_SelectedSlot = slot;
    bool loaded = SaveLoad_Transfer(false);
    game_free(ds.SaveLoad_SlotTable);
    return loaded;
}

static void test_saves(void)
{
    SDL_snprintf(dir, sizeof dir, "/tmp/reunion_test_routes_%d", (int)SDL_GetCurrentTime(nullptr) & 0xFFFF);
    SaveLoad_SetSaveDir(dir);

    // A save made while docked at a stop, loading: the port's format.
    reset();
    own_stop(A_MOON, 0, 300);
    own_stop(B_MOON, 1, 200);
    TGroup *g = trade_group(1, 2);
    two_stops(1);
    CHECK(Routes_Start(1) == nullptr);
    for (int h = 0; h < 100 && Routes_Of(1)->State != ROUTE_DOCKED; h++)
        hour();
    hour();
    hour();
    Route docked = *Routes_Of(1);
    TGroup gs = *g;
    CHECK(docked.State == ROUTE_DOCKED && docked.Hours == ROUTE_DOCK_HOURS - 2);
    CHECK(save_slot(1) && exists("reunion.1") && !exists("spidysav.1"));
    int16_t buildings = ds.Game_BuildingCount;
    TBuilding last = ds.Game_Buildings[buildings - 1];

    Routes_Reset();
    SDL_memset(g, 0xEE, sizeof *g);
    ds.Game_BuildingCount = 0;
    CHECK(load_slot(1));
    CHECK(SDL_memcmp(Routes_Of(1), &docked, sizeof docked) == 0);
    CHECK(SDL_memcmp(g, &gs, sizeof gs) == 0);
    CHECK(ds.Game_BuildingCount == buildings
          && SDL_memcmp(&ds.Game_Buildings[buildings - 1], &last, sizeof last) == 0);
    // It docks the hours left, then flies on.
    for (int h = 0; h < docked.Hours - 1; h++) {
        hour();
        CHECK(g->Status == 1);
    }
    hour();
    CHECK(g->Status != 1 && g->Cargo[0] == 300 && Routes_Of(1)->Next == 2);

    // The switch off and no route: the DOS format, and the port's file goes.
    improvements.trade_routes = false;
    Routes_Reset();
    CHECK(save_slot(1) && exists("spidysav.1") && !exists("reunion.1"));
    // A route still held: the port's format all the same.
    Routes_AddStop(1, 0, 1, 5, A_MOON);
    CHECK(save_slot(1) && exists("reunion.1") && !exists("spidysav.1"));
    // The switch on, no route: the port's format.
    improvements.trade_routes = true;
    Routes_Reset();
    CHECK(save_slot(2) && exists("reunion.2"));
    improvements.trade_routes = false;
    CHECK(save_slot(2) && exists("spidysav.2") && !exists("reunion.2"));
    // A DOS save loaded clears the routes.
    Routes_AddStop(1, 0, 1, 5, A_MOON);
    CHECK(load_slot(2) && !Routes_Any());
    CHECK(load_slot(1) && Routes_Of(1)->StopCount == 1);

    // A newer version of the format is listed, not loaded.
    char path[256];
    SDL_snprintf(path, sizeof path, "%s/reunion.1", dir);
    size_t size = 0;
    uint8_t *file = SDL_LoadFile(path, &size);
    CHECK(file && size > 10 && SDL_memcmp(file, "REUNION\x1A", 8) == 0 && file[8] == 2 && file[9] == 0);
    if (file) {
        file[8] = 99;
        SDL_SaveFile(path, file, size);
        SDL_free(file);
    }
    SaveLoad_ReadSlotNames();
    CHECK(ds.SaveLoad_SlotTable[0] == 1);
    ds.SaveLoad_SelectedSlot = 1;
    CHECK(!SaveLoad_Transfer(false));
    game_free(ds.SaveLoad_SlotTable);

    for (int n = 1; n <= 2; n++) {
        char p[256];
        SDL_snprintf(p, sizeof p, "%s/reunion.%d", dir, n);
        SDL_RemovePath(p);
        SDL_snprintf(p, sizeof p, "%s/spidysav.%d", dir, n);
        SDL_RemovePath(p);
    }
    SDL_RemovePath(dir);
    improvements.trade_routes = false;
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    test_loops();
    test_full_goes_home();
    test_new_earth_full();
    test_stop_conditions();
    test_editing();
    test_saves();
    fs_shutdown();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
