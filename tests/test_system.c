// system: Borland Pascal 7 run-time library ports (strings, Val/Str, Random,
// Real48, heap) against known BP7 results.
#include <SDL3/SDL.h>
#include <setjmp.h>
#include <stdio.h>

#include "game/dseg.h"
#include "game/mem.h"
#include "game/system.h"

static bool ok = true;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            printf("FAILED line %d: %s\n", __LINE__, #cond);                                       \
            ok = false;                                                                            \
        }                                                                                          \
    } while (0)

static jmp_buf on_error;
static uint16_t error_code;

static void catch_error(uint16_t code)
{
    error_code = code;
    longjmp(on_error, 1);
}

// Runs `stmt`, which must raise run-time error `code`.
#define EXPECT_ERROR(stmt, code)                                                                   \
    do {                                                                                           \
        error_code = 0;                                                                            \
        if (setjmp(on_error) == 0) {                                                               \
            stmt;                                                                                  \
        }                                                                                          \
        if (error_code != (code)) {                                                                \
            printf("FAILED line %d: run-time error %u, want %u\n", __LINE__, error_code, (code));  \
            ok = false;                                                                            \
        }                                                                                          \
    } while (0)

static bool str_is(const PString *s, const char *want)
{
    return s->len == SDL_strlen(want) && SDL_memcmp(s->s, want, s->len) == 0;
}

static void test_strings(void)
{
    PString t, u;
    PStr8 short_str;

    System_StrStore(PSTR("Reunion of worlds"), (PString *)&short_str, 8);
    CHECK(str_is((PString *)&short_str, "Reunion "));

    System_StrLoad(&t, PSTR("abc"));
    System_StrConcat(&t, PSTR("def"));
    CHECK(str_is(&t, "abcdef"));

    // Concatenation stops at 255 characters.
    t.len = 250;
    SDL_memset(t.s, 'x', 250);
    System_StrConcat(&t, PSTR("0123456789"));
    CHECK(t.len == 255 && t.s[249] == 'x' && t.s[250] == '0' && t.s[254] == '4');

    System_Copy(&t, PSTR("Hello"), 2, 3);
    CHECK(str_is(&t, "ell"));
    System_Copy(&t, PSTR("Hello"), 0, 2); // index < 1 counts as 1
    CHECK(str_is(&t, "He"));
    System_Copy(&t, PSTR("Hello"), 4, 100);
    CHECK(str_is(&t, "lo"));
    System_Copy(&t, PSTR("Hello"), 6, 1);
    CHECK(str_is(&t, ""));
    System_Copy(&t, PSTR("Hello"), 2, -1);
    CHECK(str_is(&t, ""));

    CHECK(System_Pos(PSTR("lo"), PSTR("Hello")) == 4);
    CHECK(System_Pos(PSTR("l"), PSTR("Hello")) == 3);
    CHECK(System_Pos(PSTR("aab"), PSTR("aaab")) == 2);
    CHECK(System_Pos(PSTR("x"), PSTR("Hello")) == 0);
    CHECK(System_Pos(PSTR(""), PSTR("Hello")) == 0);
    CHECK(System_Pos(PSTR("Hello!"), PSTR("Hello")) == 0);

    CHECK(System_StrCompare(PSTR("abc"), PSTR("abd")) < 0);
    CHECK(System_StrCompare(PSTR("abc"), PSTR("ab")) > 0);
    CHECK(System_StrCompare(PSTR("abc"), PSTR("abc")) == 0);
    CHECK(System_StrCompare(PSTR("\x80"), PSTR("a")) > 0); // unsigned bytes
    CHECK(System_StrCompare(PSTR(""), PSTR("a")) < 0);

    System_CharToStr(&t, 'Q');
    CHECK(str_is(&t, "Q"));

    System_StrLoad(&t, PSTR("Hello world"));
    System_Delete(&t, 6, 6);
    CHECK(str_is(&t, "Hello"));
    System_StrLoad(&t, PSTR("Hello"));
    System_Delete(&t, 0, 2); // index < 1 counts as 1
    CHECK(str_is(&t, "llo"));
    System_StrLoad(&t, PSTR("Hello"));
    System_Delete(&t, 3, 0);
    System_Delete(&t, 256, 1);
    System_Delete(&t, 9, 1);
    CHECK(str_is(&t, "Hello"));
    System_Delete(&t, 4, 30000);
    CHECK(str_is(&t, "Hel"));

    System_StrLoad(&u, PSTR("Hlo"));
    System_Insert(PSTR("el"), &u, 255, 2);
    CHECK(str_is(&u, "Hello"));
    System_Insert(PSTR("!"), &u, 255, 99); // past the end: appended
    CHECK(str_is(&u, "Hello!"));
    System_Insert(PSTR(">"), &u, 255, -5);
    CHECK(str_is(&u, ">Hello!"));
    System_StrStore(PSTR("abcdef"), (PString *)&short_str, 8);
    System_Insert(PSTR("XYZ"), (PString *)&short_str, 8, 3);
    CHECK(str_is((PString *)&short_str, "abXYZcde"));

    CHECK(System_UpCase('a') == 'A' && System_UpCase('z') == 'Z');
    CHECK(System_UpCase('A') == 'A' && System_UpCase('{') == '{' && System_UpCase('\xE1') == '\xE1');
}

static void test_str_val(void)
{
    PString s;
    PStr6 small;

    System_StrLong(42, 0, &s, 255);
    CHECK(str_is(&s, "42"));
    System_StrLong(42, 5, &s, 255);
    CHECK(str_is(&s, "   42"));
    System_StrLong(-7, 3, &s, 255);
    CHECK(str_is(&s, " -7"));
    System_StrLong(-2147483647 - 1, 0, &s, 255);
    CHECK(str_is(&s, "-2147483648"));
    System_StrLong(0, -3, &s, 255);
    CHECK(str_is(&s, "0"));
    System_StrLong(1234567, 0, (PString *)&small, 6); // digits cut to maxLen
    CHECK(str_is((PString *)&small, "123456"));
    System_StrLong(12, 10, (PString *)&small, 6);
    CHECK(str_is((PString *)&small, "    12"));

    static const struct {
        const char *s;
        int32_t value;
        int16_t code;
    } vals[] = {
        {"123", 123, 0},
        {"  -45", -45, 0},
        {"+7", 7, 0},
        {"0", 0, 0},
        {"-0", 0, 0},
        {"12a", 0, 3},
        {"12 ", 0, 3},
        {"", 0, 1},
        {"   ", 0, 4},
        {"+", 0, 2},
        {"-", 0, 2},
        {"$FF", 255, 0},
        {"$ff", 255, 0},
        {"-$10", -16, 0},
        {"$", 0, 2},
        {"$FFFFFFFF", -1, 0},
        {"$100000000", 0, 10},
        {"$1G", 0, 3},
        {"2147483647", 2147483647, 0},
        {"2147483648", 0, 11},
        {"-2147483648", -2147483647 - 1, 0},
        {"-2147483649", 0, 12},
        {"4294967296", 0, 10},
        {"00000000000012", 12, 0},
    };
    for (size_t i = 0; i < SDL_arraysize(vals); i++) {
        s.len = (uint8_t)SDL_strlen(vals[i].s);
        SDL_memcpy(s.s, vals[i].s, s.len);
        int16_t code = -1;
        int32_t v = System_ValLong(&s, &code);
        if (v != vals[i].value || code != vals[i].code) {
            printf("FAILED Val('%s') = %d code %d\n", vals[i].s, (int)v, code);
            ok = false;
        }
    }
}

static void test_random(void)
{
    // Borland's LCG from RandSeed 0 (the same sequence as Delphi).
    static const uint16_t want[] = {0, 3, 86, 20, 27, 67, 31, 16, 37, 42, 8, 47};
    ds.System_RandSeed = 0;
    for (size_t i = 0; i < SDL_arraysize(want); i++)
        CHECK(System_Random(100) == want[i]);

    ds.System_RandSeed = 0;
    for (int i = 0; i < 3; i++)
        System_Random(1);
    CHECK((uint32_t)ds.System_RandSeed == (1u * 0x08088405u + 1) * 0x08088405u + 1);
    CHECK(System_Random(0) == 0);

    ds.System_RandSeed = -1;
    CHECK(System_Random(0xFFFF) == (uint16_t)(((uint64_t)(0xFFFFFFFFu * 0x08088405u + 1) * 0xFFFF) >> 32));
}

static Real48 R(double d) { return real48_from_double(d); }
static double D(Real48 r) { return real48_to_double(r); }

static bool bytes_are(Real48 r, const uint8_t want[6])
{
    return SDL_memcmp(r.b, want, 6) == 0;
}

// |got - want| within `ulps` units of the last mantissa bit of want.
static bool near(double got, double want, double ulps)
{
    if (want == 0)
        return got == 0;
    int e = 0; // want = m * 2^e, 1 <= |m| < 2
    for (double m = SDL_fabs(want); m >= 2; m /= 2)
        e++;
    for (double m = SDL_fabs(want); m < 1; m *= 2)
        e--;
    return SDL_fabs(got - want) <= ulps * SDL_scalbn(1.0, e - 39);
}

static void test_real(void)
{
    // Known encodings.
    CHECK(bytes_are(System_LongToReal(1), (const uint8_t[6]){0x81, 0, 0, 0, 0, 0}));
    CHECK(bytes_are(System_LongToReal(-1), (const uint8_t[6]){0x81, 0, 0, 0, 0, 0x80}));
    CHECK(bytes_are(System_LongToReal(10), (const uint8_t[6]){0x84, 0, 0, 0, 0, 0x20}));
    CHECK(bytes_are(System_LongToReal(0), (const uint8_t[6]){0}));
    CHECK(bytes_are(R(0.5), (const uint8_t[6]){0x80, 0, 0, 0, 0, 0}));
    CHECK(bytes_are(R(SDL_PI_D), (const uint8_t[6]){0x82, 0x21, 0xA2, 0xDA, 0x0F, 0x49}));
    CHECK(D(System_LongToReal(-2147483647 - 1)) == -2147483648.0);
    CHECK(D(System_LongToReal(123456789)) == 123456789.0);
    CHECK(D(R(1e30)) > 0.99e30 && D(R(-3.25)) == -3.25);

    // Arithmetic: exact cases and BP's rounding of 1/3.
    CHECK(D(System_RealAdd(R(1), R(2))) == 3);
    CHECK(D(System_RealSub(R(1), R(2))) == -1);
    CHECK(bytes_are(System_RealSub(R(1.5), R(1.5)), (const uint8_t[6]){0}));
    CHECK(D(System_RealMul(R(3), R(-7))) == -21);
    CHECK(D(System_RealSqr(R(1.5))) == 2.25);
    CHECK(D(System_RealDiv(R(10), R(4))) == 2.5);
    CHECK(bytes_are(System_RealDiv(R(1), R(3)),
                    (const uint8_t[6]){0x7F, 0xAB, 0xAA, 0xAA, 0xAA, 0x2A}));
    CHECK(D(System_RealAdd(R(1e20), R(1))) == D(R(1e20))); // too small to count
    CHECK(System_RealCmp(R(1), R(2)) < 0 && System_RealCmp(R(-1), R(-2)) > 0);
    CHECK(System_RealCmp(R(-1), R(1)) < 0 && System_RealCmp(R(2.5), R(2.5)) == 0);
    CHECK(System_RealCmp(R(0), R(-0.0)) == 0 && System_RealCmp(R(0), R(1e-30)) < 0);

    // Against double: within one unit of the 40-bit mantissa.
    uint32_t seed = 12345;
    for (int i = 0; i < 20000; i++) {
        double x[2];
        for (int j = 0; j < 2; j++) {
            seed = seed * 1103515245u + 12345;
            double m = (seed >> 8) / 16777216.0 + 0.5;
            seed = seed * 1103515245u + 12345;
            x[j] = D(R(SDL_scalbn((seed & 0x100) ? -m : m, (int)(seed >> 20) % 40 - 20)));
        }
        double s = D(System_RealAdd(R(x[0]), R(x[1])));
        double p = D(System_RealMul(R(x[0]), R(x[1])));
        double q = D(System_RealDiv(R(x[0]), R(x[1])));
        if (!near(s, x[0] + x[1], 1.0) || !near(p, x[0] * x[1], 1.0) || !near(q, x[0] / x[1], 1.0)) {
            printf("FAILED %.17g op %.17g: %.17g %.17g %.17g\n", x[0], x[1], s, p, q);
            ok = false;
            break;
        }
    }

    // Trunc/Round/Int/Frac.
    CHECK(System_Trunc(R(2.7)) == 2 && System_Trunc(R(-2.7)) == -2);
    CHECK(System_Round(R(2.5)) == 3 && System_Round(R(-2.5)) == -3);
    CHECK(System_Round(R(0.5)) == 1 && System_Round(R(0.49)) == 0 && System_Round(R(-0.7)) == -1);
    CHECK(System_Trunc(R(0.99)) == 0 && System_Trunc(R(0)) == 0);
    CHECK(System_Trunc(R(2147483647.0)) == 2147483647);
    CHECK(System_Round(R(-2147483648.0)) == -2147483647 - 1);
    EXPECT_ERROR(System_Trunc(R(2147483648.0)), 207);
    EXPECT_ERROR(System_Round(R(2147483647.5)), 207);
    EXPECT_ERROR(System_Round(R(1e12)), 207);
    CHECK(D(System_Int(R(-2.75))) == -2 && D(System_Int(R(0.75))) == 0);
    CHECK(D(System_Int(R(1e15))) == 1e15 && D(System_Int(R(12345.5))) == 12345);
    CHECK(D(System_Frac(R(2.75))) == 0.75 && D(System_Frac(R(-2.75))) == -0.75);

    // Errors.
    EXPECT_ERROR(System_RealDiv(R(1), R(0)), 200);
    EXPECT_ERROR(System_RealMul(R(1e38), R(1e38)), 205);
    EXPECT_ERROR(System_RealAdd(R(1.7e38), R(1.7e38)), 205);
    EXPECT_ERROR(System_Sqrt(R(-1)), 207);
    CHECK(D(System_RealMul(R(1e-30), R(1e-30))) == 0); // underflow is 0

    // Sqrt and Cos are iterations/series: close to the true value.
    CHECK(D(System_Sqrt(R(4))) == 2 && D(System_Sqrt(R(0))) == 0);
    CHECK(near(D(System_Sqrt(R(2))), SDL_sqrt(2.0), 2));
    CHECK(near(D(System_Sqrt(R(1e10))), 1e5, 2));
    CHECK(near(D(System_Sqrt(R(0.0003))), SDL_sqrt(D(R(0.0003))), 2));
    static const double angles[] = {0, 0.5, 1, 1.5, 2, 3, SDL_PI_D / 3, SDL_PI_D, -1, 4, 6, 7, 10, 100, -20};
    for (size_t i = 0; i < SDL_arraysize(angles); i++) {
        double a = D(R(angles[i]));
        double c = D(System_Cos(R(a)));
        if (SDL_fabs(c - SDL_cos(a)) > 1e-9) {
            printf("FAILED Cos(%g) = %.12f, want %.12f\n", a, c, SDL_cos(a));
            ok = false;
        }
    }
}

static void test_heap(void)
{
    system_init();
    const uint16_t h = MEM_HEAP_SEG;
    CHECK(ds.System_HeapOrg == far_ptr(h, 0) && ds.System_HeapEnd == far_ptr(0xA000, 0));
    int32_t all = (0xA000 - h) * 16;
    CHECK(system_heap_free() == all && system_heap_max_block() == all);

    CHECK(System_GetMem(0) == 0);
    FarPtr p1 = System_GetMem(10); // 16 bytes
    FarPtr p2 = System_GetMem(5);  // 8
    FarPtr p3 = System_GetMem(20); // 24
    FarPtr p4 = System_GetMem(100); // 104
    CHECK(p1 == far_ptr(h, 0) && p2 == far_ptr(h + 1, 0));
    CHECK(p3 == far_ptr(h + 1, 8) && p4 == far_ptr(h + 3, 0));
    CHECK(ds.System_HeapPtr == far_ptr(h + 9, 8) && ds.System_FreeList == ds.System_HeapPtr);
    CHECK(ds.System_HeapAllocSize == 100);
    int32_t top = all - 16 * 9 - 8;
    CHECK(system_heap_free() == top);

    // A hole: listed with its size, ending at HeapPtr.
    System_FreeMem(p2, 5);
    CHECK(ds.System_FreeList == p2);
    CHECK(*(uint32_t *)far_host(p2) == ds.System_HeapPtr);
    CHECK(*(uint32_t *)far_host(p2 + 4) == far_ptr(0, 8));
    CHECK(system_heap_free() == top + 8 && system_heap_max_block() == top);

    // Freeing the block below merges the two.
    System_FreeMem(p1, 10);
    CHECK(ds.System_FreeList == p1 && *(uint32_t *)far_host(p1 + 4) == far_ptr(1, 8));
    CHECK(system_heap_free() == top + 24);

    // First fit splits the hole and keeps the rest listed.
    FarPtr p5 = System_GetMem(16);
    CHECK(p5 == p1 && ds.System_FreeList == far_ptr(h + 1, 0));
    CHECK(*(uint32_t *)far_host(far_ptr(h + 1, 4)) == far_ptr(0, 8));
    // An exact fit unlinks it.
    FarPtr p6 = System_GetMem(8);
    CHECK(p6 == far_ptr(h + 1, 0) && ds.System_FreeList == ds.System_HeapPtr);
    System_FreeMem(p6, 8);

    // Blocks at the top lower HeapPtr, swallowing free blocks below.
    System_FreeMem(p4, 100);
    CHECK(ds.System_HeapPtr == p4 && *(uint32_t *)far_host(p6) == p4);
    System_FreeMem(p3, 20);
    CHECK(ds.System_HeapPtr == far_ptr(h + 1, 0) && ds.System_FreeList == ds.System_HeapPtr);
    System_FreeMem(p5, 16);
    CHECK(ds.System_HeapPtr == far_ptr(h, 0) && system_heap_free() == all);

    // Invalid pointers.
    p1 = System_GetMem(64);
    EXPECT_ERROR(System_FreeMem(far_ptr(h, 4), 8), 204);
    EXPECT_ERROR(System_FreeMem(ds.System_HeapPtr, 8), 204);
    EXPECT_ERROR(System_FreeMem(far_ptr(h - 1, 8), 8), 204);
    System_FreeMem(far_ptr(h, 8), 8);
    EXPECT_ERROR(System_FreeMem(far_ptr(h, 8), 8), 204); // already free
    System_FreeMem(p1, 0); // size 0: ignored

    // Running out: up to video memory, then above it (B000:0000 to
    // F000:0000), with what is left below video memory a free block.
    system_init();
    volatile int n = 0;
    EXPECT_ERROR(for (;; n++) System_GetMem(0xFFF0), 203);
    int below = all / 0xFFF0, above = 0x40000 / 0xFFF0;
    CHECK(n == below + above);
    CHECK(ds.System_HeapEnd == far_ptr(0xF000, 0));
    CHECK(system_heap_free() == all - below * 0xFFF0 + 0x40000 - above * 0xFFF0);
    CHECK(system_heap_max_block() == SDL_max(all - below * 0xFFF0, 0x40000 - above * 0xFFF0));
}

int main(void)
{
    mem_init();
    system_runerror_handler = catch_error;
    test_strings();
    test_str_val();
    test_random();
    test_real();
    test_heap();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
