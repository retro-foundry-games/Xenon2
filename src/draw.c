#include "draw.h"
#include "game_all.h"

/* inlined: $400 (bra.w to the start-up code) */
/* inlined: $E0C, $E12, $E18, $E2A, $E54, $E5A (jmp thunks of the table at $E0C: callers call the target) */

/* orig $E12, $128E, $133E, $1350, $135C and $1366: draw the bob of an object, masked */
void draw_bob_masked(Object *obj)
{
    draw_bob_masked_at(obj->x, obj->y, (const uint16_t *)obj->gfx);
}

/* orig $F08, $1370 and $1378: draw the bob of an object as above */
void draw_bob_or(Object *obj)
{
    draw_bob_or_at(obj->x, obj->y, (const uint16_t *)obj->gfx);
}

/* orig $E66, $1368: the object is drawn masked from now on, this time as above */
void draw_bob_or_first(Object *obj)
{
    obj->fn_draw = (obj_fn)draw_bob_masked;
    draw_bob_or(obj);
}

/* orig $1636: draw value as 7 decimal digits (8 lines each, 1 byte a digit) at dest in this screen and in the other one; the
 * unexecuted part is read from the listing */
void draw_number(uint32_t value, uint8_t *dest)
{
    uint8_t *other = dest - ((uint8_t *)xenon2.screen_b - (uint8_t *)xenon2.screen_a);
    const uint32_t *p = xenon2.pow10;

    for (; *p != 0; p++) {
        uint16_t digit = 0;
        const uint8_t *g;

        for (;;) {
            value -= *p;
            if ((int32_t)value < 0)
                break;
            digit++;
        }
        value += *p;
        g = &xenon2.digit_gfx[(uint16_t)digit * DIGIT_GLYPH_BYTES];
        for (int row = 0; row < 8; row++) {
            for (int plane = 0; plane < 4; plane++) {
                uint8_t b = *g++;

                put_screen_byte(dest + row * SCREEN_ROW_BYTES + plane * SCREEN_PLANE_BYTES, b);
                put_screen_byte(other + row * SCREEN_ROW_BYTES + plane * SCREEN_PLANE_BYTES, b);
            }
        }
        dest += 1;
        other += 1;
    }
}

/* orig $16BE: draw the energy bar (cc6 of 39 cells) into the two screens; flag a6b says it is to be redrawn */
void redraw_energy_bar(void)
{
    uint16_t *dst_b, *dst_a;
    uint16_t fill, bit, keep;

    if (!xenon2.energy_dirty)
        return;
    xenon2.energy_dirty = 0;
    fill = xenon2.player_energy;
    if ((int16_t)fill > 0x27)
        fill = 0x27;
    dst_b = (uint16_t *)SCREEN_POS(xenon2.screen_b, 195, 10);
    dst_a = (uint16_t *)SCREEN_POS(xenon2.screen_a, 195, 10);
    bit = 0x10;
    if (xenon2.cur_player) {
        dst_b = (uint16_t *)SCREEN_POS(xenon2.screen_b, 195, 22);
        dst_a = (uint16_t *)SCREEN_POS(xenon2.screen_a, 195, 22);
        bit = 2;
    }
    keep = (uint16_t)~bit;
    for (int cell = 0; cell < 0x27; cell++) {
        int16_t row_off = 0;

        if (fill != 0) {
            fill--;
            do {
                *(dst_b + row_off) &= keep;
                *(dst_b + SCREEN_PLANE_WORDS + row_off) |= bit;
                *(dst_b + (2 * SCREEN_PLANE_WORDS) + row_off) |= bit;
                *(dst_b + (3 * SCREEN_PLANE_WORDS) + row_off) |= bit;
                *(dst_a + row_off) &= keep;
                *(dst_a + SCREEN_PLANE_WORDS + row_off) |= bit;
                *(dst_a + (2 * SCREEN_PLANE_WORDS) + row_off) |= bit;
                *(dst_a + (3 * SCREEN_PLANE_WORDS) + row_off) |= bit;
                row_off = (int16_t)(row_off + SCREEN_ROW_WORDS);
            } while (row_off <= 2 * SCREEN_ROW_WORDS);
        } else {
            do {
                *(dst_b + row_off) &= keep;
                *(dst_b + SCREEN_PLANE_WORDS + row_off) &= keep;
                *(dst_b + (2 * SCREEN_PLANE_WORDS) + row_off) &= keep;
                *(dst_b + (3 * SCREEN_PLANE_WORDS) + row_off) &= keep;
                *(dst_a + row_off) &= keep;
                *(dst_a + SCREEN_PLANE_WORDS + row_off) &= keep;
                *(dst_a + (2 * SCREEN_PLANE_WORDS) + row_off) &= keep;
                *(dst_a + (3 * SCREEN_PLANE_WORDS) + row_off) &= keep;
                row_off = (int16_t)(row_off + SCREEN_ROW_WORDS);
            } while (row_off <= 2 * SCREEN_ROW_WORDS);
        }
        {
            uint16_t carry = bit & 1;

            bit = (uint16_t)((bit >> 1) | (bit << 15));
            if (carry) {
                dst_b++;
                dst_a++;
            }
        }
        keep = (uint16_t)~bit;
    }
}

/* orig $176E: copy 8 lines of 4 plane bytes of the digit glyph cca (0-9, 32 bytes each) into both screens */
void draw_status_digit(void)
{
    int16_t lives = (int16_t)xenon2.player_lives;
    const uint8_t *glyph;
    uint8_t *dst_b = SCREEN_POS(xenon2.screen_b, 193, 17), *dst_a = SCREEN_POS(xenon2.screen_a, 193, 17);

    if (lives > 9)
        lives = 9;
    glyph = &xenon2.digit_gfx[10 * DIGIT_GLYPH_BYTES] + (int16_t)(uint16_t)lives * DIGIT_GLYPH_BYTES;
    if (xenon2.cur_player) {
        dst_b = SCREEN_POS(xenon2.screen_b, 193, 22);
        dst_a = SCREEN_POS(xenon2.screen_a, 193, 22);
    }
    for (int row = 0; row < 8; row++) {
        for (int plane = 0; plane < 4; plane++) {
            uint8_t b = *glyph++;

            put_screen_byte(dst_b + plane * SCREEN_PLANE_BYTES, b);
            put_screen_byte(dst_a + plane * SCREEN_PLANE_BYTES, b);
        }
        dst_b += SCREEN_ROW_BYTES;
        dst_a += SCREEN_ROW_BYTES;
    }
}

/* orig $00668c: fills the score area of both screens at the current player's place with the first glyph rows */
void panel_clear_score(void)
{
    const uint8_t *gfx = xenon2.digit_gfx;
    uint32_t off = xenon2.cur_player ? 0x1e : 2;   /* from the panel's start */
    uint8_t *pb = (uint8_t *)xenon2.panel_b;
    uint8_t *pa = (uint8_t *)xenon2.panel_a;
    uint16_t first = xenon2.cur_player ? 0 : 1;
    int row;

    for (row = 0; row < 8; row++) {
        uint16_t col_off = 0;
        do {
            uint8_t byte = *gfx++;
            uint16_t k;

            for (k = first; k < first + 7; k++) {
                put_screen_byte(pb + off + col_off + k, byte);
                put_screen_byte(pa + off + col_off + k, byte);
            }
            col_off += 0x28;
        } while ((int16_t)col_off <= 0x78);
        off += 0xa0;
    }
}

/* orig $006676: the score area for both players */
void panel_init(void)
{
    panel_clear_score();
    xenon2.cur_player ^= 1;
    panel_clear_score();
    xenon2.cur_player ^= 1;
}

/* orig $006766: counts the displayed score up towards the real one by 10, 100 or 1000 and draws the digit that changed */
void update_score_display(void)
{
    int32_t diff = (int32_t)(xenon2.player_score - xenon2.score_shown);
    uint8_t *digit;
    uint32_t off;   /* from the panel's start */
    uint8_t *pb = (uint8_t *)xenon2.panel_b;
    uint8_t *pa = (uint8_t *)xenon2.panel_a;

    if (diff == 0)
        return;
    if (diff < 0) {
        xenon2.score_shown = xenon2.player_score;
        return;
    }
    if (diff >= 0xc8) {
        if (diff >= 0x7d0) {
            digit = &xenon2.score_digits[3];
            off = 6;
            xenon2.score_shown += 0x3e8;
        } else {
            digit = &xenon2.score_digits[4];
            off = 7;
            xenon2.score_shown += 0x64;
        }
    } else {
        digit = &xenon2.score_digits[5];
        off = 8;
        xenon2.score_shown += 0xa;
    }
    if (xenon2.cur_player)
        off += 0x1b;
    for (;;) {
        const uint8_t *g;
        int row, k;

        (*digit)++;
        if ((int8_t)*digit >= 10)
            *digit = 0;
        g = xenon2.digit_gfx + ((uint16_t)*digit << 5);
        for (row = 0; row < 8; row++) {
            for (k = 0; k < 4; k++) {
                uint8_t b = *g++;

                put_screen_byte(pb + off + row * SCREEN_ROW_BYTES + k * SCREEN_PLANE_BYTES, b);
                put_screen_byte(pa + off + row * SCREEN_ROW_BYTES + k * SCREEN_PLANE_BYTES, b);
            }
        }
        if (*digit != 0)
            return;
        digit--;
        off--;
        if (digit < xenon2.score_digits)
            return;
    }
}

/* orig $00635E: a gauge of ten steps: the picture for fill / (max + 1) * 10 at (x, y); the original's table has no picture for a full
 * gauge (it would draw code bytes), nothing is drawn then */
void draw_gauge(uint16_t x, uint16_t y, uint16_t fill, uint16_t max)
{
    uint32_t dividend = (uint32_t)fill * 10;
    uint16_t divisor = (uint16_t)(max + 1);
    uint32_t q = dividend / divisor;
    uint16_t pic = q > 0xffff ? (uint16_t)dividend : (uint16_t)q;

    if (pic < 10) {
#ifdef GAME_SDL
        if (presentation_capturing)
            presentation_pin(1);
#endif
        draw_bob_masked_at(x, y, xenon2.gauge_pics[pic]);
#ifdef GAME_SDL
        if (presentation_capturing)
            presentation_pin(0);
#endif
    }
}
