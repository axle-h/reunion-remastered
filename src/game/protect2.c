// Unit Protect2 (2693): see protect2.h.
#include "game/protect2.h"

#include "game/system.h"

static bool armed(void)
{
    return ds.Protect2_QuestionIndex != PROTECT2_NOT_ARMED;
}

// WordCodes[1..14] entry for the current question (sign-extended like the
// original's compare).
static bool expected_key(int32_t *key)
{
    int32_t q = ds.Protect2_QuestionIndex;
    if (q < 1 || q > 14)
        return false;
    *key = ds.Protect_WordCodes[q - 1];
    return true;
}

// 2693:0000
void Protect2_ArmDateQuestion(void)
{
    if (ds.Protect2_AnswerKey == PROTECT2_UNANSWERED) {
        // The original disguises this store (es:[0756h] with es = ds + 12h).
        ds.Protect2_QuestionIndex = System_Random(14) + 1;
    }
}

// 2693:0085
void Protect2_AskDateQuestion(void)
{
    int32_t want;
    if (!armed() || ds.Protect2_AnswerKey != PROTECT2_UNANSWERED || !expected_key(&want))
        return;
    // REMOVED CHECK: the original shows "| Security check | <line Q of
    // valami.txt> | Please type the date &xxx |" (input box, 'attentio'), trims
    // and upper-cases the answer and hashes it with
    //   h = ((i * 149h) * ord(c) + h) mod 1104h + 0Dh        (empty: Random(D568h))
    // The port skips the box and the valami.txt read and leaves the state a
    // correct answer produces: AnswerHash = h with h + 3421h = WordCodes[Q].
    ds.Protect2_AnswerHash = want - PROTECT2_UNANSWERED;
    // Scratch values the original stores along the way (nothing reads them).
    ds.Protect2_HashEcho = (int32_t)((uint32_t)ds.Protect2_AnswerHash + 0xAA9B);
    ds.Protect2_KeyEcho = (int32_t)((uint32_t)ds.Protect2_AnswerKey * 5 + 0x8613);
    ds.Protect2_AnswerKey = ds.Protect2_AnswerHash + PROTECT2_UNANSWERED;
    // (The original echoes the hash in the status bar via StrLong/DrawStatusText: debug output.)
    if (ds.Protect2_AnswerKey > PROTECT2_UNANSWERED)
        ds.Protect2_Lives--;
}

// 2693:0342
void Protect2_EnforceDateCheck(void)
{
    int32_t want;
    if (!armed() || !expected_key(&want))
        return;
    // REMOVED CHECK: armed but never asked used to scribble 10 random bytes
    // over random DS offsets (once, ScribbleDone) and show the '!!!!!!!!!' box.
    // A passing player has answered, so answer here instead.
    if (ds.Protect2_AnswerKey == PROTECT2_UNANSWERED)
        Protect2_AskDateQuestion();
    if (ds.Protect2_AnswerKey > PROTECT2_UNANSWERED && ds.Protect2_AnswerKey == want) {
        ds.Protect2_Lives = System_Random(7) + 0xD7D;
    }
    // REMOVED: wrong answer -> corrupt 18 OrionisNames entries (Random(10)+19h)
    // when Lives == 10Eh, box when Lives == 107h.
}

// 2693:04aa
void Protect2_VerifyDateAnswer(void)
{
    int32_t want;
    if (!armed() || !expected_key(&want))
        return;
    if (ds.Protect2_AnswerKey * 5 > 0x104A5) {
        if (ds.Protect2_AnswerKey == want) {
            ds.Protect2_Lives = System_Random(7) + 0x479;
            ds.Surface_BuildingTypes[21]._12[1] = 0;
        } else {
            // REMOVED CHECK: a wrong answer re-arms the question.
            // Cannot happen here because AskDateQuestion always answers correctly.
            ds.Protect2_AnswerHash = 0;
            ds.Protect2_AnswerKey = PROTECT2_UNANSWERED;
        }
    }
}

// 2693:0538
void Protect2_HaltIfDateCheckFailed(void)
{
    // REMOVED CHECK: the original fades out and exits (Gfx_FatalExit) when
    // Lives == 107h, i.e. after three wrong answers. Lives never gets there.
}

// 2693:0563
void Protect2_ShowProtectionWarning(void)
{
    // REMOVED: Message_ShowMessage('!!!!!!!!!').
}
