// CREDITS.PRG smoke test: the whole program headless (about 4.7 minutes of
// retraces), a key ending it early, and a window close. Needs the game data.
#include "cutscene_test.h"

int main(int argc, char *argv[])
{
    cst_init(argc, argv, "CREDITS");
    cst_check_counts(&cs_credits, "GRWAR\\CREDITS.PRG", 0x0ce7, 0x1b);

    // Logo (mode 13h), the 640x480 still, the roll; the program ends with
    // the song (37 patterns).
    uint64_t frames;
    CHECK(cst_run(CUTSCENE_CREDITS,
                  "500:dump:credits_500.bmp;1500:dump:credits_1500.bmp;"
                  "4500:dump:credits_4500.bmp",
                  &frames));
    printf("credits: %llu frames\n", (unsigned long long)frames);
    CHECK(frames > 19000 && frames < 21000);
    int w = 0, lit;
    lit = cst_lit_percent("credits", 500, &w);
    printf("logo %d%% lit\n", lit);
    CHECK(w == 320 && lit > 2);
    lit = cst_lit_percent("credits", 1500, &w);
    printf("still %d%% lit\n", lit);
    CHECK(w == 640 && lit > 50);
    lit = cst_lit_percent("credits", 4500, &w);
    printf("roll %d%% lit\n", lit);
    CHECK(w == 320 && lit > 10);

    // Any key while waiting for a cue ends the program.
    CHECK(cst_run(CUTSCENE_CREDITS, "100:key:32", &frames));
    printf("key: %llu frames\n", (unsigned long long)frames);
    CHECK(frames < 1000);

    // Closing the window.
    CHECK(!cst_run(CUTSCENE_CREDITS, "100:quit", &frames));
    CHECK(frames <= 101);

    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
