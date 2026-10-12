// dseg: the data segment mirror, its field table (game/dsfields.c) and its
// initial values (game/dsinit.c).
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/dseg.h"
#include "game/dsfields.h"
#include "platform/fs.h"
#include "platform/video.h"

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

// Bytes a field takes in the port's DSeg (or record).
static uint32_t port_bytes(const DsField *f)
{
    if (f->flags & DSF_HOST)
        return (uint32_t)sizeof(void *) * f->count;
    if (f->rec >= 0)
        return (uint32_t)ds_records[f->rec].port_size * f->count;
    return (uint32_t)f->elem * f->count;
}

// The fields cover the original's bytes and the port's struct back to back,
// with nothing missing: dsfields.c is kept by hand alongside dseg.h, and
// game/state.c serialises only what the table lists.
static bool fields_cover(const DsField *f, int n, uint32_t size, uint32_t port_size)
{
    uint32_t ofs = 0, at = 0;
    for (int i = 0; i < n; i++) {
        if (f[i].ofs != ofs || f[i].at != at) {
            printf("field %s: at %04X/%04X, expected %04X/%04X\n", f[i].name, f[i].ofs,
                   f[i].at, (unsigned)ofs, (unsigned)at);
            return false;
        }
        ofs += (uint32_t)f[i].elem * f[i].count;
        at += port_bytes(&f[i]);
    }
    return ofs == size && at == port_size;
}

int main(int argc, char *argv[])
{
    check(fields_cover(ds_fields, ds_field_count, DSEG_SIZE, sizeof(DSeg)), "DS field table");
    for (int r = 0; r < ds_record_count; r++) {
        const DsRecord *rec = &ds_records[r];
        if (!fields_cover(rec->fields, rec->nfields, rec->size, rec->port_size))
            check(false, rec->name);
    }
    if (!ok)
        return 1;

    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    dseg_clear();
    vid_clear();
    dseg_init();

    check(pstr_is(&ds.Galaxy_SunNames[0].Name, "Mercury"), "SunNames[1]");
    // Far pointers in the typed constants are relocated like DOS does, here
    // into host pointers into DS.
    check(ds.Galaxy_Bodies[0] == &ds.Galaxy_AmnestyBodies[0], "Bodies[1] -> AmnestyBodies");
    check(ds.Galaxy_Bodies[7] == &ds.Galaxy_SunBodies[0], "Bodies[8] -> SunBodies");
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
    for (size_t i = (size_t)(dseg_orig(DSEG_INIT_SIZE) - raw); i < sizeof ds; i++)
        zero = zero && raw[i] == 0;
    check(zero, "uninitialised part is zero");

    fs_shutdown();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
