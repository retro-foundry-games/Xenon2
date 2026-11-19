#ifndef PKG_A_H
#define PKG_A_H
#include "game.h"
#include "amiga/hw.h"

void free_object(Object *obj);
HLE_WEAK void draw_background(void);
void redraw_energy_bar(void);
void draw_status_digit(void);
HLE_WEAK void draw_text(const uint8_t *text, uint8_t *dest);
HLE_WEAK void draw_number(uint32_t value, uint8_t *dest);
HLE_WEAK void draw_bob_masked(Object *obj);
HLE_WEAK void draw_bob_masked_at(uint16_t x, uint16_t y, const uint8_t *gfx);
HLE_WEAK void draw_bob_or_first(Object *obj);
HLE_WEAK void draw_bob_or(Object *obj);
HLE_WEAK void draw_bob_or_at(uint16_t x, uint16_t y, const uint8_t *gfx);
#endif
