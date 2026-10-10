#include "play/policy.h"

#include <SDL3/SDL.h>
#include <setjmp.h>

#include "play/play.h"
#include "play/run.h"
#include "play/ui.h"
#include "play/ui_battle.h"
#include "play/ui_message.h"
#include "play/ui_office.h"
#include "play/ui_planet.h"
#include "play/ui_ships.h"
#include "play/ui_talk.h"

enum : int {
    SCR_COMMAND = 1,
    SCR_RESEARCH = 3,
    SCR_SHOP = 5,
    SCR_PLANET_INFO = 8,
    SCR_MESSAGES = 11,
    SCR_SURFACE = 20,
    SCR_EQUIP = 22,
    EVENTS = 1024,
    EVENT_TEXT = 160,
    GOALS = 32,
    CLOCK_STALL = 20 * POL_HOUR,  // retraces without a new hour: the clock stopped
    DERRICK_ENERGY = 750,         // a Derrick's Energy (walkthrough.md "Buildings")
    POWER_MARGIN = 60,            // S4: percent of it below which a generator comes
    UNIVERSITY_MIN = 20000,       // the cheapest course: experience 20 at hire
};

static struct {
    uint32_t run;     // play_run_id of the run the state belongs to
    int32_t reserve;
    int32_t credits;  // last credits read off the status bar
    struct { int64_t at; char text[EVENT_TEXT]; } events[EVENTS];
    int events_n;
    int boxes;        // boxes and prompts noted
    bool fatal;       // MSG 6: a Jaanosian meeting was lost
    char fatal_text[EVENT_TEXT];
    int32_t claims[PRI_LOWEST + 1];
    PolPriority step_pri;
    int32_t step_cost;
    struct {
        const char *name;
        PolPriority pri;
        bool (*advance)(void);
        int64_t deadline;  // pol_now, 0 = none
        bool done;
    } goals[GOALS];
    int goals_n;
    int64_t next_orders;  // pol_now when the standing orders are due
    int orders_boxes;     // pol.boxes when they last ran
    bool in_orders;
    bool in_goal;         // a goal's advance() runs: its pol_afford records claims
    PolColony ne;
    int64_t last_s3;
    bool planned;         // pol_run_step registered the earlier steps' goals
    int act;              // of the step running (pol_begin)
    int64_t s8_since;     // the wave box S8 reacted to (pol_now), 0: none
} pol;

// Where an interrupt screen leaves the driver sequence running (pol_try).
static jmp_buf *abort_to;
// pol_between_goals.
static void (*between)(void);

// The state belongs to one run (play_run or play_resume): a new run in
// this process starts it afresh.
static void check_run(void)
{
    if (pol.run != play_run_id()) {
        pol = (typeof(pol)){.run = play_run_id()};
        abort_to = nullptr;
        between = nullptr;
    }
}

// --- Interrupted driver sequences ---

static void on_interrupt_screen(void)
{
    jmp_buf *to = abort_to;
    if (!to)
        return;
    abort_to = nullptr;
    play_log("screen %d came up at %s: the sequence is left", ui_screen(), pol_date());
    longjmp(*to, 1);
}

bool pol_try(void (*fn)(void *arg), void *arg)
{
    jmp_buf here;
    jmp_buf *outer = abort_to;
    if (setjmp(here) != 0) {
        abort_to = outer;
        return false;
    }
    abort_to = &here;
    fn(arg);
    abort_to = outer;
    return true;
}

typedef struct {
    bool (*check)(void *arg);
    void *arg;
    bool held;
} CheckTry;

static void check_try(void *arg)
{
    CheckTry *c = arg;
    c->held = c->check(c->arg);
}

bool pol_check(bool (*check)(void *arg), void *arg)
{
    CheckTry c = {check, arg, false};
    if (!pol_try(check_try, &c)) {
        pol_service();
        return false;
    }
    return c.held;
}

void pol_do(void (*fn)(void *arg), void *arg)
{
    for (int tries = 1; !pol_try(fn, arg); tries++) {
        if (tries == 10)
            play_fail("pol_do: interrupted 10 times");
        pol_service();
    }
}

// --- Steps ---

int pol_act(void)
{
    return pol.act;
}

int64_t pol_now(void)
{
    return ui_clock_hours();
}

const char *pol_date(void)
{
    static char s[32];
    int64_t h = pol_now();
    SDL_snprintf(s, sizeof s, "%d-%02d-%02d-%02d", (int)(2900 + h / (24 * 30 * 12)),
                 (int)(h / (24 * 30) % 12 + 1), (int)(h / 24 % 30 + 1), (int)(h % 24));
    return s;
}

static void on_box(const char *text, void *unused)
{
    (void)unused;
    pol_note(text);
}

void pol_note(const char *text)
{
    check_run();
    int i = pol.events_n++ % EVENTS;
    pol.events[i].at = pol_now();
    SDL_strlcpy(pol.events[i].text, text, sizeof pol.events[i].text);
    pol.boxes++;
    // MSG 6: a Jaanosian meeting without the Communicator, or the deadline
    // without contact (§3 C): the run can't be won any more.
    if (SDL_strcasestr(text, "received a message from deep space")) {
        pol.fatal = true;
        SDL_strlcpy(pol.fatal_text, text, sizeof pol.fatal_text);
    }
    play_log("T %s: %s", pol_date(), text);
}

void pol_begin(const char *step)
{
    check_run();
    ui_on_box(on_box, nullptr);
    ui_on_interrupt_screen(on_interrupt_screen);
    // The act from the step's name: E1-E23 act 1, J1-J13 act 2, ...
    const char *acts = "EJMLX", *a = SDL_strchr(acts, step[0]);
    pol.act = a && step[0] ? (int)(a - acts) + 1 : 1;
    if (pol.reserve == 0 || (pol.act >= 2 && pol.reserve < 50000))
        pol.reserve = pol.act >= 2 ? 50000 : 20000;
    pol.step_pri = PRI_LOWEST;
    pol.step_cost = 0;
    int32_t c;
    if (ui_credits(&c))
        pol.credits = c;
    play_log("step %s at %s, credits %d", step, pol_date(), pol.credits);
}

void pol_run_step(const PolStep *plan, int n)
{
    check_run();
    const char *name = play_step_name();
    int k = 0;
    while (k < n && SDL_strcmp(plan[k].name, name) != 0)
        k++;
    if (k == n)
        play_fail("step %s is not in the plan", name ? name : "(none)");
    pol_begin(name);
    if (!pol.planned) {
        for (int i = 0; i < k; i++)
            if (plan[i].goals)
                plan[i].goals();
        pol.planned = true;
    }
    if (plan[k].goals)
        plan[k].goals();
    plan[k].run();
}

// --- Events ---

int64_t pol_seen(const char *text)
{
    check_run();
    int first = pol.events_n > EVENTS ? pol.events_n - EVENTS : 0;
    for (int i = pol.events_n - 1; i >= first; i--)
        if (SDL_strcasestr(pol.events[i % EVENTS].text, text))
            return pol.events[i % EVENTS].at;
    return -1;
}

bool pol_seen_since(const char *text, int64_t since)
{
    check_run();
    int first = pol.events_n > EVENTS ? pol.events_n - EVENTS : 0;
    for (int i = pol.events_n - 1; i >= first && pol.events[i % EVENTS].at >= since; i--)
        if (SDL_strcasestr(pol.events[i % EVENTS].text, text))
            return true;
    return false;
}

bool pol_seen_about(const char *what, const char *body, int64_t since)
{
    check_run();
    int first = pol.events_n > EVENTS ? pol.events_n - EVENTS : 0;
    for (int i = pol.events_n - 1; i >= first && pol.events[i % EVENTS].at >= since; i--) {
        const char *t = pol.events[i % EVENTS].text;
        size_t n = SDL_strlen(t), m = SDL_strlen(body);
        while (n > 0 && t[n - 1] == ' ')
            n--;
        if (SDL_strcasestr(t, what) && n >= m && SDL_strncmp(t + n - m, body, m) == 0)
            return true;
    }
    return false;
}

// `text` at `at` with at most one character in eight different (case
// ignored).
static bool like_at(const char *at, const char *text)
{
    size_t n = SDL_strlen(text), bad = 0;
    for (size_t i = 0; i < n; i++) {
        if (!at[i])
            return false;
        if (SDL_tolower((unsigned char)at[i]) != SDL_tolower((unsigned char)text[i]) && ++bad > n / 8)
            return false;
    }
    return true;
}

int64_t pol_seen_like(const char *text, int64_t since)
{
    check_run();
    int first = pol.events_n > EVENTS ? pol.events_n - EVENTS : 0;
    for (int i = pol.events_n - 1; i >= first && pol.events[i % EVENTS].at >= since; i--)
        for (const char *t = pol.events[i % EVENTS].text; *t; t++)
            if (like_at(t, text))
                return pol.events[i % EVENTS].at;
    return -1;
}

int pol_boxes(void)
{
    check_run();
    return pol.boxes;
}

// --- Money and ores ---

void pol_set_reserve(int32_t reserve)
{
    pol.reserve = reserve;
}

int32_t pol_reserve(void)
{
    return pol.reserve;
}

int32_t pol_credits(void)
{
    int32_t c;
    if (ui_credits(&c))
        pol.credits = c;
    return pol.credits;
}

// What goals and the step before `pri` wait for.
static int32_t claims_before(PolPriority pri)
{
    int32_t sum = 0;
    for (int p = 0; p < (int)pri; p++)
        sum += pol.claims[p];
    if (pol.step_pri < pri)
        sum += pol.step_cost;
    return sum;
}

int32_t pol_spare(PolPriority pri)
{
    return pol_credits() - pol.reserve - claims_before(pri);
}

bool pol_afford(PolPriority pri, int32_t cost)
{
    bool ok = pol_credits() >= cost + pol.reserve + claims_before(pri);
    if (pol.in_goal)
        pol.claims[pri] = ok ? 0 : cost;
    return ok;
}

void pol_step_claim(PolPriority pri, int32_t cost)
{
    pol.step_pri = cost > 0 ? pri : PRI_LOWEST;
    pol.step_cost = cost;
}

typedef struct {
    PolPriority pri;
    int32_t cost;
} MoneyWait;

static bool money_there(void *arg)
{
    const MoneyWait *m = arg;
    return pol_afford(m->pri, m->cost);
}

void pol_wait_money(const char *what, PolPriority pri, int32_t cost, int hours)
{
    MoneyWait m = {pri, cost};
    if (money_there(&m))
        return;
    pol_step_claim(pri, cost);
    play_log("%s: waiting for %d credits (+ reserve %d) at %s", what, cost, pol.reserve, pol_date());
    pol_wait_for(what, money_there, &m, hours);
}

bool pol_ores(const char *item, int32_t ores[ORES])
{
    return ui_shop_ores(item, ores);
}

bool pol_ores_cover(const int32_t have[ORES], const int32_t need[ORES])
{
    for (int k = 0; k < ORES; k++)
        if (have[k] < need[k])
            return false;
    return true;
}

int pol_buy(PolPriority pri, const char *item, int n, int32_t unit_price)
{
    int32_t spare = pol_credits() - pol.reserve - claims_before(pri);
    int k = spare > 0 ? (int)SDL_min((int32_t)n, spare / unit_price) : 0;
    if (pol.in_goal)
        pol.claims[pri] = k < n ? unit_price : 0;
    if (k <= 0)
        return 0;
    int got = ui_buy(item, k);
    play_log("buy %s: %d of %d (credits %d)", item, got, n, pol_credits());
    return got > 0 ? got : 0;
}

int pol_stores(const char *item)
{
    UiShopQueue q;
    if (!ui_shop_select(item) || !ui_shop_queue(&q))
        return 0;
    return q.stores;
}

// --- Research ---

static bool icon_known(void *name)
{
    return ui_research_icon(name) != UI_RS_UNKNOWN;
}

int pol_research_state(const char *name)
{
    ui_goto(SCR_RESEARCH);
    ui_wait_until(icon_known, (void *)name, 200);
    return ui_research_icon(name);
}

typedef struct {
    const char *name;
    char ready[64];
    int64_t since;
} ResearchWait;

static bool research_check(void *arg)
{
    ResearchWait *w = arg;
    if (pol_seen_since(w->ready, w->since))
        return true;
    int st = pol_research_state(w->name);
    if (st == UI_RS_DONE)
        return true;
    if (st == UI_RS_CAN_DEVELOP || st == UI_RS_CAN_ANALYSE)
        ui_research_start(w->name);
    return false;
}

static bool research_ready(void *arg)
{
    const ResearchWait *w = arg;
    return pol_seen_since(w->ready, w->since);
}

void pol_research(const char *name, int hours)
{
    ResearchWait w = {.name = name, .since = pol_now()};
    SDL_snprintf(w.ready, sizeof w.ready, "The %s is ready for production", name);
    int64_t end = pol_now() + hours;
    // Offered (and started) first: polled after each box and twice a day.
    while (!pol_check(research_check, &w)) {
        int st = ui_research_icon(name);
        if (st == UI_RS_RUNNING || st == UI_RS_DEVELOPING || st == UI_RS_ANALYSING) {
            // Running: wait for its box (its "ready" box is one of the boxes).
            if (pol_wait(research_ready, &w, (int)SDL_max(end - pol_now(), 1)))
                break;
        } else if (!pol_poll(research_check, &w, (int)SDL_max(end - pol_now(), 1), 12)) {
            break;
        }
        if (pol_now() >= end)
            break;
    }
    if (!research_ready(&w) && pol_research_state(name) != UI_RS_DONE)
        play_fail("research %s not done in %d h (icon %d)", name, hours, ui_research_icon(name));
    play_log("research %s done at %s", name, pol_date());
}

// S11: the research queue (act 1 part; later acts' products follow in the
// order of playthrough-policy.md S11). Names as the research icons caption
// them (the product names in the game's data; an unknown name would only
// lose its rank: S11 then starts it as "anything").
static const char *const research_order[] = {
    "Satellite", "Sat.carrier", "Miner station", "Transfer ship", "Control Centre",
    "Communicator", "Radio", "Hyperspace drv", "Galleon", "Starfighter", "Twin laser gun",
    "Spy satellite", "Missile", "Destroyer", "Space station", "Aircraft", "Plasma gun",
    "Missile launcher", "Mental radar", "Trooper", "Miner droid",
};

// The research screen's icons by caption (the hint line names each).
static int research_icons(char names[35][19])
{
    int n = 0;
    for (int j = 1; j <= 35; j++) {
        const char *c = ui_hotspot_caption(j);
        if (*c && SDL_strcmp(c, "Unknown") != 0)
            SDL_strlcpy(names[n++], c, 19);
    }
    return n;
}

static void s11_research(void)
{
    ui_goto(SCR_RESEARCH);
    ui_wait_until(icon_known, "Satellite", 200);
    char names[35][19];
    int n = research_icons(names), best = -1, rank = INT32_MAX;
    for (int i = 0; i < n; i++) {
        int st = ui_research_icon(names[i]);
        if (st == UI_RS_RUNNING || st == UI_RS_DEVELOPING || st == UI_RS_ANALYSING)
            return;
        if (st != UI_RS_CAN_DEVELOP && st != UI_RS_CAN_ANALYSE)
            continue;
        int r = (int)SDL_arraysize(research_order);
        for (int k = 0; k < (int)SDL_arraysize(research_order); k++)
            if (SDL_strcasecmp(research_order[k], names[i]) == 0)
                r = k;
        if (r < rank) {
            rank = r;
            best = i;
        }
    }
    if (best >= 0) {
        play_log("S11: starting %s", names[best]);
        ui_research_start(names[best]);
    }
}

// --- New Earth ---

void pol_new_earth(int screen)
{
    if (ui_screen() != SCR_COMMAND)
        ui_command_centre();
    ui_planet_main();
    if (screen == SCR_PLANET_INFO) {
        ui_planet_info();
        if (SDL_strcmp(ui_body_name(), "New Earth") != 0)
            play_fail("PLANET MAIN showed '%s', not New Earth", ui_body_name());
    }
}

const PolColony *pol_new_earth_seen(void)
{
    return &pol.ne;
}

// S3's computed target attitude of a colony at tax level `tax` (sim.c
// ProcessBodyEvents), from what its screens show: the population and the
// working (Active) leisure buildings and University on its surface. The
// shortages it also counts (food, housing, hospital, medicine) show as
// boxes; S2 builds against them.
int pol_target_attitude(int tax, int32_t pop, const PolLeisure *l)
{
    int t = (7 - tax) * 10 + 10 * ((l->leisure > 0) + (l->stadium > 0) + (l->church > 0) + (l->park > 0));
    t -= (pop > 30000 ? 10 : 0) + (pop > 60000 ? 10 : 0) + (pop > 100000 ? 20 : 0)
         + (pop > 150000 ? 20 : 0);
    t -= (pop > 50000 && l->stadium == 0 ? 20 : 0) + (pop > 70000 && l->university == 0 ? 20 : 0);
    return t;
}

// The highest tax whose target attitude keeps 30 ("neutral men"): below
// 30 the population ceiling halves, below 20 the population falls by half.
int pol_best_tax(int32_t pop, const PolLeisure *l)
{
    for (int tax = TAX_VERY_DIFFICULT; tax > TAX_NONE; tax--)
        if (pol_target_attitude(tax, pop, l) >= 30)
            return tax;
    return TAX_NONE;
}

void pol_read_leisure(PolLeisure *l)
{
    *l = (PolLeisure){ui_buildings("Leisure Centre").active, ui_buildings("Stadium").active,
                      ui_buildings("Church").active, ui_buildings("Park").active,
                      ui_buildings("University").active};
}

// S3 on New Earth, once a day: the highest tax whose computed target
// attitude is 30 or more (lowered at once, raised one step a day while the
// mood is "neutral men" or better). The mood word catches what the count
// misses: "people hate you" or worse lowers one step more, the
// "rebellious forces" box (attitude under 10, a revolt within days) sets
// None.
static void s3_tax(void)
{
    pol_new_earth(SCR_SURFACE);
    PolLeisure l;
    pol_read_leisure(&l);
    pol_new_earth(SCR_PLANET_INFO);
    PolColony *c = &pol.ne;
    c->at = pol_now();
    c->pop = ui_population();
    c->mood = ui_mood();
    c->tax = ui_tax_level();
    c->credits = pol_credits();
    int best = pol_best_tax(c->pop, &l);
    play_log("S3 %s: New Earth pop %d, mood %d, tax %d (best %d), credits %d", pol_date(), c->pop,
             c->mood, c->tax, best, c->credits);
    if (c->mood < 0 || c->tax < 0)
        return;
    int want = pol_tax_want(c->tax, c->mood, best, pol_seen_since("rebellious forces", pol.last_s3));
    if (want != c->tax)
        ui_set_tax(want);
    c->tax = ui_tax_level();
    pol.last_s3 = pol_now();
}

int pol_tax_want(int tax, int mood, int best, bool rebellious)
{
    if (rebellious)
        return TAX_NONE;
    if (mood <= 1)
        return SDL_max(SDL_min(tax, best) - 1, TAX_NONE);
    if (best < tax)
        return best;
    if (best > tax && mood >= 3)
        return tax + 1;
    return tax;
}

// S4: a finished building on the body shown reads Passive (its outline
// blinks red): the power runs short.
static const char *const kinds[] = {
    "Nuclear Gen.", "Windtrap", "Mine", "Derrick", "Observatory", "Building", "Leisure Centre",
    "Stadium", "Church", "Storage bay", "Space Port", "University", "Park", "Hospital", "Farm",
    "Radar", "Food Resources", "Builder Plant", "Vehicle Plant", "Medicine Plant",
};

static bool power_short(void)
{
    for (size_t i = 0; i < SDL_arraysize(kinds); i++) {
        UiBuildings b = ui_buildings(kinds[i]);
        if (b.finished > b.active)
            return true;
    }
    return false;
}

bool pol_power(PolPriority pri, bool anticipate)
{
    if (!power_short()) {
        if (!anticipate)
            return false;
        // A building's info line "Energy : n kwh" is its nominal energy
        // times supply / demand (planetinfo.c apply_building_efficiency);
        // buildings switch off below half. New Earth ran at ~70%; a
        // generator comes below 60%. Read off a Derrick (New Earth), else
        // a Vehicle plant or Spaceport (the colonies' core).
        static const struct {
            const char *kind;
            int32_t energy;  // walkthrough.md "Buildings"
        } refs[] = {{"Derrick", DERRICK_ENERGY}, {"Vehicle Plant", 1230}, {"Space Port", 753}};
        int32_t e = -1;
        size_t r = 0;
        for (; r < SDL_arraysize(refs) && e < 0; r++)
            if (ui_buildings(refs[r].kind).active > 0)
                e = ui_building_info(refs[r].kind, "Energy");
        if (e < 0 || e * 100 >= refs[r - 1].energy * POWER_MARGIN)
            return false;
        play_log("S4: a %s gets %d of its %d kWh", refs[r - 1].kind, e, refs[r - 1].energy);
    }
    if (ui_buildings("Nuclear Gen.").building > 0 || ui_buildings("Windtrap").building > 0)
        return true;
    // A Windtrap where the surface offers one (Desert, Icy), else a Nuclear
    // generator.
    bool windtrap = ui_select_kind("Windtrap");
    const char *gen = windtrap ? "Windtrap" : "Nuclear Gen.";
    play_log("S4: power short, a %s first", gen);
    if (!pol_afford(pri, windtrap ? 10000 : 35000))
        return true;
    int r = ui_build(gen);
    // A colony grown to its last free ground (Russel in act 3) builds no
    // more: nothing else is built there while the power stays short.
    if (r == UI_BUILD_NO_ROOM)
        play_log("S4: no room for a %s here", gen);
    else if (r != UI_BUILD_OK)
        play_fail("S4: the generator was not built (%d: %s)", r, ui_last_box());
    return true;
}

int pol_build(PolPriority pri, const char *kind, int32_t cost)
{
    // Park, Windtrap and the generators draw no power.
    bool draws = SDL_strcmp(kind, "Park") != 0 && SDL_strcmp(kind, "Windtrap") != 0
                 && SDL_strcmp(kind, "Nuclear Gen.") != 0;
    if (draws && pol_power(pri, false))
        return UI_BUILD_NO_MONEY;
    if (!pol_afford(pri, cost))
        return UI_BUILD_NO_MONEY;
    return ui_build(kind);
}

// --- Staff, groups, map ---

// The candidate takes the job: the command centre shows him (and the
// status bar's credits drop by the price, unless a midnight's taxes came
// in meanwhile).
void pol_hire(const char *kind, const char *name, PolPriority pri, int32_t price)
{
    pol_wait_money(name, pri, price, 60 * POL_DAY);
    ui_command_centre();
    int32_t before = pol_credits();
    ui_hire(kind, name);
    if (!ui_find_hotspot(name, 1))
        play_fail("hire %s: credits %d -> %d, not on the staff", name, before, pol_credits());
    if (pol_credits() != before - price)
        play_log("hire %s: credits %d -> %d", name, before, pol_credits());
    pol_step_claim(pri, 0);
}

bool pol_university(PolPriority pri, const char *who, const char *course, int32_t done_above)
{
    if (!pol_afford(pri, UNIVERSITY_MIN))
        return false;
    ui_command_centre();
    // Replaced by a better hire (Klaatoo, Julie: L1, L2): nothing left to do
    // for him.
    if (!ui_find_hotspot(who, 1)) {
        play_log("university: %s is no longer on the staff", who);
        return true;
    }
    int32_t before = pol_credits(), price;
    bool went = ui_university_upto(who, course, SDL_min(pol_spare(pri), done_above - 1), &price);
    // "I don't think I could become more skilled...": at the cap of the
    // story stage (advisor.c), as good as been.
    bool capped = !went && price == 0 && SDL_strstr(ui_last_reply(), "more skilled");
    if (went)
        play_log("university: %s went, %d credits", who, before - pol_credits());
    else if (price >= done_above || capped)
        play_log("university: %s quoted %d: has been", who, price);
    else if (price > 0)
        pol_afford(pri, price);  // declined for money: wait for the quote
    return went || price >= done_above || capped;
}

const char *pol_where(const char *group)
{
    static char loc[128];
    if (!ui_group_find(group)) {
        ui_ship_info();
        if (!ui_group_find(group))
            return "";
    }
    ui_open_group(group, SCR_EQUIP);
    ui_group_location(group, loc, sizeof loc);
    return loc;
}

bool pol_ends_with(const char *s, const char *tail)
{
    size_t n = SDL_strlen(s), m = SDL_strlen(tail);
    return n >= m && SDL_strcmp(s + n - m, tail) == 0;
}

bool pol_on_map(const char *planet, const char *moon)
{
    ui_open_screen("GALACTIC MAP", 7);
    if (ui_toolbar_has("ZOOM OUT")) {
        ui_toolbar("ZOOM OUT");
        ui_settle();
    }
    if (!ui_find_hotspot(planet, 1))
        return false;
    if (!moon)
        return true;
    ui_hotspot(planet);
    ui_settle();
    play_wait(30);
    return ui_find_hotspot(moon, 1) != 0;
}

// --- S8: wave evacuation ---

// The groups the policy creates that a lost battle at New Earth would take
// (trade and carrier groups, intel.c:181-196; Army groups stay and fight).
static const char *const s8_groups[] = {"Trade 1", "Survey 1", "Survey 2", "Galleon 1", "Spy 1",
                                        "Scout"};

// The latest box announcing a wave at New Earth or its moon (pol_now), -1
// if none: "ATTENTION! Enemy fleet approaching <where>" (sim.c:1255), or
// the first arrival of an unmet race at a colony ("New alien race's ships
// arrived at your colony in Amnesty", sim.c:1234), which names no body:
// the Morgrul waves come to New Earth (walkthrough "Attacks on New Earth").
static int64_t wave_box(void)
{
    int first = pol.events_n > EVENTS ? pol.events_n - EVENTS : 0;
    for (int i = pol.events_n - 1; i >= first; i--) {
        const char *t = pol.events[i % EVENTS].text;
        if ((SDL_strcasestr(t, "Enemy fleet approaching") && SDL_strcasestr(t, "New Earth"))
            || (SDL_strcasestr(t, "arrived") && SDL_strcasestr(t, "at your colony in Amnesty")))
            return pol.events[i % EVENTS].at;
    }
    return -1;
}

bool pol_s8_active(void)
{
    check_run();
    int64_t t = wave_box();
    // Over with the battle (or if none came: the attack follows 5-105 h
    // after the arrival, intel.c:126-133).
    return t >= 0 && !pol_seen_since("space battle", t) && pol_now() < t + 120;
}

// S8: within the hour of the box, every trade and carrier group at New
// Earth or Apollo, or flying there, goes to Penelope (docked ones take off
// first). Once per box.
static void s8_evacuate(void)
{
    int64_t t = wave_box();
    if (!pol_s8_active() || pol.s8_since == t)
        return;
    pol.s8_since = t;
    play_log("S8: a wave at New Earth (box %d h ago): evacuating", (int)(pol_now() - t));
    for (size_t i = 0; i < SDL_arraysize(s8_groups); i++) {
        const char *g = s8_groups[i];
        ui_ship_info();
        if (!ui_group_find(g))
            continue;
        const char *loc = pol_where(g);
        if (!SDL_strstr(loc, "New Earth"))
            continue;
        if (ui_group_status(g) == SHIP_DOCKED && !ui_take_off(g)) {
            play_log("S8: %s did not take off", g);
            continue;
        }
        if (!ui_launch(g, nullptr, "Zeus", "Penelope"))
            play_log("S8: %s not launched", g);
    }
    pol_new_earth(SCR_SURFACE);  // S1: the map back on Amnesty
}

// --- Goals ---

void pol_goal(const char *name, PolPriority pri, bool (*advance)(void), int hours)
{
    check_run();
    for (int i = 0; i < pol.goals_n; i++)
        if (SDL_strcmp(pol.goals[i].name, name) == 0)
            return;
    if (pol.goals_n == GOALS)
        play_fail("too many goals");
    // Kept in priority order.
    int at = pol.goals_n;
    while (at > 0 && pol.goals[at - 1].pri > pri) {
        pol.goals[at] = pol.goals[at - 1];
        at--;
    }
    pol.goals[at] = (typeof(pol.goals[0])){name, pri, advance, hours > 0 ? pol_now() + hours : 0,
                                           false};
    pol.goals_n++;
    play_log("goal %s started at %s", name, pol_date());
}

bool pol_goal_done(const char *name)
{
    for (int i = 0; i < pol.goals_n; i++)
        if (SDL_strcmp(pol.goals[i].name, name) == 0)
            return pol.goals[i].done;
    return false;
}

bool pol_goal_pending(const char *name)
{
    for (int i = 0; i < pol.goals_n; i++)
        if (SDL_strcmp(pol.goals[i].name, name) == 0)
            return !pol.goals[i].done;
    return false;
}

typedef struct {
    void (*fn)(void);
} OrderRun;

static void order_run(void *arg)
{
    ((OrderRun *)arg)->fn();
}

// A standing order as a driver sequence: a talk or battle that comes up
// leaves it (pol_try) to be handled here; it runs again at the next pass.
static void order(void (*fn)(void), const char *what)
{
    if (!pol_try(order_run, &(OrderRun){fn}))
        play_log("%s interrupted", what);
    pol_service();
}

typedef struct {
    bool (*advance)(void);
    bool done;
} GoalRun;

static void advance_goal(void *arg)
{
    GoalRun *r = arg;
    r->done = r->advance();
}

static void run_goals(void)
{
    for (int i = 0; i < pol.goals_n; i++) {
        typeof(pol.goals[0]) *g = &pol.goals[i];
        if (g->done)
            continue;
        pol.claims[g->pri] = 0;
        pol.in_goal = true;
        // A talk or battle that comes up meanwhile ends this pass of the
        // goal (pol_service below handles it); the next pass rereads.
        GoalRun r = {g->advance, false};
        if (!pol_try(advance_goal, &r))
            play_log("goal %s interrupted", g->name);
        bool done = r.done;
        pol.in_goal = false;
        if (done) {
            g->done = true;
            pol.claims[g->pri] = 0;
            play_log("goal %s done at %s", g->name, pol_date());
        } else if (g->deadline && pol_now() > g->deadline) {
            play_fail("goal %s not done by its deadline", g->name);
        }
        pol_service();
        order(s8_evacuate, "S8");
        if (between)
            order(between, "between goals");
    }
}

void pol_between_goals(void (*fn)(void))
{
    between = fn;
}

// --- Waiting ---

void pol_service(void)
{
    if (pol.fatal)
        play_fail("lost a Jaanosian meeting: %s", pol.fatal_text);
    jmp_buf *outer = abort_to;
    abort_to = nullptr;
    for (int guard = 0; guard < 10 && ui_interrupt_screen(); guard++) {
        int s = ui_screen();
        play_log("interrupt screen %d at %s", s, pol_date());
        char note[EVENT_TEXT];
        UiLosses l;
        switch (s) {
        case 34:
            SDL_snprintf(note, sizeof note, "talk %d", ui_alien_talk_policy());
            pol_note(note);
            break;
        case 29: {
            bool won = ui_space_battle(&l);
            SDL_snprintf(note, sizeof note,
                         "space battle %s, lost H%d SF%d D%d C%d, enemy lost H%d SF%d D%d C%d",
                         won ? "won" : "lost", l.own[0], l.own[1], l.own[2], l.own[3], l.enemy[0],
                         l.enemy[1], l.enemy[2], l.enemy[3]);
            pol_note(note);
            break;
        }
        case 19:
            ui_assign_units((const int[4]){UI_LAUNCHER, UI_TROOPER, UI_TANK, UI_AIRCRAFT});
            break;
        case 32: {
            bool won = ui_ground_battle(UI_TACTIC_COUNTER, &l);
            SDL_snprintf(note, sizeof note,
                         "ground battle %s, lost Tr%d T%d Air%d ML%d, enemy lost Tr%d T%d Air%d ML%d",
                         won ? "won" : "lost", l.own[0], l.own[1], l.own[2], l.own[3], l.enemy[0],
                         l.enemy[1], l.enemy[2], l.enemy[3]);
            pol_note(note);
            break;
        }
        case 35:
            play_fail("death ending (screen 35)");
        default:
            play_wait(10);  // the victory: the game halts by itself
            break;
        }
        ui_settle();
    }
    abort_to = outer;
}

void pol_orders(void)
{
    check_run();
    if (pol.in_orders)
        return;
    pol.in_orders = true;
    pol_service();
    order(s8_evacuate, "S8");
    if (pol.last_s3 == 0 || pol_now() - pol.last_s3 >= POL_DAY)
        order(s3_tax, "S3");
    order(s11_research, "S11");
    run_goals();
    pol.next_orders = pol_now() + POL_DAY;
    pol.orders_boxes = pol.boxes;
    pol.in_orders = false;
}

static bool orders_due(void)
{
    return !pol.in_orders && (pol_now() >= pol.next_orders || pol.boxes != pol.orders_boxes);
}

typedef struct {
    bool (*pred)(void *arg);
    void *arg;
    int64_t end;
} IdleWait;

static bool idle_over(void *arg)
{
    const IdleWait *w = arg;
    return w->pred(w->arg) || ui_interrupt_screen() || pol.fatal || pol_now() >= w->end
           || orders_due();
}

static bool pol_wait_loop(bool (*pred)(void *arg), void *arg, int hours);

static void goto_messages(void *unused)
{
    (void)unused;
    ui_goto(SCR_MESSAGES);
}

bool pol_wait(bool (*pred)(void *arg), void *arg, int hours)
{
    check_run();
    // It handles the talks and battles itself.
    jmp_buf *outer = abort_to;
    abort_to = nullptr;
    bool held = pol_wait_loop(pred, arg, hours);
    abort_to = outer;
    return held;
}

static bool pol_wait_loop(bool (*pred)(void *arg), void *arg, int hours)
{
    int64_t end = pol_now() + hours;
    for (;;) {
        pol_service();
        if (pred(arg))
            return true;
        if (pol_now() >= end)
            return false;
        if (orders_due()) {
            pol_orders();
            continue;
        }
        if (between && !pol.in_orders)
            order(between, "between goals");
        // S13: the message log, clock running.
        if (!pol_try(goto_messages, nullptr))
            continue;
        if (orders_due() || pred(arg))
            continue;
        int64_t h0 = pol_now();
        IdleWait w = {pred, arg, SDL_min(end, pol.in_orders ? end : pol.next_orders)};
        if (between)
            w.end = SDL_min(w.end, h0 + 2);
        int frames = (int)SDL_min(w.end - h0, (int64_t)POL_DAY) * POL_HOUR * 2 + CLOCK_STALL;
        if (!ui_wait_until(idle_over, &w, frames) && pol_now() == h0)
            play_fail("the clock stands still on screen %d at %s", ui_screen(), pol_date());
    }
}

typedef struct {
    int boxes;
    int64_t at;
} PollTick;

static bool poll_tick(void *arg)
{
    const PollTick *t = arg;
    return pol.boxes != t->boxes || pol_now() >= t->at;
}

bool pol_poll(bool (*check)(void *arg), void *arg, int hours, int every)
{
    int64_t end = pol_now() + hours;
    for (;;) {
        if (pol_check(check, arg))
            return true;
        if (pol_now() >= end)
            return false;
        PollTick t = {pol.boxes, SDL_min(pol_now() + every, end)};
        pol_wait(poll_tick, &t, (int)(t.at - pol_now()));
    }
}

void pol_wait_for(const char *what, bool (*pred)(void *arg), void *arg, int hours)
{
    if (!pol_wait(pred, arg, hours))
        play_fail("timeout after %d h: %s", hours, what);
    play_log("seen at %s: %s", pol_date(), what);
}

void pol_poll_for(const char *what, bool (*check)(void *arg), void *arg, int hours, int every)
{
    if (!pol_poll(check, arg, hours, every))
        play_fail("timeout after %d h: %s", hours, what);
    play_log("seen at %s: %s", pol_date(), what);
}
