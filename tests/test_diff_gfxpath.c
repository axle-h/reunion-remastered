// Differential test of unit GfxPath (3a1b): the 12-icon toolbar (set, redraw,
// add, scroll, show, query), the icon index and the screen backdrops, from
// several screens. DrawIcon's prototype is not in symbols.txt; the toolbar
// routines cover it. See docs/difftest.md. Needs the game data.
#include <stdio.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/gfxpath.h"

#define I(i) ((int16_t)a[i])

DIFF_PROC(GfxPath_WaitVRetrace, GfxPath_WaitVRetrace());
DIFF_PROC(GfxPath_SetToolbarAndShow, GfxPath_SetToolbarAndShow(I(0)));
DIFF_PROC(GfxPath_SetToolbar, GfxPath_SetToolbar(I(0)));
DIFF_PROC(GfxPath_RedrawToolbar, GfxPath_RedrawToolbar());
DIFF_PROC(GfxPath_AddToolbarIcon, GfxPath_AddToolbarIcon(I(0)));
DIFF_PROC(GfxPath_ScrollToolbarNext, GfxPath_ScrollToolbarNext());
DIFF_PROC(GfxPath_ScrollToolbarPrev, GfxPath_ScrollToolbarPrev());
DIFF_PROC(GfxPath_ShowToolbar, GfxPath_ShowToolbar());
DIFF_PROC(GfxPath_ShowToolbarInstant, GfxPath_ShowToolbarInstant());
DIFF_FUNC(GfxPath_ToolbarHasIcon, GfxPath_ToolbarHasIcon(I(0)));
DIFF_PROC(GfxPath_IconAllIndex, GfxPath_IconAllIndex());
DIFF_PROC(GfxPath_ShowScreenBackdrop, GfxPath_ShowScreenBackdrop());
DIFF_PROC(GfxPath_FreeToolbarBuffer, GfxPath_FreeToolbarBuffer());

// The slot ids (DS:7C64 + 21 * slot, 1..12) and the page flags.
static int16_t *slot_id(int slot)
{
    return (int16_t *)ds_at((uint16_t)(0x7C64 + slot * 21));
}

// Some slots filled with random ids (the used ones first, as the game keeps
// them), the rest zero.
static void random_slots(DiffRng *r)
{
    if (!diff_chance(r, 60))
        return;
    int used = diff_pick(r, 0, 1, 5, 6, 7, 11, 12, diff_range(r, 0, 12));
    for (int s = 1; s <= 12; s++)
        *slot_id(s) = (int16_t)(s <= used ? diff_range(r, 1, 77) : 0);
}

static void random_pages(DiffRng *r)
{
    *(uint8_t *)ds_at(0x7C62) = (uint8_t)diff_range(r, 0, 1);
    *(uint8_t *)ds_at(0x7C63) = (uint8_t)diff_range(r, 0, 1);
}

static void setup_screen(DiffRng *r, int32_t *a)
{
    random_pages(r);
    a[0] = diff_pick(r, 1, 1, 2, 3, 5, 0x0C, 0x26, diff_range(r, 1, 38));
    ds.Game_HotspotHit = (int16_t)diff_range(r, 0, 30);
}

static void setup_redraw(DiffRng *r, int32_t *a)
{
    (void)a;
    random_slots(r);
    random_pages(r);
}

static void setup_add(DiffRng *r, int32_t *a)
{
    random_slots(r);
    random_pages(r);
    if (*slot_id(12) != 0)  // a free slot (the original scans on past 12)
        *slot_id(diff_range(r, 1, 12)) = 0;
    for (int s = 1; s <= 12; s++)  // the used ones first
        if (*slot_id(s) == 0)
            for (int t = s + 1; t <= 12; t++)
                *slot_id(t) = 0;
    a[0] = diff_pick(r, 0x10, 0x26, 0x27, diff_range(r, 1, 77));
}

static void setup_scroll(DiffRng *r, int32_t *a)
{
    (void)a;
    random_pages(r);
    *(uint8_t *)ds_at(0x7C64) = (uint8_t)diff_range(r, 0, 1);  // toggled with the pages
}

static void setup_has(DiffRng *r, int32_t *a)
{
    random_slots(r);
    a[0] = diff_chance(r, 50) ? *slot_id(diff_range(r, 1, 12)) : diff_range(r, 0, 77);
}

static void setup_backdrop(DiffRng *r, int32_t *a)
{
    (void)a;
    // Screens whose record names a picture.
    ds.Game_CurrentScreen = (int16_t)diff_pick(r, 1, 3, 5, 7, 11, 16, 20, diff_range(r, 1, 12));
}

// Base states: the command centre (screen 1 ticks its lights while the bar
// scrolls), research, shop, the system view, the message log.
static const char *const scripts[] = {"1500:quit", "900:click:72,16;1300:quit", "900:click:24,16;1300:quit",
                                      "900:click:168,16;1300:quit", "900:click:264,16;1300:quit"};
enum { STATES = sizeof scripts / sizeof scripts[0] };

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    DiffState *bases[STATES];
    for (int i = 0; i < STATES; i++) {
        if (!diff_boot(scripts[i]) || !diff_open_output()) {  // "Cannot Open" exits write to Output
            printf("FAILED to boot %s\n", scripts[i]);
            return 1;
        }
        bases[i] = diff_save();
    }
    // ICON.ALL's file variable (DS:91EE): the port keeps the file on the host.
    static const DiffRange file_rec[] = {DIFF_DS(0x91EE, 0x80), {0, 0}};
    diff_GfxPath_IconAllIndex.ignore = file_rec;
    diff_GfxPath_WaitVRetrace.near = true;  // "NEAR copy" in symbols.txt

    const struct {
        DiffFn *fn;
        DiffSetup setup;
        int cases;
    } fns[] = {
        {&diff_GfxPath_WaitVRetrace, nullptr, 2},
        {&diff_GfxPath_SetToolbar, setup_screen, 40},
        {&diff_GfxPath_SetToolbarAndShow, setup_screen, 20},
        {&diff_GfxPath_RedrawToolbar, setup_redraw, 30},
        {&diff_GfxPath_AddToolbarIcon, setup_add, 40},
        {&diff_GfxPath_ScrollToolbarNext, setup_scroll, 10},
        {&diff_GfxPath_ScrollToolbarPrev, setup_scroll, 10},
        {&diff_GfxPath_ShowToolbar, setup_scroll, 10},
        {&diff_GfxPath_ShowToolbarInstant, setup_scroll, 5},
        {&diff_GfxPath_ToolbarHasIcon, setup_has, 30},
        {&diff_GfxPath_IconAllIndex, nullptr, 5},
        {&diff_GfxPath_ShowScreenBackdrop, setup_backdrop, 10},
        {&diff_GfxPath_FreeToolbarBuffer, nullptr, 5},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, bases, STATES, fns[i].setup, diff_cases(fns[i].cases));
    for (int i = 0; i < STATES; i++)
        diff_free(bases[i]);
    return diff_finish();
}
