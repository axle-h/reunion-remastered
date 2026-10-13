#include "play/ui_planet.h"

#include <SDL3/SDL.h>

#include "game/dseg.h"
#include "game/groups.h"
#include "game/mouse.h"
#include "play/play.h"
#include "play/ui.h"
#include "play/ui_message.h"

enum : int {
    SETTLE = 30,                     // retraces for fades after a screen is set up
    TIMEOUT = 3000,
    ZOOM_HOURS = 48,                 // tries of a zoom on a planet hidden behind the sun
    MAP_X = 0x5D, MAP_Y = 0x35,      // the surface map window: 14 x 9 cells of 16 pixels
    MAP_COLS = 14, MAP_ROWS = 9,
    OUTLINE_FREE = 0x0A,             // Surface_DrawMapView: cursor footprint on free ground
    MODE_PLACING = 2,                // Surface_Mode while Build blinks
    MODE_DESTROYING = 6,             // Surface_Mode while Destroy blinks
};

// --- Reading the screen ---------------------------------------------------------

// Reads `n` cells at (x, y) into out (n + 1 bytes) and returns it.
static char *read_text(int x, int y, int n, char *out)
{
    ui_read_text(x, y, n, out, (size_t)n + 1);
    return out;
}

// Trims the blanks at both ends of s in place.
static char *trim(char *s)
{
    char *p = s;
    while (*p == ' ')
        p++;
    size_t n = SDL_strlen(p);
    while (n > 0 && p[n - 1] == ' ')
        n--;
    SDL_memmove(s, p, n);
    s[n] = 0;
    return s;
}

// --- Navigation -----------------------------------------------------------------

void ui_planet_main(void)
{
    ui_open_screen("PLANET MAIN", 20);
}

void ui_planet_info(void)
{
    ui_open_screen("PLANET INFO", 8);
}

void ui_command_centre(void)
{
    ui_open_screen("BACK TO M.SCREEN", 1);
}

// The galactic map: bodies are hotspots captioned with their names.
// A right click opens planet info, a left click zooms into a planet's moons
// (system view) or opens the surface (moon view) (Launch_MapScreen).
// False if a story scene took the map away meanwhile (the caller tries
// again from the command centre).
static bool click_body(const char *name, bool right)
{
    if (ui_hotspot_as(name, 1, right ? UI_CLICK_RIGHT : UI_CLICK))
        return true;
    if (ui_screen() != 7)
        return false;
    play_fail("no %s on the galactic map", name);
}

static bool not_yet(void *unused)
{
    (void)unused;
    return false;
}

static bool zoomed_on(void *planet)
{
    // The zoomed view: the planet is hotspot 13h, its moons follow.
    return ui_hotspot_captioned(0x13, planet);
}

void ui_open_body(const char *planet, const char *moon, int screen)
{
    char want[40];
    SDL_snprintf(want, sizeof want, "%s%s%s", planet, moon ? " " : "", moon ? moon : "");
    if (ui_screen() != 7)
        ui_open_screen("GALACTIC MAP", 7);
    // The bodies move on to their next orbit position at each game hour: a
    // click aimed just before may miss, or open another body. The info's
    // NAME line names the body; the surface doesn't, but a clock that shows
    // the same hour before and after the click means the hotspots didn't
    // move under it. A miss is tried again; an open that can't be told is
    // not counted as one.
    for (int tries = 1, unsure = 0, hidden = 0;;) {
        // A story scene that came between returns to the command centre.
        if (ui_screen() != 7)
            ui_open_screen("GALACTIC MAP", 7);
        // Back to the system view if another planet is zoomed in (or any,
        // for a planet's info).
        bool keep = (moon || screen != 8) && zoomed_on((void *)planet);
        if (!keep && ui_toolbar("ZOOM OUT"))
            play_wait(SETTLE);
        bool clicked, stable = true;
        if (!moon && screen == 8) {
            clicked = click_body(planet, true);
        } else {
            // Zoom in; there the planet itself and its moons are hotspots.
            if (!zoomed_on((void *)planet)) {
                clicked = click_body(planet, false);
                if (!clicked || !ui_wait_until(zoomed_on, (void *)planet, 200)) {
                    if (tries++ < 3)
                        continue;
                    // A planet behind the sun isn't drawn, and its hotspot
                    // stays where it was last drawn: the click does nothing
                    // until it comes out, hours later.
                    if (++hidden == ZOOM_HOURS)
                        play_fail("the galactic map did not zoom in on %s", planet);
                    play_log("%s: no zoom, again in an hour", planet);
                    ui_wait_hours(not_yet, nullptr, 1);
                    continue;
                }
            }
            if (moon && !ui_wait_hotspot(moon, 500) && ui_screen() == 7) {
                play_dump("no-moon");
                ui_log_screen();
                play_fail("no moon %s around %s", moon, planet);
            }
            play_wait(SETTLE);
            int64_t aimed = ui_clock_hours();
            clicked = click_body(moon ? moon : planet, screen == 8);
            stable = ui_clock_hours() == aimed;
        }
        bool opened = clicked && ui_wait_screen(screen, 300);
        if (opened && screen == 8 && SDL_strcmp(ui_body_name(), want) != 0) {
            play_log("%s: opened '%s' instead", want, ui_body_name());
            opened = false;
        } else if (opened && screen == 20 && !stable) {
            if (++unsure == 10)
                play_fail("%s: the hour kept turning during the click", want);
            play_log("%s: the hour turned during the click, again", want);
            continue;
        }
        if (opened)
            break;
        if (tries++ == 3)
            play_fail("%s did not open screen %d", want, screen);
        play_log("%s: the click missed, again", want);
    }
    play_log("opened %s on screen %d", want, screen);
}

static char body_name[32];

const char *ui_body_name(void)
{
    char line[40];
    body_name[0] = 0;
    if (ui_screen() != 8)
        return body_name;
    read_text(0x5E, 0x35, 0x23, line);
    const char *p = SDL_strstr(line, "NAME:");
    if (p) {
        SDL_strlcpy(body_name, p + 5, sizeof body_name);
        trim(body_name);
    }
    return body_name;
}

// --- Planet info (screen 8) -----------------------------------------------------

static const char *const tax_names[8] = {"None", "Very low", "Low", "Normal",
                                         "High", "Difficult", "Very difficult", "Oppressing"};
static const char *const moods[7] = {"revolting men", "people hate you", "people don't like you",
                                     "neutral men", "people find you sympathetic",
                                     "people like you", "loyal people"};

int ui_tax_level(void)
{
    char line[40];
    read_text(0x61, 0x55, 0x23, line);  // PlanetInfo_DrawPlanetInfoText: the TAX line
    trim(line);
    for (int i = 0; i < 8; i++)
        if (SDL_strcasecmp(line, tax_names[i]) == 0)
            return i;
    return -1;
}

static bool tax_is(void *arg)
{
    return ui_tax_level() == *(int *)arg;
}

void ui_set_tax(int level)
{
    if (ui_screen() != 8)
        play_fail("ui_set_tax: not on planet info");
    for (int tries = 0; tries < 10; tries++) {
        ui_settle();
        int now = ui_tax_level();
        if (now < 0) {
            // The mouse cursor may cover the line (the last click left it
            // there): move it off.
            int x, y;
            ui_inert_point(0, &x, &y);
            play_mouse(x, y, 0);
            play_wait(3);
            now = ui_tax_level();
        }
        if (now < 0)
            play_fail("ui_set_tax: can't read the TAX line");
        if (now == level) {
            play_log("tax %s on %s", tax_names[level], ui_body_name());
            return;
        }
        int want = now + (level > now ? 1 : -1);
        if (!ui_toolbar(level > now ? "INCREASE TAX" : "DECREASE TAX"))
            play_fail("ui_set_tax: no tax buttons (not a colony?)");
        if (!ui_wait_until(tax_is, &want, 200))
            play_fail("ui_set_tax: the TAX line did not change from %s", tax_names[now]);
    }
    play_fail("ui_set_tax: tax %d not reached", level);
}

static char pop_line[48];

static const char *read_pop(void)
{
    read_text(0x61, 0x45, 0x24, pop_line);  // the POP line
    return trim(pop_line);
}

int32_t ui_population(void)
{
    return (int32_t)SDL_strtol(read_pop(), nullptr, 10);
}

int ui_mood(void)
{
    const char *p = read_pop();
    for (int i = 6; i >= 0; i--)
        if (SDL_strstr(p, moods[i]))
            return i;
    return -1;
}

bool ui_body_lists(const char *line)
{
    char text[24];
    for (int k = 0; k < 8; k++) {
        read_text(0xC2, 0x95 + k * 8, 0x14, text);  // the objects list
        if (SDL_strncmp(trim(text), line, SDL_strlen(line)) == 0)
            return true;
    }
    return false;
}

bool ui_body_type_says(const char *s)
{
    char line[40];
    read_text(0x61, 0x75, 0x24, line);  // the TYPE line
    return SDL_strstr(line, s) != nullptr;
}

// --- Planet surface (screen 20) -------------------------------------------------

// The building list in the order Invention up / down walks it.
static const char *const kind_names[25] = {
    "Command centre", "Nuclear Gen.", "Windtrap", "Mine", "Derrick", "Observatory", "Building",
    "Leisure Centre", "Stadium", "Church", "Storage bay", "Space Port", "University", "Park",
    "Hospital", "Farm", "Radar", "Ioncannon", "Anti rad. sh.", "Energy shield", "Food Resources",
    "Builder Plant", "Vehicle Plant", "Medicine Plant", "Miner station",
};

static int kind_index(const char *name)
{
    for (int k = 0; k < 25; k++)
        if (SDL_strcasecmp(kind_names[k], name) == 0)
            return k + 1;
    return 0;
}

static char kind_line[20];

const char *ui_surface_kind(void)
{
    read_text(3, 0x82, 14, kind_line);  // Surface_DrawBuildingPreview
    return trim(kind_line);
}

static bool kind_changed(void *arg)
{
    return SDL_strcmp(ui_surface_kind(), (const char *)arg) != 0;
}

// The surface is still up: the game frees its layers on leaving it, so a
// screen that came up in between (an alien talk while a driver re-read the
// screen) is left as an interrupt before they are read.
static void surface_still_up(const char *what)
{
    if (ui_screen() != 20 || ds.Game_ScreenSwitch)
        ui_screen_lost(what);
}

bool ui_select_kind(const char *kind)
{
    int want = kind_index(kind);
    if (want == 0)
        play_fail("ui_select_kind: unknown kind '%s'", kind);
    for (int tries = 0; tries < 30; tries++) {
        ui_settle();
        char now[20];
        SDL_strlcpy(now, ui_surface_kind(), sizeof now);
        int at = kind_index(now);
        // A cell may read '?' while something is drawn over it (the mouse
        // cursor moving in): read again a few times.
        for (int i = 0; at == 0 && i < 20; i++) {
            if (i == 0) {
                int x, y;
                ui_inert_point(0, &x, &y);
                play_mouse(x, y, 0);
            }
            play_wait(2);
            SDL_strlcpy(now, ui_surface_kind(), sizeof now);
            at = kind_index(now);
        }
        // The re-reads above gave the game retraces: what they read may be
        // the last of a surface that has just been left.
        surface_still_up("ui_select_kind");
        if (at == want)
            return true;
        if (at == 0)
            play_fail("ui_select_kind: can't read the kind ('%s')", now);
        if (!ui_hotspot(want < at ? "Invention up" : "Invention down"))
            play_fail("ui_select_kind: no Invention up/down");
        if (!ui_wait_until(kind_changed, now, 30))
            return false;  // the end of the list: not offered
        int next = kind_index(ui_surface_kind());
        if ((want < at && next < want) || (want > at && next > want))
            return false;  // skipped over it
    }
    return false;
}

// The current body's buildings and map layers (Surface_* state): the map
// window draws them (building tiles, construction markers, terrain).
static const TBuilding *building(int i)
{
    return ui_building_record(i);
}

static int16_t cur_planet(void)
{
    return ds.Game_CurPlanet[ds.Game_CurSystem - 1];
}

static int16_t cur_moon(void)
{
    return ds.Game_CurMoon[ds.Game_CurSystem - 1];
}

static bool on_cur_body(const TBuilding *b)
{
    return b->System == ds.Game_CurSystem && b->Planet == cur_planet() && b->Moon == cur_moon();
}

UiBuildings ui_buildings(const char *kind)
{
    UiBuildings n = {};
    int k = kind_index(kind);
    for (int i = 1; i <= ds.Game_BuildingCount; i++) {
        const TBuilding *b = building(i);
        if (!on_cur_body(b) || b->Kind != k)
            continue;
        n.total++;
        if (b->Health != 0) {
            n.building++;
        } else {
            n.finished++;
            n.active += b->Flag != 0;
        }
    }
    return n;
}

typedef struct {
    int kind, n;
} KindCount;

static bool kind_active(void *arg)
{
    const KindCount *kc = arg;
    UiBuildings n = ui_buildings(kind_names[kc->kind - 1]);
    return n.active >= kc->n;
}

bool ui_wait_buildings_active(const char *kind, int n, int hours)
{
    KindCount kc = {kind_index(kind), n};
    return ui_fast_forward(kind_active, &kc, hours) >= 0;
}

// What the map window shows at window cell (x, y), 1-based: the occupy layer
// tells free ground (0) from blocked terrain (FFh) and buildings, as the
// tiles drawn there do.
static uint8_t occupy(int x, int y)
{
    int mx = x + ds.Surface_ScrollX, my = y + ds.Surface_ScrollY;
    const uint8_t *layer = ds.Surface_OccupyLayer;
    return layer[(uint16_t)((mx - 1) * 60 + (my - 1))];
}

static int building_under(int x, int y)
{
    int mx = x + ds.Surface_ScrollX, my = y + ds.Surface_ScrollY;
    const uint16_t *layer = (const uint16_t *)ds.Surface_BuildingLayer;
    return layer[(uint16_t)((mx - 1) * 60 + (my - 1))];
}

static const TBuildingType *btype(int kind)
{
    return &ds.Surface_BuildingTypes[kind - 1];
}

// The footprint of `kind` (as its preview shows) with its top-left at window
// cell (x, y) lies inside the window, on free ground.
static bool fits(int kind, int x, int y)
{
    const TBuildingType *bt = btype(kind);
    if (x + bt->Width - 1 > MAP_COLS || y + bt->Height - 1 > MAP_ROWS)
        return false;
    for (int i = 0; i < bt->Width; i++)
        for (int j = 0; j < bt->Height; j++)
            if (bt->Tiles[j][i] != 0xFF && occupy(x + i, y + j) != 0)
                return false;
    return true;
}

// Middle of window cell (x, y) on the screen.
static int cell_px(int x)
{
    return MAP_X + (x - 1) * 16 + 8;
}

static int cell_py(int y)
{
    return MAP_Y + (y - 1) * 16 + 8;
}

// Every footprint cell of `kind` at window cell (x, y) is drawn with the
// green outline of the build cursor (Surface_DrawMapView).
static bool footprint_green(int kind, int x, int y)
{
    const TBuildingType *bt = btype(kind);
    for (int i = 0; i < bt->Width; i++) {
        for (int j = 0; j < bt->Height; j++) {
            if (bt->Tiles[j][i] == 0xFF)
                continue;
            int l = MAP_X + (x + i - 1) * 16, t = MAP_Y + (y + j - 1) * 16;
            for (int k = 0; k < 16; k++)
                if (!ui_pixel_is(l + k, t, OUTLINE_FREE) || !ui_pixel_is(l + k, t + 15, OUTLINE_FREE)
                    || !ui_pixel_is(l, t + k, OUTLINE_FREE) || !ui_pixel_is(l + 15, t + k, OUTLINE_FREE))
                    return false;
        }
    }
    return true;
}

typedef struct {
    int kind, x, y;
} Spot;

static bool green(void *arg)
{
    const Spot *s = arg;
    return footprint_green(s->kind, s->x, s->y);
}

// Free spots in the window, scanned from the middle outwards so buildings
// stay near the colony.
static bool find_spot(int kind, int *x, int *y)
{
    surface_still_up("find_spot");
    int best = 1 << 30;
    for (int j = 1; j <= MAP_ROWS; j++) {
        for (int i = 1; i <= MAP_COLS; i++) {
            if (!fits(kind, i, j))
                continue;
            int d = SDL_abs(2 * i + btype(kind)->Width - 1 - MAP_COLS - 1)
                  + SDL_abs(2 * j + btype(kind)->Height - 1 - MAP_ROWS - 1);
            if (d < best) {
                best = d;
                *x = i;
                *y = j;
            }
        }
    }
    return best < (1 << 30);
}

// Radar clicks that move the window around the map, nearest first
// (offsets from the radar's middle, in radar pixels = map cells).
static const int8_t radar_moves[][2] = {
    {0, 0}, {-12, 0}, {12, 0}, {0, -8}, {0, 8}, {-12, -8}, {12, -8}, {-12, 8}, {12, 8},
    {-24, 0}, {24, 0}, {0, -16}, {0, 16}, {-24, -16}, {24, -16}, {-24, 16}, {24, 16},
};

static bool scrolled(void *arg)
{
    const int *was = arg;
    return ds.Surface_ScrollX != was[0] || ds.Surface_ScrollY != was[1];
}

// Clicks the radar so that the window shows the part of the map `step`
// cells from its middle (the radar draws the whole map in its box).
static void radar_move(int step)
{
    ui_settle();
    if (ui_screen() != 20)
        ui_screen_lost("radar");
    int j = ui_find_hotspot("Radar", 1);
    if (j == 0)
        play_fail("no radar on the surface");
    const THotspot *h = Mouse_Hotspot((int16_t)j);
    int was[2] = {ds.Surface_ScrollX, ds.Surface_ScrollY};
    ui_click(h->X + h->W / 2 + radar_moves[step][0], h->Y + h->H / 2 + radar_moves[step][1]);
    ui_wait_until(scrolled, was, 20);
    play_wait(12);
}

// The map view offers the Terrain hotspot; the building info view that a
// click on a building or a new site opens offers "See surface" instead.
static bool on_map_view(void *unused)
{
    (void)unused;
    return ui_screen() == 20 && ui_find_hotspot("Terrain", 1) != 0;
}

static void see_surface(void)
{
    ui_settle();
    if (ui_find_hotspot("See surface", 1)) {
        ui_hotspot("See surface");
        if (!ui_wait_until(on_map_view, nullptr, 200))
            play_fail("no way back to the map view");
        ui_settle();
    }
}

// Build blinks (Surface_BlinkModeButton) while the game waits for a cell.
static bool build_blinks(void *unused)
{
    (void)unused;
    return ds.Surface_Mode == MODE_PLACING;
}

// Build blinks, or a box closed since *arg said why not.
static bool build_answered(void *arg)
{
    return build_blinks(nullptr) || ui_box_seen("", *(const uint64_t *)arg);
}

UiBuildResult ui_build(const char *kind)
{
    int k = kind_index(kind);
    if (ui_screen() != 20)
        play_fail("ui_build: not on a planet surface");
    see_surface();
    if (!ui_select_kind(kind)) {
        play_log("build %s: not in the list", kind);
        return UI_BUILD_UNAVAILABLE;
    }
    for (int step = 0; step < (int)SDL_arraysize(radar_moves); step++) {
        if (step > 0)
            radar_move(step);
        int x, y;
        if (!find_spot(k, &x, &y))
            continue;
        // Build: the button blinks, or a box says why not.
        ui_settle();
        int32_t credits = ds.Game_Credits;
        if (!on_map_view(nullptr))
            play_fail("ui_build: not on the map view");
        uint64_t since = play_frame();
        ui_hotspot("Build");
        if (!ui_wait_until(build_answered, &since, 50) || !build_blinks(nullptr)) {
            const char *box = ui_box_seen("", since);
            if (box) {
                play_log("build %s refused: '%s'", kind, box);
                return UI_BUILD_REFUSED;
            }
            play_log("build %s: no money (%d credits)", kind, credits);
            return UI_BUILD_NO_MONEY;
        }
        // Hover the spot until the cursor footprint shows green, then click.
        Spot s = {k, x, y};
        play_mouse(cell_px(x), cell_py(y), 0);
        if (!play_wait_until(green, &s, 60)) {
            play_log("build %s: (%d,%d) not green", kind, x, y);
            ui_hotspot("Build");  // cancel
            continue;
        }
        // Placed if the map shows one more of the kind, or paid for: the
        // credits can rise meanwhile (midnight's taxes), and at 1,000
        // buildings the original takes the money and builds nothing
        // (fixes.building_limit off).
        int before = ui_buildings(kind).total;
        ui_click(cell_px(x), cell_py(y));
        play_wait(10);
        ui_settle();
        if (ui_buildings(kind).total == before && ds.Game_Credits >= credits) {
            play_log("build %s: the click at (%d,%d) didn't place it", kind, x, y);
            if (build_blinks(nullptr))
                ui_hotspot("Build");  // cancel
            see_surface();
            continue;
        }
        play_log("building %s at map (%d,%d), %d credits left", kind, x + ds.Surface_ScrollX,
                 y + ds.Surface_ScrollY, ds.Game_Credits);
        play_dump("built-%s", kind);
        see_surface();
        return UI_BUILD_OK;
    }
    play_log("build %s: no room", kind);
    return UI_BUILD_NO_ROOM;
}

static bool destroy_blinks(void *unused)
{
    (void)unused;
    return ds.Surface_Mode == MODE_DESTROYING;
}

// A finished building of kind `k` in the window.
static bool find_kind(int k, int *x, int *y);

bool ui_demolish(const char *kind)
{
    int k = kind_index(kind);
    if (ui_screen() != 20 || k == 0)
        play_fail("ui_demolish: not on a surface, or no kind '%s'", kind);
    see_surface();
    int x, y;
    bool found = false;
    for (int step = 0; step < (int)SDL_arraysize(radar_moves) && !found; step++) {
        if (step > 0)
            radar_move(step);
        found = find_kind(k, &x, &y);
    }
    if (!found)
        return false;
    ui_settle();
    int32_t credits = ds.Game_Credits;
    ui_hotspot("Destroy");
    if (!ui_wait_until(destroy_blinks, nullptr, 50))
        play_fail("ui_demolish: Destroy did not start blinking");
    ui_click(cell_px(x), cell_py(y));
    play_wait(10);
    ui_settle();
    play_log("demolish %s at (%d,%d): %d -> %d credits", kind, x, y, credits, ds.Game_Credits);
    return ds.Game_Credits < credits;
}

// --- Droids (screen 4) ------------------------------------------------------------

static TBody *cur_body(void)
{
    return ds.Game_CurBody;
}

int ui_droids(void)
{
    return cur_body()->_0A;
}

int ui_droid_stock(void)
{
    if (ds.Game_CurSystem == 1 && ds.Game_CurPlanet[0] == 5 && ds.Game_CurMoon[0] == 0)
        return ds.Research_Products[1].Stock;
    // The body's planet force (Groups_FindPlanetForce: the last match)
    // holds its droids.
    int stock = 0;
    for (int k = 1; k <= ds.Game_PlanetForceCount; k++) {
        const TGroup *f = ui_group_record(1, k);
        if (f->System == ds.Game_CurSystem && f->Planet == cur_planet() && f->Moon == cur_moon())
            stock = f->Items[12];
    }
    return stock;
}

int ui_droid_slots(void)
{
    return ui_buildings("Mine").finished + ui_buildings("Miner station").finished;
}

// A finished mine in the window: its cells draw the mine, not a site.
static bool find_mine(int *x, int *y)
{
    surface_still_up("find_mine");
    for (int j = 1; j <= MAP_ROWS; j++) {
        for (int i = 1; i <= MAP_COLS; i++) {
            int b = building_under(i, j);
            if (b == 0)
                continue;
            const TBuilding *bl = building(b);
            if ((bl->Kind == 4 || bl->Kind == 0x19) && bl->Health == 0) {
                *x = i;
                *y = j;
                return true;
            }
        }
    }
    return false;
}

// A finished building of kind `k` in the window.
static bool find_kind(int k, int *x, int *y)
{
    surface_still_up("find_kind");
    for (int j = 1; j <= MAP_ROWS; j++) {
        for (int i = 1; i <= MAP_COLS; i++) {
            int b = building_under(i, j);
            if (b != 0 && building(b)->Kind == k && building(b)->Health == 0) {
                *x = i;
                *y = j;
                return true;
            }
        }
    }
    return false;
}

static bool info_shown(void *unused)
{
    (void)unused;
    return ui_find_hotspot("See surface", 1) != 0;
}

int32_t ui_building_info(const char *kind, const char *label)
{
    int k = kind_index(kind);
    if (ui_screen() != 20 || k == 0)
        play_fail("ui_building_info: not on a surface, or no kind '%s'", kind);
    see_surface();
    int x, y;
    bool found = false;
    for (int step = 0; step < (int)SDL_arraysize(radar_moves) && !found; step++) {
        if (step > 0)
            radar_move(step);
        found = find_kind(k, &x, &y);
    }
    if (!found)
        return -1;
    ui_settle();
    ui_click(cell_px(x), cell_py(y));
    if (!ui_wait_until(info_shown, nullptr, 200))
        play_fail("ui_building_info: the %s did not show its info", kind);
    ui_settle();
    // Surface_DrawBuildingInfo: "<label> : <value>..." lines from (62h, 46h),
    // 9 pixels apart.
    int32_t value = -1;
    for (int row = 0; row < 6; row++) {
        char line[24];
        read_text(0x62, 0x46 + row * 9, 20, line);
        size_t n = SDL_strlen(label);
        if (SDL_strncmp(line, label, n) == 0) {
            const char *p = SDL_strchr(line + n, ':');
            int32_t v;
            if (p && SDL_sscanf(p + 1, "%d", &v) == 1)
                value = v;
        }
    }
    see_surface();
    return value;
}

static bool droids_changed(void *arg)
{
    return ui_droids() != *(int *)arg;
}

bool ui_add_droid(void)
{
    if (ui_screen() != 20)
        play_fail("ui_add_droid: not on a planet surface");
    see_surface();
    int x, y;
    bool found = false;
    for (int step = 0; step < (int)SDL_arraysize(radar_moves) && !found; step++) {
        if (step > 0)
            radar_move(step);
        found = find_mine(&x, &y);
    }
    if (!found)
        play_fail("ui_add_droid: no finished mine on the map");
    ui_settle();
    ui_click(cell_px(x), cell_py(y));
    if (!ui_wait_screen(4, 2000))
        play_fail("ui_add_droid: the mine did not open the droid screen");
    int before = ui_droids();
    play_dump("droids");
    if (!ui_toolbar("ADD DROIDS"))
        play_fail("ui_add_droid: no ADD DROIDS button");
    bool added = ui_wait_until(droids_changed, &before, 100);
    play_log("droids on %d:%d:%d: %d -> %d (stock %d)", ds.Game_CurSystem, cur_planet(), cur_moon(),
             before, ui_droids(), ui_droid_stock());
    ui_settle();
    if (!ui_hotspot("Back to surface") || !ui_wait_screen(20, 2000))
        play_fail("ui_add_droid: no way back to the surface");
    return added;
}

// --- Body actions -----------------------------------------------------------------

bool ui_body_action(const char *label)
{
    if (!ui_toolbar(label))
        return false;
    play_wait(10);
    ui_settle();
    return true;
}

bool ui_deploy_stock_satellite(void)
{
    if (!ui_body_action("ADD SATELLITE"))  // stock satellite: command 0x46 on screens 8 and 20
        return false;
    play_dump("satellite");
    // satellite.c Satellite_DeployStockSatellite: Satellite 1 around New
    // Earth, else E2h (lost after 5-14 hours, listed as nothing); either
    // way the body action is gone.
    return !ui_toolbar_has("ADD SATELLITE");
}

bool ui_found_colony(const char *const *starters, int n)
{
    if (!ui_body_action("COLONIZATION"))
        return false;
    if (ui_screen() != 28) {
        play_log("colonisation refused: '%s'", ui_last_box());
        return false;
    }
    ui_wait_screen(28, 2000);
    ui_log_screen();
    for (int i = 0; i < n; i++) {
        int j = ui_find_hotspot(starters[i], 1);
        if (j == 0)
            play_fail("ui_found_colony: no starter '%s'", starters[i]);
        const THotspot *h = Mouse_Hotspot((int16_t)j);
        char price[8];
        read_text(h->X + 2, h->Y - 0xB, 5, price);  // the price above it, "Sorry" if not offered
        if (SDL_strcmp(price, "Sorry") == 0) {
            play_log("colonisation: %s not offered", starters[i]);
            continue;
        }
        ui_click_hotspot(j, UI_CLICK);
    }
    char total[12];
    play_dump("colonize");
    play_log("colonisation: cost of colony %s", trim(read_text(0x114, 0xB9, 6, total)));
    if (!ui_toolbar("OK,BUILD IT"))
        play_fail("ui_found_colony: no OK,BUILD IT");
    if (!ui_wait_screen(20, 2000)) {
        play_log("colonisation: OK,BUILD IT not taken (credits %d)", ds.Game_Credits);
        ui_toolbar("ABORT");
        ui_wait_screen(20, 2000);
        return false;
    }
    return true;
}

// --- Colony list (screen 37) --------------------------------------------------------

void ui_planet_list(const char *button)
{
    ui_open_screen("MAIN COMPUTER", 37);
    if (!ui_toolbar(button))
        play_fail("no %s on the colony list", button);
    play_wait(10);
    ui_settle();
}

static char list_row[64];

const char *ui_planet_list_row(int row)
{
    read_text(10, row * 9 + 0x30, 47, list_row);  // PlanetList_DrawList (clears 47 cells)
    return trim(list_row);
}
