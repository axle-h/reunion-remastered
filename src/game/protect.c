// What is left of the copy protection: see protect.h.
#include "game/protect.h"

#include "game/system.h"

// Where the original keeps this, its protection variables (Protect_PageIndex
// and Key2, Protect2_QuestionIndex and AnswerKey) hold it. Every expected
// answer (PageCodes 1..20, WordCodes 1..14) is above the "unanswered"
// value, so a correct answer always takes the original's success branches.
static struct {
    bool page_armed, page_answered;
    bool date_armed, date_answered;
} state;

void Protect_Reset(void)
{
    state = (typeof(state)){0};
}

void Protect_ArmManualCheck(void)
{
    if (!state.page_answered) {
        System_Random(20);  // the page
        state.page_armed = true;
    }
}

void Protect_ManualCheckHour(void)
{
    if (state.page_armed) {
        state.page_answered = true;
        System_Random(7);  // lives := Random(7) + 479h
    }
}

void Protect_ArmDateQuestion(void)
{
    if (!state.date_answered) {
        System_Random(14);  // the question
        state.date_armed = true;
    }
}

void Protect_AskDateQuestion(void)
{
    if (state.date_armed)
        state.date_answered = true;
}

void Protect_DateColonyStep(void)
{
    if (state.date_armed) {
        state.date_answered = true;
        System_Random(7);  // lives := Random(7) + D7Dh
    }
}

void Protect_DateDailyCheck(void)
{
    if (state.date_armed && state.date_answered)
        System_Random(7);  // lives := Random(7) + 479h
}
