#ifndef PKG_G_H
#define PKG_G_H
#include "game.h"
#include "amiga/hw.h"

/* the menu, the high-score table, the disk check and loader, the level load, the enemy path script */
void menu(void);                                            /* orig $8CB6 */
uint16_t continue_screen(void);                             /* orig $8E88 */
void hiscore_enter(void);                                   /* orig $8A16 */
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
int game_disk_present(void);                                /* orig $8F9A: 1 = Z set = the game disk is in */
void shop_run(void);                                        /* orig $8FF2 */
void level_load(void);                                      /* orig $917E */
void palette_fade_out(int16_t wait);                        /* orig $885E */
void palette_fade_in(const uint16_t *target);               /* orig $88B6 */
HLE_WEAK int disk_read_sectors(uint16_t first_sector, uint16_t count, void *dest);   /* orig $921A */
void disk_motor_on(void);                                   /* orig $92C8 */
void disk_motor_off(void);                                  /* orig $92EE */
void disk_recalibrate(void);                                /* orig $9308 */
void disk_seek(uint16_t track);                             /* orig $932E */
void disk_step_out(uint16_t steps);                         /* orig $93AA */
void disk_step_in(uint16_t steps);                          /* orig $93B6 */
void disk_delay(void);                                      /* orig $93EE */
int disk_read_track(void);                                  /* orig $93F8 */
int32_t mfm_align(const uint16_t *src);                     /* orig $951E */
uint8_t *mfm_decode(uint8_t *dest, uint8_t *src, uint16_t bytes);   /* orig $957E: returns the end of the source */
void irq_disk_block(void);                                  /* orig $95E4 */
void work_area_save(void);                                  /* orig $95F4 */
void work_area_restore(void);                               /* orig $9618 */
void enemy_update_keep_seed(Object *o);                     /* orig $9670 */
void enemy_update_seeded(Object *o);                        /* orig $9682 */
void enemy_update(Object *o);                               /* orig $96B6 */
void obj_script_run(Object *o);                             /* orig $9734 */
void obj_script_step(Object *o);                            /* orig $9740 */

/* callees without C yet (registers as the original takes them) */
extern void fn_00847e(void);                                /* the palette back, the dot stars again (end of title_message) */
extern void fn_00318c(uint16_t x, uint16_t y, uint16_t angle, uint16_t steps, const void *gfx);
extern void fn_054e00(void);                                /* the level's code: the three pointers at its start run as harmless ori.b */

/* C of other modules this one calls (prototypes as theirs) */
extern void anim_step(Object *o);
extern void bbox_from_frame(Object *o);
extern uint16_t direction8(int16_t dx, int16_t dy);
extern void obj_nop(Object *o);
extern void draw_text(const uint8_t *text, uint8_t *dest);
extern void draw_background(void);
extern void stars_draw(void);
extern uint32_t swap_screens_200(void);
extern void irq_vblank(void);
extern void irq_vblank_hooked(void);
extern uint32_t random_next(void);
extern void clear_palette(void);
#endif
