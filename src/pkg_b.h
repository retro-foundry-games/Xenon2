#ifndef PKG_B_H
#define PKG_B_H
#include "game.h"

/* start-up flow, vertical-blank interrupts, starfield, screen flip */
void game_entry(void);
void irq_vblank(void);
void irq_vblank_hooked(void);
void swap_screens(void);
uint32_t swap_screens_200(void);
uint32_t swap_screens_192(void);
void scroll_pos_update(uint16_t d0);
void stars_scroll(uint16_t d0);
void stars_draw(void);
uint16_t box_overlap(int16_t d0, int16_t d1, int16_t d2, int16_t d3, int16_t d4, int16_t d5, int16_t d6, int16_t d7);
Object *obj_free_pop(void);

extern uint32_t random_next(void);

void irq_keyboard(void);
#endif
