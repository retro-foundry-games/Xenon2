#ifndef TITLE_H
#define TITLE_H
#include "game.h"

extern uint32_t random_next(void);
extern uint16_t random_below(uint16_t n);

/* functions of this module */
void clear_screen(void);
int32_t find_glyph(uint8_t ch, const uint8_t *list);
void draw_text_row(const uint8_t *s);
void draw_text_row_at(uint8_t *dst, const uint8_t *s, uint16_t count);
void draw_text_squeezed(const uint8_t *s);
void stars_update(void);
void stars_erase(void);
void title_fade_in(void);
void title_fade_out(void);
void title_draw_logo(void);
void show_text_sequence(uint16_t row, const uint8_t *text, const uint16_t *steps);
void title_message(const uint8_t *text);
void title_stars_start(void);
void stars_init_dots(void);
void title_restore(void);
void title_credits(void);

void draw_logo_scaled(const uint16_t *gfx, int16_t x, int16_t y, int16_t width, int16_t rows);
#endif
