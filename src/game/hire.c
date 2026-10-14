#include "game/hire.h"

#include <SDL3/SDL.h>

#include "game/blit.h"
#include "game/courses.h"
#include "game/gfx.h"
#include "game/lang.h"
#include "game/mouse.h"
#include "game/rawtext.h"
#include "game/sound.h"
#include "game/system.h"

static uint16_t hired(int16_t kind)
{
    return ds.Game_StaffHired[kind - 1];
}

// Developer skill bytes of candidate c (slot 0 of DevSkills; the hired
// developer's current skills are saved there when somebody else is hired).
static uint8_t *dev_skills(int16_t c)
{
    return &ds.Staff_DevSkills[0][c - 1][0];
}

HireResult Hire_CheckHire(int16_t kind, int16_t c)
{
    uint16_t h = hired(kind);
    // The hired candidate is compared unsigned with the sign-extended c.
    if (c < 0 || h >= (uint16_t)c)
        return (c >= 0 && h == (uint16_t)c) ? HIRE_ALREADY_HIRED : HIRE_NOT_SKILLED;
    if (ds.Staff_Price[kind - 1][c - 1] > ds.Game_Credits)
        return HIRE_NO_CREDITS;
    return HIRE_OK;
}

// 3fab:0795 / 0809 / 0874 / 08e2: the candidate's answer on the text lines.
static void say(const char *line)
{
    Gfx_TypeTextF9(line, 10, 0xA4, 0x32);
    Gfx_TypeTextF9("", 10, 0xAC, 0x32);
    Gfx_TypeTextF9("", 10, 0xB4, 0x32);
    Gfx_TypeTextF9("", 10, 0xBC, 0x32);
}

// 3fab:0032
void Hire_HireScreen(void)
{
    if (ds.Game_ScreenEntered) {
        ds.Hire_Candidate = 0;
        ds.Hire_StaffKind = 1;
        ds.Hire_InfoShown = 0;
        Hire_SetupHireHotspots();
        RawText_LoadSzFace();
    }
    if ((uint16_t)ds.Game_MenuCommand >= 0x32 && (uint16_t)ds.Game_MenuCommand <= 0x35) {
        int16_t kind = (int16_t)(ds.Game_MenuCommand - 0x31);
        if (ds.Hire_StaffKind != kind) {
            static const char *const sfx[4] = {"pilots", "builders", "fighters", "develope"};
            Sound_PlayButtonSfx(sfx[kind - 1]);
            Gfx_DrawStatusText("");
            ds.Hire_Candidate = 0;
            Hire_DrawCandidateFrames();
            Hire_DrawCandidateInfo();
            ds.Hire_StaffKind = kind;
            Hire_LoadHireBackground();
            ds.Hire_InfoShown = 0;
            Hire_SetupHireHotspots();
        } else {
            Sound_PlayButtonSfx("x");
        }
    }
    if (ds.Game_HotspotHit >= 0x15 && ds.Game_HotspotHit <= 0x17) {
        int16_t k = (int16_t)(ds.Game_HotspotHit - 0x14);
        if (ds.Hire_Candidate != k) {
            ds.Hire_InfoShown = 0;
            ds.Hire_Candidate = 0;
            Hire_DrawCandidateInfo();
            ds.Hire_Candidate = k;
            Hire_DrawCandidateFrames();
            Hire_DrawCandidateInfo();
            ds.Hire_InfoShown = 1;
        }
    }
    if (ds.Game_MenuCommand != 0x28)
        return;
    if (ds.Hire_Candidate == 0) {
        Sound_PlayButtonSfx("x");
        return;
    }
    int16_t c = ds.Hire_Candidate, t = ds.Hire_StaffKind;
    switch (Hire_CheckHire(t, c)) {
    case HIRE_OK: {
        uint16_t h = hired(t);
        if (h > 0) {
            // Keep the departing candidate's experience (and skills) with them.
            ds.Staff_Level[t - 1][h - 1] = (int16_t)ds.Game_StaffExp[t - 1];
            if (t == 4) {
                for (int k = 1; k <= 4; k++)
                    ds.Staff_DevSkills[0][ds.Game_StaffHired[3] - 1][k - 1] = (uint8_t)ds.Game_DevSkill[k - 1];
            }
        }
        say(Lang_Text("OK, I will work for you."));
        ds.Game_Credits -= ds.Staff_Price[t - 1][c - 1];
        ds.Game_StaffHired[t - 1] = (uint16_t)c;
        ds.Game_StaffExp[t - 1] = (uint16_t)ds.Staff_Level[t - 1][c - 1];
        if (t == 4) {
            for (int k = 1; k <= 4; k++)
                ds.Game_DevSkill[k - 1] = dev_skills(c)[k - 1];
        }
        Gfx_DrawCredits();
        Courses_End(ds.Hire_StaffKind);
        Sound_PlayButtonSfx("welcome");
        break;
    }
    case HIRE_NO_CREDITS:
        say(Lang_Text("You don't have enough credits to hire me."));
        Gfx_FlashCredits();
        Sound_PlayButtonSfx("hiba");
        break;
    case HIRE_ALREADY_HIRED:
        say(Lang_Text("I am still working for you."));
        Sound_PlayButtonSfx("hiba");
        break;
    case HIRE_NOT_SKILLED:
        say(Lang_TextAt(0x3fab08e5, "I'm not skilled enough for the job."));
        Sound_PlayButtonSfx("hiba");
        break;
    }
    ds.Hire_InfoShown = 0;
}

// 3fab:0321
void Hire_LoadHireBackground(void)
{
    uint8_t *buf = game_alloc(0x8AC0);
    char face[256], name[256];
    pstr_get(face, sizeof face, &ds.Staff_FacePics[ds.Hire_StaffKind - 1]);
    SDL_snprintf(name, sizeof name, "grafika\\%s", face);
    Gfx_LoadPic(name, buf, 1, 0x6E, 0, 0, 0x140, true, Gfx_PalettePtr(), 0, 0x8F,
                0x40, 0, 0x8F);
    Blit_CopyRect(buf + 0x141, gfx_screen() + 0x3E81, 0x140, 0x6D);
    Gfx_FadeIn(0x40, 0xCF, Gfx_PalettePtr());
    game_free(buf);
}

// 3fab:03ba
void Hire_DrawCandidateFrames(void)
{
    static const int16_t x1[3] = {5, 0x6E, 0xD8}, x2[3] = {0x69, 0xD2, 0x13C};
    for (int i = 0; i < 3; i++)
        Gfx_DrawRectOutline(x1[i], 0x32, x2[i], 0xA0, 0xE8);
    if (ds.Hire_Candidate >= 1 && ds.Hire_Candidate <= 3)
        Gfx_DrawRectOutline(x1[ds.Hire_Candidate - 1], 0x32, x2[ds.Hire_Candidate - 1], 0xA0, 0xE9);
}

// SzFace record of candidate c of kind t: string[40], string[81], string[40]
// (0xA4 bytes; 3fab:067c/06a4/06fc add 0, 29h and 7Bh), into text.
static const char *face_text(char text[256], int16_t t, int16_t c, int line)
{
    static const uint16_t off[3] = {0, 0x29, 0x7B};
    return pstr_get(text, 256, ds.RawText_SzFace + (t - 1) * 0x1EC + (c - 1) * 0xA4 + off[line]);
}

// 3fab:048f
void Hire_DrawCandidateInfo(void)
{
    if (ds.Hire_Candidate == 0) {
        Blit_FillRect(10, 0xA4, 0x12C, 0x20, 0xF9);
        return;
    }
    int16_t c = ds.Hire_Candidate, t = ds.Hire_StaffKind;
    bool is_hired = c >= 0 && hired(t) == (uint16_t)c;
    char s[256], text[256];
    int level = !is_hired ? ds.Staff_Level[t - 1][c - 1] : ds.Game_StaffExp[t - 1];
    SDL_snprintf(s, sizeof s, "%s%d", Lang_Text("My scholarly level:  "), level);
    if (t == 4) {
        // The developer shows his four skills instead of the level.
        SDL_strlcpy(s, Lang_Text("Level:  "), sizeof s);
        for (int k = 1; k <= 4; k++) {
            int32_t v = !is_hired ? dev_skills(c)[k - 1] : ds.Game_DevSkill[k - 1];
            char discipline[256];
            size_t n = SDL_strlen(s);
            pstr_get(discipline, sizeof discipline, Lang_Slot(&ds.Names_Disciplines[k - 1]));
            SDL_snprintf(s + n, sizeof s - n, "%s: %d  ", discipline, (int)v);
        }
    }
    Gfx_TypeTextF9(face_text(text, t, c, 0), 10, 0xA4, 0x32);
    Gfx_TypeTextF9(face_text(text, t, c, 1), 10, 0xAC, 0x32);
    Gfx_TypeTextF9(s, 10, 0xB4, 0x32);
    uint16_t h = hired(t);
    if (c >= 0 && h < (uint16_t)c)
        Gfx_TypeTextF9(face_text(text, t, c, 2), 10, 0xBC, 0x32);
    else if (c >= 0 && h == (uint16_t)c)
        Gfx_TypeTextF9(Lang_Text("I work for you"), 10, 0xBC, 0x32);
    else
        Gfx_TypeTextF9(Lang_Text("I'm no better than your advisor"), 10, 0xBC, 0x32);
}

// 3fab:092b
void Hire_SetupHireHotspots(void)
{
    static const int16_t x[3] = {5, 0x6E, 0xD8};
    ds.Mouse_HotspotCount = 3;
    for (int i = 0; i < 3; i++) {
        THotspot *h = Mouse_Hotspot((int16_t)(i + 1));
        h->X = x[i];
        h->Y = 0x32;
        h->W = 99;
        h->H = 0x6D;
        char name[256];
        pstr_get(name, sizeof name, &ds.Staff_Names[ds.Hire_StaffKind - 1][i]);
        pstr_set(&h->Caption, 0x12, name);
        h->Repeat_ = 0;
        h->Cursor = 0;
    }
}
