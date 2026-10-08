// Unit Protect2 (2693): copy protection scheme B, "Please type the date"
// (armed in the shipped EXE). The port removes the question: the answer is
// always the expected one and every value the checks write is written as for a
// correct answer. See docs/protection.md.
//
// State (DS): QuestionIndex 0876h (8615h = not armed, else 1..14), AnswerHash
// 087Ah, AnswerKey 0882h (3421h = unanswered), Lives 088Ah, WordCodes 085Ah.
#pragma once

#include <stdint.h>

#include "game/dseg.h"

enum : int32_t {
    PROTECT2_NOT_ARMED = 0x8615,
    PROTECT2_UNANSWERED = 0x3421,
};

// 2693:0000: picks QuestionIndex := Random(14) + 1 while unanswered.
void Protect2_ArmDateQuestion(void);
// 2693:0085: the "Security check" prompt; the port answers it correctly.
void Protect2_AskDateQuestion(void);
// 2693:0342: colony step enforcement; nothing left to enforce for a correct answer.
void Protect2_EnforceDateCheck(void);
// 2693:04aa: daily validation; a correct answer refreshes Lives.
void Protect2_VerifyDateAnswer(void);
// 2693:0538: the original halts at Lives == 107h; removed.
void Protect2_HaltIfDateCheckFailed(void);
// 2693:0563: the '!!!!!!!!!' box; removed.
void Protect2_ShowProtectionWarning(void);
