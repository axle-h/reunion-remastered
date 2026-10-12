#include "game/colonystats.h"

#include <SDL3/SDL.h>
#include <stdarg.h>

#include "game/blit.h"
#include "game/dseg.h"
#include "game/gfx.h"
#include "game/minmax.h"
#include "game/mouse.h"
#include "game/planetinfo.h"
#include "game/system.h"
#include "platform/config.h"

// Building kinds the daily update counts (Surface_BuildingTypes, 1-based).
enum : int {
    K_BUILDING = 7,
    K_LEISURE = 8,
    K_STADIUM = 9,
    K_CHURCH = 10,
    K_UNIVERSITY = 13,
    K_PARK = 14,
    K_HOSPITAL = 15,
    K_FARM = 16,
    K_SHIELD = 19,  // Anti-radiation shield
    K_FOOD = 21,    // Food resources
    K_MEDICINE = 24,
};

// Population above which the people want one (sim.c).
enum : int32_t {
    STADIUM_POP = 50000,
    UNIVERSITY_POP = 70000,
    NO_UNIVERSITY_CAP = 80000,
};

static TBody *body_of(int16_t system, int16_t planet, int16_t moon)
{
    int16_t idx = planet;
    if (moon != 0)
        idx = ds.Galaxy_MoonLists[system - 1][planet - 1].Moons[moon - 1];
    return ds.Galaxy_Bodies[system - 1] + (idx - 1);
}

static int32_t lmul(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a * (uint32_t)b);
}

static int32_t ldiv32(int32_t a, int32_t b)
{
    return b == 0 ? 0 : (int32_t)((int64_t)a / b);
}

// Sim_ProcessBodyEvents (sim.c) step by step, leaving out what is chance:
// the messages, radiation deaths and meteorite hits. Its population and
// attitude changes before the mood moves are applied to copies.
void ColonyStats_Compute(int16_t system, int16_t planet, int16_t moon, ColonyStats *s)
{
    const TBody *b = body_of(system, planet, moon);
    *s = (ColonyStats){0};

    // Surface_CountBuildingsByKind, without writing its table.
    for (int16_t i = 1; i <= ds.Game_BuildingCount; i++) {
        const TBuilding *bl = &ds.Game_Buildings[i - 1];
        if (bl->System == system && bl->Planet == planet && bl->Moon == moon && bl->Health == 0
            && bl->Kind >= 1 && bl->Kind <= 25)
            (bl->Flag ? s->powered : s->unpowered)[bl->Kind - 1]++;
    }
    const int16_t *n = s->powered - 1;  // n[kind]

    int32_t pop = b->Population;
    int8_t att = b->Attitude;
    s->population = pop;
    s->attitude = att;
    s->housing = lmul(n[K_BUILDING], 7000) + 10000;
    s->food = lmul(n[K_FOOD], 20000) + lmul(n[K_FARM], 8000) + 20000;
    s->health = lmul(n[K_HOSPITAL], 20000) + 20000;
    s->meteor_world = b->_14 == 100;
    s->radiation = planet < 3 && n[K_SHIELD] == 0;

    // The disease timer counts down at the start of the update.
    int8_t timer = b->_14;
    if (timer > 0 && timer < 100)
        timer--;
    s->disease_days = b->_14 == 0 ? 0 : b->_14 > 0 && b->_14 < 100 ? b->_14 : -1;
    s->sick = timer == 0 ? 2 : 1;

    int16_t target = (int16_t)((7 - (int8_t)b->TechLevel) * 10);
    if (timer == 0 && n[K_MEDICINE] == 0) {
        target -= 10;
        if (target > 25)
            target = 25;
    }
    if (lmul(s->sick, pop) > s->health) {
        target -= 10;
        pop -= 200;
        if (target > 25)
            target = 25;
    }
    if (pop > 30000)
        target -= 10;
    if (pop > 60000)
        target -= 10;
    if (pop > 100000)
        target -= 20;
    if (pop > 150000)
        target -= 20;
    if (n[K_LEISURE] > 0)
        target += 10;
    if (n[K_STADIUM] > 0)
        target += 10;
    if (n[K_CHURCH] > 0)
        target += 10;
    if (n[K_PARK] > 0)
        target += 10;
    if (pop > STADIUM_POP && n[K_STADIUM] == 0)
        target -= 20;
    if (pop > UNIVERSITY_POP && n[K_UNIVERSITY] == 0)
        target -= 20;
    if (s->radiation)
        target -= 40;
    if (pop > s->food) {
        target -= 15;
        if (target > 25)
            target = 25;
        att -= 2;
    }
    if (pop > s->housing) {
        target -= 10;
        if (target > 25)
            target = 25;
        att -= 2;
    }
    if (target < 0)
        target = 0;
    s->target = target;

    int32_t max_pop = ldiv32(lmul(att + 200, s->housing), 230);
    if (pop > s->food)
        max_pop = MinMax_Min(max_pop, s->food + 5000);
    max_pop = MinMax_Min(max_pop, ldiv32(s->health, s->sick) + 5000);
    max_pop = MinMax_Min(max_pop, 200000);
    if (att < 30)
        max_pop = ldiv32(max_pop, 2);
    if (att < 20)
        max_pop = MinMax_Min(max_pop, ldiv32(pop, 2));
    if (att < 10)
        max_pop = 500;
    if (n[K_UNIVERSITY] == 0)
        max_pop = MinMax_Min(max_pop, NO_UNIVERSITY_CAP);
    s->max_pop = max_pop;

    // Logistic growth towards max_pop (or decline above it).
    if (max_pop > pop)
        s->population_tomorrow = pop + ldiv32(lmul(max_pop - pop, ldiv32(lmul(pop, 1000), max_pop)), max_pop);
    else
        s->population_tomorrow = pop + ldiv32(lmul(max_pop - pop, 1000), pop);

    att += (int8_t)((target - att) / 10);
    if (att < target)
        att++;
    if (att > target)
        att -= 3;
    if (att < 0)
        att = 0;
    if (att > 69)
        att = 69;
    s->attitude_tomorrow = att;
}

static int16_t cur_planet(void)
{
    return ds.Game_CurPlanet[ds.Game_CurSystem - 1];
}

static int16_t cur_moon(void)
{
    return ds.Game_CurMoon[ds.Game_CurSystem - 1];
}

bool ColonyStats_Available(void)
{
    const TBody *b = ds.Game_CurBody;
    return config.colony_stats && b->Owner == 1 && b->Colony != 0;
}

// --- Drawing ------------------------------------------------------------------

// The overlay covers the planet info screen below the toolbar.
enum : int {
    TOP = 0x31,
    BOTTOM = 0xC7,
    ROW_Y = TOP + 16,
    ROW_H = 10,
    LABEL_X = 8,
    BAR_X = 74,
    BAR_W = 92,
    BAR_H = 6,
    VALUE_X = 172,
    VALUE_COLS = 24,
};

// Text colours: the planet info screen's own (Gfx_DrawTextF9 / FC).
enum : uint8_t {
    TEXT = 0xF9,
    HIGHLIGHT = 0xFC,
};

typedef enum : int { C_FRAME, C_EMPTY, C_OK, C_NEAR, C_SHORT, C_COUNT } Colour;

// The palette entry nearest each colour: screen 8's palette comes with its
// picture.
static void pick_colours(uint8_t out[C_COUNT])
{
    static const uint8_t want[C_COUNT][3] = {
        {32, 32, 36}, {12, 12, 14}, {12, 48, 12}, {60, 44, 0}, {56, 8, 8},
    };
    const uint8_t (*pal)[3] = ds.Gfx_Palette;
    for (int c = 0; c < C_COUNT; c++) {
        int best = 0, best_d = INT32_MAX;
        // Entries F0h and up are the fonts' colours.
        for (int i = 1; i < 0xF0; i++) {
            int dr = pal[i][0] - want[c][0], dg = pal[i][1] - want[c][1], db = pal[i][2] - want[c][2];
            int d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
            if (d < best_d) {
                best_d = d;
                best = i;
            }
        }
        out[c] = (uint8_t)best;
    }
}

static void text(int16_t x, int16_t y, int16_t cols, uint8_t colour, const char *fmt, ...)
{
    char s[255];
    va_list ap;
    va_start(ap, fmt);
    SDL_vsnprintf(s, sizeof s, fmt, ap);
    va_end(ap);
    Gfx_DrawText6x8(s, x, y, cols, colour);
}

// Blit_FillRect leaves the cursor alone. Hiding it only around the whole
// overlay doesn't work: hiding doesn't nest, so the text and outline
// routines would show it again in between with a stale background.
static void fill(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t colour)
{
    Mouse_HideCursor();
    Blit_FillRect(x, y, w, h, colour);
    Mouse_ShowCursor();
}

// Numbers are plain digits, as the planet info screen writes them (the
// font's comma reads as a full stop).

static void part_bar(const uint8_t *col, int16_t y, int32_t value, int32_t limit, Colour c)
{
    fill(BAR_X, (int16_t)(y + 1), BAR_W, BAR_H, col[C_EMPTY]);
    int16_t w = (int16_t)(limit > 0 ? (int64_t)SDL_clamp(value, 0, limit) * BAR_W / limit : 0);
    if (w > 0)
        fill(BAR_X, (int16_t)(y + 1), w, BAR_H, col[c]);
}

// A gauge of value against limit: green, amber from 90%, red over the limit.
static void bar(const uint8_t *col, int16_t y, int32_t value, int32_t limit)
{
    Colour c = C_OK;
    if (value > limit)
        c = C_SHORT;
    else if ((int64_t)value * 10 >= (int64_t)limit * 9)
        c = C_NEAR;
    part_bar(col, y, value, limit, c);
}

static void full_bar(const uint8_t *col, int16_t y, Colour c)
{
    fill(BAR_X, (int16_t)(y + 1), BAR_W, BAR_H, col[c]);
}

static void gauge(const uint8_t *col, int16_t y, const char *label, int32_t value, int32_t limit,
                  const char *note)
{
    text(LABEL_X, y, 11, TEXT, "%s", label);
    bar(col, y, value, limit);
    text(VALUE_X, y, VALUE_COLS, value > limit ? HIGHLIGHT : TEXT, "%d / %d%s", (int)value, (int)limit, note);
}

// A building the people ask for above a population: built, or how close.
static void demand(const uint8_t *col, int16_t y, const char *label, const ColonyStats *s, int kind,
                   int32_t from)
{
    text(LABEL_X, y, 11, TEXT, "%s", label);
    if (s->powered[kind - 1] > 0) {
        full_bar(col, y, C_OK);
        text(VALUE_X, y, VALUE_COLS, TEXT, "built");
    } else {
        bar(col, y, s->population, from);
        text(VALUE_X, y, VALUE_COLS, s->population > from ? HIGHLIGHT : TEXT, "%d / %d", (int)s->population,
             (int)from);
    }
}

static const char *kind_name(char *buf, size_t size, int kind)
{
    const PStr14 *nm = &ds.Surface_BuildingTypes[kind - 1].Name;
    return pstr_get(buf, SDL_min(size, sizeof *nm), nm);
}

void ColonyStats_Draw(void)
{
    int16_t system = ds.Game_CurSystem, planet = cur_planet(), moon = cur_moon();
    ColonyStats s;
    ColonyStats_Compute(system, planet, moon, &s);
    uint8_t col[C_COUNT];
    pick_colours(col);
    fill(0, TOP, 320, BOTTOM - TOP + 1, TEXT);
    Gfx_DrawRectOutline(2, TOP + 1, 317, BOTTOM - 1, col[C_FRAME]);

    char name[256];
    if (moon == 0)
        PlanetInfo_GetBodyName(name, sizeof name, system, planet, 0);
    else
        PlanetInfo_GetBodyFullName(name, sizeof name, system, planet, moon);
    text(LABEL_X, TOP + 5, 26, HIGHLIGHT, "%s", name);
    text(VALUE_X, TOP + 5, VALUE_COLS, HIGHLIGHT, "%d people", (int)s.population);

    int16_t y = ROW_Y;
    gauge(col, y, "Housing", s.population, s.housing, "");
    y += ROW_H;
    gauge(col, y, "Food", s.population, s.food, "");
    y += ROW_H;
    gauge(col, y, "Health", s.sick * s.population, s.health, s.sick > 1 ? " sick x2" : "");
    y += ROW_H;
    demand(col, y, "Stadium", &s, K_STADIUM, STADIUM_POP);
    y += ROW_H;
    demand(col, y, "University", &s, K_UNIVERSITY, UNIVERSITY_POP);
    y += ROW_H;

    // Disease and the Medicine plant.
    text(LABEL_X, y, 11, TEXT, "Disease");
    bool medicine = s.powered[K_MEDICINE - 1] > 0;
    if (s.disease_days == 0) {
        full_bar(col, y, medicine ? C_NEAR : C_SHORT);
        text(VALUE_X, y, VALUE_COLS, medicine ? TEXT : HIGHLIGHT,
             medicine ? "yes; plant working" : "needs Medicine plant");
    } else if (s.disease_days > 0) {
        full_bar(col, y, C_EMPTY);
        text(VALUE_X, y, VALUE_COLS, TEXT, "in %d day%s%s", s.disease_days, s.disease_days == 1 ? "" : "s",
             medicine ? "; plant ok" : "");
    } else {
        full_bar(col, y, C_OK);
        text(VALUE_X, y, VALUE_COLS, TEXT, "none");
    }
    y += ROW_H;

    // Radiation (planets near the sun) or meteorites.
    if (planet < 3) {
        text(LABEL_X, y, 11, TEXT, "Radiation");
        full_bar(col, y, s.radiation ? C_SHORT : C_OK);
        text(VALUE_X, y, VALUE_COLS, s.radiation ? HIGHLIGHT : TEXT,
             s.radiation ? "needs Anti-rad shield" : "shielded");
        y += ROW_H;
    } else if (s.meteor_world) {
        text(LABEL_X, y, 11, TEXT, "Meteorites");
        full_bar(col, y, C_NEAR);
        text(VALUE_X, y, VALUE_COLS, TEXT, "may hit buildings");
        y += ROW_H;
    }

    // Leisure: +10 mood for each kind that is powered.
    static const struct { int kind; const char *name; } leisure[] = {
        {K_LEISURE, "Leisure"}, {K_CHURCH, "Church"}, {K_PARK, "Park"}, {K_STADIUM, "Stadium"},
    };
    char have[48] = "";
    int count = 0;
    for (int i = 0; i < 4; i++) {
        if (s.powered[leisure[i].kind - 1] == 0)
            continue;
        SDL_snprintf(have + SDL_strlen(have), sizeof have - SDL_strlen(have), "%s%s", count ? "  " : "",
                     leisure[i].name);
        count++;
    }
    text(LABEL_X, y, 11, TEXT, "Leisure");
    part_bar(col, y, count, 4, C_OK);
    text(VALUE_X, y, VALUE_COLS, TEXT, count ? "+%d mood" : "none", count * 10);
    y += ROW_H;
    if (count) {
        text(BAR_X, y, 40, TEXT, "%s", have);
        y += ROW_H;
    }

    // Mood: today and where it is heading.
    // Below 30 the colony grows half as far, below 20 it shrinks, below 10
    // it revolts.
    text(LABEL_X, y, 11, TEXT, "Mood");
    part_bar(col, y, s.attitude, 69, s.attitude >= 30 ? C_OK : s.attitude >= 20 ? C_NEAR : C_SHORT);
    const char *trend = s.attitude_tomorrow > s.attitude   ? "rising"
                        : s.attitude_tomorrow < s.attitude ? "falling"
                                                           : "steady";
    text(VALUE_X, y, VALUE_COLS, s.target < 30 ? HIGHLIGHT : TEXT, "%d %s to %d", s.attitude, trend,
         s.target);
    y += ROW_H;
    text(LABEL_X, y, 11, TEXT, "Grows to");
    bar(col, y, s.population, SDL_max(s.max_pop, 1));
    int32_t change = s.population_tomorrow - s.population;
    text(VALUE_X, y, VALUE_COLS, change < 0 ? HIGHLIGHT : TEXT, "%d; %s%d a day", (int)s.max_pop,
         change < 0 ? "" : "+", (int)change);
    y += ROW_H;
    if (s.powered[K_UNIVERSITY - 1] == 0 && s.max_pop == NO_UNIVERSITY_CAP) {
        text(BAR_X, y, 40, TEXT, "at most %d without a University", (int)NO_UNIVERSITY_CAP);
        y += ROW_H;
    }

    // Unpowered buildings count for nothing.
    static const int counted[] = {K_BUILDING, K_FOOD,     K_FARM,   K_HOSPITAL, K_MEDICINE, K_STADIUM,
                                  K_UNIVERSITY, K_LEISURE, K_CHURCH, K_PARK,     K_SHIELD};
    char line[64] = "";
    for (size_t i = 0; i < SDL_arraysize(counted); i++) {
        if (s.unpowered[counted[i] - 1] == 0)
            continue;
        char nm[16];
        kind_name(nm, sizeof nm, counted[i]);
        SDL_snprintf(line + SDL_strlen(line), sizeof line - SDL_strlen(line), "%s%s", line[0] ? "  " : "",
                     nm);
    }
    if (line[0]) {
        text(LABEL_X, y, 11, TEXT, "No power");
        text(BAR_X, y, 40, HIGHLIGHT, "%s", line);
    }
}
