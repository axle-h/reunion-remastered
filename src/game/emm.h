// A LIM EMS 4.0 driver (EMM) in the emulated address space, for unit Ems
// (4719), which moves the game's two large blocks out of the heap when it
// finds one. The original's start-up memory check relies on that: without
// EMS the heap is too small with a GUS, as on a DOS machine without EMS.
//
// The driver is found the way the game looks for it: INT 67h's vector points
// into a device header named "EMMXXXX0" (at EMM_DRIVER_SEG). The page frame
// is four 16 KB physical pages at EMM_FRAME_SEG:0000. Logical pages live
// outside mem[]; mapping copies: the page leaving a physical page is written
// back, the new one copied in. That equals real bank switching as long as a
// logical page is mapped into one physical page at a time (mapping it into a
// second one unmaps it from the first), which is all the game does.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "game/dseg.h"

enum : uint16_t {
#ifdef REUNION_EMM_FRAME_SEG
    EMM_FRAME_SEG = REUNION_EMM_FRAME_SEG,
#else
    EMM_FRAME_SEG = 0xE000,
#endif
    EMM_DRIVER_SEG = 0xC800,  // DOSBox's private segment, where its EMM lives
    EMM_PAGE_SIZE = 0x4000,
    EMM_PHYS_PAGES = 4,
    // 256 KB: the most the game asks for ("... or EMS (max 256k)").
    EMM_DEFAULT_PAGES = 16,
    EMM_MAX_PAGES = 64,
};

// Installs a driver with `pages` 16 KB pages (at most EMM_MAX_PAGES), all
// free, nothing mapped; 0 removes it (INT 67h's vector := 0000:0000).
// mem_init() installs EMM_DEFAULT_PAGES.
void emm_install(uint16_t pages);

// INT 67h: functions 40h-46h (status, page frame, page counts, allocate,
// map/unmap, release, version) and 4Ch (pages of a handle). Status in AH;
// other functions, or no driver installed, answer 84h.
void emm_int67(Registers *r);

// Test support (tests/diff): the driver's state outside mem[] (logical pages,
// handles, mapping), saved to / restored from a buffer of emm_state_size()
// bytes, so a test can run code twice from the same state.
size_t emm_state_size(void);
void emm_state_save(void *buf);
void emm_state_load(const void *buf);

// Test support (tests/play observers): logical page `page` of `handle` as
// a program that mapped it would read it, i.e. its physical page in the
// frame if it is mapped there, else the copy outside mem[]; nullptr if the
// handle has no such page. Read-only.
const uint8_t *emm_page_view(uint16_t handle, uint16_t page);
