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

/* callees without C yet (other packages); a result "in Z" is returned as 1 when Z is set */
extern void fn_006fe8(void);
extern void fn_006bc8(void);
extern void fn_007d9e(void);
extern void fn_008220(void);
extern void fn_008cb6(void);
extern void fn_00917e(void);
extern void fn_006bea(void);
extern void fn_006f94(void);
extern void fn_00842a(const uint8_t *a0);
extern void fn_019f9e(uint16_t d0);
extern void fn_0017d6(void);
extern void fn_0088b6(const uint32_t *a2);
extern void fn_006766(void);
extern void fn_0016be(void);
extern void fn_00683c(void);
extern void fn_054e24(void);
extern void fn_0035d4(void);
extern void fn_006372(void);
extern void fn_0061e6(void);
extern void fn_003554(void);
extern void fn_00176e(void);
extern void fn_008a16(void);
extern uint16_t fn_008e88(void);           /* result in Z */
extern void fn_00668c(void);
extern void fn_006ec6(void);
extern void fn_019fba(void);
extern void fn_008ff2(void);
extern void fn_004a0a(void);
extern void fn_005c9e(Object *a1);
extern void fn_005540(void);
extern void fn_00352e(void);
extern void fn_006d22(void);
extern void fn_006862(Object *a1);
extern void fn_006eca(void);
extern void fn_006d74(void);
extern void fn_000fac(void);               /* level 2 interrupt handler */
extern void fn_0095e4(void);               /* level 1 interrupt handler */
extern void fn_00861c(void);
extern void fn_00854c(void);
#endif
