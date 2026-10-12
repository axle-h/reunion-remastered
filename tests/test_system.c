// system: Borland Pascal 7 run-time library ports (Val, Random, the heap,
// the exit path) against known BP7 results, and the Pascal string boundary.
#include <SDL3/SDL.h>
#include <setjmp.h>
#include <stdio.h>

#include "game/dseg.h"
#include "game/system.h"
#include "platform/video.h"

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

static void test_val(void)
{
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
        int16_t code = -1;
        int32_t v = System_ValLong(vals[i].s, &code);
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

// pstr_set leaves the bytes the original's StrStore leaves (tail included);
// pstr_get reads them back.
static void test_pstr(void)
{
    static const char *const texts[] = {"", "a", "Hello, world", "exactly ten",
                                        "a string longer than the field's fifty characters, "
                                        "so it is cut"};
    static const uint8_t maxes[] = {0, 1, 5, 11, 50, 255};
    for (size_t t = 0; t < SDL_arraysize(texts); t++) {
        for (size_t m = 0; m < SDL_arraysize(maxes); m++) {
            // StrStore: the length byte and up to maxLen characters.
            PString want, got;
            SDL_memset(&want, 0xAA, sizeof want);
            SDL_memset(&got, 0xAA, sizeof got);
            want.len = (uint8_t)SDL_min(SDL_strlen(texts[t]), (size_t)maxes[m]);
            SDL_memcpy(want.s, texts[t], want.len);
            pstr_set(&got, maxes[m], texts[t]);
            CHECK(SDL_memcmp(&want, &got, sizeof want) == 0);
            char back[256];
            CHECK(pstr_get(back, sizeof back, &got) == back);
            CHECK(SDL_strlen(back) == got.len && SDL_memcmp(back, got.s, got.len) == 0);
        }
    }
    // A string[N] field, and a destination shorter than the string.
    PStr7 field = {0};
    pstr_set(&field, 7, "planets");
    char small[4];
    CHECK(SDL_strcmp(pstr_get(small, sizeof small, &field), "pla") == 0);
    // A 255-character string fits a 256-byte buffer.
    PString full;
    full.len = 255;
    SDL_memset(full.s, 'x', 255);
    char buf[256];
    CHECK(SDL_strlen(pstr_get(buf, sizeof buf, &full)) == 255);
    // NUL bytes (game data read past a table) survive the round trip.
    PString in = {.len = 5, .s = {'a', 0, 'b', 0, 0}}, out;
    SDL_memset(&out, 0xAA, sizeof out);
    pstr_get(buf, sizeof buf, &in);
    CHECK(SDL_strlen(buf) == 5 && buf[1] == PSTR_NUL && buf[4] == PSTR_NUL);
    pstr_set(&out, 255, buf);
    CHECK(SDL_memcmp(&in, &out, 6) == 0);
}

static void test_heap(void)
{
    system_init();
    CHECK(system_heap_used() == 0);
    uint8_t *a = game_alloc(10), *b = game_alloc(5);
    CHECK(a && b && system_heap_used() == 10 + 5);
    bool zero = true;
    for (int i = 0; i < 10; i++)
        zero = zero && a[i] == 0;
    CHECK(zero);
    game_free(a);
    CHECK(system_heap_used() == 5);
    EXPECT_ERROR(game_free(nullptr), 204);

    // A whole segment: 16-bit offsets stay inside it.
    uint8_t *seg = game_alloc(GAME_SEGMENT);
    seg[0xFFFF] = 1;
    CHECK(system_heap_used() == 5 + 0x10000);
    game_free(seg);
    CHECK(system_heap_used() == 5);

    // A new start frees what the last run left (b).
    system_init();
    CHECK(system_heap_used() == 0);
}

static int hook_calls;
static uint8_t exit_code;

static void count_hook(void)
{
    hook_calls++;
}

[[noreturn]] static void catch_exit(uint8_t code)
{
    exit_code = code;
    longjmp(on_error, 1);
}

// Halt and fatal errors run the exit hook once, then the exit handler with
// the code (a run-time error's own code; headless, so no message box).
static void test_exit(void)
{
    system_init();
    CHECK(system_exit_hook == nullptr);
    system_runerror_handler = nullptr;
    system_exit_handler = catch_exit;
    static const struct {
        int kind;
        uint8_t code;
    } runs[] = {{0, 15}, {1, 200}, {2, 0}};
    for (size_t i = 0; i < SDL_arraysize(runs); i++) {
        system_exit_hook = count_hook;
        hook_calls = 0;
        exit_code = 0xFF;
        if (setjmp(on_error) == 0) {
            if (runs[i].kind == 0)
                System_Halt(runs[i].code);
            else if (runs[i].kind == 1)
                System_RunError(runs[i].code, "test");
            else
                system_fatal(runs[i].code, "Cannot Open test");
        }
        CHECK(hook_calls == 1 && system_exit_hook == nullptr && exit_code == runs[i].code);
    }
    system_exit_handler = nullptr;
    system_runerror_handler = catch_error;
}

int main(void)
{
    dseg_clear();
    vid_clear();
    system_runerror_handler = catch_error;
    test_val();
    test_random();
    test_pstr();
    test_heap();
    test_exit();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
