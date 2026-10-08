// Differential test of unit Title (321d): SetMainMenuPalette. HeroSelect,
// StartMenu and InitTitleAndMainMenu (which runs StartMenu) wait for a mouse
// click, which the original reads from Mouse::BtnDown as set by the INT 33h
// callback the harness does not run, so they are not tested.
// See docs/difftest.md. Needs the game data.
#include <stdio.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/title.h"

DIFF_PROC(Title_SetMainMenuPalette, Title_SetMainMenuPalette());

static void setup_palette(DiffRng *r, int32_t *a)
{
    (void)a;
    for (int i = 0; i < 256; i++)
        for (int c = 0; c < 3; c++)
            if (diff_chance(r, 50))
                ds.Gfx_Palette[i][c] = (uint8_t)diff_rand(r, 64);
}

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    if (!diff_boot_new_game(1500)) {
        printf("FAILED to boot\n");
        return 1;
    }
    DiffState *base = diff_save();
    diff_run(&diff_Title_SetMainMenuPalette, &base, 1, setup_palette, diff_cases(8));
    diff_free(base);
    return diff_finish();
}
