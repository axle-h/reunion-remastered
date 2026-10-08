// dseg: the data segment mirror, initialised from GRWAR\REUNION.PRG.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/dseg.h"
#include "platform/fs.h"

static bool ok = true;

static void check(bool good, const char *what)
{
    if (!good)
        printf("FAILED %s\n", what);
    ok = ok && good;
}

// Compares a Pascal string (length byte + chars) with a C string.
static bool pstr_is(const void *pstr, const char *want)
{
    const uint8_t *p = pstr;
    return p[0] == SDL_strlen(want) && SDL_memcmp(p + 1, want, p[0]) == 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    mem_init();
    check(dseg_init(), "dseg_init");

    check(pstr_is(&ds.Galaxy_SunNames[0].Name, "Mercury"), "SunNames[1]");
    // Far pointers in the typed constants are relocated like DOS does.
    check((ds.Galaxy_Bodies[0] >> 16) == MEM_DS_SEG, "Galaxy_Bodies relocated");
    check(far_host(ds.Galaxy_Bodies[7]) == (void *)&ds.Galaxy_SunBodies, "Bodies[8] -> SunBodies");
    check(pstr_is(&ds.Galaxy_SunNames[2].Name, "Earth"), "SunNames[3]");
    static const int16_t planets[8] = {7, 5, 5, 3, 3, 7, 2, 8};
    check(SDL_memcmp(ds.Galaxy_PlanetCount, planets, sizeof planets) == 0, "PlanetCount");

    const TBuildingType *cc = &ds.Surface_BuildingTypes[0];
    check(pstr_is(&cc->Name, "Command centre") && cc->Cost == 100000, "BuildingTypes[1]");

    check(pstr_is(&ds.Staff_Names[0][0], "Bruce Jordan"), "Staff::Names[1,1]");
    check(pstr_is(&ds.Staff_Names[0][2], "Julie-Anne Roxford"), "Staff::Names[1,3]");
    check(pstr_is(&ds.Staff_Names[3][2], "Klaatoo"), "Staff::Names[4,3]");
    check(pstr_is(&ds.Names_Races[11], "Earthling"), "Names::Races[12]");

    // Past the typed constants everything starts zeroed (System's ClearBss).
    const uint8_t *raw = (const uint8_t *)&ds;
    bool zero = true;
    for (size_t i = DSEG_INIT_SIZE; i < sizeof ds; i++)
        zero = zero && raw[i] == 0;
    check(zero, "uninitialised part is zero");

    fs_shutdown();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
