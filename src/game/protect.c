// Unit Protect (24eb): see protect.h.
#include "game/protect.h"

// 24eb:00a5
void Protect_CheckToken(void)
{
    // REMOVED CHECK: the original compares PageIndex with 505FCh div 13 (=
    // 62ECh, "not armed") and, if the scheme were armed and unanswered, asks
    // "What is the word at the bottom of page <n>" (title picture 'attentio',
    // 9 characters), hashes the answer into Key2 and lowers Key1. In this EXE
    // that body is unreachable, so there is no state to keep.
}

void Protect_PassManualCheck(void)
{
    if (ds.Protect_PageIndex == PROTECT_PAGE_UNARMED)
        return;
    int32_t page = ds.Protect_PageIndex;
    if (page < 1 || page > 21)
        return;
    // Key2 is the hash of the typed word + 1F5Fh; PageCodes holds the value a
    // correct answer produces. Key2 > 1F5Fh means "answered".
    if (ds.Protect_Key2 > PROTECT_KEY2_UNANSWERED)
        return;
    ds.Protect_Key2 = ds.Protect_PageCodes[page - 1];
}
