#ifndef SHOP_H
#define SHOP_H
/* The shop: the program the game loads from disk 2 (248 sectors from sector 1476) over the level's blob at $54E00 and calls (menus shop_run).
 * Its code, its tables and its pictures are that blob; its members are shop_data (struct ShopData, blobs/shop.blob; the level's members that lie there are left alone while it is loaded: guest_blob_raw).  A function has `shop $ADDR` for the
 * address it has in the original (not `orig $`: the level's code is at the same addresses, the code table is the level's). */
#include "game.h"
#include "draw.h"

#ifdef GAME_SDL
void guest_blob_raw(int on);
uint32_t guest_address(const void *p);
#define GADDR(p) guest_address(p)
#else
#define guest_blob_raw(on) ((void)0)
#define GADDR(p) ((uint32_t)(uintptr_t)(p))
#endif

/* raw words, longs and bytes of the original's memory at an address of the blob */

/* the shop's code (shop.c) and its sound (shop_sound.c): the interrupt calls shop_irq_tick ($2090), shop_sound_cmd is $2094 */
void shop_draw_both(uint16_t x, uint16_t y, const uint16_t *gfx);   /* shop $553F8 */
uint8_t *shop_clear_columns(uint8_t *dst, uint16_t rows);             /* shop $5537A */
void shop_copy_block(const uint16_t *src, uint8_t *dst, uint16_t w, uint16_t h); /* shop $55200 */
void shop_put_rows(const uint16_t *src, uint8_t *dst, uint16_t rows);   /* shop $56912 */
void shop_lights_draw(void);                                       /* shop $56E8E */
void shop_text_newline(void);                                      /* shop $56E44 */
void shop_text_char(uint8_t ch);                                   /* shop $56D8C */
void shop_records_tick(void);                                      /* shop $56988 */
void shop_sparkle(const struct ShopRec *rec);                             /* shop $56C02 */
void shop_records_draw(void);                                      /* shop $569BC */
void shop_keeper_blink(void);                                      /* shop $56848 */
void shop_door_draw(void);                                         /* shop $568E6 */
void shop_room_draw(void);                                         /* shop $5662A */
void shop_room_update(void);                                       /* shop $5661C */
void shop_keeper_draw(void);                                       /* shop $5648C */
void shop_hold(uint16_t frames);                                       /* shop $564B2 */
void shop_wait(uint16_t frames);                                       /* shop $551E6 */
void shop_board_clear(void);                                       /* shop $55360 */
void shop_text_home(void);                                         /* shop $56CA0 */
void shop_cash_draw(void);                                         /* shop $56CB2 */
void shop_print(const uint8_t *text);                                /* shop $56CC0 */
void shop_print_number(uint16_t value);                               /* shop $56D56 */
void shop_stick(void);                                             /* shop $55F60 */
int32_t shop_can_buy(void);                                        /* shop $5593E */
void shop_item_ca2(void);                                          /* shop $558E4 */
void shop_item_advice(void);                                       /* shop $558EC */
void shop_item_name(uint16_t kind, uint16_t power);                /* shop $55DE8 */
void shop_price_text(struct ShopRec *rec);                                /* shop $55C60 */
void shop_stock_update(void);                                      /* shop $55424 */
void shop_stock_sold_out(void);                                    /* shop $55512 */
void shop_stock_recheck(void);                                     /* shop $55534 */
void shop_sell_list(void);                                         /* shop $55AFC */
void shop_sell_show(struct ShopRec *rec);                                 /* shop $55C9A */
void shop_sell(void);                                              /* shop $55B48 */
void shop_buy(void);                                               /* shop $55594 */
void shop_delay(void);                                             /* shop $5504E */
void shop_slide_in(void);                                          /* shop $55264 */
void shop_slide_out(void);                                         /* shop $552BA */
void shop_keeper_in(void);                                         /* shop $564CE */
void shop_open(void);                                              /* shop $56568 */
void shop_close(void);                                             /* shop $565C2 */
void shop_blank_screen(void);                                      /* shop $55198 */
void shop_leave(void);                                             /* shop $55016 */
void shop_end_of_broadcast(void);                                  /* shop $5505A */
void shop_main(void);                                              /* shop $54E00 */
void shop_sound_delay(void);                                       /* shop $64BB2 */
void shop_sound_tables(void);                                      /* shop $6492A */
void shop_sound_tables2(void);                                     /* shop $64972 */
void shop_sound_stop(void);                                        /* shop $647CE */
void shop_sound_reset(void);                                       /* shop $64828 */
void shop_sound_tick(void);                                        /* shop $64BBC */
void shop_irq_tick(void);                                          /* shop $6477C */
void shop_sound_cmd(uint16_t cmd);                                  /* shop $649C6 */
void shop_cursor_draw(void);                                       /* shop $56040 */
void shop_cursor_erase(void);                                      /* shop $560A6 */
void shop_pillars_a(void);                                         /* shop $5530C */
void shop_pillars_b(void);                                         /* shop $55324 */
void shop_item_c90(void);                                          /* shop $55930 */
void shop_item_cca(void);                                          /* shop $55938 */

#endif
