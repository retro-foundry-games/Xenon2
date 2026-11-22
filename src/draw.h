#ifndef DRAW_H
#define DRAW_H
#include "game.h"
#include "amiga/hw.h"

/* the byte COL of the row ROW of a screen (rows of 160 bytes: four planes of 40) */
#define SCREEN_POS(screen, row, col) ((uint8_t *)(screen) + (row) * SCREEN_ROW_BYTES + (col))
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
