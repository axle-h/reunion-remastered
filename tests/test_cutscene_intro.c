// INTRO.PRG smoke test: the whole program headless (about 10 minutes of
// retraces), a key skipping blocks and ESC ending it. Needs the game data.
#include "cutscene_test.h"

int main(int argc, char *argv[])
{
    cst_init(argc, argv, "INTRO");
    cst_check_counts(&cs_intro, "GRWAR\\INTRO.PRG", 0x0e62, 0x41);

    // The alien face, a 640x480 still, the fly-by, the rebel base monitor,
    // Gabor at the end.
    uint64_t frames;
    CHECK(cst_run(CUTSCENE_INTRO,
                  "300:dump:intro_300.bmp;2000:dump:intro_2000.bmp;18000:dump:intro_18000.bmp;"
                  "42000:dump:intro_42000.bmp",
                  &frames));
    printf("intro: %llu frames\n", (unsigned long long)frames);
    CHECK(frames > 41000 && frames < 44500);
    int w = 0, lit;
    lit = cst_lit_percent("intro", 300, &w);
    printf("face %d%% lit\n", lit);
    CHECK(w == 320 && lit > 10);
    lit = cst_lit_percent("intro", 2000, &w);
    printf("still %d%% lit\n", lit);
    CHECK(w == 640 && lit > 50);
    lit = cst_lit_percent("intro", 18000, &w);
    printf("fly-by %d%% lit\n", lit);
    CHECK(w == 320 && lit > 50);
    lit = cst_lit_percent("intro", 42000, &w);
    printf("Gabor %d%% lit\n", lit);
    CHECK(w == 320 && lit > 50);

    // A key skips to the next block that resets the skip flag (INTRO1-4 go
    // together, REBEL starts with the stars); ESC ends the program.
    CHECK(cst_run(CUTSCENE_INTRO, "400:key:32;1000:dump:intro_1000.bmp;1100:key:27", &frames));
    printf("keys: %llu frames\n", (unsigned long long)frames);
    CHECK(frames > 1100 && frames < 1300);
    lit = cst_lit_percent("intro", 1000, &w);
    printf("REBEL after a key: %d%% lit\n", lit);
    CHECK(w == 320 && lit > 5);

    CHECK(!cst_run(CUTSCENE_INTRO, "100:quit", &frames));
    CHECK(frames <= 101);

    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
