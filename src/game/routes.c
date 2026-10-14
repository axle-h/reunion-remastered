#include "game/routes.h"

#include <SDL3/SDL.h>

#include "game/blit.h"
#include "game/cargo.h"
#include "game/gfx.h"
#include "game/gfxpath.h"
#include "game/groups.h"
#include "game/improvements.h"
#include "game/intel.h"
#include "game/lang.h"
#include "game/message.h"
#include "game/mouse.h"
#include "game/planetinfo.h"
#include "game/portcmd.h"
#include "game/satellite.h"
#include "game/shipinfo.h"
#include "game/shipmove.h"
#include "game/sound.h"
#include "game/system.h"

Route Routes_Table[ROUTE_SLOTS];

static const RouteStop HOME = {1, 5, 0};  // New Earth

void Routes_Reset(void)
{
    SDL_zeroa(Routes_Table);
}

bool Routes_Any(void)
{
    for (int i = 0; i < ROUTE_SLOTS; i++)
        if (Routes_Table[i].StopCount != 0 || Routes_Table[i].State != ROUTE_IDLE)
            return true;
    return false;
}

Route *Routes_Of(int16_t group)
{
    return &Routes_Table[group - 1];
}

static bool valid(int16_t group)
{
    return group >= 1 && group <= ROUTE_SLOTS && group <= ds.Game_GroupCount;
}

static bool active(const Route *r)
{
    return r->State == ROUTE_FLYING || r->State == ROUTE_DOCKED;
}

bool Routes_Running(int16_t group)
{
    return improvements.trade_routes && valid(group) && active(Routes_Of(group));
}

// --- The route ----------------------------------------------------------------------

static bool same(RouteStop a, RouteStop b)
{
    return a.System == b.System && a.Planet == b.Planet && a.Moon == b.Moon;
}

static RouteStop target(const Route *r)
{
    return r->Next == ROUTE_HOME ? HOME : r->Stops[r->Next - 1];
}

static TBody *body(RouteStop s)
{
    return PlanetInfo_BodyAt(s.System, s.Planet, s.Moon);
}

// Docked or in orbit there.
static bool at(const TGroup *g, RouteStop s)
{
    return (g->Status == 1 || g->Status == 2) && same((RouteStop){g->System, g->Planet, g->Moon}, s);
}

// A body the cargo screen trades with: the player's colony or Miner stations.
static bool own(RouteStop s)
{
    const TBody *b = body(s);
    return b->Owner == 1 && (b->Colony != 0 || b->Droids != 0);
}

// The original's refusal of the group's flight to a body of system `sys`
// from anywhere on the route (Move, Launch_DestinationScreen): no pilot, no
// ships, or out of the system without a Galleon or beyond the pilot's
// reach. nullptr if it can fly there.
static const char *reach_refusal(int16_t group, int16_t sys)
{
    const TGroup *g = Groups_At(0, group);
    if (ds.Game_StaffExp[0] == 0)
        return Lang_Text(" You haven't got a pilot! ");
    if (Groups_IsGroupEmpty(0, group))
        return Lang_Text(" No ships in this unit! ");
    if (sys == HOME.System)
        return nullptr;
    if (g->Units[0][3].Count <= 0)
        return Lang_Text(" No hyperdrive in this unit! ");
    if (ds.Game_StaffHired[0] == 3 || (ds.Game_StaffHired[0] == 2 && sys < 4))
        return nullptr;
    return Lang_Text(" Your pilot is unable to undertake |        such a long voyage!");
}

static int32_t ore_aboard(const TGroup *g)
{
    int32_t sum = 0;
    for (int i = 0; i < 6; i++)
        sum += g->Cargo[i];
    return sum;
}

static bool ore_stored(RouteStop s)
{
    const TBody *b = body(s);
    for (int i = 0; i < 6; i++)
        if (b->Stores[i] > 0)
            return true;
    return false;
}

// Redraws the screen showing the group after a route docked, launched or
// loaded it.
static void refresh_views(int16_t group)
{
    if (ds.Game_ScreenSwitch)
        return;
    bool shown = ds.Game_GroupList == 0 && ds.Game_SelGroup == group;
    switch (ds.Game_CurrentScreen) {
    case 0x10:
        ShipInfo_RedrawGroup(group);
        break;
    case 0x11:
        if (shown) {
            const TGroup *g = Groups_At(0, group);
            if (g->Status == 1 || g->Status == 2) {
                ds.Game_CurSystem = g->System;
                ds.Game_CurPlanet[ds.Game_CurSystem - 1] = g->Planet;
                ds.Game_CurMoon[ds.Game_CurSystem - 1] = g->Moon;
                PlanetInfo_SelectCurBody();
            }
            ShipMove_SetupGroupScreenHotspots();
            ShipMove_RedrawView();
            Satellite_SetupGroupScreenMenu();
        }
        break;
    case 0x16:
        if (shown) {
            Groups_SetupGroupsHotspots();
            Satellite_UpdateGroupsMenu();
            Groups_DrawGroupPanel(true);
        }
        break;
    case 0xD:
        if (shown) {
            Cargo_InitCargoScreen();
            Cargo_SetupCargoHotspots();
            Cargo_DrawCargoNames(0);
            Cargo_DrawCargoAmounts();
        }
        break;
    default:
        break;
    }
}

// The reason a route ended, as its footer and log entry give it.
static void reason_text(const Route *r, char *out, size_t size)
{
    switch (r->Why) {
    case ROUTE_END_ORDER:
        SDL_strlcpy(out, Lang_Text("another order"), size);
        break;
    case ROUTE_END_BATTLE:
        SDL_strlcpy(out, Lang_Text("battle lost"), size);
        break;
    case ROUTE_END_LOST:
        SDL_snprintf(out, size, Lang_Text("stop %d lost"), r->WhyStop);
        break;
    case ROUTE_END_REACH:
        SDL_snprintf(out, size, Lang_Text("stop %d out of reach"), r->WhyStop);
        break;
    case ROUTE_END_DISBANDED:
        SDL_strlcpy(out, Lang_Text("disbanded"), size);
        break;
    default:
        out[0] = '\0';
        break;
    }
}

// Ends a running route on its own, with a log entry (sample and flash, no
// box).
static void end(int16_t group, RouteEnd why, int stop)
{
    if (!Routes_Running(group))
        return;
    Route *r = Routes_Of(group);
    r->State = ROUTE_ENDED;
    r->Why = why;
    r->WhyStop = (uint8_t)stop;
    r->Hours = 0;
    char name[32], reason[64], t[128];
    pstr_get(name, sizeof name, &Groups_At(0, group)->Name);
    size_t n = SDL_strlen(name);
    while (n > 0 && name[n - 1] == ' ')
        name[--n] = '\0';
    reason_text(r, reason, sizeof reason);
    SDL_snprintf(t, sizeof t, "%s%s%s", name, Lang_Text(": route ended, "), reason);
    Message_AddMessageLogEntry(2, t, "message");
    if (why != ROUTE_END_DISBANDED)
        refresh_views(group);
}

static void arrive(int16_t group);

// Off to the target: the game's own flight, from the dock or orbit (or
// from a flight elsewhere, as Change destination does).
static void go_to(int16_t group)
{
    Route *r = Routes_Of(group);
    TGroup *g = Groups_At(0, group);
    RouteStop t = target(r);
    r->State = ROUTE_FLYING;
    r->Hours = 0;
    if (at(g, t)) {
        arrive(group);
        return;
    }
    if (g->Status == 1)
        g->Status = 2;
    Intel_SetGroupTarget(group, t.System, t.Planet, t.Moon);
}

// The next target after stop Next or home: the next stop, or home after the
// last one or once the group is full.
static void advance(int16_t group)
{
    Route *r = Routes_Of(group);
    if (r->Next == ROUTE_HOME)
        r->Next = 1;
    else if (r->Next >= r->StopCount || Cargo_ShipRoom(Groups_At(0, group)) <= 0)
        r->Next = ROUTE_HOME;
    else
        r->Next++;
}

// At the target: docks for ROUTE_DOCK_HOURS, or passes by when there is
// nothing to load (or no room) or nothing to unload.
static void arrive(int16_t group)
{
    Route *r = Routes_Of(group);
    TGroup *g = Groups_At(0, group);
    RouteStop t = target(r);
    if (!at(g, t)) {
        go_to(group);  // sent elsewhere meanwhile
        return;
    }
    bool pass = r->Next == ROUTE_HOME ? ore_aboard(g) <= 0
                                      : Cargo_ShipRoom(g) <= 0 || !ore_stored(t);
    if (pass) {
        advance(group);
        go_to(group);
        return;
    }
    TBody *b = body(t);
    if (b->InfoLevel < 6)
        b->InfoLevel = 6;  // as docking by hand does
    g->Status = 1;
    r->State = ROUTE_DOCKED;
    r->Hours = ROUTE_DOCK_HOURS;
}

// The docking hours are over: load or unload, and on.
static void depart(int16_t group)
{
    Route *r = Routes_Of(group);
    Cargo_MoveAllOre(Groups_At(0, group), r->Next != ROUTE_HOME);
    advance(group);
    go_to(group);
}

// A stop the player lost or the group can't reach ends the route.
static bool stop_conditions(int16_t group)
{
    const Route *r = Routes_Of(group);
    for (int k = 1; k <= r->StopCount; k++)
        if (!own(r->Stops[k - 1])) {
            end(group, ROUTE_END_LOST, k);
            return true;
        }
    for (int k = 1; k <= r->StopCount; k++)
        if (reach_refusal(group, r->Stops[k - 1].System)) {
            end(group, ROUTE_END_REACH, k);
            return true;
        }
    return false;
}

static void screen_hour(void);

void Routes_Hour(void)
{
    if (!improvements.trade_routes)
        return;
    for (int16_t i = 1; i <= ds.Game_GroupCount && i <= ROUTE_SLOTS; i++) {
        Route *r = Routes_Of(i);
        if (!active(r) || stop_conditions(i))
            continue;
        const TGroup *g = Groups_At(0, i);
        uint8_t status = g->Status;
        int32_t ore = ore_aboard(g);
        if (r->State == ROUTE_DOCKED) {
            if (r->Hours > 0)
                r->Hours--;
            if (r->Hours == 0)
                depart(i);
        } else if (g->Status == 1 || g->Status == 2) {
            arrive(i);
        }
        if (g->Status != status || ore_aboard(g) != ore)
            refresh_views(i);
    }
    if (ds.Game_CurrentScreen == SCREEN_ROUTE && !ds.Game_ScreenSwitch)
        screen_hour();
}

void Routes_Order(int16_t group)
{
    end(group, ROUTE_END_ORDER, 0);
}

void Routes_BattleLost(int16_t group)
{
    end(group, ROUTE_END_BATTLE, 0);
}

void Routes_GroupDisbanded(int16_t group)
{
    if (group < 1 || group > ROUTE_SLOTS)
        return;
    end(group, ROUTE_END_DISBANDED, 0);
    SDL_memmove(&Routes_Table[group - 1], &Routes_Table[group],
                (size_t)(ROUTE_SLOTS - group) * sizeof(Route));
    Routes_Table[ROUTE_SLOTS - 1] = (Route){};
}

// --- Editing ----------------------------------------------------------------------

const char *Routes_StopRefusal(int16_t group, int16_t system, int16_t planet, int16_t moon)
{
    const Route *r = Routes_Of(group);
    RouteStop s = {(uint8_t)system, (uint8_t)planet, (uint8_t)moon};
    if (r->StopCount >= ROUTE_MAX_STOPS)
        return Lang_Text(" The route has all its 6 stops! ");
    if (same(s, HOME))
        return Lang_Text(" New Earth is where the route unloads! ");
    if (planet == 0 || !own(s))
        return Lang_Text(" Only your colonies can be stops! ");
    if (body(s)->SurfaceType == 2)
        return Lang_Text(" You cannot land on this type of planet, |          since it is gaseous!");
    for (int k = 0; k < r->StopCount; k++)
        if (same(r->Stops[k], s))
            return Lang_Text(" This stop is on the route already! ");
    return reach_refusal(group, system);
}

int Routes_AddStop(int16_t group, int after, int16_t system, int16_t planet, int16_t moon)
{
    Route *r = Routes_Of(group);
    if (r->StopCount >= ROUTE_MAX_STOPS)
        return 0;
    after = SDL_clamp(after, 0, (int)r->StopCount);
    SDL_memmove(&r->Stops[after + 1], &r->Stops[after], (size_t)(r->StopCount - after) * sizeof(RouteStop));
    r->Stops[after] = (RouteStop){(uint8_t)system, (uint8_t)planet, (uint8_t)moon};
    r->StopCount++;
    return after + 1;
}

void Routes_RemoveStop(int16_t group, int stop)
{
    Route *r = Routes_Of(group);
    if (stop < 1 || stop > r->StopCount)
        return;
    SDL_memmove(&r->Stops[stop - 1], &r->Stops[stop], (size_t)(r->StopCount - stop) * sizeof(RouteStop));
    r->StopCount--;
    r->Stops[r->StopCount] = (RouteStop){};
}

void Routes_MoveStop(int16_t group, int stop, int dir)
{
    Route *r = Routes_Of(group);
    int other = dir < 0 ? stop - 1 : stop + 1;
    if (stop < 1 || stop > r->StopCount || other < 1 || other > r->StopCount)
        return;
    RouteStop t = r->Stops[stop - 1];
    r->Stops[stop - 1] = r->Stops[other - 1];
    r->Stops[other - 1] = t;
}

const char *Routes_Start(int16_t group)
{
    Route *r = Routes_Of(group);
    for (int k = 0; k < r->StopCount; k++) {
        if (!own(r->Stops[k]))
            return Lang_Text(" Only your colonies can be stops! ");
        const char *refusal = reach_refusal(group, r->Stops[k].System);
        if (refusal)
            return refusal;
    }
    r->Next = 1;
    r->Why = ROUTE_END_NONE;
    r->WhyStop = 0;
    go_to(group);
    return nullptr;
}

void Routes_Stop(int16_t group)
{
    Route *r = Routes_Of(group);
    r->State = ROUTE_IDLE;
    r->Hours = 0;
    r->Why = ROUTE_END_NONE;
}

void Routes_StateText(int16_t group, char *out, size_t size)
{
    const Route *r = Routes_Of(group);
    char reason[64];
    switch (r->State) {
    case ROUTE_FLYING:
        if (r->Next == ROUTE_HOME)
            SDL_strlcpy(out, Lang_Text("Flying home to unload"), size);
        else
            SDL_snprintf(out, size, Lang_Text("Flying to stop %d"), r->Next);
        break;
    case ROUTE_DOCKED:
        if (r->Next == ROUTE_HOME)
            SDL_snprintf(out, size, Lang_Text("Unloading at home, %d h left"), r->Hours);
        else
            SDL_snprintf(out, size, Lang_Text("Loading at stop %d, %d h left"), r->Next, r->Hours);
        break;
    case ROUTE_ENDED:
        reason_text(r, reason, sizeof reason);
        SDL_snprintf(out, size, "%s%s", Lang_Text("Route ended: "), reason);
        break;
    default:
        SDL_strlcpy(out, Lang_Text("Route not running"), size);
        break;
    }
}

void Routes_PanelText(int16_t group, char *out, size_t size)
{
    out[0] = '\0';
    if (!Routes_Running(group))
        return;
    const Route *r = Routes_Of(group);
    if (r->Next == ROUTE_HOME)
        SDL_strlcpy(out, Lang_Text(r->State == ROUTE_DOCKED ? "Route: unloading" : "Route: homeward"),
                    size);
    else
        SDL_snprintf(out, size, Lang_Text(r->State == ROUTE_DOCKED ? "Route: at stop %d/%d" : "Route: to stop %d/%d"),
                     r->Next, r->StopCount);
}

// --- The editor (screen SCREEN_ROUTE) ------------------------------------------

// In the equipment screen's frame (Groups_DrawGroupPanel): the list of stops
// over its unit grid, rows ROW_PITCH apart with cells like the grid's, and
// the footer's two lines.
enum : int16_t {
    LIST_Y = 0x62,      // the first row's cells; the grid's start at 6Ch
    ROW_PITCH = 10,
    CELL_H = 9,
    NR_X = 0x13, NR_W = 0x14,
    STOP_X = 0x2C, STOP_W = 0xF4,  // to the grid's right edge, 11Fh
    FOOTER_X = 0xC, FOOTER_Y1 = 0xB0, FOOTER_Y2 = 0xBA, FOOTER_CHARS = 0x31,
    ARROW_X = 0x126, UP_Y = 0x57, DOWN_Y = 0x7C, ARROW_W = 0x11, ARROW_H = 0x1C,
    GROUP_ARROW_Y = 0x35, PREV_X = 9, NEXT_X = 0x83,
    TAB_X = 2, TAB_Y = 0xA3, TAB_W = 0x3D, TAB_H = 0xB,  // the "Stock:" label
};

// Colours of the frame (BEOSZTAS): a cell, its lit border and its shadow,
// the cave behind; the number of the stop the route heads for.
enum : uint8_t { CELL = 0x40, CELL_LIT = 0x5A, CELL_SHADOW = 0x49, CAVE = 0x59, NEXT_COLOUR = 0xF6 };

enum : int16_t {
    HIT_PREV = 0x15,
    HIT_NEXT,
    HIT_UP,
    HIT_DOWN,
    HIT_ROW1,  // .. HIT_ROW1 + 5
};

static uint8_t *arrows;  // grafika\benyil, as the equipment screen loads it
static int sel;          // the selected stop, 0 none
static int buttons = -1; // the toolbar shown, as a set of the buttons below

enum : int { B_ADD = 1, B_REMOVE = 2, B_START = 4, B_END = 8 };

static int16_t shown_group(void)
{
    return ds.Game_SelGroup;
}

static const Route *shown_route(void)
{
    return Routes_Of(shown_group());
}

static bool editable(void)
{
    return !active(shown_route());
}

static uint8_t *screen_at(int16_t x, int16_t y)
{
    return gfx_screen() + (uint16_t)(y * 320 + x);
}

// A cell like the grid's: lit above and on the left, a shadow line below.
static void cell(int16_t x, int16_t y, int16_t w)
{
    Blit_FillRect((int16_t)(x - 1), (int16_t)(y - 1), (int16_t)(w + 1), 1, CELL_LIT);
    Blit_FillRect((int16_t)(x - 1), y, 1, CELL_H, CELL_LIT);
    Blit_FillRect(x, y, w, CELL_H, CELL);
    Blit_FillRect(x, (int16_t)(y + CELL_H), w, 1, CELL_SHADOW);
}

// The frame's parts the equipment screen fills and the editor does not: the
// grid gives way to the list, the "Stock:" label to the rock beside it.
static void draw_frame(void)
{
    Mouse_HideCursor();
    Blit_FillRect(NR_X + NR_W, LIST_Y - 1, STOP_X - NR_X - NR_W, 6 * ROW_PITCH + 1, CAVE);
    for (int k = 0; k < ROUTE_MAX_STOPS; k++) {
        int16_t y = (int16_t)(LIST_Y + k * ROW_PITCH);
        cell(NR_X, y, NR_W);
        cell(STOP_X, y, STOP_W);
    }
    for (int16_t y = TAB_Y; y < TAB_Y + TAB_H; y++)
        SDL_memmove(screen_at(TAB_X, y), screen_at(TAB_X + TAB_W + 2, y), TAB_W);
    Blit_FillRect(8, 0xAF, 0x12F, 9, CELL);
    Blit_FillRect(8, 0xB9, 0x12F, 9, CELL);
    Mouse_ShowCursor();
}

// The previous (dir < 0) or next trade group of the fleet list, 0 if none.
static int16_t other_group(int dir)
{
    for (int16_t i = (int16_t)(shown_group() + dir); i >= 1 && i <= ds.Game_GroupCount; i += dir)
        if (Groups_At(0, i)->Kind == 2)
            return i;
    return 0;
}

static void draw_arrow(const uint8_t *icon, int16_t x, int16_t y, int16_t w, int16_t h, bool on)
{
    if (on)
        Blit_CopyRect(icon, screen_at(x, y), w, h);
    else
        Blit_FillRect(x, y, w, h, CELL);
}

static void draw_arrows(void)
{
    bool up = editable() && sel > 1;
    bool down = editable() && sel >= 1 && sel < shown_route()->StopCount;
    Mouse_HideCursor();
    draw_arrow(arrows + 0x24, PREV_X, GROUP_ARROW_Y, 10, 12, other_group(-1) != 0);
    draw_arrow(arrows + 0x2F, NEXT_X, GROUP_ARROW_Y, 10, 12, other_group(1) != 0);
    draw_arrow(arrows, ARROW_X, UP_Y, ARROW_W, ARROW_H, up);
    draw_arrow(arrows + 0x12, ARROW_X, DOWN_Y, ARROW_W, ARROW_H, down);
    Mouse_ShowCursor();
}

static void draw_list(void)
{
    const Route *r = shown_route();
    bool running = active(r);
    for (int k = 1; k <= ROUTE_MAX_STOPS; k++) {
        int16_t y = (int16_t)(LIST_Y + (k - 1) * ROW_PITCH + 1);
        char nr[8] = "", name[256] = "";
        if (k <= r->StopCount) {
            const RouteStop *s = &r->Stops[k - 1];
            SDL_snprintf(nr, sizeof nr, "%3d", k);
            PlanetInfo_GetBodyLocationName(name, sizeof name, s->System, s->Planet, s->Moon);
        }
        if (running && k == r->Next)
            Gfx_DrawText6x8(nr, NR_X + 1, y, 3, NEXT_COLOUR);
        else
            Gfx_DrawTextF9(nr, NR_X + 1, y, 3);
        if (k == sel)
            Gfx_DrawTextFC(name, STOP_X + 2, y, 40);
        else
            Gfx_DrawTextF9(name, STOP_X + 2, y, 40);
    }
}

static void draw_footer(void)
{
    char t[256], home[256];
    PlanetInfo_GetBodyLocationName(home, sizeof home, HOME.System, HOME.Planet, HOME.Moon);
    SDL_snprintf(t, sizeof t, "%s%s", Lang_Text("Unload at "), home);
    Gfx_DrawTextF9(t, FOOTER_X, FOOTER_Y1, FOOTER_CHARS);
    Routes_StateText(shown_group(), t, sizeof t);
    Gfx_DrawTextF9(t, FOOTER_X, FOOTER_Y2, FOOTER_CHARS);
}

static void setup_hotspots(void)
{
    const Route *r = shown_route();
    ds.Mouse_HotspotCount = 10;
    Mouse_ClearHotspots(1, 10);
    if (other_group(-1))
        Mouse_SetHotspot(1, 7, GROUP_ARROW_Y, 0xE, 0xC, Lang_Text("Previous group"), 1, 0);
    if (other_group(1))
        Mouse_SetHotspot(2, 0x81, GROUP_ARROW_Y, 0xD, 0xC, Lang_Text("Next group"), 1, 0);
    if (editable() && sel > 1)
        Mouse_SetHotspot(3, 0x123, 0x54, 0x1C, 0x23, Lang_Text("Move stop up"), 1, 0);
    if (editable() && sel >= 1 && sel < r->StopCount)
        Mouse_SetHotspot(4, 0x123, 0x79, 0x1C, 0x23, Lang_Text("Move stop down"), 1, 0);
    for (int k = 1; k <= r->StopCount; k++)
        Mouse_SetHotspot((int16_t)(4 + k), NR_X, (uint8_t)(LIST_Y + (k - 1) * ROW_PITCH),
                         STOP_X + STOP_W - NR_X, CELL_H, Lang_Text("Select stop"), 1, 0);
}

// The toolbar: what can be done to the route as it is; a button that can't
// act isn't shown.
static void setup_toolbar(bool force)
{
    const Route *r = shown_route();
    int b = editable() ? B_ADD | (r->StopCount > 0 ? B_REMOVE | B_START : 0) : B_END;
    if (b == buttons && !force)
        return;
    buttons = b;
    int16_t hit = ds.Game_HotspotHit;
    GfxPath_SetToolbar(SCREEN_ROUTE);
    if (b & B_ADD)
        GfxPath_AddToolbarIcon(CMD_ADD_STOP);
    if (b & B_REMOVE)
        GfxPath_AddToolbarIcon(CMD_REMOVE_STOP);
    if (b & B_START)
        GfxPath_AddToolbarIcon(CMD_START_ROUTE);
    if (b & B_END)
        GfxPath_AddToolbarIcon(CMD_END_ROUTE);
    GfxPath_ShowToolbar();
    ds.Game_HotspotHit = hit;
}

static void redraw(void)
{
    Groups_DrawGroupHeader();
    draw_list();
    draw_footer();
    draw_arrows();
    setup_hotspots();
    setup_toolbar(false);
}

// The shown group is gone (disbanded meanwhile: SelGroup is 0) or not a
// trade group: back to the groups list.
static bool group_gone(void)
{
    if (valid(shown_group()) && ds.Game_GroupList == 0 && Groups_At(0, shown_group())->Kind == 2)
        return false;
    ds.Game_ScreenSwitch = 1;
    ds.Game_CurrentScreen = 0x10;
    return true;
}

static void screen_hour(void)
{
    if (group_gone())
        return;
    if (sel > shown_route()->StopCount)
        sel = shown_route()->StopCount;
    redraw();
}

// A body picked on the destination screen for ADD STOP: added after the
// selected stop, or refused (the box's text).
static const char *add_picked(void)
{
    int16_t sys = ds.Game_CurSystem;
    int16_t planet = ds.Game_CurPlanet[sys - 1], moon = ds.Game_CurMoon[sys - 1];
    const char *refusal = Routes_StopRefusal(shown_group(), sys, planet, moon);
    if (!refusal)
        sel = Routes_AddStop(shown_group(), sel, sys, planet, moon);
    return refusal;
}

void Routes_RouteScreen(void)
{
    if (group_gone())
        return;
    int16_t group = shown_group();
    if (ds.Game_ScreenEntered) {
        const char *refusal = nullptr;
        if (ds.Game_PrevScreen != 0x12)
            sel = shown_route()->StopCount > 0 ? 1 : 0;
        else if (ds.Launch_DestinationActive == 0)
            refusal = add_picked();
        // The group's own location, as its other screens select it.
        const TGroup *g = Groups_At(0, group);
        if (g->Status == 1 || g->Status == 2) {
            ds.Game_CurSystem = g->System;
            ds.Game_CurPlanet[ds.Game_CurSystem - 1] = g->Planet;
            ds.Game_CurMoon[ds.Game_CurSystem - 1] = g->Moon;
            PlanetInfo_SelectCurBody();
        }
        GfxPath_ShowScreenBackdrop();
        arrows = game_alloc(0x2300);
        Gfx_LoadPic("grafika\\benyil", arrows, 1, 0x1C, 0, 0, 0x140, false, Gfx_PalettePtr(), 0, 0xB0,
                    0x40, 0, 0xBE);
        draw_frame();
        setup_toolbar(true);
        redraw();
        Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
        if (refusal)
            Message_ShowMessageNoLog(refusal);
    }

    const Route *r = shown_route();
    if (ds.Game_MenuCommand == CMD_ADD_STOP && editable()) {
        if (r->StopCount >= ROUTE_MAX_STOPS) {
            Message_ShowMessageNoLog(Lang_Text(" The route has all its 6 stops! "));
        } else {
            // The destination screen, at the selected stop's system or the
            // group's.
            int16_t sys = sel > 0 ? r->Stops[sel - 1].System : Groups_At(0, group)->System;
            ds.Game_CurSystem = sys;
            ds.Game_CurPlanet[sys - 1] = 0;
            ds.Game_CurMoon[sys - 1] = 0;
            ds.Game_ScreenSwitch = 1;
            ds.Game_CurrentScreen = 0x12;
        }
    }
    if (ds.Game_MenuCommand == CMD_REMOVE_STOP && editable() && sel > 0) {
        Routes_RemoveStop(group, sel);
        if (sel > shown_route()->StopCount)
            sel = shown_route()->StopCount;
        redraw();
    }
    if (ds.Game_MenuCommand == CMD_START_ROUTE && editable() && r->StopCount > 0) {
        const char *refusal = Routes_Start(group);
        if (refusal)
            Message_ShowMessageNoLog(refusal);
        redraw();
    }
    if (ds.Game_MenuCommand == CMD_END_ROUTE && !editable()) {
        Routes_Stop(group);
        redraw();
    }

    int16_t hit = ds.Game_HotspotHit;
    if (hit == HIT_PREV || hit == HIT_NEXT) {
        int16_t other = other_group(hit == HIT_PREV ? -1 : 1);
        Sound_PlayButtonSfx("x");
        if (other != 0) {
            ds.Game_SelGroup = other;
            ds.Game_SelGroup0 = other;
            sel = shown_route()->StopCount > 0 ? 1 : 0;
            setup_toolbar(true);
            redraw();
        }
    }
    if ((hit == HIT_UP || hit == HIT_DOWN) && editable()) {
        int dir = hit == HIT_UP ? -1 : 1;
        int to = sel + dir;
        Sound_PlayButtonSfx("x");
        if (sel >= 1 && to >= 1 && to <= r->StopCount) {
            Routes_MoveStop(group, sel, dir);
            sel = to;
            redraw();
        }
    }
    if (hit >= HIT_ROW1 && hit < HIT_ROW1 + ROUTE_MAX_STOPS) {
        int k = hit - HIT_ROW1 + 1;
        Sound_PlayButtonSfx("x");
        if (k <= r->StopCount && k != sel) {
            sel = k;
            redraw();
        }
    }
}

void Routes_LeaveScreen(void)
{
    if (arrows)
        game_free(arrows);
    arrows = nullptr;
    buttons = -1;
}
