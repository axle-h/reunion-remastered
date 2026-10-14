#include "game/courses.h"

#include <SDL3/SDL.h>

#include "game/advisor.h"
#include "game/dseg.h"
#include "game/improvements.h"

enum : uint8_t { K_UNIVERSITY = 13 };

Course Courses_Extra[COURSE_KINDS];

void Courses_Reset(void)
{
    SDL_zeroa(Courses_Extra);
}

bool Courses_Any(void)
{
    for (int i = 0; i < COURSE_KINDS; i++)
        if (Courses_Extra[i].Hours != 0)
            return true;
    return false;
}

bool Courses_Away(int16_t kind)
{
    return ds.Advisor_TrainingKind == kind || Courses_Extra[kind - 1].Hours != 0;
}

static int16_t away_count(void)
{
    int16_t n = 0;
    for (int16_t k = 1; k <= COURSE_KINDS; k++)
        n += Courses_Away(k);
    return n;
}

int16_t Courses_Universities(void)
{
    int16_t n = 0;
    for (int16_t i = 1; i <= ds.Game_BuildingCount; i++) {
        const TBuilding *bl = &ds.Game_Buildings[i - 1];
        if (bl->Kind == K_UNIVERSITY && bl->BuildLeft == 0 && bl->Powered != 0)
            n++;
    }
    return n;
}

int16_t Courses_Answer(int16_t free)
{
    // The original: one away at a time.
    int16_t places = 1;
    if (improvements.university_courses) {
        places = Courses_Universities();
        if (places == 0)
            return ADVISOR_NO_UNIVERSITY;
    }
    return away_count() < places ? free : 9;
}

void Courses_Start(int16_t kind, int16_t hours)
{
    if (ds.Advisor_TrainingKind < 1) {
        ds.Advisor_TrainingKind = kind;
        ds.Advisor_TrainingTimer = hours;
    } else {
        Courses_Extra[kind - 1] = (Course){(uint8_t)hours, (uint8_t)ds.Advisor_TrainingCourse};
    }
}

void Courses_End(int16_t kind)
{
    if (ds.Advisor_TrainingKind == kind)
        ds.Advisor_TrainingKind = 0;
    Courses_Extra[kind - 1] = (Course){};
    Courses_Settle();
}

void Courses_Settle(void)
{
    if (ds.Advisor_TrainingKind > 0)
        return;
    for (int16_t k = 1; k <= COURSE_KINDS; k++) {
        Course *c = &Courses_Extra[k - 1];
        if (c->Hours == 0)
            continue;
        ds.Advisor_TrainingKind = k;
        ds.Advisor_TrainingTimer = c->Hours;
        if (k == 4)
            ds.Advisor_TrainingCourse = c->Course;
        *c = (Course){};
        return;
    }
}
