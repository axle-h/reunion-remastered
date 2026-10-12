// Unit System (48c7): what is left of the Borland Pascal 7.0 run-time
// library: Random and Val, ported from the original code so results match
// the DOS program exactly, the heap, Halt and RunError, and the boundary
// between C strings and the Pascal strings of game data.
//
// Not ported: file and text I/O (game code uses platform/fs.h), sets (C uses
// bit operations), LongMul/LongDivMod/LongShl/LongShr (C does 32-bit integer
// arithmetic natively, with the same truncating division), overlays, the
// string routines (game code uses C strings and SDL_snprintf, which truncate
// as the original's 255-character temporaries do), and the 6-byte Real: the
// game's few Real computations are integer or double code with the same
// results (game/vec3d.c, game/sim.c).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "game/dseg.h"

// 48c7:0000 InitSystem, the part the port needs: the selector variables and
// Test8086, an empty heap and no exit hook. Needs dseg_init() first.
void system_init(void);

// Called instead of ending the program on a run-time error (tests use it to
// catch them; it must not return). nullptr: report the error and exit.
extern void (*system_runerror_handler)(uint16_t code);

// Called by Halt and fatal errors after the exit hook, instead of exit(),
// with the DOS exit code (the launcher restarts the game on 15 and shows
// VICTORY on 10). It must not return. nullptr: exit(code).
extern void (*system_exit_handler)(uint8_t code);

// Run once by Halt and fatal errors before they end the program. The
// original's ExitProc chain had one procedure, Gfx_ExitHandler, which
// Gfx_Init installs here. nullptr: none.
extern void (*system_exit_hook)(void);

// 48c7:010f RunError: a fatal error with the original's run-time error code
// (2 file not found, 100 disk read error, 103 file not open, 200 division by
// zero, 204 invalid pointer operation, 207 invalid floating point
// operation). The report names the port's routine (System_RunError passes
// __func__) and `detail` when not nullptr (for file errors, the path).
// system_runerror_handler gets the code first; without one this ends as
// system_fatal with exit code `code`.
[[noreturn]] void system_run_error(uint16_t code, const char *where, const char *detail);
#define System_RunError(code, detail) system_run_error(code, __func__, detail)
// Port: ends the program after a fatal error. Runs the exit hook, writes
// `message` to stderr and, when a player is watching (a window on a real
// video driver, not a scripted run), shows it in a message box, then exits
// with `code` through system_exit_handler. The original showed an error
// screen instead (code and address, in text mode).
[[noreturn]] void system_fatal(uint8_t code, const char *message);
// 48c7:0116 Halt: runs the exit hook and ends the program with exit code
// `code`.
[[noreturn]] void System_Halt(uint16_t code);
// ---------------------------------------------------------------------------
// Strings. Game code uses C strings; the strings in DS and its records stay
// Pascal strings (PString, PStrN in dseg.h: a length byte and the
// characters), since they are saved and hashed byte for byte. pstr_get and
// pstr_set are the boundary.
// ---------------------------------------------------------------------------

// The game's text has no NUL bytes, but a string the original reads past a
// table can: planet 0's name (a group in a system, not at a planet) starts
// in the system's moon list and runs on into its names. pstr_get carries
// each NUL as PSTR_NUL and pstr_set turns it back, so such a string still
// ends up in the message log byte for byte. The text routines draw both as
// the same glyph (neither is in Gfx_CharsetChars), and no game string holds
// PSTR_NUL itself.
enum : char { PSTR_NUL = 0x7F };

// dst := the Pascal string at pstr (any string[N]), cut to cap - 1
// characters, NULs as PSTR_NUL. Returns dst.
char *pstr_get(char *dst, size_t cap, const void *pstr);
// The Pascal string at pstr, a string[maxLen], := src (PSTR_NULs as NULs),
// leaving exactly the bytes the original's StrStore leaves: the length byte
// and up to maxLen characters; the bytes past them are untouched.
void pstr_set(void *pstr, uint8_t maxLen, const char *src);

// 48c7:1a83: Val(s, result, code) for a longint result. code is 0 on
// success, else the 1-based position of the offending character (result 0).
int32_t System_ValLong(const char *s, int16_t *code);

// ---------------------------------------------------------------------------
// Heap. Blocks come from the C allocator, zeroed (the original's held
// whatever the heap held before; the game reads nothing it hasn't written).
// ---------------------------------------------------------------------------

// 48c7:028a: GetMem(size). The original's GetMem(0) gives nil; no caller
// asks for 0 bytes. A fatal "out of memory" (exit code 203) if the C
// allocator fails; the original's heap limit isn't emulated.
void *game_alloc(size_t size);
// 48c7:029f: FreeMem(p, size) without the size: the whole block. Every
// FreeMem of the original frees the size its GetMem took (checked
// 2026-10-12), so none was a partial free. Run-time error 204 for nil.
void game_free(void *p);
// A whole 64 KB segment, game_alloc(GAME_SEGMENT): a port addition for the
// buffers the original addressed through their segment with 16-bit offsets
// that can wrap or run past the block (each allocation says why), so the
// wrapped offsets stay inside it. The block starts at the segment's start
// (the original's was at offset 0 or 8 of its segment; only the flight
// view's backdrop depends on which, game/shipmove.c).
enum : uint32_t { GAME_SEGMENT = 0x10000 };
// Bytes in use (the sizes of the blocks), for tests' leak checks.
size_t system_heap_used(void);

// ---------------------------------------------------------------------------
// Random numbers (Borland's LCG on ds.System_RandSeed).
// ---------------------------------------------------------------------------

// 48c7:18b3: 0 <= result < range (0 for range 0).
uint16_t System_Random(uint16_t range);
// 48c7:1948: seeds from the time of day as DOS reports it.
void System_Randomize(void);
