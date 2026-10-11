#include "play/ui_battle.h"

#include <SDL3/SDL.h>
#include <string.h>

#include "game/dseg.h"
#include "play/play.h"
#include "play/ui.h"

enum : int {
    SCREEN_SPACE = 29,
    SCREEN_ASSIGN = 19,
    SCREEN_GROUND = 32,
    CMD_OK_ATTACK = 0x0B,
    CMD_ADD_TROOPER = 0x0C,
    CMD_ADD_TANK = 0x13,
    CMD_ADD_AIRCRAFT = 0x2C,
    CMD_ADD_LAUNCHER = 0x24,
    CMD_END_BATTLE = 0x41,
    CMD_GROUND_WAR = 0x47,
    BATTLE_TIMEOUT = 200000,
    TILE = 16,
    FIELD_X = 0x40,        // screen 32: the field's top-left pixel (hotspot 1)
    FIELD_Y = 0x34,
    THINK = 8,             // retraces between looks at the ground battle
};

// --- Observers ---

bool ui_battle_over(void)
{
    // END BATTLE comes with the toolbar change; the result picture follows
    // in the same pass, so it is drawn once the main loop waits for input.
    int s = ui_screen();
    return (s == SCREEN_SPACE || s == SCREEN_GROUND) && !ds.Game_ScreenSwitch
           && ui_toolbar_has_command(CMD_END_BATTLE)
           && (s == SCREEN_SPACE ? ds.SpWar_BattleOver != 0 : ds.GrWar_BattleOver != 0);
}

bool ui_battle_won(void)
{
    // The picture EndSpaceBattle / EndGroundBattle loads: spvict or splost,
    // grvict or grlost.
    return ui_screen() == SCREEN_GROUND ? ds.GrWar_PlayerWon != 0 : ds.SpWar_PlayerWon != 0;
}

void ui_battle_losses(UiLosses *out)
{
    // EndSpaceBattle / EndGroundBattle print these longints on the picture.
    bool ground = ui_screen() == SCREEN_GROUND;
    for (int c = 1; c <= 4; c++) {
        out->own[c - 1] = ground ? ds.GrWar_Losses[0][c - 1] : ds.SpWar_AttackerLosses[c - 1];
        out->enemy[c - 1] = ground ? ds.GrWar_Losses[1][c - 1] : ds.SpWar_DefenderLosses[c - 1];
    }
}

int ui_space_ships(bool own)
{
    // StepSpaceBattle draws the ships of both lists that are alive.
    return own ? ds.SpWar_AttackerShipCount : ds.SpWar_DefenderShipCount;
}

// The GrWar unit lists (GrWarUnits::UnitLists): side 1 the player's, side 2
// the enemy's, with their slot counts. Records are 39 bytes (grwar.h).
static const uint8_t *battle_unit(int side, int i)
{
    return ds.GrWarUnits_UnitLists[side - 1] + (i - 1) * 0x27;
}

static int battle_slots(int side)
{
    return side == 1 ? ds.GrWarUnits_AttackerSlotCount : ds.GrWarUnits_DefenderSlotCount;
}

int ui_ground_stacks(bool own, UiStack out[20])
{
    // The result picture covers the field.
    if (ui_screen() != SCREEN_GROUND || ds.GrWar_BattleOver != 0)
        return 0;
    int side = own ? 1 : 2, n = 0;
    for (int i = 1; i <= battle_slots(side) && n < 20; i++) {
        const uint8_t *u = battle_unit(side, i);
        int16_t count;
        memcpy(&count, u + 1, 2);
        if (count <= 0)
            continue;
        // RenderBattle draws the sprite at the tile plus the step towards the
        // next tile (GrWar::DirX/DirY), field origin (64, 52).
        uint8_t dir = u[8], step = u[0x0A];
        int dx = ds.GrWar_DirX[dir - 1], dy = ds.GrWar_DirY[dir - 1];
        out[n++] = (UiStack){
            .slot = i,
            .type = u[0],
            .count = count,
            .x = FIELD_X + u[4] * TILE + step * dx,
            .y = FIELD_Y + u[5] * TILE + step * dy,
        };
    }
    return n;
}

int ui_assign_slots(UiStack out[20])
{
    if (ui_screen() != SCREEN_ASSIGN)
        return 0;
    int n = 0;
    for (int i = 1; i <= ds.GrWarUnits_AttackerSlotCount && n < 20; i++) {
        const uint8_t *u = battle_unit(1, i);
        int16_t count;
        memcpy(&count, u + 1, 2);
        out[n++] = (UiStack){.slot = i, .type = u[0], .count = count};
    }
    return n;
}

int ui_assign_pool(int type)
{
    // "Trooper / Battle tank / Aircraft / Rocket launcher  <n>" (GrWarUnits::Pool).
    return ui_screen() == SCREEN_ASSIGN ? ds.GrWarUnits_Pool[type - 1] : 0;
}

// --- Star map: own invasion ---

bool ui_invade(const char *system, const char *planet, const char *group)
{
    if (ui_screen() != 7 && (!ui_toolbar("GALACTIC MAP") || !ui_wait_screen(7, 2000)))
        return false;
    // Zoomed in on a planet (the map opens on the selected one) the system
    // list gives way to the fleets there.
    if (ui_toolbar_has("ZOOM OUT")) {
        ui_toolbar("ZOOM OUT");
        ui_settle();
    }
    if (!ui_wait_hotspot(system, 200) || !ui_hotspot(system))
        return false;
    play_wait(20);
    // The planets circle their sun: a click aimed at one can land on its
    // neighbour as they pass (seed 1, Mirach 2 for Mirach 3). The zoomed
    // view names only the planet zoomed on; on another, zoom out and aim
    // again.
    for (int aim = 0;; aim++) {
        if (!ui_wait_hotspot(planet, 200) || !ui_hotspot(planet)) {
            ui_log_screen();
            return false;
        }
        play_wait(20);
        if (!ui_toolbar_has("ZOOM OUT") || ui_find_hotspot(planet, 1) != 0)
            break;
        play_log("ui_invade: zoomed on another planet than %s", planet);
        if (aim == 3) {
            ui_log_screen();
            return false;
        }
        ui_toolbar("ZOOM OUT");
        ui_settle();
    }
    if (!ui_wait_hotspot(group, 200) || !ui_hotspot(group)) {
        ui_log_screen();
        return false;
    }
    play_wait(20);
    if (!ui_toolbar_has_command(CMD_GROUND_WAR)) {
        ui_log_screen();
        return false;
    }
    play_log("GROUND WAR with %s at %s", group, planet);
    return ui_toolbar_command(CMD_GROUND_WAR) && ui_wait_screen(SCREEN_SPACE, 3000);
}

// --- Space battle ---

static bool battle_over(void *unused)
{
    (void)unused;
    return ui_battle_over();
}

bool ui_space_battle(UiLosses *losses)
{
    if (!ui_wait_screen(SCREEN_SPACE, 3000))
        play_fail("ui_space_battle: no space battle screen");
    // (Log only: the fleets as they were when the screen opened, SpWar::AttackerStart, DefenderStart.)
    play_log("space battle: %d own ships against %d", ds.SpWar_AttackerStart,
             ds.SpWar_DefenderStart);
    if (!ui_wait_until(battle_over, nullptr, BATTLE_TIMEOUT) || !ui_wait_idle(2000))
        play_fail("ui_space_battle: the battle did not end");
    bool won = ui_battle_won();
    UiLosses l;
    ui_battle_losses(&l);
    if (losses)
        *losses = l;
    play_log("space battle %s: lost %dH %dSF %dD %dC, destroyed %dH %dSF %dD %dC", won ? "won" : "lost",
             l.own[0], l.own[1], l.own[2], l.own[3], l.enemy[0], l.enemy[1], l.enemy[2], l.enemy[3]);
    play_dump("space-result");
    if (!ui_toolbar_command(CMD_END_BATTLE))
        play_fail("ui_space_battle: no END BATTLE");
    return won;
}

// --- Unit assignment ---

// Screen 19 boxes: four per row, "Remove unit" at (10 + 77 col, 56 + 23 row).
static void remove_slot(int slot)
{
    int k = slot - 1;
    ui_click((k % 4) * 0x4D + 10 + 9, k / 4 * 0x17 + 0x38 + 8);
}

static const int add_command[5] = {0, CMD_ADD_TROOPER, CMD_ADD_TANK, CMD_ADD_AIRCRAFT, CMD_ADD_LAUNCHER};

void ui_assign_units(const int order[4])
{
    if (!ui_wait_screen(SCREEN_ASSIGN, 3000))
        play_fail("ui_assign_units: no unit assignment screen");
    UiStack s[20];
    int n = ui_assign_slots(s);
    play_log("unit assignment: %d slots, pool %d/%d/%d/%d", n, ui_assign_pool(1), ui_assign_pool(2),
             ui_assign_pool(3), ui_assign_pool(4));
    // Slots stand in columns of nine from the rear (slot 1) forwards: remove
    // every slot and add them back type by type.
    while (order && ui_assign_slots(s) > 0) {
        int before = ui_assign_slots(s);
        remove_slot(1);
        play_wait(4);
        if (ui_assign_slots(s) >= before)
            play_fail("ui_assign_units: Remove unit did nothing");
    }
    for (int k = 0; order && k < 4; k++) {
        int t = order[k];
        while (ui_assign_pool(t) > 0 && ui_assign_slots(s) < 20) {
            int before = ui_assign_slots(s);
            ui_toolbar_command(add_command[t]);
            play_wait(4);
            if (ui_assign_slots(s) <= before)
                play_fail("ui_assign_units: adding a slot of type %d did nothing", t);
        }
    }
    n = ui_assign_slots(s);
    char line[256] = "";
    for (int i = 0; i < n; i++)
        SDL_snprintf(line + SDL_strlen(line), sizeof line - SDL_strlen(line), " %d:%dx%d", s[i].slot,
                     s[i].count, s[i].type);
    play_log("unit assignment:%s", line);
    play_dump("assign");
    if (!ui_toolbar_command(CMD_OK_ATTACK))
        play_fail("ui_assign_units: no OK, ATTACK");
}

// --- Ground battle ---

// Tile distances between two drawn stacks, as GrWar measures them from the
// sprites' pixel positions (grwar.c:530-573).
static int dist_manhattan(const UiStack *a, const UiStack *b)
{
    int d = SDL_abs(a->x - b->x) + SDL_abs(a->y - b->y);
    return (int)(((uint16_t)(d - 5) >> 4) + 1);
}

static int dist_chebyshev(const UiStack *a, const UiStack *b)
{
    int m = SDL_max(SDL_abs(a->x - b->x), SDL_abs(a->y - b->y));
    return (int)(((uint32_t)(m - 3) >> 4) + 1);
}

static int dist_squared(const UiStack *a, const UiStack *b)
{
    int dx = a->x - b->x, dy = a->y - b->y;
    return (int)(((uint32_t)(dx * dx + dy * dy - 1) >> 8) + 1);
}

// Whether stack a's weapon reaches b (Trooper adjacent, Tank 2 tiles
// Manhattan, Aircraft 2 tiles any way, launcher shells 2..4 tiles).
static bool in_range(const UiStack *a, const UiStack *b)
{
    switch (a->type) {
    case UI_TROOPER: return dist_chebyshev(a, b) < 2;
    case UI_TANK: return dist_manhattan(a, b) < 3;
    case UI_AIRCRAFT: return dist_chebyshev(a, b) < 3;
    case UI_LAUNCHER: {
        int d = dist_squared(a, b);
        return d >= 4 && d <= 16;
    }
    default: return false;
    }
}

static int nearest(const UiStack *a, const UiStack *list, int n)
{
    int best = -1, bd = 1 << 30;
    for (int j = 0; j < n; j++) {
        int dx = a->x - list[j].x, dy = a->y - list[j].y, d = dx * dx + dy * dy;
        if (d < bd) {
            bd = d;
            best = j;
        }
    }
    return best;
}

// Selection shown in the left panel: the stack's name, Pcs and Str
// (GrWar::Selected slot, GrWar::SelectedOwn side).
static bool own_selected(int slot)
{
    return ds.GrWar_Selected == slot && ds.GrWar_SelectedOwn != 0;
}

// Selects own stack `s` and orders it to attack enemy stack `e`.
static bool order_attack(const UiStack *s, const UiStack *e)
{
    ui_click(s->x + 8, s->y + 8);
    if (!own_selected(s->slot))
        return false;
    if (!ui_hotspot("Attack"))
        return false;
    // The target is picked nearest to the click among the enemy stacks; the
    // field has moved on by a few retraces, so look again.
    UiStack enemy[20];
    int n = ui_ground_stacks(false, enemy);
    for (int j = 0; j < n; j++) {
        if (enemy[j].slot == e->slot) {
            ui_click(enemy[j].x + 2, enemy[j].y + 2);
            return true;
        }
    }
    return false;
}

// What the player remembers between looks: the enemy stack each own stack
// was sent against (0: none). An order lapses when that stack is gone (the
// game turns the stack back to holding, grwar.c:708-710).
typedef struct {
    int target[21];
} Plan;

// The enemy stack own stack `s` should be sent against now, or -1.
static int pick_target(const UiStack *s, const UiStack *enemy, int ne, UiTactic tactic)
{
    if (tactic == UI_TACTIC_CHARGE)
        return nearest(s, enemy, ne);
    // Counter: only tanks and aircraft move, and only when an enemy stack
    // reaches them while nothing is in their own reach.
    if (s->type != UI_TANK && s->type != UI_AIRCRAFT)
        return -1;
    int threat = -1;
    for (int j = 0; j < ne; j++) {
        if (in_range(s, &enemy[j]))
            return -1;
        if (!in_range(&enemy[j], s))
            continue;
        if (threat < 0 || dist_squared(s, &enemy[j]) < dist_squared(s, &enemy[threat]))
            threat = j;
    }
    return threat;
}

static void think(Plan *p, UiTactic tactic)
{
    UiStack own[20], enemy[20];
    int no = ui_ground_stacks(true, own), ne = ui_ground_stacks(false, enemy);
    bool alive[21] = {0};
    for (int j = 0; j < ne; j++)
        alive[enemy[j].slot] = true;
    for (int i = 0; i < no; i++) {
        UiStack *s = &own[i];
        if (p->target[s->slot] != 0 && alive[p->target[s->slot]])
            continue;
        p->target[s->slot] = 0;
        int j = pick_target(s, enemy, ne, tactic);
        if (j < 0)
            continue;
        if (order_attack(s, &enemy[j])) {
            p->target[s->slot] = enemy[j].slot;
            play_log("ground battle: own %d (%dx%d) attacks enemy %d (%dx%d)", s->slot, s->count, s->type,
                     enemy[j].slot, enemy[j].count, enemy[j].type);
        }
        return;  // one order per look: the field moves on while clicking
    }
}

static void log_forces(const char *when)
{
    UiStack s[20];
    char line[400] = "";
    for (int side = 0; side < 2; side++) {
        int n = ui_ground_stacks(side == 0, s), units[5] = {0};
        for (int i = 0; i < n; i++)
            units[s[i].type] += s[i].count;
        SDL_snprintf(line + SDL_strlen(line), sizeof line - SDL_strlen(line),
                     " %s %d stacks (%d Tr %d T %d Air %d ML)", side == 0 ? "own" : "enemy", n, units[1],
                     units[2], units[3], units[4]);
    }
    play_log("ground battle %s:%s", when, line);
}

bool ui_ground_battle(UiTactic tactic, UiLosses *losses)
{
    if (!ui_wait_screen(SCREEN_GROUND, 3000))
        play_fail("ui_ground_battle: no ground battle screen");
    log_forces("start");
    play_dump("ground-start");
    Plan plan = {0};
    uint64_t start = play_frame(), end = start + BATTLE_TIMEOUT;
    while (!ui_battle_over()) {
        if (play_frame() > end)
            play_fail("ui_ground_battle: the battle did not end");
        if (tactic != UI_TACTIC_HOLD)
            think(&plan, tactic);
        play_wait(THINK);
        if ((play_frame() - start) % 200 < THINK)
            play_dump("ground");
    }
    if (!ui_wait_idle(2000))
        play_fail("ui_ground_battle: no result screen");
    bool won = ui_battle_won();
    UiLosses l;
    ui_battle_losses(&l);
    if (losses)
        *losses = l;
    play_log("ground battle %s after %llu retraces: lost %dTr %dT %dAir %dML, destroyed %dTr %dT %dAir %dML",
             won ? "won" : "lost", (unsigned long long)(play_frame() - start), l.own[0], l.own[1], l.own[2],
             l.own[3], l.enemy[0], l.enemy[1], l.enemy[2], l.enemy[3]);
    play_dump("ground-result");
    if (!ui_toolbar_command(CMD_END_BATTLE))
        play_fail("ui_ground_battle: no END BATTLE");
    return won;
}
