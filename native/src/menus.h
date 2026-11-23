#ifndef MENUS_H
#define MENUS_H
#include "game.h"
#include "amiga/hw.h"

/* the menu, the high-score table, the disk check and loader, the level load, the enemy path script */
void menu(void);                                            /* orig $8CB6 */
uint16_t continue_screen(void);                             /* orig $8E88 */
void hiscore_enter(void);                                   /* orig $8A16 */
uint8_t hiscore_stick_first(void);                         /* orig $8AB0 */
uint8_t hiscore_stick_again(void);                         /* orig $8AEE */
void hiscore_show(void);                                    /* orig $89DC */
void hiscore_draw(void);                                    /* orig $8B20 */
void draw_big_text(const uint8_t *text, uint16_t *dest);    /* orig $8B6A */
void title_in_hiscore(void);                                /* orig $8C58 */
void title_in_shop(void);                                   /* orig $8C60 */
void title_in_insert_disk(void);                            /* orig $8C68 */
void title_in_loading(void);                                /* orig $8C70 */
void title_out_hiscore(void);                               /* orig $8C88 */
void title_out_shop(void);                                  /* orig $8C90 */
void title_out_insert_disk(void);                           /* orig $8C98 */
void title_out_loading(void);                               /* orig $8CA0 */
void credits_draw(void);                                    /* orig $8F56 */
void shop_run(void);                                        /* orig $8FF2 */
void level_load(void);                                      /* orig $917E */
void palette_fade_out(int16_t wait);                        /* orig $885E */
void palette_fade_in(const uint16_t *target);               /* orig $88B6 */
void work_area_save(void);                                  /* orig $95F4 */
void work_area_restore(void);                               /* orig $9618 */

/* callees without C yet (registers as the original takes them) */
extern void fn_054e00(void);                                /* the level's code: the three pointers at its start run as harmless ori.b */

/* C of other modules this one calls (prototypes as theirs) */
extern void anim_step(Object *o);
extern void bbox_from_frame(Object *o);
extern uint16_t direction8(int16_t dx, int16_t dy);
extern void obj_nop(Object *o);
extern void draw_text(const uint8_t *text, uint8_t *dest);
extern void draw_background(void);
extern uint32_t swap_screens_200(void);
extern void irq_vblank(void);
extern void irq_vblank_hooked(void);
extern void clear_palette(void);
#endif
