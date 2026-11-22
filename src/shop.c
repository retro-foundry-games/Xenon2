/* The shop (see shop.h): "COLIN'S BARGAIN BASEMENT".  The blob it is loaded as holds the program ($54E00-$56EB6), the sound ($6477C-$64D4A, shop_sound.c),
 * and the pictures and tables; its own variables are raw memory at the addresses below.  The records at SH_RECS (20 of 16 bytes, a 5 x 4 grid of
 * the items on offer or on the ship): word 0 = frames until the sparkle (or the opening animation's state), 2 = x, 4 = y, 6 = item (0 empty, 1-$19;
 * $1A sold out, $1B next page), 8 = the animation list, $C = the object of the ship the item is (selling). */
#include "shop.h"
#include "game_all.h"

#define SH_PAGE        0x55422u   /* word: the first item of the page of the stock */
#define SH_LAST_X      0x55590u   /* words: the grid position last asked about */
#define SH_LAST_Y      0x55592u
#define SH_PRICE       0x55dc2u   /* word: the price asked (-1 none) */
#define SH_REC         0x55dc4u   /* long: the record being asked about */
#define SH_REPEAT      0x55f5eu   /* word: frames until the stick repeats */
#define SH_SELLING     0x560a2u   /* byte: $FF while the player sells */
#define SH_NO_BLINK    0x560a3u   /* bytes: the keeper's eyes and mouth */
#define SH_BLINK_A     0x560a4u
#define SH_BLINK_B     0x560a5u
#define SH_KX          0x56484u   /* words and long: the keeper's picture being drawn */
#define SH_KY          0x56486u
#define SH_KGFX        0x56488u
#define SH_RECS        0x56ac2u
#define SH_RECS_END    0x56bf2u   /* the last record */
#define SH_NTSC        0x64d61u

#define SH_TAB_ANIM    0x5581cu   /* long for each item: its animation list */
#define SH_TAB_PRICE   0x55888u   /* word for each item: its price */
#define SH_TAB_NAMES   0x557b4u   /* long for each item (index = item): its name */
#define SH_TAB_FLAGS   0x5579eu   /* byte for each item: the checks of can_buy */
#define SH_TAB_PICS    0x55224u   /* the pictures of the room: long address, word offset, word width-1, word rows-1; 0 ends */
#define SH_TAB_SPARK   0x56a96u   /* long: the opening pictures of a record, by its state */
#define SH_TAB_PUFF    0x56abau   /* long: the pictures of the record that blink (by the parity of the frame counter) */
#define SH_TAB_LIGHTS  0x56eb8u   /* 7 of { word x, word y, long list } */
#define SH_TAB_ADVICE  0x5702cu

#define SH_GFX_BEDROOM 0x5bdbcu
#define SH_GFX_BEDROOM2 0x5c0ecu
#define SH_GFX_PLAYER2 0x5cd4cu
#define SH_GFX_KEEPER_A 0x5c82cu
#define SH_GFX_KEEPER_B 0x5c59cu
#define SH_GFX_KEEPER_C 0x5cabcu
#define SH_GFX_PILLAR_A 0x5777cu
#define SH_GFX_PILLAR_B 0x577e6u
#define SH_GFX_CURSOR_ON_A 0x578bau
#define SH_GFX_CURSOR_ON_B 0x57850u
#define SH_GFX_EXIT_ON_A 0x5ab58u
#define SH_GFX_EXIT_ON_B 0x5afd8u
#define SH_GFX_EXIT_ON_C 0x5ad98u
#define SH_GFX_EXIT_OFF_A 0x5b218u
#define SH_GFX_EXIT_OFF_B 0x5b698u
#define SH_GFX_EXIT_OFF_C 0x5b458u
#define SH_GFX_EYES_A 0x5b97eu
#define SH_GFX_EYES_B 0x5b8d8u
#define SH_GFX_MOUTH_A 0x5bb60u
#define SH_GFX_MOUTH_B 0x5ba24u
#define SH_LIST_SOLD_OUT 0x56114u
#define SH_LIST_NEXT 0x56108u

#define SH_PIC_TOP     0x5f43eu   /* the rows of the room drawn with the keeper's mask: pictures, 56 bytes a row */
#define SH_PIC_MID     0x5ff9eu
#define SH_PIC_LOW     0x601ceu
#define SH_PIC_DOOR    0x61f8eu
#define SH_PIC_DOOR2   0x6284eu
#define SH_PIC_SLIDE   0x6310eu
#define SH_PIC_LAMP_A  0x5df76u
#define SH_PIC_LAMP_B  0x5e1b6u
#define SH_FONT        0x63feeu
#define SH_GFX_BAR_1   0x5721cu
#define SH_GFX_BAR_2   0x5731cu
#define SH_GFX_BAR_3   0x573ccu
#define SH_GFX_BAR_4   0x574ccu
#define SH_GFX_BAR_5   0x575ccu
#define SH_GFX_BAR_6   0x5767cu
#define SH_PALETTE     0x6474eu
#define SH_PALETTE_END 0x55118u

#define SH_TXT_OUT_OF_STOCK 0x55aa6u
#define SH_TXT_GIVE_CASH    0x55acdu
#define SH_TXT_WHAT_ELSE    0x55ae2u
#define SH_TXT_NO_ROOM      0x558bau
#define SH_TXT_HERE_CASH    0x55c2eu
#define SH_TXT_COST         0x55dd6u
#define SH_TXT_PAY          0x55dc8u
#define SH_TXT_POWER        0x55f56u
#define SH_TXT_SELL         0x56e50u
#define SH_TXT_BUY          0x56e6eu
#define SH_TXT_WELCOME_A    0x56fd8u
#define SH_TXT_WELCOME_B    0x56ffcu
#define SH_TXT_LAST_CHANCE  0x5700fu
#define SH_TXT_THATS_IT     0x55138u
#define SH_TXT_FORGET       0x55153u
#define SH_TXT_GOOD_ENOUGH  0x5517au

#define CIAA_CRA CIA_BYTE(0xbfe001, 0xe)   /* bit 7: the mains frequency */

/* ---- drawing helpers ---- */

/* shop $553F8: a picture on both screens (draw_bob_masked_at) */
void shop_draw_both(uint16_t x, uint16_t y, const uint8_t *gfx)
{
    uint8_t *saved = xenon2.draw_screen;

    xenon2.draw_screen = xenon2.screen_b;
    draw_bob_masked_at(x, y, gfx);
    xenon2.draw_screen = xenon2.screen_a;
    draw_bob_masked_at(x, y, gfx);
    xenon2.draw_screen = saved;
}

/* shop $5537A, $553F2: `rows` rows of 7 words (the first word keeps its top 7 bits, the next five are cleared, the last keeps its low 9 bits) of 4 interleaved planes;
 * returns the address after the rows */
uint8_t *shop_clear_columns(uint8_t *dst, uint16_t rows)
{
    while (rows-- != 0) {
        int p;

        for (p = 0; p < 4; p++) {
            uint8_t *b = dst + p * 0x28;
            int c;

            wr16(b, (uint16_t)(rd16(b) & 0xfe00));
            for (c = 1; c <= 5; c++)
                wr16(b + c * 2, 0);
            wr16(b + 12, (uint16_t)(rd16(b + 12) & 0x01ff));
        }
        dst += 0xa0;
    }
    return dst;
}

/* shop $55200: a block of w+1 words by h+1 rows of 4 planes from src to dst (a row is $A0 bytes) */
void shop_copy_block(const uint8_t *src, uint8_t *dst, uint16_t w, uint16_t h)
{
    do {
        uint8_t *next = dst + 0xa0;
        uint16_t n = w;

        do {
            wr16(dst, rd16(src));
            wr16(dst + 0x28, rd16(src + 2));
            wr16(dst + 0x50, rd16(src + 4));
            wr16(dst + 0x78, rd16(src + 6));
            src += 8;
            dst += 2;
        } while (n-- != 0);
        dst = next;
    } while (h-- != 0);
}

/* shop $56912: rows+1 rows of 28 words of a picture over the screen: 7 columns of 4 planes, the first and the last mixed in under masks */
void shop_put_rows(const uint8_t *src, uint8_t *dst, uint16_t rows)
{
    do {
        int p, c;

        for (p = 0; p < 4; p++)
            wr16(dst + p * 0x28, (uint16_t)(rd16(dst + p * 0x28) & 0xfe00));
        for (p = 0; p < 4; p++) {
            wr16(dst + p * 0x28, (uint16_t)(rd16(dst + p * 0x28) | rd16(src)));
            src += 2;
        }
        for (c = 1; c <= 5; c++)
            for (p = 0; p < 4; p++) {
                wr16(dst + c * 2 + p * 0x28, rd16(src));
                src += 2;
            }
        for (p = 0; p < 4; p++)
            wr16(dst + 12 + p * 0x28, (uint16_t)(rd16(dst + 12 + p * 0x28) & 0x01ff));
        for (p = 0; p < 4; p++) {
            wr16(dst + 12 + p * 0x28, (uint16_t)(rd16(dst + 12 + p * 0x28) | rd16(src)));
            src += 2;
        }
        dst += 0xa0;
    } while (rows-- != 0);
}

/* the next picture of a looping list at the long SLOT: the list is addresses, 0 then the address of the list to loop to */
static uint32_t list_next(uint32_t slot)
{
    uint32_t p = sh_r32(slot);
    uint32_t g = sh_r32(p);

    p += 4;
    if (g == 0) {
        p = sh_r32(p);
        g = sh_r32(p);
        p += 4;
    }
    sh_w32(slot, p);
    return g;
}

/* shop $56E8E: the seven lights of the sign, each at the next picture of its list */
void shop_lights_draw(void)
{
    uint32_t rec = SH_TAB_LIGHTS;
    int16_t n = 7;

    do {
        uint16_t x = sh_r16(rec), y = sh_r16(rec + 2);
        uint32_t g = list_next(rec + 4);

        draw_bob_masked_at(x, y, GPTR(g));
        rec += 8;
    } while (--n != 0);
}

/* shop $56E44: the next line of text */
void shop_text_newline(void)
{
    xenon2.text_x = 0x37;
    xenon2.text_y += 7;
}

/* shop $56D8C: a character of the text at the text position (4 pixels wide: even positions write a byte, odd ones the low nibble) */
void shop_text_char(uint8_t ch)
{
    uint8_t *dst_b, *dst_a;
    const uint8_t *glyph;
    int16_t row;

    if (ch == 0x0d) {
        shop_text_newline();
        return;
    }
    dst_b = xenon2.screen_b + (int16_t)xenon2.row_offset[xenon2.text_y];
    dst_b += (int16_t)(xenon2.text_x >> 1);
    dst_a = xenon2.screen_a + (dst_b - xenon2.screen_b);
    glyph = (const uint8_t *)GPTR(SH_FONT) + (int16_t)((uint16_t)(ch - 0x20) << 5);
    if (!(xenon2.text_x & 1)) {
        for (row = 7; row >= 0; row--) {
            int p;

            for (p = 0; p < 4; p++) {
                uint8_t bits = *glyph++;

                dst_b[p * 0x28] = bits;
                dst_a[p * 0x28] = bits;
            }
            dst_b += 0xa0;
            dst_a += 0xa0;
        }
    } else {
        for (row = 7; row >= 0; row--) {
            int p;

            for (p = 0; p < 4; p++) {
                uint8_t bits = (uint8_t)(*glyph++ >> 4);

                dst_b[p * 0x28] |= bits;
                dst_a[p * 0x28] |= bits;
            }
            dst_b += 0xa0;
            dst_a += 0xa0;
        }
    }
    xenon2.text_x++;
}


/* shop $56988: the countdown of each record: an empty one stays 0, the others start again at $14-$113 */
void shop_records_tick(void)
{
    uint8_t *rec = GPTR(SH_RECS);
    int16_t n = 0x13;

    do {
        if (rd16(rec + 6) == 0) {
            wr16(rec, 0);
        } else {
            wr16(rec, (uint16_t)(rd16(rec) - 1));
            if ((int16_t)rd16(rec) <= 0)
                wr16(rec, (uint16_t)((random_next() & 0xff) + 0x14));
        }
        rec += 0x10;
    } while (n-- != 0);
}

/* shop $56C02: the sparkle at the record's place: 28 rows of a noise pattern 3 words wide drawn into the planes */
void shop_sparkle(const uint8_t *rec)
{
    uint16_t x = rd16(rec + 2), y = rd16(rec + 4);
    uint8_t *dst = xenon2.draw_screen + (int16_t)xenon2.row_offset[y];
    uint16_t shift = x & 0xf, noise, head, mask, inv, rnd;
    int16_t n = 0x1b;

    dst += (int16_t)((uint16_t)(x ^ shift) >> 3);
    mask = (uint16_t)(0xffff >> shift);
    inv = (uint16_t)~mask;
    rnd = (uint16_t)random_next();
    do {
        noise = rnd;
        rnd = (uint16_t)(((rnd << 8) & 0xff00) | 0x0a);
        rnd = (uint16_t)(rnd - noise);
        rnd = (uint16_t)(rnd + rnd);
        rnd = (uint16_t)(rnd + rnd);
        rnd = (uint16_t)(rnd + noise);
        rnd = (uint16_t)(rnd + 1);
        noise = (uint16_t)((rnd >> 8) ^ rnd);
        head = (uint16_t)(noise & mask);
        wr16(dst, (uint16_t)(rd16(dst) & inv));
        wr16(dst + 0x28, (uint16_t)(rd16(dst + 0x28) & inv));
        wr16(dst + 0x50, (uint16_t)(rd16(dst + 0x50) & inv));
        wr16(dst + 0x78, (uint16_t)(rd16(dst + 0x78) & inv));
        wr16(dst, (uint16_t)(rd16(dst) | head));
        wr16(dst + 0x28, (uint16_t)(rd16(dst + 0x28) | head));
        wr16(dst + 0x50, (uint16_t)(rd16(dst + 0x50) | head));
        wr16(dst + 2, noise);
        wr16(dst + 0x2a, noise);
        wr16(dst + 0x52, noise);
        wr16(dst + 0x7a, 0);
        noise = (uint16_t)(noise & inv);
        wr16(dst + 4, (uint16_t)(rd16(dst + 4) & mask));
        wr16(dst + 0x2c, (uint16_t)(rd16(dst + 0x2c) & mask));
        wr16(dst + 0x54, (uint16_t)(rd16(dst + 0x54) & mask));
        wr16(dst + 0x7c, (uint16_t)(rd16(dst + 0x7c) & mask));
        wr16(dst + 4, (uint16_t)(rd16(dst + 4) | noise));
        wr16(dst + 0x2c, (uint16_t)(rd16(dst + 0x2c) | noise));
        wr16(dst + 0x54, (uint16_t)(rd16(dst + 0x54) | noise));
        dst += 0xa0;
    } while (n-- != 0);
}

/* shop $569BC: each record's picture: opening (state -9..-1) the pictures of SH_TAB_SPARK, closing the sparkle (1, 2), in between (more than 2) the puff and the item's animation */
void shop_records_draw(void)
{
    uint8_t *rec = GPTR(SH_RECS);
    uint16_t cnt = 0x14;

    do {
        int16_t st = (int16_t)rd16(rec);

        if (st > 0) {
            if (st <= 2) {
                if (st == 2 && (int16_t)xenon2.sfx_cmd < 0)
                    xenon2.sfx_cmd2 = 0xb;
                shop_sparkle(rec);
            } else {
                const uint8_t *pic = GPTR(sh_r32(SH_TAB_PUFF + 4u * (cnt & 1)));
                uint16_t px, py, adj;

                draw_bob_masked_at(rd16(rec + 2), rd16(rec + 4), pic);
                pic = GPTR(list_next(GADDR(rec) + 8));
                px = (uint16_t)(rd16(rec + 2) + rd16(pic));
                py = (uint16_t)(rd16(rec + 4) + rd16(pic + 2));
                adj = (uint16_t)((rd16(pic + 4) >> 1) - 0x10);
                px = (uint16_t)(px - adj);
                adj = (uint16_t)(((uint16_t)(rd16(pic + 6) + 1) >> 1) - 0xe);
                py = (uint16_t)(py - adj);
                draw_bob_masked_at(px, py, pic);
            }
        } else {
            if (st != (int16_t)0xfff8) {
                shop_sparkle(rec);
                if (rd16(rec) == 0)
                    goto next;
            }
            draw_bob_masked_at(rd16(rec + 2), rd16(rec + 4), GPTR(sh_r32(SH_TAB_SPARK + 4u * (uint16_t)(st + 9))));
        }
    next:
        rec += 0x10;
    } while (--cnt != 0);
}

/* shop $56848: the keeper's eyes and mouth blink: pictures at random times */
void shop_keeper_blink(void)
{
    if (sh_r8(SH_NO_BLINK) == 0) {
        if (sh_r8(SH_BLINK_A) != 0)
            goto eyes_on;
        if (((uint16_t)random_next() & 0x2654) != 0)
            goto mouth;
    }
    draw_bob_masked_at(0x107, 0x29, GPTR(SH_GFX_EYES_A));
eyes_on:
    draw_bob_masked_at(0xf7, 0x29, GPTR(SH_GFX_EYES_B));
    if (sh_r8(SH_BLINK_A) != 0)
        sh_w8(SH_BLINK_A, (uint8_t)(sh_r8(SH_BLINK_A) - 1));
mouth:
    if (sh_r8(SH_NO_BLINK) != 0)
        return;
    if ((int8_t)sh_r8(SH_BLINK_B) >= 0) {
        if (sh_r8(SH_BLINK_B) == 0) {
            if (((uint16_t)random_next() & 0x2654) != 0)
                return;
            sh_w8(SH_BLINK_B, 8);
        } else {
            sh_w8(SH_BLINK_B, (uint8_t)(sh_r8(SH_BLINK_B) - 1));
        }
        draw_bob_masked_at(0x107, 0x23, GPTR(SH_GFX_MOUTH_A));
    }
    draw_bob_masked_at(0xf5, 0x23, GPTR(SH_GFX_MOUTH_B));
}

/* shop $568E6: the door pillar of the right: the rows it still covers cleared, the rest of it drawn */
void shop_door_draw(void)
{
    uint8_t *dst;

    if (xenon2.shop_door_right == 0)
        return;
    dst = shop_clear_columns(xenon2.draw_screen + 0x475a, (uint16_t)(0x44 - xenon2.shop_door_right));
    shop_put_rows(GPTR(SH_PIC_SLIDE), dst, (uint16_t)(xenon2.shop_door_right - 1));
}

/* shop $5662A: the room: its pictures, the keeper's lights, the doors */
void shop_room_draw(void)
{
    const uint8_t *pic;
    uint8_t *dst;
    uint16_t rows;

    shop_put_rows(GPTR(SH_PIC_TOP), xenon2.draw_screen + 0x5ba, 0x33);
    shop_put_rows(GPTR(SH_PIC_MID), xenon2.draw_screen + 0x3b7a, 9);
    pic = GPTR(SH_PIC_LOW);
    if (xenon2.shop_keeper_talking != 0) {
        uint32_t rnd = random_next();
        int16_t tries = 3;

        for (;;) {
            int carry = rnd & 1;

            rnd = (rnd & 0xffff0000u) | ((rnd & 0xffff) >> 1);
            if (carry)
                break;
            if (--tries == -1)
                break;
            pic += 0x770;
        }
    }
    shop_put_rows(pic, xenon2.draw_screen + 0x263a, 0x21);
    shop_lights_draw();
    shop_keeper_blink();
    if (xenon2.shop_door_left == 0)
        return;
    if ((int16_t)xenon2.shop_door_left <= 8) {
        /* (the closed doors' pictures are drawn only when more than 8 rows have opened) */
    } else {
        dst = xenon2.draw_screen + 0x5ba;
        pic = (const uint8_t *)GPTR(SH_PIC_DOOR) + (int16_t)((uint16_t)(0x30 - xenon2.shop_door_left) * 0x38);
        rows = (uint16_t)(xenon2.shop_door_left - 9);
        shop_put_rows(pic, dst, rows);
        dst = xenon2.draw_screen + 0x46ba - (int16_t)((uint16_t)(xenon2.shop_door_left * 0xa0));
        shop_put_rows(GPTR(SH_PIC_DOOR2), dst, rows);
    }
    draw_bob_masked_at(0xd7, (uint16_t)(xenon2.shop_door_left + 1), GPTR(SH_GFX_BAR_1));
    draw_bob_masked_at(0xf7, (uint16_t)(xenon2.shop_door_left + 1), GPTR(SH_GFX_BAR_2));
    draw_bob_masked_at(0x117, (uint16_t)(xenon2.shop_door_left + 1), GPTR(SH_GFX_BAR_3));
    draw_bob_masked_at(0xd7, (uint16_t)(0x70 - xenon2.shop_door_left), GPTR(SH_GFX_BAR_4));
    draw_bob_masked_at(0xf7, (uint16_t)(0x70 - xenon2.shop_door_left), GPTR(SH_GFX_BAR_5));
    draw_bob_masked_at(0x117, (uint16_t)(0x70 - xenon2.shop_door_left), GPTR(SH_GFX_BAR_6));
    if ((int16_t)xenon2.shop_door_left >= 8)
        return;
    {
        uint8_t *dst = xenon2.draw_screen + 0x1a, *dst2 = xenon2.draw_screen + 0x41ba;
        const uint8_t *lamp_a = GPTR(SH_PIC_LAMP_A), *lamp_b = GPTR(SH_PIC_LAMP_B);
        int16_t n = 8;

        do {
            int g, p;

            for (g = 0; g < 7; g++) {
                for (p = 0; p < 4; p++) {
                    wr16(dst + p * 0x28, rd16(lamp_a));
                    lamp_a += 2;
                }
                for (p = 0; p < 4; p++) {
                    wr16(dst2 + p * 0x28, rd16(lamp_b));
                    lamp_b += 2;
                }
                dst += 2;
                dst2 += 2;
            }
            dst += 0x92;
            dst2 += 0x92;
            lamp_a += 8;
            lamp_b += 8;
        } while (n-- != 0);
    }
}

/* shop $5661C: the records and the room, the door */
void shop_room_update(void)
{
    shop_records_draw();
    shop_room_draw();
    shop_door_draw();
}

/* shop $5648C: the keeper's picture at SH_KX, SH_KY (clipped at line $68) */
void shop_keeper_draw(void)
{
    uint16_t saved = xenon2.clip_bottom;

    xenon2.clip_bottom = 0x68;
    draw_bob_masked_at(sh_r16(SH_KX), sh_r16(SH_KY), GPTR(sh_r32(SH_KGFX)));
    xenon2.clip_bottom = saved;
}

/* shop $564B2: `frames` frames of the room and the keeper */
void shop_hold(uint16_t frames)
{
    do {
        swap_screens_200();
        shop_records_tick();
        shop_records_draw();
        shop_room_draw();
        shop_keeper_draw();
    } while (--frames != 0);
}

/* shop $551E6: `frames` frames of the room (the records' sparkles run) */
void shop_wait(uint16_t frames)
{
    do {
        shop_room_draw();
        shop_records_tick();
        shop_records_draw();
        swap_screens_200();
    } while (--frames != 0);
}

/* shop $55360: the text board is cleared on both screens */
void shop_board_clear(void)
{
    shop_clear_columns(xenon2.screen_b + 0x475a, 0x44);
    shop_clear_columns(xenon2.screen_a + 0x475a, 0x44);
}

/* shop $56CA0: the board is cleared and the text begins again at its top */
void shop_text_home(void)
{
    shop_board_clear();
    xenon2.text_x = 0x37;
    xenon2.text_y = 0x76;
}

/* shop $56CB2: the cash is drawn */
void shop_cash_draw(void)
{
    draw_number(xenon2.player_money, xenon2.screen_b + 0x6b91);
}

/* shop $56CC0: the text (words, $0D = a new line, 0 ends) is drawn a character at a time, the keeper talks at the start of each word, every second character
 * a frame of the room goes by */
void shop_print(const uint8_t *text)
{
    for (;;) {
        uint16_t rnd, width;
        const uint8_t *p;

        xenon2.shop_keeper_talking = 0xff;
        do
            rnd = (uint16_t)random_next() & 3;
        while (rnd == 0);
        shop_sound_cmd((uint16_t)((rnd - 1) | 0x80));
        width = 0;
        for (p = text; *p != 0 && *p != 0x20 && *p != 0x0d; p++)
            width++;
        width = (uint16_t)(width + xenon2.text_x);
        if ((int16_t)width > 0x4b)
            shop_text_newline();
        for (;;) {
            uint8_t ch = *text++;

            if (ch == 0) {
                xenon2.shop_keeper_talking = 0;
                return;
            }
            if (ch == 0x20) {
                for (;;) {
                    xenon2.text_x++;
                    ch = *text++;
                    if (ch == 0) {
                        xenon2.shop_keeper_talking = 0;
                        return;
                    }
                    if (ch != 0x20)
                        break;
                }
                text--;
                break;
            }
            if (ch == 0x0d) {
                shop_text_newline();
                break;
            }
            shop_text_char(ch);
            if (xenon2.text_x & 1) {
                shop_records_tick();
                shop_records_draw();
                shop_room_draw();
                swap_screens_200();
            }
        }
    }
}

/* shop $56D56: the number in value (a word, decimal) is drawn, a frame goes by */
void shop_print_number(uint16_t value)
{
    xenon2.shop_keeper_talking = 0xff;
    if ((int16_t)value >= 10) {
        uint32_t q = (uint32_t)(int32_t)(int16_t)value;
        uint32_t d = (q / 10u) & 0xffff;
        uint32_t rem = q % 10u;

        shop_print_number((uint16_t)d);
        value = (uint16_t)rem;
    }
    shop_text_char((uint8_t)(value + 0x30));
    swap_screens_200();
    shop_records_tick();
    shop_records_draw();
    shop_room_draw();
    xenon2.shop_keeper_talking = 0;
}

/* shop $56040 (the cursor drawn), $560A6 (the cursor taken away): the picture of the cursor at the item under it, or of the buttons below the grid */
static void cursor_show(uint32_t on_item, uint32_t exit_a, uint32_t exit_b, uint32_t exit_c)
{
    uint16_t row = xenon2.shop_cursor_row, x, y, offset;
    const uint8_t *pic;

    if (row == 4) {
        x = 4;
        pic = GPTR(exit_a);
        if (xenon2.shop_cursor_col != 0) {
            x = 0x2d;
            pic = GPTR(exit_b);
            if (sh_r8(SH_SELLING) == 0)
                pic = GPTR(exit_c);
        }
        shop_draw_both(x, 0xa8, pic);
        return;
    }
    offset = (uint16_t)((uint16_t)(row * 5 + xenon2.shop_cursor_col) * 0x10);
    y = sh_r16(SH_RECS + (int16_t)offset + 4);
    x = sh_r16(SH_RECS + (int16_t)offset + 2);
    shop_draw_both((uint16_t)(x + 0xa), (uint16_t)(y + 0x1f), GPTR(on_item));
}

void shop_cursor_draw(void) { cursor_show(SH_GFX_CURSOR_ON_A, SH_GFX_EXIT_ON_A, SH_GFX_EXIT_ON_B, SH_GFX_EXIT_ON_C); }
void shop_cursor_erase(void) { cursor_show(SH_GFX_CURSOR_ON_B, SH_GFX_EXIT_OFF_A, SH_GFX_EXIT_OFF_B, SH_GFX_EXIT_OFF_C); }

/* shop $55F60: the cursor moved over the grid by the stick, every 5 frames, until the fire button (the room goes on) */
void shop_stick(void)
{
    for (;;) {
        if (sh_r16(SH_REPEAT) != 0) {
            sh_w16(SH_REPEAT, (uint16_t)(sh_r16(SH_REPEAT) - 1));
        } else if (xenon2.key_pressed != 0) {
            xenon2.key_pressed = 0;
            xenon2.sfx_cmd = 0xd;
            return;
        } else if (xenon2.joy_stick != 0) {
            xenon2.sfx_cmd = 0xc;
            sh_w16(SH_REPEAT, 4);
            shop_cursor_erase();
            if (xenon2.joy_stick & 4) {
                if (xenon2.shop_cursor_row == 4) {
                    xenon2.shop_cursor_col = (uint16_t)(xenon2.shop_cursor_col == 0);
                } else {
                    xenon2.shop_cursor_col--;
                    if ((int16_t)xenon2.shop_cursor_col < 0)
                        xenon2.shop_cursor_col = 4;
                }
            } else if (xenon2.joy_stick & 8) {
                if (xenon2.shop_cursor_row == 4) {
                    xenon2.shop_cursor_col = (uint16_t)(xenon2.shop_cursor_col == 0);
                } else {
                    xenon2.shop_cursor_col++;
                    if ((int16_t)xenon2.shop_cursor_col > 4)
                        xenon2.shop_cursor_col = 0;
                }
            } else if (xenon2.joy_stick & 1) {
                xenon2.shop_cursor_row--;
                if ((int16_t)xenon2.shop_cursor_row < 0)
                    xenon2.shop_cursor_row = 4;
            } else if (xenon2.joy_stick & 2) {
                xenon2.shop_cursor_row++;
                if ((int16_t)xenon2.shop_cursor_row > 4)
                    xenon2.shop_cursor_row = 0;
            }
            shop_cursor_draw();
        }
        shop_room_draw();
        shop_records_tick();
        shop_records_draw();
        swap_screens_200();
        xenon2.frame_tick++;
    }
}

/* shop $5593E: -1 if the item of the record at SH_REC can be bought (the ship has room for it), else 0 */
int32_t shop_can_buy(void)
{
    uint16_t kind = sh_r16(sh_r32(SH_REC) + 6);
    const Object *weapon;
    uint8_t flags;
    int i;

    switch (kind) {
    case 3: return xenon2.speed_level == 2 ? 0 : -1;
    case 4:
    case 7: return xenon2.player_energy == 0x27 ? 0 : -1;
    case 5: return xenon2.autofire_level == 3 ? 0 : -1;
    case 6: return xenon2.nashwan_timer != 0 ? 0 : -1;
    case 0xc:
        for (i = 0; i < 7; i++)
            if (xenon2.weapon_slots[i].type != 0 && xenon2.weapon_slots[i].obj->score != xenon2.weapon_slots[i].obj->energy)
                return -1;
        return 0;
    case 0x10: return xenon2.dive_charges != 0 ? 0 : -1;
    case 0x16: return xenon2.player_lives == 9 ? 0 : -1;
    case 0x18: return xenon2.protection != 0 ? 0 : -1;
    case 0x19: return xenon2.shades_timer != 0 ? 0 : -1;
    }
    flags = sh_r8(SH_TAB_FLAGS + (uint16_t)(kind - 1));
    if (flags & 1) {
        weapon = xenon2.weapon_slots[0].obj;
        if (kind != (uint16_t)weapon->kind) {
            if (xenon2.weapon_slots[0].type == 0 || xenon2.weapon_slots[0].type == 0x94)
                return -1;
            return 0;
        }
    } else if (flags & 2) {
        for (i = 1; i <= 4; i++)
            if (xenon2.weapon_slots[i].type == 0)
                return -1;
        return 0;
    } else if (flags & 4) {
        if (xenon2.weapon_slots[5].type == 0)
            return -1;
        weapon = xenon2.weapon_slots[5].obj;
        if (kind != (uint16_t)weapon->kind)
            return 0;
    } else if (flags & 0x10) {
        if (xenon2.weapon_slots[6].type == 0 || xenon2.weapon_slots[5].type == 0)
            return -1;
        return 0;
    } else if (flags & 8) {
        if (xenon2.weapon_slots[6].type == 0)
            return -1;
        weapon = xenon2.weapon_slots[6].obj;
        if (kind != (uint16_t)weapon->kind)
            return 0;
    } else {
        return -1;
    }
    return (uint16_t)weapon->score != (uint16_t)weapon->energy ? -1 : 0;
}

/* shop $558E4, $558EC, $55930, $55938: the items that are not a weapon */
void shop_item_ca2(void) { xenon2.nashwan_timer = 0xaa; }

/* shop $558EC: the keeper's advice: the next sentence for the level and the number of hints so far */
void shop_item_advice(void)
{
    uint16_t index;

    sh_w8(SH_BLINK_A, 0xc);
    shop_text_home();
    index = (uint16_t)((uint16_t)(xenon2.level_number - 1) * 0x18 + xenon2.advice_hints);
    if (xenon2.advice_hints != 8 && xenon2.advice_hints != 0x14)
        xenon2.advice_hints += 4;
    shop_print((const uint8_t *)GPTR(sh_r32(SH_TAB_ADVICE + (int16_t)index)));
    sh_w8(SH_BLINK_A, 0);
}

void shop_item_c90(void) { xenon2.shades_timer = 0xdc; }
void shop_item_cca(void) { xenon2.player_lives++; }

/* the routine the item is granted by (the table at $5573A: the pickup and weapon routines of the base game, and the four above) */
static void item_grant(uint16_t kind)
{
    switch (kind) {
    case 1: obj_nop(0); break;
    case 2: shop_item_advice(); break;
    case 3: pickup_move(0); break;
    case 4: pickup_shield_small(0); break;
    case 5: pickup_speed(0); break;
    case 6: shop_item_ca2(); break;
    case 7: pickup_shield_big(0); break;
    case 8: weapon_44(0, 0); break;
    case 9: weapon_38(0, 0); break;
    case 10: weapon_98(0, 0); break;
    case 11: weapon_4c(0, 0); break;
    case 12: pickup_grow_weakest(0); break;
    case 13: weapon_3c(0, 0); break;
    case 14: weapon_c0(0, 0); break;
    case 15: weapon_34(0, 0); break;
    case 16: pickup_ca0(0); break;
    case 17: weapon_90(0, 0); break;
    case 18: weapon_20(0, 0); break;
    case 19: weapon_2c(0, 0); break;
    case 20: weapon_b0(0, 0); break;
    case 21: weapon_28(0, 0); break;
    case 22: shop_item_cca(); break;
    case 23: weapon_48(0, 0); break;
    case 24: pickup_flag_d83(0); break;
    case 25: shop_item_c90(); break;
    }
}

/* shop $55DE8: the item's name is shown; while selling, the power of the weapon (power + 1) for the ones that have one */
void shop_item_name(uint16_t kind, uint16_t power)
{
    shop_text_home();
    shop_print((const uint8_t *)GPTR(sh_r32(SH_TAB_NAMES + (uint16_t)(kind << 2))));
    if (sh_r8(SH_SELLING) == 0)
        return;
    if (kind == 1 || kind == 0x13 || kind == 0x12 || kind == 8 || kind == 0xa || kind == 0xe || kind == 9 || kind == 0xd) {
        shop_print((const uint8_t *)GPTR(SH_TXT_POWER));
        shop_print_number((uint16_t)(power + 1));
    }
}

/* shop $55C60: "THAT WILL COST" and the price of the item of the record (halved on the level 5 setting) */
void shop_price_text(uint8_t *rec)
{
    uint16_t price;

    sh_w32(SH_REC, GADDR(rec));
    shop_print((const uint8_t *)GPTR(SH_TXT_COST));
    rec = (uint8_t *)GPTR(sh_r32(SH_REC));
    price = sh_r16(SH_TAB_PRICE + (uint16_t)(rd16(rec + 6) * 2) - 2);
    if (xenon2.level_number == 5)
        price >>= 1;
    sh_w16(SH_PRICE, price);
    shop_print_number(price);
}

/* the price of item price (1 on) on this setting */
static uint16_t item_price_at(uint32_t table_entry)
{
    uint16_t price = sh_r16(table_entry);

    if (xenon2.level_number == 5)
        price >>= 1;
    return price;
}

/* shop $55424: the records of the page of the stock: an item that is affordable (price not above the cash and not above the limit $9FA) is at its animation, the others
 * are sold out ($1A); a page that runs out of items is padded with empty records, the last record is the next page ($1B) unless it is the first page and ends there */
void shop_stock_update(void)
{
    uint32_t rec, price_ptr, anim_ptr;
    uint16_t price, item;
    int32_t cash;
    int16_t cost;

    price = (uint16_t)(sh_r16(SH_PAGE) * 2);
    price = sh_r16(SH_TAB_PRICE + 2 + (int16_t)price);
    if (xenon2.level_number == 5)
        price >>= 1;
    if ((int32_t)(int16_t)price > (int32_t)xenon2.player_money)
        sh_w16(SH_PAGE, 0);
    rec = SH_RECS;
    item = (uint16_t)(sh_r16(SH_PAGE) + 2);
    price_ptr = SH_TAB_PRICE + (uint32_t)(int32_t)(int16_t)(uint16_t)((uint16_t)(item - 1) * 2);
    anim_ptr = SH_TAB_ANIM + (uint32_t)(int32_t)(int16_t)(uint16_t)((uint16_t)(item - 1) * 4);
    cash = (int32_t)xenon2.player_money;
    for (;;) {
        cost = (int16_t)item_price_at(price_ptr);
        if (cost > (int16_t)xenon2.shop_price_cap || cash < (int32_t)cost) {
sold:
            sh_w16(rec + 6, 0x1a);
            sh_w32(rec + 8, SH_LIST_SOLD_OUT);
        } else {
            sh_w16(rec + 6, item);
            sh_w32(rec + 8, sh_r32(anim_ptr));
        }
        item++;
        price_ptr += 2;
        anim_ptr += 4;
        rec += 0x10;
        if (item == 0x1a) {
            while (rec != SH_RECS_END) {
                sh_w16(rec + 6, 0);
                rec += 0x10;
            }
            if (sh_r16(SH_PAGE) == 0) {
                sh_w16(rec + 6, 0);
                return;
            }
            break;
        }
        if (rec < SH_RECS_END)
            continue;
        if (rec > SH_RECS_END)
            return;
        if (sh_r16(SH_PAGE) == 0) {
            cost = (int16_t)item_price_at(price_ptr);
            if (cost > (int16_t)xenon2.shop_price_cap || cash < (int32_t)cost)
                goto sold;
        }
        break;
    }
    sh_w16(rec + 6, 0x1b);
    sh_w32(rec + 8, SH_LIST_NEXT);
}

/* shop $55512: every record but the last is sold out */
void shop_stock_sold_out(void)
{
    uint32_t rec = SH_RECS;

    do {
        sh_w16(rec + 6, 0x1a);
        sh_w32(rec + 8, SH_LIST_SOLD_OUT);
        rec += 0x10;
    } while (rec < SH_RECS_END);
}

/* shop $55534: after a purchase the items that cost more than the cash left are sold out, and the next page record becomes sold out when the one before it is */
void shop_stock_recheck(void)
{
    uint32_t rec = SH_RECS;
    int32_t cash = (int32_t)xenon2.player_money;
    int16_t n = 0x13;

    do {
        uint16_t item = sh_r16(rec + 6);

        if (item != 0) {
            int sold = 0;

            if (item == 0x1b) {
                if (sh_r16(rec - 0xa) == 0x1a)
                    sold = 1;
            } else if ((int16_t)item < 0x1a) {
                uint16_t price = sh_r16(SH_TAB_PRICE - 2 + (uint16_t)(item * 2));

                if (xenon2.level_number == 5)
                    price >>= 1;
                if (!(cash >= (int32_t)(int16_t)price))
                    sold = 1;
            }
            if (sold) {
                sh_w16(rec + 6, 0x1a);
                sh_w32(rec + 8, SH_LIST_SOLD_OUT);
            }
        }
        rec += 0x10;
    } while (n-- != 0);
}

/* shop $55AFC: the records of the player's own weapons: the objects of the ship's list, each with the animation of its kind (the ones of kind $94 are not for sale) */
void shop_sell_list(void)
{
    uint32_t rec = SH_RECS;
    Object *weapon = xenon2.obj_list[1].prev;

    while (weapon->type != 0) {
        if (weapon->type != 0x94) {
            uint16_t kind = (uint16_t)weapon->kind;

            sh_w16(rec + 6, kind);
            sh_w32(rec + 0xc, GADDR(weapon));
            sh_w32(rec + 8, sh_r32(SH_TAB_ANIM - 4 + (uint16_t)(kind * 4)));
            rec += 0x10;
        }
        weapon = weapon->prev;
    }
    do {
        sh_w16(rec + 6, 0);
        rec += 0x10;
    } while (rec <= SH_RECS_END);
}

/* the record under the cursor (the grid is 5 wide) */
static uint8_t *cursor_record(void)
{
    uint16_t index = (uint16_t)(xenon2.shop_cursor_row * 5 + xenon2.shop_cursor_col);

    return (uint8_t *)GPTR(SH_RECS + (int16_t)(uint16_t)(index * 0x10));
}

/* shop $55C9A: the keeper shows the item (his picture goes up, blinks, comes down), and what the keeper pays for it is shown: (price of the item + $7D0 times its power) / 2 */
void shop_sell_show(uint8_t *rec)
{
    uint16_t price, bonus;
    const Object *weapon;

    sh_w32(SH_REC, GADDR(rec));
    sh_w16(SH_KX, 0xf7);
    sh_w16(SH_KY, 0x78);
    sh_w32(SH_KGFX, SH_GFX_KEEPER_A);
    do {
        swap_screens_200();
        shop_room_draw();
        shop_records_tick();
        shop_records_draw();
        shop_keeper_draw();
        sh_w16(SH_KY, (uint16_t)(sh_r16(SH_KY) - 2));
    } while ((int16_t)sh_r16(SH_KY) > 0x68);
    shop_hold(3);
    sh_w32(SH_KGFX, SH_GFX_KEEPER_B); shop_hold(3);
    sh_w32(SH_KGFX, SH_GFX_KEEPER_A); shop_hold(3);
    sh_w32(SH_KGFX, SH_GFX_KEEPER_C); shop_hold(3);
    sh_w32(SH_KGFX, SH_GFX_KEEPER_A); shop_hold(3);
    sh_w32(SH_KGFX, SH_GFX_KEEPER_B); shop_hold(3);
    sh_w32(SH_KGFX, SH_GFX_KEEPER_A); shop_hold(3);
    sh_w32(SH_KGFX, SH_GFX_KEEPER_C); shop_hold(3);
    sh_w32(SH_KGFX, SH_GFX_KEEPER_A); shop_hold(3);
    do {
        swap_screens_200();
        shop_room_draw();
        shop_records_tick();
        shop_records_draw();
        shop_keeper_draw();
        sh_w16(SH_KY, (uint16_t)(sh_r16(SH_KY) + 2));
    } while ((int16_t)sh_r16(SH_KY) < 0x80);
    shop_print((const uint8_t *)GPTR(SH_TXT_PAY));
    rec = (uint8_t *)GPTR(sh_r32(SH_REC));
    price = sh_r16(SH_TAB_PRICE - 2 + (uint16_t)(rd16(rec + 6) * 2));
    weapon = (const Object *)GPTR(rd32(rec + 0xc));
    bonus = (uint16_t)(weapon->score * 0x7d0);
    price = (uint16_t)(price + bonus);
    price >>= 1;
    sh_w16(SH_PRICE, price);
    shop_print_number(price);
}

/* shop $55B48: the player sells: the weapons on the ship are the records; the fire button on one shows it and what the keeper pays, a second one on the same
 * sells it (the cash is paid and the weapon goes); the button below the grid leaves */
void shop_sell(void)
{
    xenon2.shop_cursor_col = 0;
    xenon2.shop_cursor_row = 0;
    sh_w16(SH_LAST_X, 0xffff);
    shop_cursor_draw();
    sh_w16(SH_PRICE, 0xffff);
    for (;;) {
        xenon2.key_pressed = 0;
        shop_stick();
        if (xenon2.shop_cursor_row == 4) {
            if (xenon2.shop_cursor_col == 0) {
                shop_cursor_erase();
                return;
            }
        } else if (!(xenon2.shop_cursor_col == sh_r16(SH_LAST_X) && xenon2.shop_cursor_row == sh_r16(SH_LAST_Y))) {
            uint8_t *rec = cursor_record();
            uint16_t kind = rd16(rec + 6);

            if (kind == 0)
                continue;
            sh_w16(SH_LAST_X, xenon2.shop_cursor_col);
            sh_w16(SH_LAST_Y, xenon2.shop_cursor_row);
            sh_w8(SH_BLINK_B, 0xff);
            shop_item_name(kind, (uint16_t)((const Object *)GPTR(rd32(rec + 0xc)))->score);
            shop_sell_show(rec);
            sh_w8(SH_BLINK_B, 0);
            continue;
        }
        sh_w16(SH_LAST_X, 0xffff);
        if ((int16_t)sh_r16(SH_PRICE) < 0)
            continue;
        xenon2.player_money += (uint32_t)(int32_t)(int16_t)sh_r16(SH_PRICE);
        sh_w16(SH_PRICE, 0xffff);
        shop_cash_draw();
        {
            uint8_t *rec = (uint8_t *)GPTR(sh_r32(SH_REC));

            wr16(rec + 6, 0);
            kill_with_slot((Object *)GPTR(rd32(rec + 0xc)));
        }
        shop_text_home();
        shop_print((const uint8_t *)GPTR(SH_TXT_HERE_CASH));
    }
}

/* shop $55594: the player buys: the items on offer are the records; the fire button on one shows its name and price, a second one on the same buys it (the cash is
 * counted down and the item is given); the button below the grid leaves */
void shop_buy(void)
{
    xenon2.shop_cursor_col = 0;
    xenon2.shop_cursor_row = 0;
    sh_w16(SH_LAST_X, 0xffff);
    shop_cursor_draw();
    sh_w16(SH_PRICE, 0xffff);
    for (;;) {
        xenon2.key_pressed = 0;
        shop_stick();
        if (xenon2.shop_cursor_row == 4) {
            if (xenon2.shop_cursor_col == 0) {
                shop_cursor_erase();
                return;
            }
        } else if (!(xenon2.shop_cursor_col == sh_r16(SH_LAST_X) && xenon2.shop_cursor_row == sh_r16(SH_LAST_Y))) {
            uint8_t *rec = cursor_record();
            uint16_t kind = rd16(rec + 6);

            if (kind == 0)
                continue;
            if (kind == 0x1a) {
                shop_text_home();
                shop_print((const uint8_t *)GPTR(SH_TXT_OUT_OF_STOCK));
                sh_w16(SH_PRICE, 0xffff);
                continue;
            }
            if (kind == 0x1b) {
                uint16_t page = (uint16_t)(sh_r16(SH_PAGE) + 0x13);

                if ((int16_t)page >= 0x19)
                    page = 0;
                sh_w16(SH_PAGE, page);
                shop_stock_update();
                sh_w16(SH_PRICE, 0xffff);
                continue;
            }
            sh_w16(SH_LAST_X, xenon2.shop_cursor_col);
            sh_w16(SH_LAST_Y, xenon2.shop_cursor_row);
            shop_item_name(kind, (uint16_t)(xenon2.shop_cursor_row * 5));
            shop_price_text(rec);
            continue;
        }
        sh_w16(SH_LAST_X, 0xffff);
        if ((int16_t)sh_r16(SH_PRICE) < 0)
            continue;
        if (shop_can_buy() == 0) {
            shop_text_home();
            shop_print((const uint8_t *)GPTR(SH_TXT_NO_ROOM));
            sh_w16(SH_PRICE, 0xffff);
            continue;
        }
        shop_text_home();
        shop_print((const uint8_t *)GPTR(SH_TXT_GIVE_CASH));
        {
            int32_t price = (int32_t)(int16_t)sh_r16(SH_PRICE);
            const uint8_t *rec;

            while (price >= 0x64) {
                xenon2.sfx_cmd = 0xd;
                xenon2.player_money -= 0x64;
                price -= 0x64;
                shop_cash_draw();
                shop_room_draw();
                shop_records_tick();
                shop_records_draw();
                swap_screens_200();
            }
            xenon2.player_money -= (uint32_t)price;
            rec = (const uint8_t *)GPTR(sh_r32(SH_REC));
            item_grant(rd16(rec + 6));
            sh_w16(SH_PRICE, 0xffff);
            shop_cash_draw();
            shop_stock_recheck();
            rec = (const uint8_t *)GPTR(sh_r32(SH_REC));
            if (rd16(rec + 6) != 2)
                shop_print((const uint8_t *)GPTR(SH_TXT_WHAT_ELSE));
        }
    }
}

/* shop $5530C ($4EC) and $55324 ($4EE): the two pillars of the doorway at their place, their bit 2 selects the picture */
static void pillars_draw(uint16_t y, uint16_t state)
{
    uint32_t gfx = (state & 4) ? SH_GFX_PILLAR_B : SH_GFX_PILLAR_A;

    shop_draw_both(0xe2, y, GPTR(gfx));
    shop_draw_both(0x102, y, GPTR(gfx));
    shop_draw_both(0x122, y, GPTR(gfx));
}

void shop_pillars_a(void) { pillars_draw(4, xenon2.shop_door_left); }
void shop_pillars_b(void) { pillars_draw(0x6d, xenon2.shop_door_right); }

/* shop $5504E */
void shop_delay(void)
{
#ifdef GAME_SDL
    for (int i = 0; i < 26; i++)   /* (0x30D40 passes of a 18-cycle loop: half a second of frames, the interrupts running) */
        HOST_IDLE();
#else
    volatile uint32_t n;

    for (n = 0x30d40; n != 0; n--)
        ;
#endif
}

/* shop $55264: the doorway opens: the pillars slide, a sound, until both are home; then the text board is cleared */
void shop_slide_in(void)
{
    do {
        swap_screens_200();
        if (xenon2.shop_door_left != 0) {
            xenon2.shop_door_left -= 4;
            if ((int16_t)xenon2.shop_door_left < 0 || xenon2.shop_door_left == 0) {
                shop_sound_cmd(0x83);
                xenon2.shop_door_left = 0;
            }
        }
        if (xenon2.shop_door_right != 0) {
            xenon2.shop_door_right -= 4;
            if (xenon2.shop_door_right == 0 || (int16_t)xenon2.shop_door_right < 0) {
                if ((int16_t)xenon2.shop_door_right < 0)
                    xenon2.shop_door_right = 0;
                xenon2.sfx_cmd = 0;
            }
        }
        shop_room_update();
        shop_pillars_a();
        shop_pillars_b();
    } while (xenon2.shop_door_left != 0 || xenon2.shop_door_right != 0);
    shop_board_clear();
}

/* shop $552BA: the doorway closes: the pillars slide back */
void shop_slide_out(void)
{
    shop_board_clear();
    do {
        swap_screens_200();
        if (xenon2.shop_door_right != 0x44) {
            xenon2.shop_door_right += 4;
            if ((int16_t)xenon2.shop_door_right > 0x44)
                xenon2.shop_door_right = 0x44;
        }
        if (xenon2.shop_door_left != 0x30) {
            xenon2.shop_door_left += 4;
            if ((int16_t)xenon2.shop_door_left > 0x30)
                xenon2.shop_door_left = 0x30;
        }
        shop_room_update();
        shop_pillars_b();
        shop_pillars_a();
    } while (xenon2.shop_door_right != 0x44);
}

/* shop $564CE: the keeper walks to the counter, talks, and goes back */
void shop_keeper_in(void)
{
    sh_w16(SH_KX, 0x106);
    sh_w16(SH_KY, 0x88);
    sh_w32(SH_KGFX, SH_GFX_KEEPER_A);
    for (;;) {
        swap_screens_200();
        shop_records_tick();
        shop_records_draw();
        shop_room_draw();
        shop_keeper_draw();
        if (sh_r16(SH_KX) == 0x11c)
            break;
        sh_w16(SH_KX, (uint16_t)(sh_r16(SH_KX) + 2));
        sh_w16(SH_KY, (uint16_t)(sh_r16(SH_KY) - 2));
    }
    sh_w32(SH_KGFX, SH_GFX_KEEPER_C);
    shop_hold(6);
    shop_sound_stop();
    sh_w8(SH_NO_BLINK, 0);
    sh_w32(SH_KGFX, SH_GFX_KEEPER_A);
    shop_hold(6);
    do {
        swap_screens_200();
        shop_records_tick();
        shop_records_draw();
        shop_room_draw();
        shop_keeper_draw();
        sh_w16(SH_KX, (uint16_t)(sh_r16(SH_KX) - 2));
        sh_w16(SH_KY, (uint16_t)(sh_r16(SH_KY) + 2));
    } while ((int16_t)sh_r16(SH_KX) > 0x106);
}

/* shop $56568: the items appear: the records open one frame after the other, then four frames of the finished room */
void shop_open(void)
{
    uint32_t rec;
    int16_t n;

    for (;;) {
        swap_screens_200();
        for (rec = SH_RECS, n = 0x13; n >= 0; n--, rec += 0x10)
            sh_w16(rec, (uint16_t)(sh_r16(rec) + 1));
        if (sh_r16(SH_RECS) == 0)
            break;
        shop_records_draw();
        shop_room_draw();
    }
    for (rec = SH_RECS, n = 0x13; n >= 0; n--, rec += 0x10)
        sh_w16(rec, 1);
    n = 4;
    for (;;) {
        shop_records_draw();
        shop_room_draw();
        if (--n == 0)
            break;
        swap_screens_200();
    }
}

/* shop $565C2: the items go away: the records close one frame after the other, then ten frames of the room */
void shop_close(void)
{
    uint32_t rec;
    int16_t n;

    for (rec = SH_RECS, n = 0x13; n >= 0; n--, rec += 0x10)
        sh_w16(rec, 0);
    for (;;) {
        swap_screens_200();
        for (rec = SH_RECS, n = 0x13; n >= 0; n--, rec += 0x10)
            sh_w16(rec, (uint16_t)(sh_r16(rec) - 1));
        if (sh_r16(SH_RECS) == 0xfff7)
            break;
        shop_records_draw();
        shop_room_draw();
    }
    n = 10;
    for (;;) {
        shop_records_draw();
        shop_room_draw();
        if (--n == 0)
            break;
        swap_screens_200();
    }
}

/* shop $55198: the screen is cleared and two rows of the picture of the end of the broadcast are drawn */
void shop_blank_screen(void)
{
    uint8_t *dst = xenon2.draw_screen;
    int16_t n = 0x1f3f;

    do {
        wr32(dst, 0);
        dst += 4;
    } while (n-- != 0);
    dst = xenon2.draw_screen + 0x3e94;
    wr16(dst, 0xf);
    wr16(dst + 0x28, 6);
    wr16(dst + 0x50, 0xf);
    wr16(dst + 0xa0, 0xf);
    wr16(dst + 0xc8, 0xf);
    wr16(dst + 0xf0, 0xf);
    wr16(dst + 0x140, 0xf);
    wr16(dst + 0x168, 6);
    wr16(dst + 0x190, 0xf);
}

/* the weapon slots' latch of the level ($352E) without its test of nashwan_timer: type and score of each of the 7 slots to slot_snapshot; an empty slot repeats last_score */
static void latch_slots_all(uint16_t last_score)
{
    uint16_t *out = xenon2.slot_snapshot;
    int i;

    for (i = 0; i < 7; i++) {
        *out++ = xenon2.weapon_slots[i].type;
        if (xenon2.weapon_slots[i].type != 0)
            last_score = (uint16_t)xenon2.weapon_slots[i].obj->score;
        *out++ = last_score;
    }
}

/* shop $55016: the shop is left: the fade, the level's screen, weapons, sound and interrupt handlers are given back */
void shop_leave(void)
{
    palette_fade_out(2);
    work_area_restore();
    xenon2.clip_bottom = 0xbf;
    refill_slot0(0, 0);
    latch_slots_all(0);
    shop_sound_stop();
    shop_sound_reset();
    xenon2.music_tick_fn = music_tick;
    xenon2.music_cmd_fn = (irq_fn)(void *)sfx_play;
    xenon2.advice_hints = 0xc;
}

/* shop $5505A: the last words of the broadcast, the doorway closes, the screen goes dark in flickering colours */
void shop_end_of_broadcast(void)
{
    int16_t n;

    shop_print((const uint8_t *)GPTR(SH_TXT_THATS_IT));
    shop_wait(0x1e);
    shop_print((const uint8_t *)GPTR(SH_TXT_FORGET));
    shop_wait(0x1e);
    shop_text_home();
    shop_print((const uint8_t *)GPTR(SH_TXT_GOOD_ENOUGH));
    shop_wait(0x11);
    shop_close();
    xenon2.sfx_cmd = 0x16;
    shop_slide_out();
    xenon2.sfx_cmd = 8;
    swap_screens_200();
    shop_delay();
    shop_room_update();
    swap_screens_200();
    shop_room_update();
    palette_fade_out(2);
    xenon2.sfx_cmd = 0x12;
    for (n = 0x96; n != 0; n--) {
        uint32_t c[8];
        int i;

        shop_blank_screen();
        swap_screens_200();
        for (i = 0; i < 8; i++) {
            c[i] = sh_r32(SH_PALETTE_END + 4u * i);
            xenon2.palette_shadow[2 * i] = (uint16_t)(c[i] >> 16);
            xenon2.palette_shadow[2 * i + 1] = (uint16_t)c[i];
        }
        for (i = 0; i < 8; i++) {
            c[i] += c[i];
            custom->color[2 * i] = (uint16_t)(c[i] >> 16);
            custom->color[2 * i + 1] = (uint16_t)c[i];
        }
    }
    palette_fade_out(4);
    shop_sound_reset();
    for (n = 0x50; n != 0; n--)
        swap_screens_200();
}

/* shop $54E00: the shop: the room is drawn, the doorway opens, the player sells and then buys, the doorway closes and the level's code, sound and
 * weapons are given back */
void shop_main(void)
{
    uint8_t *screen;
    uint32_t pic, rec;
    int16_t x, y, i;
    int32_t n;

    xenon2.clip_bottom = 0xc7;
    work_area_save();
    xenon2.music_tick_fn = shop_irq_tick;
    xenon2.music_cmd_fn = (irq_fn)(void *)shop_sound_cmd;
    sh_w8(SH_NTSC, 0);
    if (!(CIAA_CRA & 0x80))
        sh_w8(SH_NTSC, 0xff);
    screen = xenon2.draw_screen;
    for (n = 0; n < 0x1f40; n++)
        wr32(screen + 4 * n, 0);
    for (y = 0; y < 0xa4; y = (int16_t)(y + 0x29))
        for (x = 0; x < 0xcd; x = (int16_t)(x + 0x29)) {
            draw_bob_masked_at((uint16_t)x, (uint16_t)y, GPTR(SH_GFX_BEDROOM));
            draw_bob_masked_at((uint16_t)x, (uint16_t)y, GPTR(SH_GFX_BEDROOM2));
        }
    for (pic = SH_TAB_PICS; sh_r32(pic) != 0; pic += 10)
        shop_copy_block((const uint8_t *)GPTR(sh_r32(pic)), xenon2.draw_screen + (int16_t)sh_r16(pic + 4), sh_r16(pic + 6), sh_r16(pic + 8));
    if (xenon2.cur_player != 0)
        draw_bob_masked_at(0xc1, 0xbc, GPTR(SH_GFX_PLAYER2));
    screen = xenon2.draw_screen;
    for (n = 0; n < 0x1f40; n++)
        wr32(xenon2.show_screen + 4 * n, rd32(screen + 4 * n));
    sh_w8(SH_SELLING, 0xff);
    sh_w8(SH_NO_BLINK, 0xff);
    sh_w8(SH_BLINK_A, 0);
    sh_w16(SH_PAGE, 0);
    for (rec = SH_RECS, i = 0x13; i >= 0; i--, rec += 0x10)
        sh_w16(rec, 0xfff7);
    xenon2.shop_door_left = 0x30;
    xenon2.shop_door_right = 0x44;
    xenon2.shop_keeper_talking = 0;
    xenon2.shop_keeper_aux = 0;
    xenon2.shop_cursor_col = 1;
    xenon2.shop_cursor_row = 4;
    shop_cursor_erase();
    shop_cash_draw();
    shop_room_update();
    swap_screens_200();
    shop_room_update();
    {
        uint16_t colors[16];

        for (i = 0; i < 16; i++)
            colors[i] = sh_r16(SH_PALETTE + 2u * (uint16_t)i);
        palette_fade_in(colors);
    }
    xenon2.sfx_cmd = 8;
    shop_delay();
    xenon2.sfx_cmd = 0x16;
    shop_slide_in();
    if (xenon2.shop_closing)
        shop_stock_sold_out();
    else
        shop_sell_list();
    shop_open();
    shop_keeper_in();
    shop_text_home();
    if (xenon2.shop_closing) {
        shop_end_of_broadcast();
    } else {
        const uint8_t *text = (const uint8_t *)GPTR(SH_TXT_SELL);

        if (xenon2.level_number == 5) {
            shop_print((const uint8_t *)GPTR(SH_TXT_WELCOME_A));
            shop_hold(0x11);
            shop_text_home();
            shop_print((const uint8_t *)GPTR(SH_TXT_WELCOME_B));
            shop_hold(0x11);
            shop_text_home();
            text = (const uint8_t *)GPTR(SH_TXT_LAST_CHANCE);
        }
        shop_print(text);
        shop_sell();
        shop_close();
        sh_w8(SH_SELLING, 0);
        xenon2.shop_cursor_col = 1;
        xenon2.shop_cursor_row = 4;
        shop_cursor_erase();
        shop_stock_update();
        shop_open();
        shop_text_home();
        shop_print((const uint8_t *)GPTR(SH_TXT_BUY));
        shop_buy();
        shop_close();
        xenon2.sfx_cmd = 0x16;
        shop_slide_out();
        xenon2.sfx_cmd = 8;
        swap_screens_200();
        shop_delay();
        shop_room_update();
        swap_screens_200();
        shop_room_update();
    }
    shop_leave();
}
