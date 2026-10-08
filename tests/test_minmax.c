// minmax: Max/Min on longints, Swap on integers/words.
#include <stdio.h>

#include "game/minmax.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

int main(void)
{
    CHECK(MinMax_Max(3, 5) == 5 && MinMax_Max(5, 3) == 5 && MinMax_Max(-1, -2) == -1);
    CHECK(MinMax_Min(3, 5) == 3 && MinMax_Min(5, 3) == 3 && MinMax_Min(-1, -2) == -2);
    CHECK(MinMax_Max(-70000, 70000) == 70000 && MinMax_Min(-70000, 70000) == -70000);
    CHECK(MinMax_Max(INT32_MIN, INT32_MAX) == INT32_MAX && MinMax_Min(INT32_MIN, INT32_MAX) == INT32_MIN);
    CHECK(MinMax_Min(4, 4) == 4);
    int16_t a = -5, b = 7;
    MinMax_SwapInt(&a, &b);
    CHECK(a == 7 && b == -5);
    uint16_t c = 0xFFFF, d = 1;
    MinMax_SwapWord(&c, &d);
    CHECK(c == 1 && d == 0xFFFF);
    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
