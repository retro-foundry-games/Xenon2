/* The shop (see shop.h): "COLIN'S BARGAIN BASEMENT".  The blob it is loaded as holds the program ($54E00-$56EB6), the sound ($6477C-$64D4A, shop_sound.c),
 * and the pictures and tables; its own variables, tables and records are the members of shop_data (blobs/shop.blob).  The records, shop_data.recs (20 of 16 bytes, a 5 x 4 grid of
 * the items on offer or on the ship): word 0 = frames until the sparkle (or the opening animation's state), 2 = x, 4 = y, 6 = item (0 empty, 1-$19;
 * $1A sold out, $1B next page), 8 = the animation list, $C = the object of the ship the item is (selling). */
#include "shop.h"
#include "game_all.h"

/* ---- drawing helpers ---- */

/* shop $553F8: a picture on both screens (draw_bob_masked_at) */
void shop_draw_both(uint16_t x, uint16_t y, const uint16_t *gfx)
{
    uint8_t *saved = xenon2.draw_screen;

    xenon2.draw_screen = (uint8_t *)xenon2.screen_b;
    draw_bob_masked_at(x, y, gfx);
    xenon2.draw_screen = (uint8_t *)xenon2.screen_a;
    draw_bob_masked_at(x, y, gfx);
    xenon2.draw_screen = saved;
}

/* shop $5537A, $553F2: `rows` rows of 7 words (the first word keeps its top 7 bits, the next five are cleared, the last keeps its low 9 bits) of 4 interleaved planes;
 * returns the address after the rows */
uint8_t *shop_clear_columns(uint8_t *dst, uint16_t rows)
{
    uint16_t *d = (uint16_t *)dst;

    while (rows-- != 0) {
        int p;

        for (p = 0; p < 4; p++) {
            uint16_t *b = d + p * SCREEN_PLANE_WORDS;
            int c;

            b[0] = (uint16_t)(b[0] & 0xfe00);
            for (c = 1; c <= 5; c++)
                b[c] = 0;
            b[6] = (uint16_t)(b[6] & 0x01ff);
        }
        d += SCREEN_ROW_WORDS;
    }
    return (uint8_t *)d;
}

/* shop $55200: a block of w+1 words by h+1 rows of 4 planes from src to dst (a row is $A0 bytes) */
void shop_copy_block(const uint16_t *src, uint8_t *dst, uint16_t w, uint16_t h)
{
    const struct Cell4 *s = (const struct Cell4 *)src;
    uint16_t *d = (uint16_t *)dst;

    do {
        uint16_t *next = d + SCREEN_ROW_WORDS;
        uint16_t n = w;

        do {
            d[0] = s->plane[0];
            d[SCREEN_PLANE_WORDS] = s->plane[1];
            d[2 * SCREEN_PLANE_WORDS] = s->plane[2];
            d[3 * SCREEN_PLANE_WORDS] = s->plane[3];
            s++;
            d++;
        } while (n-- != 0);
        d = next;
    } while (h-- != 0);
}

/* shop $56912: rows+1 rows of 28 words of a picture over the screen: 7 columns of 4 planes, the first and the last mixed in under masks */
void shop_put_rows(const uint16_t *src, uint8_t *dst, uint16_t rows)
{
    uint16_t *d = (uint16_t *)dst;

    do {
        int p, c;

        for (p = 0; p < 4; p++)
            d[p * SCREEN_PLANE_WORDS] = (uint16_t)(d[p * SCREEN_PLANE_WORDS] & 0xfe00);
        for (p = 0; p < 4; p++) {
            d[p * SCREEN_PLANE_WORDS] = (uint16_t)(d[p * SCREEN_PLANE_WORDS] | *src++);
        }
        for (c = 1; c <= 5; c++)
            for (p = 0; p < 4; p++) {
                d[c + p * SCREEN_PLANE_WORDS] = *src++;
            }
        for (p = 0; p < 4; p++)
            d[6 + p * SCREEN_PLANE_WORDS] = (uint16_t)(d[6 + p * SCREEN_PLANE_WORDS] & 0x01ff);
        for (p = 0; p < 4; p++) {
            d[6 + p * SCREEN_PLANE_WORDS] = (uint16_t)(d[6 + p * SCREEN_PLANE_WORDS] | *src++);
        }
        d += SCREEN_ROW_WORDS;
    } while (rows-- != 0);
}

/* the next picture of a looping list at SLOT (the place in the list): the list is pictures, 0 then the address of the list to loop to */
static const uint16_t *list_next(const uint8_t **slot)
{
    const uint16_t *const *p = (const uint16_t *const *)*slot;
    const uint16_t *g = *p++;

    if (g == 0) {
        p = (const uint16_t *const *)*p;
        g = *p++;
    }
    *slot = (const uint8_t *)p;
    return g;
}

/* shop $56E8E: the seven lights of the sign, each at the next picture of its list */
void shop_lights_draw(void)
{
    struct ShopLight *light = xenon2.shop_data.tab_lights;
    int16_t n = 7;

    do {
        draw_bob_masked_at(light->x, light->y, list_next(&light->list));
        light++;
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
    dst_b = (uint8_t *)xenon2.screen_b + (int16_t)xenon2.row_offset[xenon2.text_y];
    dst_b += (int16_t)(xenon2.text_x >> 1);
    dst_a = (uint8_t *)xenon2.screen_a + (dst_b - (uint8_t *)xenon2.screen_b);
    glyph = xenon2.shop_data.font + (int16_t)((uint16_t)(ch - 0x20) << 5);
    if (!(xenon2.text_x & 1)) {
        for (row = 7; row >= 0; row--) {
            int p;

            for (p = 0; p < 4; p++) {
                uint8_t bits = *glyph++;

                put_screen_byte(dst_b + p * SCREEN_PLANE_BYTES, bits);
                put_screen_byte(dst_a + p * SCREEN_PLANE_BYTES, bits);
            }
            dst_b += SCREEN_ROW_BYTES;
            dst_a += SCREEN_ROW_BYTES;
        }
    } else {
        for (row = 7; row >= 0; row--) {
            int p;

            for (p = 0; p < 4; p++) {
                uint8_t bits = (uint8_t)(*glyph++ >> 4);

                or_screen_byte(dst_b + p * SCREEN_PLANE_BYTES, bits);
                or_screen_byte(dst_a + p * SCREEN_PLANE_BYTES, bits);
            }
            dst_b += SCREEN_ROW_BYTES;
            dst_a += SCREEN_ROW_BYTES;
        }
    }
    xenon2.text_x++;
}


/* shop $56988: the countdown of each record: an empty one stays 0, the others start again at $14-$113 */
void shop_records_tick(void)
{
    struct ShopRec *rec = xenon2.shop_data.recs;
    int16_t n = 0x13;

    do {
        if (rec->kind == 0) {
            rec->state = 0;
        } else {
            rec->state--;
            if (rec->state <= 0)
                rec->state = (int16_t)((random_next() & 0xff) + 0x14);
        }
        rec++;
    } while (n-- != 0);
}

/* shop $56C02: the sparkle at the record's place: 28 rows of a noise pattern 3 words wide drawn into the planes */
void shop_sparkle(const struct ShopRec *rec)
{
    uint16_t x = rec->x, y = rec->y;
    uint16_t *dst = (uint16_t *)(xenon2.draw_screen + (int16_t)xenon2.row_offset[y] + (int16_t)((uint16_t)(x ^ (x & 0xf)) >> 3));
    uint16_t shift = x & 0xf, noise, head, mask, inv, rnd;
    int16_t n = 0x1b;
    int p;

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
        for (p = 0; p < 4; p++)
            dst[p * SCREEN_PLANE_WORDS] = (uint16_t)(dst[p * SCREEN_PLANE_WORDS] & inv);
        for (p = 0; p < 3; p++)
            dst[p * SCREEN_PLANE_WORDS] = (uint16_t)(dst[p * SCREEN_PLANE_WORDS] | head);
        for (p = 0; p < 3; p++)
            dst[1 + p * SCREEN_PLANE_WORDS] = noise;
        dst[1 + 3 * SCREEN_PLANE_WORDS] = 0;
        noise = (uint16_t)(noise & inv);
        for (p = 0; p < 4; p++)
            dst[2 + p * SCREEN_PLANE_WORDS] = (uint16_t)(dst[2 + p * SCREEN_PLANE_WORDS] & mask);
        for (p = 0; p < 3; p++)
            dst[2 + p * SCREEN_PLANE_WORDS] = (uint16_t)(dst[2 + p * SCREEN_PLANE_WORDS] | noise);
        dst += SCREEN_ROW_WORDS;
    } while (n-- != 0);
}

/* shop $569BC: each record's picture: opening (state -9..-1) the pictures of tab_spark, closing the sparkle (1, 2), in between (more than 2) the puff and the item's animation */
void shop_records_draw(void)
{
    struct ShopRec *rec = xenon2.shop_data.recs;
    uint16_t cnt = 0x14;

    do {
        int16_t st = rec->state;

        if (st > 0) {
            if (st <= 2) {
                if (st == 2 && (int16_t)xenon2.sfx_cmd < 0)
                    xenon2.sfx_cmd2 = 0xb;
                shop_sparkle(rec);
            } else {
                const uint16_t *pic = xenon2.shop_data.tab_puff[cnt & 1];
                const struct Pic *header;
                uint16_t px, py, adj;

                draw_bob_masked_at(rec->x, rec->y, pic);
                pic = list_next(&rec->list);
                header = (const struct Pic *)pic;
                px = (uint16_t)(rec->x + header->hot_x);
                py = (uint16_t)(rec->y + header->hot_y);
                adj = (uint16_t)((header->width >> 1) - 0x10);
                px = (uint16_t)(px - adj);
                adj = (uint16_t)(((uint16_t)(header->height + 1) >> 1) - 0xe);
                py = (uint16_t)(py - adj);
                draw_bob_masked_at(px, py, pic);
            }
        } else {
            if (st != (int16_t)0xfff8) {
                shop_sparkle(rec);
                if (rec->state == 0)
                    goto next;
            }
            draw_bob_masked_at(rec->x, rec->y, xenon2.shop_data.tab_spark[(uint16_t)(st + 9)]);
        }
    next:
        rec++;
    } while (--cnt != 0);
}

/* shop $56848: the keeper's eyes and mouth blink: pictures at random times */
void shop_keeper_blink(void)
{
    if (xenon2.shop_data.no_blink == 0) {
        if (xenon2.shop_data.blink_a != 0)
            goto eyes_on;
        if (((uint16_t)random_next() & 0x2654) != 0)
            goto mouth;
    }
    draw_bob_masked_at(0x107, 0x29, xenon2.shop_data.gfx_eyes_a);
eyes_on:
    draw_bob_masked_at(0xf7, 0x29, xenon2.shop_data.gfx_eyes_b);
    if (xenon2.shop_data.blink_a != 0)
        xenon2.shop_data.blink_a = (uint8_t)(xenon2.shop_data.blink_a - 1);
mouth:
    if (xenon2.shop_data.no_blink != 0)
        return;
    if ((int8_t)xenon2.shop_data.blink_b >= 0) {
        if (xenon2.shop_data.blink_b == 0) {
            if (((uint16_t)random_next() & 0x2654) != 0)
                return;
            xenon2.shop_data.blink_b = 8;
        } else {
            xenon2.shop_data.blink_b = (uint8_t)(xenon2.shop_data.blink_b - 1);
        }
        draw_bob_masked_at(0x107, 0x23, xenon2.shop_data.gfx_mouth_a);
    }
    draw_bob_masked_at(0xf5, 0x23, xenon2.shop_data.gfx_mouth_b);
}

/* shop $568E6: the door pillar of the right: the rows it still covers cleared, the rest of it drawn */
void shop_door_draw(void)
{
    uint8_t *dst;

    if (xenon2.shop_door_right == 0)
        return;
    dst = shop_clear_columns(xenon2.draw_screen + 0x475a, (uint16_t)(0x44 - xenon2.shop_door_right));
    shop_put_rows(xenon2.shop_data.pic_slide, dst, (uint16_t)(xenon2.shop_door_right - 1));
}

/* shop $5662A: the room: its pictures, the keeper's lights, the doors */
void shop_room_draw(void)
{
    const uint16_t *pic;
    uint8_t *dst;
    uint16_t rows;

    shop_put_rows(xenon2.shop_data.pic_top, xenon2.draw_screen + 0x5ba, 0x33);
    shop_put_rows(xenon2.shop_data.pic_mid, xenon2.draw_screen + 0x3b7a, 9);
    pic = xenon2.shop_data.pic_low;
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
            pic += 0x770 / 2;
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
        pic = xenon2.shop_data.pic_door + (int16_t)((uint16_t)(0x30 - xenon2.shop_door_left) * 0x38 / 2);
        rows = (uint16_t)(xenon2.shop_door_left - 9);
        shop_put_rows(pic, dst, rows);
        dst = xenon2.draw_screen + 0x46ba - (int16_t)((uint16_t)(xenon2.shop_door_left * SCREEN_ROW_BYTES));
        shop_put_rows(xenon2.shop_data.pic_door2, dst, rows);
    }
    draw_bob_masked_at(0xd7, (uint16_t)(xenon2.shop_door_left + 1), xenon2.shop_data.gfx_bar_1);
    draw_bob_masked_at(0xf7, (uint16_t)(xenon2.shop_door_left + 1), xenon2.shop_data.gfx_bar_2);
    draw_bob_masked_at(0x117, (uint16_t)(xenon2.shop_door_left + 1), xenon2.shop_data.gfx_bar_3);
    draw_bob_masked_at(0xd7, (uint16_t)(0x70 - xenon2.shop_door_left), xenon2.shop_data.gfx_bar_4);
    draw_bob_masked_at(0xf7, (uint16_t)(0x70 - xenon2.shop_door_left), xenon2.shop_data.gfx_bar_5);
    draw_bob_masked_at(0x117, (uint16_t)(0x70 - xenon2.shop_door_left), xenon2.shop_data.gfx_bar_6);
    if ((int16_t)xenon2.shop_door_left >= 8)
        return;
    {
        uint16_t *dst = (uint16_t *)(xenon2.draw_screen + 0x1a), *dst2 = (uint16_t *)(xenon2.draw_screen + 0x41ba);
        const uint16_t *lamp_a = xenon2.shop_data.pic_lamp_a, *lamp_b = xenon2.shop_data.pic_lamp_b;
        int16_t n = 8;

        do {
            int g, p;

            for (g = 0; g < 7; g++) {
                for (p = 0; p < 4; p++) {
                    dst[p * SCREEN_PLANE_WORDS] = *lamp_a++;
                }
                for (p = 0; p < 4; p++) {
                    dst2[p * SCREEN_PLANE_WORDS] = *lamp_b++;
                }
                dst++;
                dst2++;
            }
            dst += SCREEN_ROW_WORDS - 7;
            dst2 += SCREEN_ROW_WORDS - 7;
            lamp_a += 4;
            lamp_b += 4;
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

/* shop $5648C: the keeper's picture at shop_data.kx, ky (clipped at line $68) */
void shop_keeper_draw(void)
{
    uint16_t saved = xenon2.clip_bottom;

    xenon2.clip_bottom = 0x68;
    draw_bob_masked_at(xenon2.shop_data.kx, xenon2.shop_data.ky, xenon2.shop_data.kgfx);
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
    shop_clear_columns((uint8_t *)xenon2.screen_b + 0x475a, 0x44);
    shop_clear_columns((uint8_t *)xenon2.screen_a + 0x475a, 0x44);
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
    draw_number(xenon2.player_money, (uint8_t *)xenon2.screen_b + 0x6b91);
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
static void cursor_show(const uint16_t *on_item, const uint16_t *exit_a, const uint16_t *exit_b, const uint16_t *exit_c)
{
    uint16_t row = xenon2.shop_cursor_row, x, y, index;
    const uint16_t *pic;

    if (row == 4) {
        x = 4;
        pic = exit_a;
        if (xenon2.shop_cursor_col != 0) {
            x = 0x2d;
            pic = exit_b;
            if (xenon2.shop_data.selling == 0)
                pic = exit_c;
        }
        shop_draw_both(x, 0xa8, pic);
        return;
    }
    index = (uint16_t)(row * 5 + xenon2.shop_cursor_col);
    y = xenon2.shop_data.recs[index].y;
    x = xenon2.shop_data.recs[index].x;
    shop_draw_both((uint16_t)(x + 0xa), (uint16_t)(y + 0x1f), on_item);
}

void shop_cursor_draw(void) { cursor_show(xenon2.shop_data.gfx_cursor_on_a, xenon2.shop_data.gfx_exit_on_a, xenon2.shop_data.gfx_exit_on_b, xenon2.shop_data.gfx_exit_on_c); }
void shop_cursor_erase(void) { cursor_show(xenon2.shop_data.gfx_cursor_on_b, xenon2.shop_data.gfx_exit_off_a, xenon2.shop_data.gfx_exit_off_b, xenon2.shop_data.gfx_exit_off_c); }

/* shop $55F60: the cursor moved over the grid by the stick, every 5 frames, until the fire button (the room goes on) */
void shop_stick(void)
{
    for (;;) {
        if (xenon2.shop_data.repeat != 0) {
            xenon2.shop_data.repeat = (uint16_t)(xenon2.shop_data.repeat - 1);
        } else if (xenon2.key_pressed != 0) {
            xenon2.key_pressed = 0;
            xenon2.sfx_cmd = 0xd;
            return;
        } else if (xenon2.joy_stick != 0) {
            xenon2.sfx_cmd = 0xc;
            xenon2.shop_data.repeat = 4;
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

/* shop $5593E: -1 if the item of the record at shop_data.rec can be bought (the ship has room for it), else 0 */
int32_t shop_can_buy(void)
{
    uint16_t kind = xenon2.shop_data.rec->kind;
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
    flags = xenon2.shop_data.tab_flags[(uint16_t)(kind - 1)];
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

    xenon2.shop_data.blink_a = 0xc;
    shop_text_home();
    index = (uint16_t)((uint16_t)(xenon2.level_number - 1) * 0x18 + xenon2.advice_hints);
    if (xenon2.advice_hints != 8 && xenon2.advice_hints != 0x14)
        xenon2.advice_hints += 4;
    shop_print(xenon2.shop_data.tab_advice[(uint16_t)index / 4]);
    xenon2.shop_data.blink_a = 0;
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
    shop_print(xenon2.shop_data.tab_names[(uint16_t)(kind - 1)]);
    if (xenon2.shop_data.selling == 0)
        return;
    if (kind == 1 || kind == 0x13 || kind == 0x12 || kind == 8 || kind == 0xa || kind == 0xe || kind == 9 || kind == 0xd) {
        shop_print(xenon2.shop_data.txt_power);
        shop_print_number((uint16_t)(power + 1));
    }
}

/* shop $55C60: "THAT WILL COST" and the price of the item of the record (halved on the level 5 setting) */
void shop_price_text(struct ShopRec *rec)
{
    uint16_t price;

    xenon2.shop_data.rec = rec;
    shop_print(xenon2.shop_data.txt_cost);
    price = xenon2.shop_data.tab_price[(uint16_t)(rec->kind - 1)];
    if (xenon2.level_number == 5)
        price >>= 1;
    xenon2.shop_data.price = price;
    shop_print_number(price);
}

/* the price of item price (1 on) on this setting */
static uint16_t item_price_at(const uint16_t *table_entry)
{
    uint16_t price = *table_entry;

    if (xenon2.level_number == 5)
        price >>= 1;
    return price;
}

/* shop $55424: the records of the page of the stock: an item that is affordable (price not above the cash and not above the limit $9FA) is at its animation, the others
 * are sold out ($1A); a page that runs out of items is padded with empty records, the last record is the next page ($1B) unless it is the first page and ends there */
void shop_stock_update(void)
{
    struct ShopRec *rec;
    const uint16_t *price_ptr;
    const uint16_t **anim_ptr;
    uint16_t price, item;
    int32_t cash;
    int16_t cost;

    price = (uint16_t)(xenon2.shop_data.page * 2);
    price = xenon2.shop_data.tab_price[price / 2 + 1];
    if (xenon2.level_number == 5)
        price >>= 1;
    if ((int32_t)(int16_t)price > (int32_t)xenon2.player_money)
        xenon2.shop_data.page = 0;
    rec = xenon2.shop_data.recs;
    item = (uint16_t)(xenon2.shop_data.page + 2);
    price_ptr = &xenon2.shop_data.tab_price[(uint16_t)(item - 1)];
    anim_ptr = &xenon2.shop_data.tab_anim[(uint16_t)(item - 1)];
    cash = (int32_t)xenon2.player_money;
    for (;;) {
        cost = (int16_t)item_price_at(price_ptr);
        if (cost > (int16_t)xenon2.shop_price_cap || cash < (int32_t)cost) {
sold:
            rec->kind = 0x1a;
            rec->list = (const uint8_t *)&xenon2.shop_data.lists[3];
        } else {
            rec->kind = item;
            rec->list = (const uint8_t *)*anim_ptr;
        }
        item++;
        price_ptr++;
        anim_ptr++;
        rec++;
        if (item == 0x1a) {
            while (rec != &xenon2.shop_data.recs[19]) {
                rec->kind = 0;
                rec++;
            }
            if (xenon2.shop_data.page == 0) {
                rec->kind = 0;
                return;
            }
            break;
        }
        if (rec < &xenon2.shop_data.recs[19])
            continue;
        if (rec > &xenon2.shop_data.recs[19])
            return;
        if (xenon2.shop_data.page == 0) {
            cost = (int16_t)item_price_at(price_ptr);
            if (cost > (int16_t)xenon2.shop_price_cap || cash < (int32_t)cost)
                goto sold;
        }
        break;
    }
    rec->kind = 0x1b;
    rec->list = (const uint8_t *)&xenon2.shop_data.lists[0];
}

/* shop $55512: every record but the last is sold out */
void shop_stock_sold_out(void)
{
    struct ShopRec *rec = xenon2.shop_data.recs;

    do {
        rec->kind = 0x1a;
        rec->list = (const uint8_t *)&xenon2.shop_data.lists[3];
        rec++;
    } while (rec < &xenon2.shop_data.recs[19]);
}

/* shop $55534: after a purchase the items that cost more than the cash left are sold out, and the next page record becomes sold out when the one before it is */
void shop_stock_recheck(void)
{
    struct ShopRec *rec = xenon2.shop_data.recs;
    int32_t cash = (int32_t)xenon2.player_money;
    int16_t n = 0x13;

    do {
        uint16_t item = rec->kind;

        if (item != 0) {
            int sold = 0;

            if (item == 0x1b) {
                if (rec[-1].kind == 0x1a)
                    sold = 1;
            } else if ((int16_t)item < 0x1a) {
                uint16_t price = xenon2.shop_data.tab_price[(uint16_t)(item - 1)];

                if (xenon2.level_number == 5)
                    price >>= 1;
                if (!(cash >= (int32_t)(int16_t)price))
                    sold = 1;
            }
            if (sold) {
                rec->kind = 0x1a;
                rec->list = (const uint8_t *)&xenon2.shop_data.lists[3];
            }
        }
        rec++;
    } while (n-- != 0);
}

/* shop $55AFC: the records of the player's own weapons: the objects of the ship's list, each with the animation of its kind (the ones of kind $94 are not for sale) */
void shop_sell_list(void)
{
    struct ShopRec *rec = xenon2.shop_data.recs;
    Object *weapon = xenon2.obj_list[1].prev;

    while (weapon->type != 0) {
        if (weapon->type != 0x94) {
            uint16_t kind = (uint16_t)weapon->kind;

            rec->kind = kind;
            rec->weapon = weapon;
            rec->list = (const uint8_t *)xenon2.shop_data.tab_anim[(uint16_t)(kind - 1)];
            rec++;
        }
        weapon = weapon->prev;
    }
    do {
        rec->kind = 0;
        rec++;
    } while (rec <= &xenon2.shop_data.recs[19]);
}

/* the record under the cursor (the grid is 5 wide) */
static struct ShopRec *cursor_record(void)
{
    uint16_t index = (uint16_t)(xenon2.shop_cursor_row * 5 + xenon2.shop_cursor_col);

    return &xenon2.shop_data.recs[index];
}

/* shop $55C9A: the keeper shows the item (his picture goes up, blinks, comes down), and what the keeper pays for it is shown: (price of the item + $7D0 times its power) / 2 */
void shop_sell_show(struct ShopRec *rec)
{
    uint16_t price, bonus;
    const Object *weapon;

    xenon2.shop_data.rec = rec;
    xenon2.shop_data.kx = 0xf7;
    xenon2.shop_data.ky = 0x78;
    xenon2.shop_data.kgfx = xenon2.shop_data.gfx_keeper_a;
    do {
        swap_screens_200();
        shop_room_draw();
        shop_records_tick();
        shop_records_draw();
        shop_keeper_draw();
        xenon2.shop_data.ky = (uint16_t)(xenon2.shop_data.ky - 2);
    } while ((int16_t)xenon2.shop_data.ky > 0x68);
    shop_hold(3);
    xenon2.shop_data.kgfx = xenon2.shop_data.gfx_keeper_b; shop_hold(3);
    xenon2.shop_data.kgfx = xenon2.shop_data.gfx_keeper_a; shop_hold(3);
    xenon2.shop_data.kgfx = xenon2.shop_data.gfx_keeper_c; shop_hold(3);
    xenon2.shop_data.kgfx = xenon2.shop_data.gfx_keeper_a; shop_hold(3);
    xenon2.shop_data.kgfx = xenon2.shop_data.gfx_keeper_b; shop_hold(3);
    xenon2.shop_data.kgfx = xenon2.shop_data.gfx_keeper_a; shop_hold(3);
    xenon2.shop_data.kgfx = xenon2.shop_data.gfx_keeper_c; shop_hold(3);
    xenon2.shop_data.kgfx = xenon2.shop_data.gfx_keeper_a; shop_hold(3);
    do {
        swap_screens_200();
        shop_room_draw();
        shop_records_tick();
        shop_records_draw();
        shop_keeper_draw();
        xenon2.shop_data.ky = (uint16_t)(xenon2.shop_data.ky + 2);
    } while ((int16_t)xenon2.shop_data.ky < 0x80);
    shop_print(xenon2.shop_data.txt_pay);
    price = xenon2.shop_data.tab_price[(uint16_t)(xenon2.shop_data.rec->kind - 1)];
    weapon = xenon2.shop_data.rec->weapon;
    bonus = (uint16_t)(weapon->score * 0x7d0);
    price = (uint16_t)(price + bonus);
    price >>= 1;
    xenon2.shop_data.price = price;
    shop_print_number(price);
}

/* shop $55B48: the player sells: the weapons on the ship are the records; the fire button on one shows it and what the keeper pays, a second one on the same
 * sells it (the cash is paid and the weapon goes); the button below the grid leaves */
void shop_sell(void)
{
    xenon2.shop_cursor_col = 0;
    xenon2.shop_cursor_row = 0;
    xenon2.shop_data.last_x = 0xffff;
    shop_cursor_draw();
    xenon2.shop_data.price = 0xffff;
    for (;;) {
        xenon2.key_pressed = 0;
        shop_stick();
        if (xenon2.shop_cursor_row == 4) {
            if (xenon2.shop_cursor_col == 0) {
                shop_cursor_erase();
                return;
            }
        } else if (!(xenon2.shop_cursor_col == xenon2.shop_data.last_x && xenon2.shop_cursor_row == xenon2.shop_data.last_y)) {
            struct ShopRec *rec = cursor_record();
            uint16_t kind = rec->kind;

            if (kind == 0)
                continue;
            xenon2.shop_data.last_x = xenon2.shop_cursor_col;
            xenon2.shop_data.last_y = xenon2.shop_cursor_row;
            xenon2.shop_data.blink_b = 0xff;
            shop_item_name(kind, rec->weapon->score);
            shop_sell_show(rec);
            xenon2.shop_data.blink_b = 0;
            continue;
        }
        xenon2.shop_data.last_x = 0xffff;
        if ((int16_t)xenon2.shop_data.price < 0)
            continue;
        xenon2.player_money += (uint32_t)(int32_t)(int16_t)xenon2.shop_data.price;
        xenon2.shop_data.price = 0xffff;
        shop_cash_draw();
        {
            struct ShopRec *rec = xenon2.shop_data.rec;

            rec->kind = 0;
            kill_with_slot(rec->weapon);
        }
        shop_text_home();
        shop_print(xenon2.shop_data.txt_here_cash);
    }
}

/* shop $55594: the player buys: the items on offer are the records; the fire button on one shows its name and price, a second one on the same buys it (the cash is
 * counted down and the item is given); the button below the grid leaves */
void shop_buy(void)
{
    xenon2.shop_cursor_col = 0;
    xenon2.shop_cursor_row = 0;
    xenon2.shop_data.last_x = 0xffff;
    shop_cursor_draw();
    xenon2.shop_data.price = 0xffff;
    for (;;) {
        xenon2.key_pressed = 0;
        shop_stick();
        if (xenon2.shop_cursor_row == 4) {
            if (xenon2.shop_cursor_col == 0) {
                shop_cursor_erase();
                return;
            }
        } else if (!(xenon2.shop_cursor_col == xenon2.shop_data.last_x && xenon2.shop_cursor_row == xenon2.shop_data.last_y)) {
            struct ShopRec *rec = cursor_record();
            uint16_t kind = rec->kind;

            if (kind == 0)
                continue;
            if (kind == 0x1a) {
                shop_text_home();
                shop_print(xenon2.shop_data.txt_out_of_stock);
                xenon2.shop_data.price = 0xffff;
                continue;
            }
            if (kind == 0x1b) {
                uint16_t page = (uint16_t)(xenon2.shop_data.page + 0x13);

                if ((int16_t)page >= 0x19)
                    page = 0;
                xenon2.shop_data.page = page;
                shop_stock_update();
                xenon2.shop_data.price = 0xffff;
                continue;
            }
            xenon2.shop_data.last_x = xenon2.shop_cursor_col;
            xenon2.shop_data.last_y = xenon2.shop_cursor_row;
            shop_item_name(kind, (uint16_t)(xenon2.shop_cursor_row * 5));
            shop_price_text(rec);
            continue;
        }
        xenon2.shop_data.last_x = 0xffff;
        if ((int16_t)xenon2.shop_data.price < 0)
            continue;
        if (shop_can_buy() == 0) {
            shop_text_home();
            shop_print(xenon2.shop_data.txt_no_room);
            xenon2.shop_data.price = 0xffff;
            continue;
        }
        shop_text_home();
        shop_print(xenon2.shop_data.txt_give_cash);
        {
            int32_t price = (int32_t)(int16_t)xenon2.shop_data.price;
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
            item_grant(xenon2.shop_data.rec->kind);
            xenon2.shop_data.price = 0xffff;
            shop_cash_draw();
            shop_stock_recheck();
            if (xenon2.shop_data.rec->kind != 2)
                shop_print(xenon2.shop_data.txt_what_else);
        }
    }
}

/* shop $5530C ($4EC) and $55324 ($4EE): the two pillars of the doorway at their place, their bit 2 selects the picture */
static void pillars_draw(uint16_t y, uint16_t state)
{
    const uint16_t *gfx = (state & 4) ? xenon2.shop_data.gfx_pillar_b : xenon2.shop_data.gfx_pillar_a;

    shop_draw_both(0xe2, y, gfx);
    shop_draw_both(0x102, y, gfx);
    shop_draw_both(0x122, y, gfx);
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
    xenon2.shop_data.kx = 0x106;
    xenon2.shop_data.ky = 0x88;
    xenon2.shop_data.kgfx = xenon2.shop_data.gfx_keeper_a;
    for (;;) {
        swap_screens_200();
        shop_records_tick();
        shop_records_draw();
        shop_room_draw();
        shop_keeper_draw();
        if (xenon2.shop_data.kx == 0x11c)
            break;
        xenon2.shop_data.kx = (uint16_t)(xenon2.shop_data.kx + 2);
        xenon2.shop_data.ky = (uint16_t)(xenon2.shop_data.ky - 2);
    }
    xenon2.shop_data.kgfx = xenon2.shop_data.gfx_keeper_c;
    shop_hold(6);
    shop_sound_stop();
    xenon2.shop_data.no_blink = 0;
    xenon2.shop_data.kgfx = xenon2.shop_data.gfx_keeper_a;
    shop_hold(6);
    do {
        swap_screens_200();
        shop_records_tick();
        shop_records_draw();
        shop_room_draw();
        shop_keeper_draw();
        xenon2.shop_data.kx = (uint16_t)(xenon2.shop_data.kx - 2);
        xenon2.shop_data.ky = (uint16_t)(xenon2.shop_data.ky + 2);
    } while ((int16_t)xenon2.shop_data.kx > 0x106);
}

/* shop $56568: the items appear: the records open one frame after the other, then four frames of the finished room */
void shop_open(void)
{
    struct ShopRec *rec;
    int16_t n;

    for (;;) {
        swap_screens_200();
        for (rec = xenon2.shop_data.recs, n = 0x13; n >= 0; n--, rec++)
            rec->state++;
        if (xenon2.shop_data.recs[0].state == 0)
            break;
        shop_records_draw();
        shop_room_draw();
    }
    for (rec = xenon2.shop_data.recs, n = 0x13; n >= 0; n--, rec++)
        rec->state = 1;
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
    struct ShopRec *rec;
    int16_t n;

    for (rec = xenon2.shop_data.recs, n = 0x13; n >= 0; n--, rec++)
        rec->state = 0;
    for (;;) {
        swap_screens_200();
        for (rec = xenon2.shop_data.recs, n = 0x13; n >= 0; n--, rec++)
            rec->state--;
        if (xenon2.shop_data.recs[0].state == -9)
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
        *(uint32_t *)dst = 0;
        dst += 4;
    } while (n-- != 0);
    {
        uint16_t *bar = (uint16_t *)(xenon2.draw_screen + 0x3e94);

        bar[0] = 0xf;
        bar[SCREEN_PLANE_WORDS] = 6;
        bar[2 * SCREEN_PLANE_WORDS] = 0xf;
        bar[SCREEN_ROW_WORDS] = 0xf;
        bar[SCREEN_ROW_WORDS + SCREEN_PLANE_WORDS] = 0xf;
        bar[SCREEN_ROW_WORDS + 2 * SCREEN_PLANE_WORDS] = 0xf;
        bar[2 * SCREEN_ROW_WORDS] = 0xf;
        bar[2 * SCREEN_ROW_WORDS + SCREEN_PLANE_WORDS] = 6;
        bar[2 * SCREEN_ROW_WORDS + 2 * SCREEN_PLANE_WORDS] = 0xf;
    }
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

    shop_print(xenon2.shop_data.txt_thats_it);
    shop_wait(0x1e);
    shop_print(xenon2.shop_data.txt_forget);
    shop_wait(0x1e);
    shop_text_home();
    shop_print(xenon2.shop_data.txt_good_enough);
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
        int i;

        shop_blank_screen();
        swap_screens_200();
        for (i = 0; i < 16; i++)
            xenon2.palette_shadow[i] = xenon2.shop_data.palette_a[i];
        for (i = 0; i < 16; i++)
            custom->color[i] = (uint16_t)(xenon2.shop_data.palette_a[i] + xenon2.shop_data.palette_a[i]);
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
    const struct ShopPic *pic;
    struct ShopRec *rec;
    int16_t x, y, i;
    int32_t n;

    xenon2.clip_bottom = 0xc7;
    work_area_save();
    xenon2.music_tick_fn = shop_irq_tick;
    xenon2.music_cmd_fn = (irq_fn)(void *)shop_sound_cmd;
    xenon2.shop_data.snd.chan[0].head.sample_id = 0;
    if (!(CIAA_CRA & 0x80))
        xenon2.shop_data.snd.chan[0].head.sample_id = 0xff;
    screen = xenon2.draw_screen;
    for (n = 0; n < 0x1f40; n++)
        *(uint32_t *)(screen + 4 * n) = 0;
    for (y = 0; y < 0xa4; y = (int16_t)(y + 0x29))
        for (x = 0; x < 0xcd; x = (int16_t)(x + 0x29)) {
            draw_bob_masked_at((uint16_t)x, (uint16_t)y, xenon2.shop_data.gfx_bedroom);
            draw_bob_masked_at((uint16_t)x, (uint16_t)y, xenon2.shop_data.gfx_bedroom2);
        }
    for (pic = xenon2.shop_data.tab_pics; pic->gfx != 0; pic++)
        shop_copy_block(pic->gfx, xenon2.draw_screen + (int16_t)pic->offset, pic->width_m1, pic->height_m1);
    if (xenon2.cur_player != 0)
        draw_bob_masked_at(0xc1, 0xbc, xenon2.shop_data.gfx_player2);
    screen = xenon2.draw_screen;
    for (n = 0; n < 0x1f40; n++)
        *(uint32_t *)(xenon2.show_screen + 4 * n) = *(const uint32_t *)(screen + 4 * n);
    xenon2.shop_data.selling = 0xff;
    xenon2.shop_data.no_blink = 0xff;
    xenon2.shop_data.blink_a = 0;
    xenon2.shop_data.page = 0;
    for (rec = xenon2.shop_data.recs, i = 0x13; i >= 0; i--, rec++)
        rec->state = -9;
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
            colors[i] = xenon2.shop_data.palette_b[i];
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
        const uint8_t *text = xenon2.shop_data.txt_sell;

        if (xenon2.level_number == 5) {
            shop_print(xenon2.shop_data.txt_welcome_a);
            shop_hold(0x11);
            shop_text_home();
            shop_print(xenon2.shop_data.txt_welcome_b);
            shop_hold(0x11);
            shop_text_home();
            text = xenon2.shop_data.txt_last_chance;
        }
        shop_print(text);
        shop_sell();
        shop_close();
        xenon2.shop_data.selling = 0;
        xenon2.shop_cursor_col = 1;
        xenon2.shop_cursor_row = 4;
        shop_cursor_erase();
        shop_stock_update();
        shop_open();
        shop_text_home();
        shop_print(xenon2.shop_data.txt_buy);
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
