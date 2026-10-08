#include "game/emm.h"

#include <SDL3/SDL.h>

#include "game/mem.h"

enum : uint8_t {
    ST_OK = 0x00,
    ST_BAD_HANDLE = 0x83,
    ST_BAD_FUNCTION = 0x84,
    ST_NO_HANDLES = 0x85,
    ST_TOO_MANY_PAGES = 0x87,  // more than the driver has
    ST_NOT_ENOUGH_FREE = 0x88,
    ST_ZERO_PAGES = 0x89,
    ST_BAD_LOGICAL = 0x8A,
    ST_BAD_PHYSICAL = 0x8B,
};

enum : uint16_t {
    MAX_HANDLES = 32,  // handle 0 is the operating system's (no pages)
    UNMAPPED = 0xFFFF,
};

static uint8_t store[EMM_MAX_PAGES][EMM_PAGE_SIZE];
static uint16_t total_pages;
static uint8_t owner[EMM_MAX_PAGES];     // handle owning each page, 0 = free
static uint16_t logical[EMM_MAX_PAGES];  // its logical page number there
static bool handle_open[MAX_HANDLES];
static uint16_t frame[EMM_PHYS_PAGES];   // store page in each physical page

static uint8_t *frame_host(uint16_t phys)
{
    return far_host(far_ptr(EMM_FRAME_SEG, (uint16_t)(phys * EMM_PAGE_SIZE)));
}

static uint16_t free_pages(void)
{
    uint16_t n = 0;
    for (uint16_t i = 0; i < total_pages; i++)
        n += owner[i] == 0;
    return n;
}

static uint16_t handle_pages(uint16_t h)
{
    uint16_t n = 0;
    for (uint16_t i = 0; i < total_pages; i++)
        n += owner[i] == h;
    return n;
}

static bool valid_handle(uint16_t h)
{
    return h > 0 && h < MAX_HANDLES && handle_open[h];
}

// Takes physical page `phys` off its logical page, writing the frame back.
static void unmap(uint16_t phys)
{
    if (frame[phys] != UNMAPPED)
        SDL_memcpy(store[frame[phys]], frame_host(phys), EMM_PAGE_SIZE);
    frame[phys] = UNMAPPED;
}

void emm_install(uint16_t pages)
{
    SDL_memset(owner, 0, sizeof owner);
    SDL_memset(handle_open, 0, sizeof handle_open);
    for (uint16_t p = 0; p < EMM_PHYS_PAGES; p++)
        frame[p] = UNMAPPED;
    total_pages = pages < EMM_MAX_PAGES ? pages : EMM_MAX_PAGES;

    uint8_t *ivt = mem + 0x67 * 4;
    uint8_t *hdr = far_host(far_ptr(EMM_DRIVER_SEG, 0));
    SDL_memset(hdr, 0, 0x13);
    if (pages == 0) {
        SDL_memset(ivt, 0, 4);
        return;
    }
    // Character device header: next = FFFF:FFFF, attributes C000h, name at
    // +0Ah. The INT 67h "handler" after it is an IRET; emm_int67 stands in.
    SDL_memset(hdr, 0xFF, 4);
    hdr[5] = 0xC0;
    SDL_memcpy(hdr + 0x0A, "EMMXXXX0", 8);
    hdr[0x12] = 0xCF;
    const uint8_t vec[4] = {0x12, 0x00, EMM_DRIVER_SEG & 0xFF, EMM_DRIVER_SEG >> 8};
    SDL_memcpy(ivt, vec, 4);
}

static uint8_t allocate(Registers *r)
{
    uint16_t n = r->BX;
    if (n == 0)
        return ST_ZERO_PAGES;
    if (n > total_pages)
        return ST_TOO_MANY_PAGES;
    if (n > free_pages())
        return ST_NOT_ENOUGH_FREE;
    uint16_t h = 1;
    while (h < MAX_HANDLES && handle_open[h])
        h++;
    if (h == MAX_HANDLES)
        return ST_NO_HANDLES;
    handle_open[h] = true;
    for (uint16_t i = 0, l = 0; l < n; i++) {
        if (owner[i] == 0) {
            owner[i] = (uint8_t)h;
            logical[i] = l++;
        }
    }
    r->DX = h;
    return ST_OK;
}

// AL = physical page, BX = logical page (FFFFh unmaps), DX = handle.
static uint8_t map(Registers *r)
{
    uint16_t phys = r->AX & 0xFF, log = r->BX, h = r->DX;
    if (!valid_handle(h))
        return ST_BAD_HANDLE;
    if (phys >= EMM_PHYS_PAGES)
        return ST_BAD_PHYSICAL;
    if (log == 0xFFFF) {
        unmap(phys);
        return ST_OK;
    }
    uint16_t page = 0;
    while (page < total_pages && !(owner[page] == h && logical[page] == log))
        page++;
    if (page == total_pages)
        return ST_BAD_LOGICAL;
    if (frame[phys] == page)
        return ST_OK;
    for (uint16_t p = 0; p < EMM_PHYS_PAGES; p++)
        if (frame[p] == page)
            unmap(p);
    unmap(phys);
    SDL_memcpy(frame_host(phys), store[page], EMM_PAGE_SIZE);
    frame[phys] = page;
    return ST_OK;
}

static uint8_t release(Registers *r)
{
    uint16_t h = r->DX;
    if (!valid_handle(h))
        return ST_BAD_HANDLE;
    for (uint16_t p = 0; p < EMM_PHYS_PAGES; p++)
        if (frame[p] != UNMAPPED && owner[frame[p]] == h)
            frame[p] = UNMAPPED;  // the frame keeps the stale contents
    for (uint16_t i = 0; i < total_pages; i++)
        if (owner[i] == h)
            owner[i] = 0;
    handle_open[h] = false;
    return ST_OK;
}

void emm_int67(Registers *r)
{
    uint8_t st = ST_OK;
    switch (total_pages == 0 ? 0 : r->AX >> 8) {  // no driver: nothing answers
    case 0x40:  // status
        break;
    case 0x41:  // page frame segment
        r->BX = EMM_FRAME_SEG;
        break;
    case 0x42:  // unallocated / total pages
        r->BX = free_pages();
        r->DX = total_pages;
        break;
    case 0x43:
        st = allocate(r);
        break;
    case 0x44:
        st = map(r);
        break;
    case 0x45:
        st = release(r);
        break;
    case 0x46:  // version 4.0
        r->AX = (r->AX & 0xFF00) | 0x40;
        break;
    case 0x4C:  // pages of handle DX
        if (valid_handle(r->DX))
            r->BX = handle_pages(r->DX);
        else
            st = ST_BAD_HANDLE;
        break;
    default:
        st = ST_BAD_FUNCTION;
        break;
    }
    r->AX = (uint16_t)(st << 8 | (r->AX & 0xFF));
}

// The driver's variables, in one fixed order.
#define EMM_STATE_VARS X(store) X(total_pages) X(owner) X(logical) X(handle_open) X(frame)

size_t emm_state_size(void)
{
    size_t n = 0;
#define X(v) n += sizeof v;
    EMM_STATE_VARS
#undef X
    return n;
}

void emm_state_save(void *buf)
{
    uint8_t *p = buf;
#define X(v) SDL_memcpy(p, &v, sizeof v); p += sizeof v;
    EMM_STATE_VARS
#undef X
}

void emm_state_load(const void *buf)
{
    const uint8_t *p = buf;
#define X(v) SDL_memcpy(&v, p, sizeof v); p += sizeof v;
    EMM_STATE_VARS
#undef X
}
