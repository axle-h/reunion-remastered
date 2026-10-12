// The whole-game playthrough policy's shared machinery
// (docs/playthrough.md, "The policy"): money and ore gates, the waits
// that run the standing orders, background goals, the interrupt dispatch and
// the dates of visible events. The steps of each act (act1.h, ...) build on
// it; tests/test_play_game.c lists them.
//
// Resuming: the steps run from checkpoints (play/run.h), and a step must be
// able to go on knowing only its name. So nothing here is carried from one
// step to the next except what policy_begin() rereads from the screen: the
// background goals the step list started before it (the steps register them
// again: pol_goal is idempotent) and whatever each goal or step rereads when
// it runs. Event dates (pol_seen) are of this run only; a resumed step
// measures its timeouts from where it starts.
//
// Decisions read what a player sees: the status bar (credits, date), the
// message boxes (closed by the ui_* waits; pol_seen notes each), the planet,
// research, shop and group screens through the ui_* observers.
#pragma once

#include <stddef.h>
#include <stdint.h>

enum : int {
    POL_HOUR = 101,  // retraces per game hour while the clock runs (main_program.c)
    POL_DAY = 24,    // hours
};

// Act 1's money order, highest first: when credits are short, a goal may spend only what the
// goals before it that are waiting for money leave over.
// - S4 (a generator when New Earth's power runs short) right after the
//   first hires: unpowered leisure buildings cost attitude, S3 then lowers
//   the tax, and the money for the generator would not come.
// - E12 (Dempsey's course) with E17: Control Centre stalls without it.
// - S2 after the hard steps: a shortage it builds against (S2_SHORT, an
//   attitude penalty) first of all, then the E9 Stadium once New Earth
//   nears 50,000 people (without one -20 attitude, 40-50% of the income),
//   then S2's lead (it pays for itself within days).
// - S12 (university for the others) after the survey; the colonies in the
//   order Apollo, Penelope, Mir, each colony's core (S2, power, Vehicle
//   plant, Spaceport) before the next founding.
// Act 2 adds: the army for wave 1
// (J8) and its commander (J7) right after S4; the Galleon (J13) and the
// course it and the Hyperspace drive need (J12) after the shortages; the
// droids and ships that bring the colonies' ore (S6, S7) after S2's lead;
// Russel (J11) after Mir.
// Act 3 adds: the army for the waves
// and Mirach (M8, its lines kept busy) right after J8; Dempsey's courses
// (M10) with J12's; the spy satellite and its carrier (M14), the space
// station (M16) after the Galleon.
// Act 4 adds: the new staff, their
// courses and the pub's tips (L1-L4) right after the army; the Antares
// colony and its carrier (L5) and the spy satellites (L8) after the space
// station.
typedef enum {
    PRI_E1, PRI_E2, PRI_S4, PRI_J8, PRI_M8, PRI_L1, PRI_L2, PRI_J7, PRI_E4, PRI_E7, PRI_E15,
    PRI_E14, PRI_E17, PRI_E12, PRI_S2_SHORT, PRI_E9_URGENT, PRI_J12, PRI_M10, PRI_J13, PRI_M14,
    PRI_M16, PRI_L5, PRI_L8, PRI_S2,
    PRI_HAUL, PRI_E5, PRI_E6, PRI_E10, PRI_E16, PRI_S12, PRI_E18, PRI_APOLLO, PRI_E9, PRI_E20,
    PRI_PENELOPE, PRI_E19, PRI_MIR, PRI_J11, PRI_E21,
    PRI_LOWEST,
} PolPriority;

// --- Steps ---

// A step of the whole-game plan: run() after the checkpoint `name`;
// goals() (may be nullptr) registers the background goals the step starts.
typedef struct {
    const char *name;
    void (*run)(void);
    void (*goals)(void);
} PolStep;

// The PlayStep.run of every step of `plan` (tests/test_play_game.c): runs
// the step play_step_name() names. The first step of a run (a new game, or
// one resumed at a checkpoint) first registers the goals of the steps before
// it; then the step's own goals, then the step.
void pol_run_step(const PolStep *plan, int n);
// Called by pol_run_step before each step: sets the reserve for the act,
// installs the box hook, logs the step.
void pol_begin(const char *step);
// The act of the step running (from its name: E act 1, J 2, M 3, L 4, X 5).
int pol_act(void);
// Game hours since 2900-01-01 as the status bar clock shows them.
int64_t pol_now(void);
// The date as text ("2927-08-13-23") for the logs.
const char *pol_date(void);

// --- Events: boxes seen (T(x) in the policy) ---

// Notes the boxes the waits close (and rename prompts accepted), with the
// date. pol_seen: the date (pol_now) of the latest box containing `text`
// (case-insensitive) seen in this run, -1 if none.
int64_t pol_seen(const char *text);
// The same, but only boxes seen at or after hour `since`.
bool pol_seen_since(const char *text, int64_t since);
// pol_seen for boxes seen at or after `since` that tolerates misread
// characters (one in eight): the box reader skips the pixels the mouse
// cursor hides, and a glyph partly hidden can read as another
// ("Superhova").
int64_t pol_seen_like(const char *text, int64_t since);
// A box containing `what` and ending in `body` (a colony box names the body
// last: "... on Amnesty system,planet New Earth") was seen since `since`.
bool pol_seen_about(const char *what, const char *body, int64_t since);
// Boxes seen so far (a counter that a wait compares to notice a new one).
int pol_boxes(void);
// Notes an event the policy saw that is no box, as if a box said `text`:
// pol_service notes each talk ("talk 2", by the talk number of ui_talk.h) and battle
// ("space battle won, lost H1 ...", "ground battle lost, ...").
void pol_note(const char *text);

// --- Money and ores ---

// Reserve R: 20,000 in act 1, 50,000 from act 2 on.
void pol_set_reserve(int32_t reserve);
int32_t pol_reserve(void);
// Credits on the status bar (the last value read if they are not drawn).
int32_t pol_credits(void);
// A purchase of `cost` at priority `pri` is allowed: credits >= cost + R +
// what higher-priority goals and the current step are waiting for. If not,
// records that `pri` waits for `cost` (pol_claims).
bool pol_afford(PolPriority pri, int32_t cost);
// What a purchase at `pri` may spend now: credits - R - the claims before
// it.
int32_t pol_spare(PolPriority pri);
// The current step waits for `cost` at `pri` (0 when it stops waiting).
void pol_step_claim(PolPriority pri, int32_t cost);
// The current step waits (pol_wait, claiming it) until pol_afford(pri,
// cost); fails the run after `hours`, naming `what`.
void pol_wait_money(const char *what, PolPriority pri, int32_t cost, int hours);

enum : int { ORE_D, ORE_E, ORE_K, ORE_L, ORE_R, ORE_T, ORES };
// New Earth's ore stock as the shop's quantity editor shows it ("Ores in
// stock", which includes the queue's ores), read through product `item`'s
// editor (any buyable product). False if it can't be read.
bool pol_ores(const char *item, int32_t ores[ORES]);
// Each ore in `have` covers `need`.
bool pol_ores_cover(const int32_t have[ORES], const int32_t need[ORES]);
// Buys up to `n` more of `item` (short money: what credits allow
// at `pri`, k = min(n, (credits - R - claims) / unit price)). Returns how many
// were queued (0 if none could be).
int pol_buy(PolPriority pri, const char *item, int n, int32_t unit_price);
// The shop's "Stores:" of `item` (goes to screen 5).
int pol_stores(const char *item);

// --- Research (S11) ---

// The research icon state of `name` (UI_RS_*, goes to screen 3).
int pol_research_state(const char *name);
// Waits until `name`'s research is done ("The <name> is ready for
// production" or the icon reads Done), starting it as soon as it is offered
// (S11 puts the current step's product first). Fails the run after `hours`.
void pol_research(const char *name, int hours);

// --- Staff, groups, map ---

// Hires `name` of category `kind` (the hire screen's captions) once
// pol_afford(pri, price) holds (the current step waits, claiming it);
// fails the run unless the credits drop by `price`.
void pol_hire(const char *kind, const char *name, PolPriority pri, int32_t price);
// The university for `who` (`course` for the developer, nullptr for the
// others) once nobody is away (a refusal says so) and the course's quote
// ("It will be N credits", (Random(10) + experience) * 1000) leaves the
// reserve. A quote of `done_above` or more means the advisor has been there
// already (a course adds 50 experience). True when he went or has been, or
// is no longer on the staff.
bool pol_university(PolPriority pri, const char *who, const char *course, int32_t done_above);
// The group's location line on screen 22, e.g. "In orbit Amnesty system
// New Earth Apollo" ("" if there is no such group). Goes to screen 22.
const char *pol_where(const char *group);
bool pol_ends_with(const char *s, const char *tail);
// The galactic map shows `planet` in the system view, and with moon
// `moon` (nullptr: just the planet) when zoomed in on it.
bool pol_on_map(const char *planet, const char *moon);

// --- Background goals (steps marked ‖) ---

// A goal runs between the waits, at least once a game day and after every
// box: advance() does what it can now (it may navigate; it rereads all it
// needs from the screen) and returns true when the goal is done, after which
// it is not called again. Goals run in priority order. Registering a goal
// twice keeps the first. `hours` > 0 fails the run if the goal is not done
// that many game hours after it was registered.
void pol_goal(const char *name, PolPriority pri, bool (*advance)(void), int hours);
// The goal is registered and done.
bool pol_goal_done(const char *name);
// The goal is registered and not done.
bool pol_goal_pending(const char *name);

// --- Waiting (S13 idle, standing orders, interrupts) ---

// Idles on the message log (screen 11, map on Amnesty, clock running) until
// pred(arg) holds, running the standing orders and the goals at least once a
// game day and after every box, and handling alien talks and battles.
// pred must be cheap and read no other screen (boxes seen, the date, the
// credits). Returns false after `hours` game hours.
bool pol_wait(bool (*pred)(void *arg), void *arg, int hours);
// pol_wait for a check that needs its own screen: check(arg) is called now,
// after every box and every `every` hours.
bool pol_poll(bool (*check)(void *arg), void *arg, int hours, int every);
// pol_wait / pol_poll that fail the run on the timeout, naming `what`.
void pol_wait_for(const char *what, bool (*pred)(void *arg), void *arg, int hours);
void pol_poll_for(const char *what, bool (*check)(void *arg), void *arg, int hours, int every);
// Runs the standing orders and goals now.
void pol_orders(void);
// Handles a talk, battle or ending screen shown.
void pol_service(void);
// Runs the driver sequence fn(arg) and returns true, or false if a talk,
// battle or ending screen came up meanwhile: the sequence is left at its
// next wait (longjmp; it must hold nothing that needs undoing) for the
// caller to pol_service it. Goals run this way (a pass that is left is
// taken up again at the next one); pol_wait and pol_service themselves are
// never left.
bool pol_try(void (*fn)(void *arg), void *arg);
// pol_try until fn(arg) runs through, handling what comes between: fn must
// reread what it needs, as a goal does.
void pol_do(void (*fn)(void *arg), void *arg);
// check(arg) through pol_try: false (the screen handled) if it was left.
// pol_poll checks this way.
bool pol_check(bool (*check)(void *arg), void *arg);
// Runs fn() (nullptr: nothing) after every goal of an orders pass and every
// two idle hours, under pol_try: a step's chore that must not wait for the
// next pass (M5 keeps a group flying).
void pol_between_goals(void (*fn)(void));
// S8: a box announced a wave at New Earth and its space battle has not
// come yet. Trade and carrier groups are then sent to Penelope (once per
// box) and no goal may send one to New Earth or Apollo.
bool pol_s8_active(void);

// --- New Earth ---

// Goes to New Earth's surface (20) or planet info (8) via the command centre
// (PLANET MAIN there selects Amnesty and New Earth: S1).
void pol_new_earth(int screen);
// What the daily S3 read of New Earth's planet info saw.
typedef struct {
    int64_t at;      // pol_now of the read (all 0 before the first)
    int32_t pop;
    int mood, tax;   // ui_mood, ui_tax_level
    int32_t credits;
} PolColony;
const PolColony *pol_new_earth_seen(void);
// S3's computation (sim.c ProcessBodyEvents, from visible counts): the
// working leisure buildings and University of a colony (its surface shown),
// the target attitude at a tax level, and the highest tax that keeps the
// target at 30 or more.
typedef struct {
    int leisure, stadium, church, park, university;
} PolLeisure;
void pol_read_leisure(PolLeisure *l);
int pol_target_attitude(int tax, int32_t pop, const PolLeisure *l);
int pol_best_tax(int32_t pop, const PolLeisure *l);
// S3's step from tax level `tax` with the mood word `mood` (ui_mood) and
// the computed level `best`: lower at once to `best`, raise one step a day
// while the mood is "neutral men" or better, one below on "people hate
// you" or worse, None after a "rebellious forces" box.
int pol_tax_want(int tax, int mood, int best, bool rebellious);
// Builds `kind` on the body whose surface is shown once S4 allows it:
// power first (when a finished building reads Passive, a Windtrap where the
// surface offers one, else a Nuclear Gen.).
// Returns ui_build's result; UI_BUILD_NO_MONEY also if pol_afford says no.
int pol_build(PolPriority pri, const char *kind, int32_t cost);
// S4 on the body whose surface is shown: if a finished building reads
// Passive (or, with `anticipate`, a Derrick's info shows its Energy below
// 60% of its 750 kWh) and no generator is being built, builds one at `pri`
// (a Windtrap where offered, else a Nuclear Gen.). True if the power is
// short.
bool pol_power(PolPriority pri, bool anticipate);
