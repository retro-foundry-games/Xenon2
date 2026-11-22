#ifndef MAINLOOP_H
#define MAINLOOP_H
#include "game.h"

/* start-up flow, vertical-blank interrupts, starfield, screen flip */
void game_entry(void);
void irq_vblank(void);
void irq_vblank_hooked(void);
void swap_screens(void);
uint32_t swap_screens_200(void);
uint32_t swap_screens_192(void);
void scroll_pos_update(uint16_t speed);
void stars_scroll(uint16_t speed);
void stars_draw(void);
uint16_t box_overlap(int16_t a_left, int16_t a_top, int16_t a_right, int16_t a_bottom, int16_t b_left, int16_t b_top, int16_t b_right, int16_t b_bottom);
Object *obj_free_pop(void);


void irq_keyboard(void);
#endif
