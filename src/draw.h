#ifndef DRAW_H
#define DRAW_H
#include "game.h"
#include "amiga/hw.h"

/* An address stored in the original's memory (a long in a raw buffer) as a pointer: the identity on the 68000; in the SDL port the address of the
 * original's memory is an offset into the block, an address of code is the C function that has it. */
extern void *guest_code(uint32_t address);   /* code_table.c (generated): the C function that has this address of the original's code */
extern void *guest_code_stub(uint32_t address);   /* sdl/guest.c: guest_code(), or a function that does nothing for level 2's code with no C */
#ifdef GAME_SDL
#define GFN(a) ((void *)guest_code_stub(a))
#else
#define GFN(a) ((void *)guest_code(a))
#endif
#ifdef GAME_SDL
extern void *guest_ptr(uint32_t address);
#define GPTR(a) ((void *)guest_ptr(a))
#else
#define GPTR(a) ((void *)(uintptr_t)(a))
#endif
/* a byte of the screen memory (words): the byte at an even address is the left half of its word, the one at an odd address the right half */
static inline void put_screen_byte(uint8_t *at, uint8_t v)
{
    uint16_t *word = (uint16_t *)((uintptr_t)at & ~(uintptr_t)1);
    unsigned shift = ((uintptr_t)at & 1) ? 0 : 8;

    *word = (uint16_t)((*word & ~(0xffu << shift)) | ((unsigned)v << shift));
}

/* the bits of a byte of the screen memory are put in with OR */
static inline void or_screen_byte(uint8_t *at, uint8_t v)
{
    uint16_t *word = (uint16_t *)((uintptr_t)at & ~(uintptr_t)1);

    *word |= (uint16_t)((unsigned)v << (((uintptr_t)at & 1) ? 0 : 8));
}

/* an animation header (words): picture address (long), delay (word), then the script */
static inline void anim_header_set(Object *o, const uint16_t *header)
{
    const struct AnimStep *h = (const struct AnimStep *)header;

    o->gfx = h->gfx;
    o->anim_delay = (int16_t)h->delay;
    o->anim = (const uint16_t *)(h + 1);
}

void free_object(Object *obj);
HLE_WEAK void draw_background(void);
HLE_WEAK void draw_tile_grid(const uint16_t *cells, uint16_t cols, uint16_t rows, int16_t y, uint16_t x);
HLE_WEAK void draw_tile_grid_flash(const uint16_t *cells, uint16_t cols, uint16_t rows, int16_t y, uint16_t x);
HLE_WEAK void draw_bob_behind_map(Object *obj);
void redraw_energy_bar(void);
void draw_status_digit(void);
HLE_WEAK void draw_text(const uint8_t *text, uint8_t *dest);
HLE_WEAK void draw_number(uint32_t value, uint8_t *dest);
HLE_WEAK void draw_bob_masked(Object *obj);
HLE_WEAK void draw_bob_masked_at(uint16_t x, uint16_t y, const uint16_t *gfx);
HLE_WEAK void draw_bob_or_first(Object *obj);
HLE_WEAK void draw_bob_or(Object *obj);
HLE_WEAK void draw_bob_or_at(uint16_t x, uint16_t y, const uint16_t *gfx);
#endif
