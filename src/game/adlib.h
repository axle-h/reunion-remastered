// Unit Adlib (428e): the assembler AdLib (OPL2) tune player. It ran from the
// BIOS user timer (INT 1Ch, 18.2 Hz); here audio.h's mixer-driven timer calls
// it at the same rate and its OPL writes go to the emulated chip.
//
// Tune layout (after AdlibMusic's un-obfuscation), all offsets from the start:
//   +000h  51 order bytes (pattern numbers, FFh ends the list)
//   +033h  128 instruments of 12 bytes: 20h/40h/60h/80h/E0h register values
//          as pairs (op A, op B) at +0/+2/+4/+6/+9, C0h value at +8,
//          high nibble of +11 = fixed offset added to the A0h value
//   +633h  patterns of 64 rows x 9 channels x 2 bytes (480h bytes):
//          byte 0: note 1..97 (FreqTable[note - 1]; entry 0 is silent),
//          7Fh key off, 80h = byte 1 is an instrument (no effect then),
//          0 = none; byte 1: effect (see adlib.c)
#pragma once

#include <stddef.h>
#include <stdint.h>

// 428e:000c: AX = port, or -1 (the original returned CF set). The port has no
// ports to probe: the emulated OPL sits at 388h whenever audio is available.
int16_t Adlib_DetectAdlib(void);
// 428e:0012: ES:SI = tune (here a host buffer of size bytes). Stops a
// playing tune, programs the OPL and hooks the timer.
void Adlib_StartPlayer(const uint8_t *tune, size_t size);
// 428e:0029: unhooks the timer and silences the OPL, if playing.
void Adlib_StopPlayer(void);
// CS:0743, for tests: a tune is playing.
bool Adlib_Playing(void);
