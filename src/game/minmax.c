#include "game/minmax.h"

// 484a:0000
int32_t MinMax_Max(int32_t a, int32_t b)
{
    return a > b ? a : b;
}

// 484a:003a
int32_t MinMax_Min(int32_t a, int32_t b)
{
    return a < b ? a : b;
}

// 484a:0074
void MinMax_SwapInt(int16_t *a, int16_t *b)
{
    int16_t t = *a;
    *a = *b;
    *b = t;
}

// 484a:009a
void MinMax_SwapWord(uint16_t *a, uint16_t *b)
{
    uint16_t t = *a;
    *a = *b;
    *b = t;
}

// 484a:00c0
void MinMax_Init(void)
{
}
