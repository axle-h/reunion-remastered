// Improvement (game/improvements.h, university_courses): game/courses.h with
// the advisor, the hourly return and saves. No course without a finished,
// powered University; New Earth's lets one go; two let two staff go
// together and refuse a third; each returns with the original's results,
// the DOS fields taking over a port record's course as theirs ends; away
// staff are gone from the command centre; a save with two courses
// round-trips in the port's format (one of version 1 loads without them)
// and one with one course stays in the DOS format; a replaced staff
// member's course ends; a running course carries on without its
// University; with the switch off it is the original's one course,
// University or not. Needs the game data.
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "game/advisor.h"
#include "game/command.h"
#include "game/courses.h"
#include "game/dseg.h"
#include "game/gfx.h"
#include "game/hire.h"
#include "game/improvements.h"
#include "game/lang.h"
#include "game/main_program.h"
#include "game/message.h"
#include "game/mouse.h"
#include "game/rawtext.h"
#include "game/saveload.h"
#include "game/sim.h"
#include "game/system.h"
#include "game/textfile.h"
#include "platform/fs.h"
#include "platform/input.h"
#include "platform/video.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

enum { PILOT = 1, BUILDER = 2, FIGHTER = 3, DEVELOPER = 4, UNIVERSITY = 13 };
enum { Q_UNIVERSITY = 5, Q_PAY = 6, Q_MATHS = 8 };  // rkerdes1.sp lines
enum { A_BUSY = 9, A_PRICE = 10, A_GONE = 0xD };    // valasz1.sp lines

static bool wait_retrace(void) { return true; }
// Message boxes wait for a key: answer at once. Each return's box is a
// log entry.
static bool key_pressed(void) { return true; }
static char read_key(void) { return '\r'; }

static char dir[128];

// New Earth's University, finished and powered in a new game (SAVE\INIT).
static TBuilding *start_university;

static void reset(bool on)
{
    dseg_clear();
    vid_clear();
    dseg_init();
    system_init();
    Game_InitGameData();
    Message_Init();
    SaveLoad_LoadInit();
    RawText_LoadSzFace();  // the staff's faces
    Courses_Reset();
    ds.Gfx_LineStride = 320;
    ds.Gfx_LineStrideCopy = 320;
    gfx_wait_retrace = wait_retrace;
    TextFile_KeyPressed = key_pressed;
    TextFile_ReadKey = read_key;
    ds.Message_LogCount = 0;
    improvements.university_courses = on;
    ds.Game_Credits = 1000000;
    ds.Game_StoryStage = 1;
    for (int k = 0; k < 4; k++) {
        ds.Game_StaffHired[k] = 1;
        ds.Game_StaffExp[k] = 10;
    }
    for (int k = 0; k < 4; k++)
        ds.Game_DevSkill[k] = 1;
    ds.System_RandSeed = 1234;
    start_university = nullptr;
    for (int i = 0; i < ds.Game_BuildingCount; i++)
        if (ds.Game_Buildings[i].Kind == UNIVERSITY)
            start_university = &ds.Game_Buildings[i];
}

// A University at New Earth, finished and powered unless said otherwise.
static TBuilding *add_university(bool finished, bool powered)
{
    TBuilding *b = &ds.Game_Buildings[ds.Game_BuildingCount++];
    *b = (TBuilding){.Kind = UNIVERSITY, .System = 1, .Planet = 5, .Moon = 0,
                     .BuildLeft = finished ? 0 : 100, .Powered = powered};
    return b;
}

// Asks question q of the menu on screen, as a click on its line; returns
// the answer.
static int16_t ask(int16_t q)
{
    int line = 0;
    for (int i = 1; i <= ds.Advisor_CurMenu.Count; i++)
        if (ds.Advisor_CurMenu.Items[i - 1] == q)
            line = i;
    CHECK(line != 0);
    if (line == 0)
        return -1;
    input_set_mouse(0x80, 0x54 + line * 9 + 4, 0, 0);
    ds.Game_ScreenEntered = false;
    ds.Game_HotspotHit = 0x15;
    Advisor_AdvisorScreen();
    return ds.Advisor_Answer;
}

// Talks to staff member `kind` on the advisor screen.
static void talk(int16_t kind)
{
    ds.Command_StaffPicked = kind;
    ds.Game_CurrentScreen = 24;
    ds.Game_ScreenEntered = true;
    ds.Game_HotspotHit = 0;
    Advisor_AdvisorScreen();
}

// Sends `kind` to the university (the developer to Maths & physics);
// true when the course is paid.
static bool send(int16_t kind, int16_t *answer)
{
    talk(kind);
    int16_t a = ask(Q_UNIVERSITY);
    if (kind == DEVELOPER && a == 0xE)
        a = ask(Q_MATHS);
    if (answer)
        *answer = a;
    if (a != A_PRICE)
        return false;
    int32_t before = ds.Game_Credits;
    a = ask(Q_PAY);
    return a == A_GONE && ds.Game_Credits < before;
}

static int16_t answer_of(int16_t kind)
{
    int16_t a = 0;
    send(kind, &a);
    return a;
}

// Hires the next candidate of staff kind `kind` (the hire button).
static void hire(int16_t kind)
{
    ds.Game_ScreenEntered = false;
    ds.Game_HotspotHit = 0;
    ds.Game_MenuCommand = 0x28;
    ds.Hire_StaffKind = kind;
    ds.Hire_Candidate = (int16_t)(ds.Game_StaffHired[kind - 1] + 1);
    Hire_HireScreen();
    CHECK(ds.Game_StaffHired[kind - 1] == (uint16_t)ds.Hire_Candidate);
}

static bool away_only(bool p, bool b, bool f, bool d)
{
    return Courses_Away(PILOT) == p && Courses_Away(BUILDER) == b && Courses_Away(FIGHTER) == f
           && Courses_Away(DEVELOPER) == d;
}

// Staff kind `kind`'s experience cap (Sim's ability_cap with hired 1).
static uint16_t cap_of(int16_t kind)
{
    return ds.Staff_Ability[kind - 1][0];
}

static bool slot_file(int16_t slot, const char *name)
{
    char path[256];
    SDL_snprintf(path, sizeof path, "%s/%s.%d", dir, name, slot);
    return SDL_GetPathInfo(path, nullptr);
}

static bool save_slot(int16_t slot)
{
    SaveLoad_ReadSlotNames();
    uint8_t *e = ds.SaveLoad_SlotTable + (slot - 1) * 0x16;
    memcpy(e + 1, "\x07" "courses", 8);
    ds.SaveLoad_SelectedSlot = slot;
    return SaveLoad_Transfer(true);
}

static bool load_slot(int16_t slot)
{
    SaveLoad_ReadSlotNames();
    ds.SaveLoad_SelectedSlot = slot;
    return SaveLoad_Transfer(false);
}

// No course without a finished, powered University; the advisor says so
// and nothing is paid.
static void no_university(void)
{
    reset(true);
    CHECK(start_university && Courses_Universities() == 1);
    start_university->Powered = 0;
    int16_t a = answer_of(PILOT);
    CHECK(a == ADVISOR_NO_UNIVERSITY);
    CHECK(ds.Advisor_MenuNumber == 1);  // back to the main menu
    CHECK(ds.Game_Credits == 1000000 && away_only(false, false, false, false));
    CHECK(answer_of(DEVELOPER) == ADVISOR_NO_UNIVERSITY);
    TBuilding *b = add_university(false, true);
    CHECK(answer_of(PILOT) == ADVISOR_NO_UNIVERSITY);  // still being built
    b->BuildLeft = 0;
    CHECK(send(PILOT, nullptr) && away_only(true, false, false, false));
    // A staff member at the cap is told so, as in the original.
    reset(true);
    ds.Game_StaffExp[BUILDER - 1] = cap_of(BUILDER);
    CHECK(answer_of(BUILDER) == 0xF);
}

// New Earth's University lets one go, as in the original. Two: two staff
// go together, a third is refused; a third University lets the developer
// go too. Each comes back with the original's results; the DOS fields hold
// one course throughout.
static void two_away(void)
{
    reset(true);
    CHECK(send(PILOT, nullptr));
    CHECK(answer_of(BUILDER) == A_BUSY && !Courses_Any());
    add_university(true, true);
    CHECK(ds.Advisor_TrainingKind == PILOT);
    CHECK(ds.Advisor_TrainingTimer >= 50 && ds.Advisor_TrainingTimer < 70);
    CHECK(send(BUILDER, nullptr));
    CHECK(ds.Advisor_TrainingKind == PILOT && Courses_Extra[BUILDER - 1].Hours >= 50
          && Courses_Extra[BUILDER - 1].Hours < 70);
    CHECK(away_only(true, true, false, false));
    int32_t credits = ds.Game_Credits;
    CHECK(answer_of(FIGHTER) == A_BUSY && answer_of(DEVELOPER) == A_BUSY);
    CHECK(ds.Game_Credits == credits && away_only(true, true, false, false));
    add_university(true, true);
    CHECK(send(DEVELOPER, nullptr));
    CHECK(Courses_Extra[DEVELOPER - 1].Course == 1);  // Maths & physics
    CHECK(away_only(true, true, false, true));
    CHECK(answer_of(FIGHTER) == A_BUSY);

    // Away staff are gone from the command centre.
    Command_SetupCommandHotspots();
    CHECK(ds.Command_StaffHotspotCount == 1 && ds.Command_StaffHotspotKind[0] == FIGHTER);
    // A save with three courses: the port's format, and back.
    CHECK(SaveLoad_PortFormat());
    CHECK(save_slot(3));
    CHECK(slot_file(3, "reunion") && !slot_file(3, "spidysav"));
    Course saved[COURSE_KINDS];
    memcpy(saved, Courses_Extra, sizeof saved);
    int16_t timer = ds.Advisor_TrainingTimer;
    Courses_Reset();
    ds.Advisor_TrainingKind = 0;
    CHECK(load_slot(3));
    CHECK(memcmp(saved, Courses_Extra, sizeof saved) == 0);
    CHECK(ds.Advisor_TrainingKind == PILOT && ds.Advisor_TrainingTimer == timer);

    // A save of version 1 (before the courses): the DOS fields' course only.
    char path[256];
    SDL_snprintf(path, sizeof path, "%s/reunion.3", dir);
    size_t size;
    uint8_t *file = SDL_LoadFile(path, &size);
    CHECK(file && size > 12 && file[8] == 2 && file[9] == 0);
    if (file && size > 12) {
        file[8] = 1;
        CHECK(SDL_SaveFile(path, file, size - 12));  // without the courses' table
    }
    SDL_free(file);
    CHECK(load_slot(3) && !Courses_Any() && ds.Advisor_TrainingKind == PILOT);
    memcpy(Courses_Extra, saved, sizeof saved);

    // The returns, at set hours: the pilot (DOS fields) after 3, the
    // developer after 4, the builder after 5. The builder's course moves
    // into the DOS fields as the pilot's ends.
    ds.Advisor_TrainingTimer = 3;
    Courses_Extra[BUILDER - 1].Hours = 5;
    Courses_Extra[DEVELOPER - 1].Hours = 4;
    for (int k = 0; k < 4; k++)
        ds.Game_StaffExp[k] = 10;
    for (int h = 1; h <= 2; h++)
        Sim_GrowStaffExperience();
    CHECK(away_only(true, true, false, true) && ds.Message_LogCount == 0);
    Sim_GrowStaffExperience();
    CHECK(away_only(false, true, false, true) && ds.Message_LogCount == 1);
    CHECK(ds.Game_StaffExp[PILOT - 1] == cap_of(PILOT));
    CHECK(ds.Advisor_TrainingKind == BUILDER && ds.Advisor_TrainingTimer == 2);
    CHECK(Courses_Extra[BUILDER - 1].Hours == 0);
    Sim_GrowStaffExperience();
    CHECK(away_only(false, true, false, false) && ds.Message_LogCount == 2);
    CHECK(ds.Game_StaffExp[DEVELOPER - 1] == cap_of(DEVELOPER));
    // Maths & physics: M+2 P+1 (under the stage-1 caps).
    CHECK(ds.Game_DevSkill[0] == 1 + ds.Advisor_CourseSkills[0][0]
          && ds.Game_DevSkill[1] == 1 + ds.Advisor_CourseSkills[0][1]
          && ds.Game_DevSkill[2] == 1 && ds.Game_DevSkill[3] == 1);
    CHECK(ds.Advisor_CourseSkills[0][0] == 2 && ds.Advisor_CourseSkills[0][1] == 1);

    // One course left, in the DOS fields: a save in the DOS format.
    CHECK(!Courses_Any() && !SaveLoad_PortFormat());
    CHECK(save_slot(4));
    CHECK(slot_file(4, "spidysav") && !slot_file(4, "reunion"));
    CHECK(load_slot(4) && ds.Advisor_TrainingKind == BUILDER && away_only(false, true, false, false));

    CHECK(ds.Advisor_TrainingTimer == 1);
    Sim_GrowStaffExperience();
    CHECK(away_only(false, false, false, false) && ds.Message_LogCount == 3);
    CHECK(ds.Game_StaffExp[BUILDER - 1] == cap_of(BUILDER));
}

// The original's results: one course, the same random stream and
// experience with the switch on and off.
static void same_results(void)
{
    uint16_t exp[2];
    uint32_t seed[2];
    for (int on = 0; on <= 1; on++) {
        reset(on);
        CHECK(send(FIGHTER, nullptr));
        ds.Game_StaffExp[FIGHTER - 1] = 10;
        ds.Staff_Ability[FIGHTER - 1][0] = 200;  // no cap in the way
        for (int h = 0; h < 70; h++)
            Sim_GrowStaffExperience();
        CHECK(!Courses_Away(FIGHTER));
        exp[on] = ds.Game_StaffExp[FIGHTER - 1];
        seed[on] = ds.System_RandSeed;
    }
    CHECK(exp[0] == exp[1] && exp[0] >= 10 + 0x69);
    CHECK(seed[0] == seed[1]);
}

// A replaced staff member's course ends, wherever it is kept; a running
// course carries on without its University.
static void hire_and_loss(void)
{
    reset(true);
    add_university(true, true);
    CHECK(send(PILOT, nullptr) && send(BUILDER, nullptr));
    start_university->Powered = 0;
    ds.Game_BuildingCount--;  // the new one gone
    CHECK(away_only(true, true, false, false));
    CHECK(answer_of(FIGHTER) == ADVISOR_NO_UNIVERSITY);
    for (int h = 0; h < 70; h++)
        Sim_GrowStaffExperience();
    CHECK(away_only(false, false, false, false) && ds.Message_LogCount == 2);
    CHECK(ds.Game_StaffExp[PILOT - 1] == cap_of(PILOT)
          && ds.Game_StaffExp[BUILDER - 1] == cap_of(BUILDER));

    reset(true);
    add_university(true, true);
    CHECK(send(PILOT, nullptr) && send(BUILDER, nullptr));
    hire(BUILDER);  // in a port record
    CHECK(away_only(true, false, false, false) && !Courses_Any());
    hire(PILOT);  // in the DOS fields
    CHECK(away_only(false, false, false, false));

    // The DOS fields' course replaced: a port record's moves in.
    reset(true);
    add_university(true, true);
    add_university(true, true);
    CHECK(send(PILOT, nullptr) && send(FIGHTER, nullptr));
    uint8_t hours = Courses_Extra[FIGHTER - 1].Hours;
    hire(PILOT);
    CHECK(away_only(false, false, true, false) && !Courses_Any());
    CHECK(ds.Advisor_TrainingKind == FIGHTER && ds.Advisor_TrainingTimer == hours);
}

// With the switch off: the original's one course, University or not.
static void switch_off(void)
{
    reset(false);
    start_university->Powered = 0;
    CHECK(send(PILOT, nullptr));
    add_university(true, true);
    add_university(true, true);
    CHECK(answer_of(BUILDER) == A_BUSY && away_only(true, false, false, false));
    CHECK(!Courses_Any() && !SaveLoad_PortFormat());
}

// The advisor's new answer in each language, each of its lines (split at
// '|') within the 49 characters of the original's longest.
static void translated(void)
{
    const char *english = "We have no University I could go to.";
    for (int l = LANGUAGE_ENGLISH; l <= LANGUAGE_ITALIAN; l++) {
        Lang_Set((Language)l);
        const char *t = Lang_Text(english);
        CHECK((l == LANGUAGE_ENGLISH) == (SDL_strcmp(t, english) == 0));
        for (const char *p = t; *p;) {
            size_t n = strcspn(p, "|");
            CHECK(n <= 49);
            p += n + (p[n] == '|');
        }
    }
    Lang_Set(LANGUAGE_ENGLISH);
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    // Never touch the real save directory.
    SDL_snprintf(dir, sizeof dir, "/tmp/reunion_test_courses_%d",
                 (int)SDL_GetCurrentTime(nullptr) & 0xFFFF);
    SaveLoad_SetSaveDir(dir);

    no_university();
    two_away();
    same_results();
    hire_and_loss();
    switch_off();
    translated();

    for (int n = 3; n <= 4; n++) {
        char p[256];
        SDL_snprintf(p, sizeof p, "%s/reunion.%d", dir, n);
        SDL_RemovePath(p);
        SDL_snprintf(p, sizeof p, "%s/spidysav.%d", dir, n);
        SDL_RemovePath(p);
    }
    SDL_RemovePath(dir);
    SaveLoad_SetSaveDir(nullptr);
    fs_shutdown();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
