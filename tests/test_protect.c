// protect: the copy protection checks of units Protect, Protect2, Credits and
// AlienTalk always pass without asking, and leave the state a correct answer
// leaves (Key1 / Lives refreshed, AnswerKey = WordCodes[question], ...).
#include <stdio.h>

#include "game/alientalk.h"
#include "game/credits.h"
#include "game/dseg.h"
#include "game/protect.h"
#include "game/protect2.h"
#include "game/system.h"
#include "platform/fs.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static void reset(void)
{
    mem_init();
    CHECK(dseg_init());
    system_init();
}

int main(int argc, char *argv[])
{
    mem_init();
    if (argc < 2 || !fs_init(argv[1]) || !dseg_init()) {
        printf("no game data\n");
        return 77;
    }

    // Initial EXE state: scheme A not armed, scheme B not armed and unanswered.
    reset();
    CHECK(ds.Protect_PageIndex == 0x62EC);
    CHECK(ds.Protect_Key1 == 0x94 && ds.Protect_Key2 == 0x1F5F);
    CHECK(ds.Protect2_QuestionIndex == 0x8615 && ds.Protect2_AnswerKey == 0x3421);
    CHECK(ds.Protect2_Lives == 0x10A);

    // Scheme A while unarmed: every routine is a no-op and draws no random numbers.
    ds.System_RandSeed = 777;
    Protect_CheckToken();
    Credits_EnforceManualCheck();
    AlienTalk_VerifyManualAnswer();
    CHECK(ds.System_RandSeed == 777);
    CHECK(ds.Protect_Key1 == 0x94 && ds.Protect_Key2 == 0x1F5F);

    // Scheme A armed for page 5 (RandomizeProtectPage): the check passes by itself.
    ds.Protect_PageIndex = 5;
    ds.Surface_BuildingTypes[21]._12[1] = 7;
    ds.System_RandSeed = 4242;
    uint16_t expected = (uint16_t)(System_Random(7) + 0x479);
    ds.System_RandSeed = 4242;
    Credits_EnforceManualCheck();
    CHECK(ds.Protect_Key2 == ds.Protect_PageCodes[4]);
    CHECK(ds.Protect_Key2 > 0x1F5F);
    CHECK(ds.Protect_Key1 == expected);
    CHECK(ds.Surface_BuildingTypes[21]._12[1] == 7);  // only 2e4b clears it
    ds.Protect_Key1 = 0x94;
    ds.System_RandSeed = 4242;
    AlienTalk_VerifyManualAnswer();
    CHECK(ds.Protect_Key1 == expected);
    CHECK(ds.Surface_BuildingTypes[21]._12[1] == 0);
    // Key1 never comes near the Halt value 91h again.
    CHECK(ds.Protect_Key1 != 0x91 && ds.Protect_Key1 >= 0x479);
    // Out-of-range pages are treated as unarmed.
    reset();
    ds.Protect_PageIndex = 99;
    Credits_EnforceManualCheck();
    AlienTalk_VerifyManualAnswer();
    CHECK(ds.Protect_Key1 == 0x94);

    // Scheme B: unarmed routines do nothing.
    reset();
    ds.System_RandSeed = 99;
    Protect2_AskDateQuestion();
    Protect2_EnforceDateCheck();
    Protect2_VerifyDateAnswer();
    Protect2_HaltIfDateCheckFailed();
    CHECK(ds.System_RandSeed == 99);
    CHECK(ds.Protect2_AnswerKey == 0x3421 && ds.Protect2_Lives == 0x10A);

    // Arming picks Random(14) + 1 while unanswered.
    ds.System_RandSeed = 31337;
    int32_t q = System_Random(14) + 1;
    ds.System_RandSeed = 31337;
    Protect2_ArmDateQuestion();
    CHECK(ds.Protect2_QuestionIndex == q && q >= 1 && q <= 14);
    Protect2_ArmDateQuestion();  // second call re-rolls while still unanswered
    CHECK(ds.Protect2_QuestionIndex >= 1 && ds.Protect2_QuestionIndex <= 14);
    ds.Protect2_QuestionIndex = q;

    // The question answers itself: AnswerKey = WordCodes[q], hash = key - 3421h.
    int32_t key = ds.Protect_WordCodes[q - 1];
    Protect2_AskDateQuestion();
    CHECK(ds.Protect2_AnswerKey == key);
    CHECK(ds.Protect2_AnswerHash == key - 0x3421);
    CHECK(ds.Protect2_Lives == 0x109);
    CHECK(*(uint32_t *)ds_at(0x087E) == (uint32_t)(key - 0x3421) + 0xAA9B);
    CHECK(*(uint32_t *)ds_at(0x0886) == 0x3421u * 5 + 0x8613);
    // A second call does not ask again.
    Protect2_AskDateQuestion();
    CHECK(ds.Protect2_Lives == 0x109);

    // Daily validation: correct -> Lives refreshed, flag cleared.
    ds.Surface_BuildingTypes[21]._12[1] = 9;
    ds.System_RandSeed = 5;
    int32_t lives = System_Random(7) + 0x479;
    ds.System_RandSeed = 5;
    Protect2_VerifyDateAnswer();
    CHECK(ds.Protect2_Lives == lives);
    CHECK(ds.Surface_BuildingTypes[21]._12[1] == 0);
    CHECK(ds.Protect2_AnswerKey == key);

    // Colony step: correct -> Lives := Random(7) + D7Dh.
    ds.System_RandSeed = 6;
    lives = System_Random(7) + 0xD7D;
    ds.System_RandSeed = 6;
    Protect2_EnforceDateCheck();
    CHECK(ds.Protect2_Lives == lives);
    CHECK(ds.Protect2_ScribbleDone == 0);

    // Armed but never asked: the colony step answers instead of scribbling.
    reset();
    ds.Protect2_QuestionIndex = 3;
    uint8_t before[0x100];
    for (int i = 0; i < 0x100; i++)
        before[i] = *(uint8_t *)ds_at((uint16_t)(0x2000 + i));
    ds.System_RandSeed = 1;
    Protect2_EnforceDateCheck();
    CHECK(ds.Protect2_AnswerKey == ds.Protect_WordCodes[2]);
    CHECK(ds.Protect2_Lives >= 0xD7D && ds.Protect2_Lives <= 0xD7D + 6);
    CHECK(ds.Protect2_ScribbleDone == 0);
    for (int i = 0; i < 0x100; i++)
        CHECK(*(uint8_t *)ds_at((uint16_t)(0x2000 + i)) == before[i]);

    // The Halt at Lives == 107h is gone.
    ds.Protect2_Lives = 0x107;
    Protect2_HaltIfDateCheckFailed();
    Protect2_ShowProtectionWarning();
    CHECK(ds.Protect2_Lives == 0x107);

    printf(ok ? "protect OK\n" : "protect FAILED\n");
    return ok ? 0 : 1;
}
