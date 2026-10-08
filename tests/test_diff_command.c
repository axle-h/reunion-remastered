// Differential test of unit Command (3abd): the command centre's screen
// handler, its drawing, the room / ground-battle animations, the panel
// slides and the ambient animations, original (under Unicorn) against the
// port, from a new game's command centre. See docs/difftest.md. Needs the
// game data.
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/anim.h"
#include "game/command.h"
#include "game/dseg.h"
#include "game/system.h"

DIFF_PROC(Command_HandleCommandCentre, Command_HandleCommandCentre());
DIFF_PROC(Command_DrawCommandBackdrop, Command_DrawCommandBackdrop());
DIFF_PROC(Command_LoadCommandGfx, Command_LoadCommandGfx());
DIFF_PROC(Command_ShowHologram, Command_ShowHologram((int16_t)a[0]));
DIFF_PROC(Command_SetupCommandHotspots, Command_SetupCommandHotspots());
DIFF_PROC(Command_InitRoomAnim, Command_InitRoomAnim());
DIFF_PROC(Command_FreeRoomAnim, Command_FreeRoomAnim());
DIFF_PROC(Command_StartMainAnim, Command_StartMainAnim((int16_t)a[0]));
DIFF_PROC(Command_StopMainAnim, Command_StopMainAnim());
DIFF_PROC(Command_TickRoomAnim, Command_TickRoomAnim());
DIFF_PROC(Command_StepMainAnim, Command_StepMainAnim());
DIFF_PROC(Command_FreeGrRoomAnim, Command_FreeGrRoomAnim());
DIFF_PROC(Command_StopGrAnim, Command_StopGrAnim());
DIFF_PROC(Command_TickGrAnim, Command_TickGrAnim());
DIFF_PROC(Command_OpenPanelMaina1, Command_OpenPanelMaina1());
DIFF_PROC(Command_OpenPanelMaina2, Command_OpenPanelMaina2());
DIFF_PROC(Command_OpenPanelMaina3And4, Command_OpenPanelMaina3And4());
DIFF_PROC(Command_TickPanelDoorAnim, Command_TickPanelDoorAnim());
DIFF_PROC(Command_TickAmbientLights, Command_TickAmbientLights());
DIFF_PROC(Command_DrawHeroPortrait, Command_DrawHeroPortrait());
DIFF_PROC(Command_DrawHeroPortraitToBuffer, Command_DrawHeroPortraitToBuffer());
DIFF_PROC(Command_DrawStaffIcon, Command_DrawStaffIcon((int16_t)a[0]));
DIFF_PROC(Command_DrawAllStaffIcons, Command_DrawAllStaffIcons());
DIFF_PROC(Command_ShowDeathEnding, Command_ShowDeathEnding());
DIFF_PROC(Command_FadeOutAndHalt, Command_FadeOutAndHalt());

static int16_t *w16(uint16_t off) { return (int16_t *)ds_at(off); }
static uint8_t *b8(uint16_t off) { return (uint8_t *)ds_at(off); }
static FarPtr *fp(uint16_t off) { return (FarPtr *)ds_at(off); }

#define ANIM_ID (*w16(0x7E2A))
#define ANIM_FRAME (*w16(0x7E2C))
#define ANIM_DELAY (*w16(0x7E2E))
#define ANIM_TICK (*w16(0x7E30))
#define ANIM_REPEAT (*w16(0x7E32))
#define ANIM_SPEED (*w16(0x7E34))
#define GR_BUF (*fp(0x7E36))
#define ANIM_REC (*fp(0x7E40))

static uint8_t room_frames(int id) { return *b8((uint16_t)(0x55D7 + 7 * id)); }
static uint8_t gr_frames(int id) { return *b8((uint16_t)(0x5651 + 5 * id)); }

static void seed(DiffRng *r)
{
    ds.System_RandSeed = (int32_t)diff_rand(r, 0xFFFFFFFFu);
}

// Hired staff (Game::StaffHired[1..4]), the one at the university (5D9E) and
// the developer's block (5D52): which icons and hotspots exist.
static void staff(DiffRng *r)
{
    for (int k = 0; k < 4; k++)
        ds.Game_StaffHired[k] = (uint16_t)diff_pick(r, 0, 1, 2, 3, diff_range(r, 1, 3));
    *w16(0x5D9E) = (int16_t)diff_pick(r, 0, 0, 0, 1, 2, 3, 4);
    *w16(0x5D52) = (int16_t)diff_pick(r, 0, 0, 0, 3);
}

// Room animations that exist (no MAIN7) and fit in the heap.
static int pick_room(DiffRng *r)
{
    return diff_pick(r, 1, 2, 3, 4, 5, 6, diff_range(r, 1, 6), 8, 11, 12, 13);
}

// The animation record holds animation `id`'s frames (loaded = 1) or nothing.
static void room_anim(DiffRng *r, int id)
{
    ANIM_ID = (int16_t)id;
    *(uint8_t *)far_host(ANIM_REC) = 0;
    if (id > 0 && diff_chance(r, 60))
        Anim_LoadMainAnim((uint16_t)id);
}

// --- The screen handler ---------------------------------------------------------------

static void setup_handle(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    staff(r);
    ds.Game_ScreenEntered = (uint8_t)diff_pick(r, 0, 0, 0, 1);
    *b8(0x7E29) = (uint8_t)diff_pick(r, 0, 0, 1);  // hologram on entry
    *b8(0x7E3A) = (uint8_t)diff_range(r, 0, 1);    // backdrop shown before
    ds.Game_HotspotHit = (int16_t)diff_pick(r, 0, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A,
                                            0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, diff_range(r, 0x14, 0x20));
    for (int k = 0; k < 4; k++)
        *w16((uint16_t)(0x89A2 + 2 * k)) = (int16_t)diff_range(r, 0, 4);  // staff of hotspot 1..4
}

static void setup_backdrop(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    staff(r);
    *b8(0x7E3A) = (uint8_t)diff_range(r, 0, 1);
    ds.Game_HeroSpriteX = (int16_t)diff_pick(r, 0x45, 1);
}

static void setup_hologram(DiffRng *r, int32_t *a)
{
    seed(r);
    staff(r);
    ds.Game_Hero = (int16_t)diff_range(r, 1, 2);
    ds.Game_HeroSpriteX = ds.Game_Hero == 1 ? 0x45 : 1;
    a[0] = diff_pick(r, 9, 9, diff_range(r, 1, 35));
}

static void setup_hotspots(DiffRng *r, int32_t *a)
{
    (void)a;
    staff(r);
    for (int k = 0; k < 5; k++)
        *w16((uint16_t)(0x89A0 + 2 * k)) = (int16_t)diff_range(r, 0, 9);
}

// --- Room animation ------------------------------------------------------------------

static void setup_init_room(DiffRng *r, int32_t *a)
{
    (void)a;
    ANIM_ID = (int16_t)diff_range(r, 0, 6);
    ANIM_FRAME = (int16_t)diff_range(r, 0, 9);
    ANIM_TICK = (int16_t)diff_range(r, 0, 9);
}

static void setup_free_room(DiffRng *r, int32_t *a)
{
    (void)a;
    room_anim(r, diff_pick(r, 0, pick_room(r)));
}

static void setup_start(DiffRng *r, int32_t *a)
{
    seed(r);
    room_anim(r, diff_pick(r, 0, pick_room(r)));
    a[0] = diff_pick(r, 1, 2, 6, 11, 12, 13, diff_range(r, 1, 16));
}

static void setup_tick_room(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    staff(r);
    int id = diff_pick(r, 0, pick_room(r), pick_room(r), pick_room(r));
    room_anim(r, id);
    int n = id ? room_frames(id) : 3;
    ANIM_FRAME = (int16_t)diff_pick(r, 0, 1, n - 1, n, diff_range(r, 0, n));
    ANIM_DELAY = (int16_t)diff_pick(r, 0, 0, 0, 1, 50);
}

// --- Ground-battle animation --------------------------------------------------------------

static void setup_gr(DiffRng *r, bool loaded)
{
    seed(r);
    int id = diff_pick(r, 0, diff_range(r, 1, 14), diff_range(r, 1, 14));
    ANIM_ID = (int16_t)id;
    *(uint8_t *)far_host(ANIM_REC) = 0;
    if (id > 0 && loaded)
        Anim_LoadGrAnim((uint16_t)id);
}

static void setup_free_gr(DiffRng *r, int32_t *a)
{
    (void)a;
    setup_gr(r, diff_chance(r, 70));
}

static void setup_tick_gr(DiffRng *r, int32_t *a)
{
    (void)a;
    setup_gr(r, diff_chance(r, 50));
    GR_BUF = System_GetMem(58 * 80 + 0x100);
    int n = ANIM_ID ? gr_frames(ANIM_ID) : 3;
    ANIM_TICK = (int16_t)diff_pick(r, 0, 1, 1, 2);
    ANIM_SPEED = (int16_t)diff_range(r, 1, 4);
    ANIM_FRAME = (int16_t)diff_pick(r, 0, 1, n - 1, n, diff_range(r, 0, n));
    ANIM_REPEAT = (int16_t)diff_pick(r, 0, 1, 1, 2, 5);
    // A missing (nil) next frame ends the animation.
    uint8_t *rec = far_host(ANIM_REC);
    if (rec[0] && ANIM_FRAME + 1 <= n && diff_chance(r, 30))
        memset(rec + 1 + ANIM_FRAME * 4, 0, 4);
    // Both sides' units of every type on the field (Game::GrWarProc's table),
    // so the next animation is found.
    for (int side = 1; side <= 2; side++)
        for (int t = 0; t < 4; t++)
            *(int32_t *)ds_at((uint16_t)(0x7646 + 16 * side + 4 * t)) = diff_range(r, 0, 3) + (t == 1);
    for (int k = 0; k < 15; k++)  // the unit types each animation shows: 1 (all present)
        *b8((uint16_t)(0x5654 + 5 * k)) = *b8((uint16_t)(0x5655 + 5 * k)) = 1;
    ds.Game_GrWarProc = far_ptr(0x1537, 0x11F2);
}

// --- Panels, door, lights ----------------------------------------------------------------------

static void setup_panel(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    staff(r);
    *b8(0x7E28) = (uint8_t)diff_range(r, 0, 1);  // door direction
    *w16(0x7E20) = (int16_t)diff_range(r, 1, 30);
    *w16(0x7E22) = (int16_t)diff_range(r, 1, 4);
    *w16(0x7E14) = (int16_t)diff_range(r, 0, 0x25);
    *w16(0x7E16) = (int16_t)diff_range(r, 0, 5);
    int id = diff_pick(r, 0, pick_room(r));
    room_anim(r, id);
    ANIM_FRAME = 1;
    ANIM_DELAY = (int16_t)diff_pick(r, 0, 1, 100);
}

static void setup_door(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    staff(r);
    *b8(0x7E28) = (uint8_t)diff_range(r, 0, 1);
    *w16(0x7E20) = (int16_t)diff_pick(r, 1, 1, 2, 8, 15, 16, 17, diff_range(r, 1, 300));
    *w16(0x7E22) = (int16_t)diff_pick(r, 0, 1, 1, 2, 4);
    int id = diff_pick(r, 0, pick_room(r));
    room_anim(r, id);
    int n = id ? room_frames(id) : 3;
    ANIM_FRAME = (int16_t)diff_range(r, 0, n);
    ANIM_DELAY = (int16_t)diff_pick(r, 0, 0, 1, 100);
}

static void setup_lights(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    *w16(0x7E14) = (int16_t)diff_pick(r, 0, 0x24, 0x25, diff_range(r, 0, 0x25));
    *w16(0x7E16) = (int16_t)diff_pick(r, 0, 1, 2, 5);
    *w16(0x7E1C) = (int16_t)diff_pick(r, 0, 8, 9, diff_range(r, 0, 9));
    *w16(0x7E1E) = (int16_t)diff_pick(r, 0, 1, 2, 7);
    for (int i = 0xC; i <= 0x13; i++)  // blink counters
        *b8((uint16_t)(0x5525 + 3 * i)) = (uint8_t)diff_pick(r, 1, 2, 0x65, 0x66, 0x67, 0, diff_rand(r, 256));
}

// --- Portraits and icons ------------------------------------------------------------------------

static void setup_portrait(DiffRng *r, int32_t *a)
{
    (void)a;
    ds.Game_HeroSpriteX = (int16_t)diff_pick(r, 0x45, 1);
    *fp(0x7E10) = diff_chance(r, 50) ? far_ptr(ds.Gfx_ScreenSeg, 0) : System_GetMem(64000);
}

static void setup_icon(DiffRng *r, int32_t *a)
{
    staff(r);
    a[0] = diff_range(r, 1, 4);
}

static void setup_icons(DiffRng *r, int32_t *a)
{
    (void)a;
    staff(r);
}

// --- Endings ----------------------------------------------------------------------------------------

static void setup_death(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    ds.Game_PrevScreen = (int16_t)diff_pick(r, 0x20, 0x21);
    ds.Game_Hero = (int16_t)diff_range(r, 1, 2);
}

// The original's file variable of the graphics data file (TP FileRec, used
// by the .ANI loaders): the port keeps its open file on the host side.
static const DiffRange file_rec[] = {DIFF_DS(0x91EE, 0x80), {0, 0}};
// The same, and Mouse::HotspotUnder: on entry the handler sets up new
// hotspots, and the port's wait loops call Mouse_Update (as the mouse
// driver's callback would), which finds the one under the pointer; the
// emulator has no callback.
static const DiffRange handle_ignore[] = {DIFF_DS(0x91EE, 0x80), DIFF_DS(0x9192, 1), {0, 0}};

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    if (!diff_boot("1500:quit")) {
        printf("FAILED to reach the command centre\n");
        return 1;
    }
    DiffState *base = diff_save();

    const struct {
        DiffFn *fn;
        DiffSetup setup;
        int cases;
    } fns[] = {
        {&diff_Command_HandleCommandCentre, setup_handle, 40},
        {&diff_Command_DrawCommandBackdrop, setup_backdrop, 6},
        {&diff_Command_LoadCommandGfx, setup_backdrop, 6},
        {&diff_Command_ShowHologram, setup_hologram, 6},
        {&diff_Command_SetupCommandHotspots, setup_hotspots, 30},
        {&diff_Command_InitRoomAnim, setup_init_room, 5},
        {&diff_Command_FreeRoomAnim, setup_free_room, 10},
        {&diff_Command_StartMainAnim, setup_start, 20},
        {&diff_Command_StopMainAnim, setup_free_room, 15},
        {&diff_Command_TickRoomAnim, setup_tick_room, 40},
        {&diff_Command_StepMainAnim, setup_tick_room, 30},
        {&diff_Command_FreeGrRoomAnim, setup_free_gr, 10},
        {&diff_Command_StopGrAnim, setup_free_gr, 15},
        {&diff_Command_TickGrAnim, setup_tick_gr, 40},
        {&diff_Command_OpenPanelMaina1, setup_panel, 4},
        {&diff_Command_OpenPanelMaina2, setup_panel, 4},
        {&diff_Command_OpenPanelMaina3And4, setup_panel, 6},
        {&diff_Command_TickPanelDoorAnim, setup_door, 40},
        {&diff_Command_TickAmbientLights, setup_lights, 30},
        {&diff_Command_DrawHeroPortrait, setup_portrait, 4},
        {&diff_Command_DrawHeroPortraitToBuffer, setup_portrait, 4},
        {&diff_Command_DrawStaffIcon, setup_icon, 30},
        {&diff_Command_DrawAllStaffIcons, setup_icons, 20},
        {&diff_Command_ShowDeathEnding, setup_death, 4},
        {&diff_Command_FadeOutAndHalt, setup_icons, 2},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        fns[i].fn->ignore = file_rec;
    diff_Command_HandleCommandCentre.ignore = handle_ignore;
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, &base, 1, fns[i].setup, diff_cases(fns[i].cases));
    diff_free(base);
    return diff_finish();
}
