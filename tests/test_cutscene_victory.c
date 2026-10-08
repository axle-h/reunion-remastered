// VICTORY.PRG smoke test: the whole program headless, keys being ignored, and
// a window close. Needs the game data.
#include "cutscene_test.h"

int main(int argc, char *argv[])
{
    cst_init(argc, argv, "VICTORY");
    cst_check_counts(&cs_victory, "GRWAR\\VICTORY.PRG", 0x0d3c, -5);

    // "Congratulations", the soldiers in colour, a credits page; keys pressed
    // on the way change nothing.
    uint64_t frames;
    CHECK(cst_run(CUTSCENE_VICTORY,
                  "150:dump:victory_150.bmp;300:key:27;1800:dump:victory_1800.bmp;"
                  "4400:dump:victory_4400.bmp",
                  &frames));
    printf("victory: %llu frames\n", (unsigned long long)frames);
    CHECK(frames > 5800 && frames < 6600);
    int w = 0, lit;
    lit = cst_lit_percent("victory", 150, &w);
    printf("text %d%% lit\n", lit);
    CHECK(w == 320 && lit > 0 && lit < 20);
    lit = cst_lit_percent("victory", 1800, &w);
    printf("soldiers %d%% lit\n", lit);
    CHECK(w == 320 && lit > 80);
    lit = cst_lit_percent("victory", 4400, &w);
    printf("credits page %d%% lit\n", lit);
    CHECK(w == 320 && lit > 0 && lit < 30);

    CHECK(!cst_run(CUTSCENE_VICTORY, "100:quit", &frames));
    CHECK(frames <= 101);

    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
