// Unit MinMax (484a).
#pragma once

#include <stdint.h>

// 484a:0000 / 003a (signed 32-bit)
int32_t MinMax_Max(int32_t a, int32_t b);
int32_t MinMax_Min(int32_t a, int32_t b);
// 484a:0074 / 009a
void MinMax_SwapInt(int16_t *a, int16_t *b);
void MinMax_SwapWord(uint16_t *a, uint16_t *b);
// 484a:00c0 (empty)
void MinMax_Init(void);
