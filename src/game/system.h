// Unit System (48c7): the parts of the Borland Pascal 7.0 run-time library
// that game code calls. Strings, Random, the 6-byte Real and the heap are
// ported from the original code so results (random sequences, rounding, far
// pointers handed out by GetMem) match the DOS program exactly.
//
// Not ported: file and text I/O (game code uses platform/fs.h), sets (C uses
// bit operations), LongMul/LongDivMod/LongShl/LongShr (C does 32-bit integer
// arithmetic natively, with the same truncating division), overlays.
#pragma once

#include <stdint.h>

#include "game/dseg.h"

// 48c7:0000 InitSystem, the part the port needs: the selector variables and
// Test8086, and an empty heap. Needs dseg_init() first.
void system_init(void);

// Called instead of terminating on a run-time error (tests use it to catch
// them; it must not return). nullptr: print the message and exit.
extern void (*system_runerror_handler)(uint16_t code);

// Called by Halt/RunError after the ExitProc chain, instead of exit(), with
// the DOS exit code (the launcher restarts the game on 15 and shows VICTORY on
// 10). It must not return. nullptr: exit(code).
extern void (*system_exit_handler)(uint8_t code);

// 48c7:010f: run-time error `code` (200 division by zero, 203 heap
// overflow, 204 invalid pointer, 205 floating point overflow, 207 invalid
// floating point operation). Does not return.
[[noreturn]] void System_RunError(uint16_t code);
// 48c7:0116: ends the program with exit code `code` (ErrorAddr := nil),
// running the ExitProc chain first.
[[noreturn]] void System_Halt(uint16_t code);
// ---------------------------------------------------------------------------
// Pascal strings. Any string[N] (PStrN in dseg.h) has the same layout as
// PString, so pass it cast to PString *; `maxLen` is the declared N.
// ---------------------------------------------------------------------------

// A PString literal: PSTR("abc") is a const PString * to {3, "abc"}. It is a
// compound literal, so it lives only to the end of the enclosing block: an
// if or else branch is a block of its own (docs/port-bugs.md #32).
#define PSTR(lit) (&(const PString){sizeof(lit) - 1, lit})

// 48c7:0c7c: tmp := src (tmp is a 255-character temporary).
void System_StrLoad(PString *tmp, const PString *src);
// 48c7:0c96: dst := src, truncated to maxLen characters.
void System_StrStore(const PString *src, PString *dst, uint16_t maxLen);
// 48c7:0cba: result := Copy(s, index, count).
void System_Copy(PString *result, const PString *s, int16_t index, int16_t count);
// 48c7:0cfb: tmp := tmp + src, truncated to 255 characters.
void System_StrConcat(PString *tmp, const PString *src);
// 48c7:0d27: position (1-based) of sub in s, 0 if absent or sub is empty.
int16_t System_Pos(const PString *sub, const PString *s);
// 48c7:0d6d: compares a and b like Pascal's <, =, > on strings (bytes
// unsigned, then length): <0, 0 or >0.
int System_StrCompare(const PString *a, const PString *b);
// 48c7:0d98: result := c (a one-character string).
void System_CharToStr(PString *result, char c);
// 48c7:0dc5: Insert(src, s, index) for s: string[maxLen].
void System_Insert(const PString *src, PString *s, uint16_t maxLen, int16_t index);
// 48c7:0e24: Delete(s, index, count).
void System_Delete(PString *s, int16_t index, int16_t count);
// 48c7:1a38: Str(value:width, s) for s: string[maxLen].
void System_StrLong(int32_t value, int16_t width, PString *s, uint16_t maxLen);
// 48c7:1a83: Val(s, result, code) for a longint result. code is 0 on
// success, else the position of the offending character (result 0).
int32_t System_ValLong(const PString *s, int16_t *code);
// 48c7:1b78
char System_UpCase(char c);

// ---------------------------------------------------------------------------
// Memory. The original takes far pointers; these take host pointers.
// ---------------------------------------------------------------------------

// 48c7:0b4f: forward byte copy (the compiler's record/array assignment), so
// an overlapping copy to a higher address repeats the source like the
// original's rep movsb.
void System_MoveFwd(const void *src, void *dst, uint16_t count);
// 48c7:1b40: Move. The original picks the copy direction by comparing the
// offsets only; for the same segment that is memmove.
void System_Move(const void *src, void *dst, uint16_t count);
// 48c7:1b64
void System_FillChar(void *dst, uint16_t count, uint8_t value);

// ---------------------------------------------------------------------------
// Heap. Blocks come from the C allocator, zeroed (the original's held
// whatever the heap held before; the game reads nothing it hasn't written).
// ---------------------------------------------------------------------------

// 48c7:028a: GetMem(size). nil for size 0; run-time error 203 when full.
void *System_GetMem(uint16_t size);
// 48c7:029f: FreeMem. Run-time error 204 for nil (size 0 is ignored).
void System_FreeMem(void *p, uint16_t size);
// 48c7:02e7 / 0303: total free bytes and the largest free block, as the
// game sees them: always SYSTEM_MEM_PLENTY, so its memory checks pass.
enum : int32_t { SYSTEM_MEM_PLENTY = 0xA0000 };
int32_t System_MemAvail(void);
int32_t System_MaxAvail(void);
// Port addition: a buffer the original addressed through its 64 KB segment
// (drawing routines' 16-bit offsets, indices that wrap below the block): a
// whole zeroed segment, so the wrapped offsets stay inside it. The block
// starts at its start (the original's was at offset 0 or 8 of its segment;
// the game never depends on which).
uint8_t *System_GetSegment(void);
void System_FreeSegment(uint8_t *seg);
// Bytes in use (GetMem sizes rounded to 8, segments), for tests' leak checks.
uint32_t system_heap_used(void);

// ---------------------------------------------------------------------------
// Random numbers (Borland's LCG on ds.System_RandSeed).
// ---------------------------------------------------------------------------

// 48c7:18b3: 0 <= result < range (0 for range 0).
uint16_t System_Random(uint16_t range);
// 48c7:1948: seeds from the time of day as DOS reports it.
void System_Randomize(void);

// ---------------------------------------------------------------------------
// 6-byte Real. Bit-exact ports of the original's integer algorithms
// (truncation, round-half-up on a guard byte, Sqrt by Newton iteration,
// Cos by a polynomial), so results match the DOS program, not IEEE.
// Layout: b[0] exponent (bias 129, 0 = zero), b[1..5] mantissa, top bit of
// b[5] the sign.
// ---------------------------------------------------------------------------

// Run-time error 205 on overflow; results that underflow are 0.
// 48c7:13f4
Real48 System_RealAdd(Real48 a, Real48 b);
// 48c7:13fa
Real48 System_RealSub(Real48 a, Real48 b);
// 48c7:1400: a * a
Real48 System_RealSqr(Real48 a);
// 48c7:1406
Real48 System_RealMul(Real48 a, Real48 b);
// 48c7:140c: run-time error 200 if b is 0.
Real48 System_RealDiv(Real48 a, Real48 b);
// 48c7:1416: <0, 0 or >0 as a <, =, > b.
int System_RealCmp(Real48 a, Real48 b);
// 48c7:141a: Real(value). symbols.txt calls it RealCmpEq; it converts the
// longint in DX:AX (the decompile shows System::RealCmpEq).
Real48 System_LongToReal(int32_t value);
// 48c7:141e / 1426: run-time error 207 if the result is not a longint.
// Round rounds halves away from zero.
int32_t System_Trunc(Real48 a);
int32_t System_Round(Real48 a);
// 48c7:1468 / 14b9
Real48 System_Int(Real48 a);
Real48 System_Frac(Real48 a);
// 48c7:14cd: run-time error 207 if a < 0.
Real48 System_Sqrt(Real48 a);
// 48c7:152c
Real48 System_Cos(Real48 a);

// Conversions for C code and tests (not in the original). to_double is
// exact; from_double rounds to nearest (ties to even), gives 0 below the
// smallest Real and saturates above the largest.
double real48_to_double(Real48 r);
Real48 real48_from_double(double d);
