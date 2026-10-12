#include "game/research.h"

#include <SDL3/SDL.h>

#include "game/blit.h"
#include "game/gfx.h"
#include "game/gfxpath.h"
#include "game/mouse.h"
#include "game/sound.h"
#include "game/system.h"

#define PRODUCT(k) ds.Research_Products[(k) - 1]

// Offset of invention k's icon on screen (rows of 16 lines, 5 per row). The
// original computes it two ways (0a1a and 0bae) that give the same value.
static uint16_t icon_offset(uint16_t k)
{
    return (uint16_t)((k - 1) / 5 * 0x1400 + 0x4100 + (k - 1) % 5 * 0x20);
}
#define icon_offset2 icon_offset

// The developer can't work: a story event halts research, or he is at the
// university.
static bool developer_away(void)
{
    return ds.Intel_DevAwayTimer != 0 || ds.Advisor_TrainingKind == 4;
}

// Selection frame position: top-left pixel of the two-pixel mark.
static uint16_t frame_offset(int16_t inv, int16_t rowoff)
{
    return (uint16_t)(((inv - 1) / 5 * 16 + rowoff) * 0x140 + (inv - 1) % 5 * 0x20 + 0x1A);
}

// 2d66:0029
void Research_ResearchScreen(void)
{
    if (ds.Game_ScreenEntered) {
        Research_SetupResearchHotspots();
        if (ds.Game_PrevScreen == 5)
            ds.Game_SelInvention = ds.Shop_List[ds.Shop_Sel - 1];
        else
            ds.Game_SelInvention = ds.Game_CurResearch;
        GfxPath_ShowScreenBackdrop();
        ds.Research_IconBuffer = game_alloc(0x6901);
        Gfx_LoadPic("grafika\\cds", ds.Research_IconBuffer, 1, 0x54, 0, 0,
                    0x140, false, Gfx_PalettePtr(), 0, 0x1F, 0x40, 0, 0x1F);
        Research_DrawInventionIcons();
        if (ds.Game_CurResearch > 0)
            Research_DrawInventionInfo();
        if (ds.Game_SelInvention > 0)
            Research_DrawInventionInfo();
        Research_DrawDeveloperSkills();
        Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
    }
    if (ds.Game_HotspotHit > 0x13 && PRODUCT((int16_t)(ds.Game_HotspotHit - 0x14)).State > 0) {
        TProduct *p = &PRODUCT((int16_t)(ds.Game_HotspotHit - 0x14));
        Research_ResetSelectionFrame();
        ds.Game_SelInvention = (int16_t)(ds.Game_HotspotHit - 0x14);
        if (ds.Game_SelInvention == 14 && ds.Intel_Invention14Revealed == 0) {
            Sound_PlayButtonSfx("x");
            Research_DrawInventionInfo();
        } else {
            int16_t st = p->State;
            // Starting something else stops the running project first.
            #define STOP_CURRENT()                                                        \
                if (ds.Game_CurResearch != 0) {                                           \
                    if (PRODUCT(ds.Game_CurResearch).State == 2) {                        \
                        Research_PlayIconTransitionOut(2, ds.Game_CurResearch);           \
                        PRODUCT(ds.Game_CurResearch).State = 1;                           \
                    } else {                                                              \
                        Research_PlayIconTransitionOut(4, ds.Game_CurResearch);           \
                        PRODUCT(ds.Game_CurResearch).State = 3;                           \
                    }                                                                     \
                }
            if (st == 3) {
                Sound_PlayButtonSfx("startdes");
                p->State = 4;
                STOP_CURRENT();
                ds.Game_CurResearch = ds.Game_SelInvention;
                Research_DrawInventionInfo();
                Research_PlayIconTransitionIn(4, ds.Game_SelInvention);
                ds.Research_ProgressFrame = 0;
            } else if (st == 4) {
                Sound_PlayButtonSfx("designab");
                p->State = 3;
                ds.Game_CurResearch = 0;
                Research_DrawInventionInfo();
                Research_PlayIconTransitionOut(4, ds.Game_SelInvention);
            } else if (st == 1) {
                Sound_PlayButtonSfx("startdes");
                p->State = 2;
                STOP_CURRENT();
                ds.Game_CurResearch = ds.Game_SelInvention;
                Research_DrawInventionInfo();
                Research_PlayIconTransitionIn(2, ds.Game_SelInvention);
                ds.Research_ProgressFrame = 0;
            } else if (st == 2) {
                Sound_PlayButtonSfx("designab");
                p->State = 1;
                ds.Game_CurResearch = 0;
                Research_DrawInventionInfo();
                Research_PlayIconTransitionOut(2, ds.Game_SelInvention);
            } else if (st == 5) {
                Sound_PlayButtonSfx("designre");
                Research_DrawInventionInfo();
                Research_PlayIconTransitionIn(1, ds.Game_SelInvention);
                Research_PlayIconTransitionOut(1, ds.Game_SelInvention);
            }
            #undef STOP_CURRENT
        }
        Research_ResetSelectionFrame();
    }
}

// 2d66:0299
void Research_SetupResearchHotspots(void)
{
    ds.Mouse_HotspotCount = 0x23;
    for (int16_t j = 1; j <= 0x23; j++) {
        THotspot *h = Mouse_Hotspot(j);
        h->X = (int16_t)((j - 1) % 5 * 0x20 - 2);
        if (h->X < 0)
            h->X = 0;
        h->Y = (uint8_t)(((j - 1) / 5 << 4) + 0x34);
        h->W = 0x20;
        h->H = 0x10;
        char name[256];
        if (PRODUCT(j).State < 1)
            pstr_set(&h->Caption, 0x12, "");
        else
            pstr_set(&h->Caption, 0x12, pstr_get(name, sizeof name, &PRODUCT(j).Name));
        if (j == 14 && ds.Intel_Invention14Revealed == 0)
            pstr_set(&h->Caption, 0x12, "Unknown");
        h->Repeat_ = 1;
        h->_1A = 0;
    }
}

// 2d66:03e9: invention name, status line, progress and required skills.
void Research_DrawInventionInfo(void)
{
    if (ds.Game_SelInvention <= 0)
        return;
    const TProduct *p = &PRODUCT(ds.Game_SelInvention);
    if (ds.Game_SelInvention == 14 && ds.Intel_Invention14Revealed == 0) {
        Gfx_DrawTextF9("Project name :", 0xBE, 0x4C, 0x12);
        Gfx_DrawTextF9("Unknown", 0xC8, 0x58, 0x10);
        Gfx_DrawTextF9("", 0xBE, 0x64, 0x12);
        Gfx_DrawTextF9("", 0xBE, 0x70, 0x12);
        Blit_FillRect(0xBD, 0x8B, 0x72, 0x14, 0x40);
        return;
    }
    char s[256], t[256];
    Gfx_DrawTextF9("Project name :", 0xBE, 0x4C, 0x12);
    Gfx_DrawTextF9(pstr_get(t, sizeof t, &p->Name), 0xC8, 0x58, 0x10);
    const char *state = "";
    switch (p->State) {
    case 1: state = "Can be developed"; break;
    case 2: state = "Under development"; break;
    case 3: state = "Can be analysed"; break;
    case 4: state = "Under analysis"; break;
    case 5: state = "Done"; break;
    default: break;
    }
    Gfx_DrawTextF9(state, 0xBE, 0x64, 0x12);
    if (p->State == 2 || p->State == 4) {
        if (ds.Game_StaffExp[3] == 0) {
            Gfx_DrawTextF9("No developer", 0xBE, 0x70, 0x12);
        } else {
            SDL_snprintf(t, sizeof t, "Completed: %d%%", p->Progress / 100);
            Gfx_DrawTextF9(t, 0xBE, 0x70, 0x12);
        }
    } else {
        Gfx_DrawTextF9("", 0xBE, 0x70, 0x12);
    }
    if (p->State >= 5) {
        Blit_FillRect(0xBD, 0x8B, 0x72, 0x14, 0x40);
        return;
    }
    // The four required skills, two per line.
    static const struct { int16_t nx, vx, y; int16_t w; const char *sep; } pos[4] = {
        {0xBD, 0xE0, 0x8B, 6, " :"}, {0xEB, 0x11A, 0x8B, 8, ":"},
        {0xBD, 0xE0, 0x95, 6, ":"},  {0xEB, 0x11A, 0x95, 8, "  :"},
    };
    for (int k = 0; k < 4; k++) {
        char discipline[256];
        pstr_get(discipline, sizeof discipline, &ds.Names_Disciplines[k]);
        SDL_snprintf(t, sizeof t, "%s%s", discipline, pos[k].sep);
        SDL_snprintf(s, sizeof s, "%d", p->Skills[k]);
        Gfx_DrawTextF9(t, pos[k].nx, pos[k].y, pos[k].w);
        Gfx_DrawTextFC(s, pos[k].vx, pos[k].y, 1);
    }
}

// 2d66:074c
void Research_DrawDeveloperSkills(void)
{
    char s[256];
    if (ds.Game_StaffHired[3] == 0) {
        for (int k = 1; k <= 4; k++)
            Gfx_DrawTextF9("-", (int16_t)(ds.Research_SkillX[k - 1] + 100), 0xBC, 1);
        Gfx_DrawTextF9("No developer", 8, 0xBC, 0xE);
    } else {
        for (int k = 1; k <= 4; k++) {
            SDL_snprintf(s, sizeof s, "%d", (int)ds.Game_DevSkill[k - 1]);
            Gfx_DrawTextF9(s, (int16_t)(ds.Research_SkillX[k - 1] + 100), 0xBC, 1);
        }
        Gfx_DrawTextF9(pstr_get(s, sizeof s, &ds.Staff_Names[3][ds.Game_StaffHired[3] - 1]), 8,
                       0xBC, 0xE);
    }
    // Update arrows beside the developer's name: dim when no developer can work.
    const uint8_t *base = ds.Research_IconBuffer;
    bool idle = ds.Game_StaffHired[3] == 0 || developer_away();
    uint16_t o1 = idle ? 0x62D2 : 0x62C0, o2 = idle ? 0x62DE : 0x62CC;
    Blit_CopyRect(base + o1, gfx_screen() + 0xD4CC, 0xB, 5);
    Blit_CopyRect(base + o2, gfx_screen() + 0xD515, 5, 5);
}

// 2d66:0891
void Research_AnimateSelectionFrame(void)
{
    if (ds.Game_SelInvention == 0)
        return;
    int16_t row = 0x40;
    int16_t st = PRODUCT(ds.Game_SelInvention).State;
    if (st == 2 || st == 4)
        row = 0x37;
    uint8_t colour = ds.Research_FrameCounter < 0x15 ? 0x50 : 0x41;
    Mouse_HideCursor();
    uint16_t off = frame_offset(ds.Game_SelInvention, row);
    gfx_screen()[off] = colour;
    gfx_screen()[(uint16_t)(off + 1)] = colour;
    Mouse_ShowCursor();
    ds.Research_FrameCounter--;
    if (ds.Research_FrameCounter < 0)
        ds.Research_FrameCounter = 0x28;
}

// 2d66:0966
void Research_ResetSelectionFrame(void)
{
    if (ds.Game_SelInvention == 0)
        return;
    int16_t row;
    uint8_t colour;
    int16_t st = PRODUCT(ds.Game_SelInvention).State;
    if (st == 2 || st == 4) {
        row = 0x37;
        colour = 0x41;
    } else {
        row = 0x40;
        colour = 0x50;
    }
    ds.Research_FrameCounter = 0x14;
    uint16_t off = frame_offset(ds.Game_SelInvention, row);
    // BUG? unlike AnimateSelectionFrame this does not hide the mouse cursor.
    gfx_screen()[off] = colour;
    gfx_screen()[(uint16_t)(off + 1)] = colour;
}

// 2d66:0a1a
void Research_DrawInventionIcons(void)
{
    static const uint8_t frame_of_state[6] = {0, 10, 0x13, 0x1E, 0x27, 0};
    for (uint16_t k = 1; k <= 0x23; k++) {
        int16_t st = PRODUCT(k).State;
        if (st >= 1 && st <= 5)
            Research_DrawIconFrame(frame_of_state[st], icon_offset(k), gfx_screen());
    }
}

// 2d66:0b5c
void Research_DrawIconFrame(uint8_t frame, uint16_t dest, uint8_t *seg)
{
    Mouse_HideCursor();
    const uint8_t *src = ds.Research_IconBuffer + frame / 10 * 0x12C0 + frame * 0x20;
    Blit_CopyRect(src, seg + (uint16_t)(dest + 0x140), 0x1F, 0xE);
    Mouse_ShowCursor();
}

int16_t Research_SkillShortfall(int16_t inv)
{
    int16_t sum = 0;
    for (int k = 1; k <= 4; k++) {
        uint16_t need = PRODUCT(inv).Skills[k - 1];
        if (ds.Game_DevSkill[k - 1] < need)
            sum = (int16_t)(sum + (need - ds.Game_DevSkill[k - 1]));
    }
    return sum;
}

bool Research_ProgressVisible(int16_t inv)
{
    int16_t short_ = Research_SkillShortfall(inv);
    return (int16_t)(10000 - (int16_t)(10000 / ((int32_t)short_ + 1))) < PRODUCT(inv).Progress;
}

// 2d66:0bae
void Research_AnimateCurrentResearch(void)
{
    if (ds.Game_CurResearch != 0 && ds.Game_StaffExp[3] != 0 && !developer_away()) {
        if (Research_ProgressVisible(ds.Game_CurResearch)) {
            Research_DrawIconFrame((uint8_t)(ds.Research_ProgressFrame / 5 + 0x28),
                                   icon_offset2((uint16_t)ds.Game_CurResearch), gfx_screen());
            ds.Research_ProgressFrame++;
            if (ds.Research_ProgressFrame > 0x2C)
                ds.Research_ProgressFrame = 0;
        }
    }
}

// Shared body of the two transitions: frames from..to, three retraces each,
// the working animation running meanwhile unless it is the current project.
static void play_transition(int16_t kind, int16_t inv, int16_t from, int16_t to)
{
    for (int16_t i = from; i <= to; i++) {
        Research_DrawIconFrame((uint8_t)((kind - 1) * 10 + ds.Research_TransitionFrames[i % 20]),
                               icon_offset2((uint16_t)inv), gfx_screen());
        for (int r = 0; r < 3; r++) {
            if (ds.Game_CurResearch < 0 || inv != ds.Game_CurResearch)
                Research_AnimateCurrentResearch();
            Gfx_WaitVRetrace();
        }
    }
}

// 2d66:0caa
void Research_PlayIconTransitionOut(int16_t kind, int16_t inv)
{
    play_transition(kind, inv, 0, 10);
}

// 2d66:0d7b
void Research_PlayIconTransitionIn(int16_t kind, int16_t inv)
{
    play_transition(kind, inv, 10, 0x14);
}
