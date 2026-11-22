#include "title.h"
#include "game_all.h"
#include "amiga/hw.h"

#define WORD(p, off) rd16((const uint8_t *)(p) + (off))
#define SETW(p, off, v) wr16((uint8_t *)(p) + (off), (v))

/* divs.w: the low word the register holds afterwards (quotient; the dividend's low word when the quotient overflows) */
static uint16_t divs_low(int32_t dividend, int16_t divisor)
{
    int32_t q;

    if (divisor == 0)
        return (uint16_t)dividend;
    q = dividend / divisor;
    if (q < -32768 || q > 32767)
        return (uint16_t)dividend;
    return (uint16_t)q;
}

/* palette_shadow and the colour registers from 8 longs; the colour registers get every long doubled (add.l d,d) */
static void set_palette(const uint16_t *src)
{
    int i;

    for (i = 0; i < 16; i += 2) {
        uint32_t v = (((uint32_t)(uint16_t)src[i] << 16) | (uint16_t)src[i + 1]);

        palette_shadow[i] = src[i];
        palette_shadow[i + 1] = src[i + 1];
        v += v;
        custom->color[i] = (uint16_t)(v >> 16);
        custom->color[i + 1] = (uint16_t)v;
    }
}

/* orig $007EB6: clears the screen being drawn into (32000 bytes) */
void clear_screen(void)
{
    uint32_t *p = (uint32_t *)(draw_screen + 0x7d00);
    int i;

    for (i = 0; i < 8000; i++)
        *--p = 0;
}

/* orig $007EF8: position of ch in the zero-terminated list, -1 when it is not in it */
int32_t find_glyph(uint8_t ch, const uint8_t *list)
{
    const uint8_t *p = list;

    while (*p != 0) {
        if (*p++ == ch)
            return (int32_t)(p - list) - 1;
    }
    return -1;
}

/* orig $007F24: 20 characters (count + 1 of them) of a string drawn at dst, each 16 pixels wide and
 * 22 rows high in 4 planes; characters not in the font leave a gap */
void draw_text_row_at(uint8_t *dst, const uint8_t *s, uint16_t count)
{
    uint8_t *a0 = dst;
    uint16_t n = count;

    do {
        int32_t idx = find_glyph(*s++, title_font_chars);

        if (idx >= 0) {
            const uint16_t *g = title_font + ((uint16_t)idx * 0xb0u) / 2;
            int row;

            for (row = 0; row < 22; row++) {
                SETW(a0, 0, *g++);
                SETW(a0, 0x28, *g++);
                SETW(a0, 0x50, *g++);
                SETW(a0, 0x78, *g++);
                a0 += 0xa0;
            }
            a0 -= 0xdc0;
        }
        a0 += 2;
    } while (n-- != 0);
}

/* orig $007F12: a line of text (20 characters) in the row title_y - 8 */
void draw_text_row(const uint8_t *s)
{
    uint16_t d1 = (uint16_t)(title_y - 8);

    draw_text_row_at(draw_screen + (int16_t)(uint16_t)(d1 * 0xa0u), s, 0x13);
}

/* orig $007F64: a line of text squeezed by the step title_scale: the columns of each character the mask keeps are compacted,
 * the rows the rotating mask drops are left out; from step 16 on the line is drawn plain */
void draw_text_squeezed(const uint8_t *s)
{
    uint8_t *a0, *a6;
    uint16_t col, t;
    const uint16_t *mask;
    int n;

    if ((int16_t)title_scale >= 0x10) {
        draw_text_row(s);
        return;
    }
    a0 = draw_screen;
    a6 = a0 + 0x7d00;
    col = title_scale;
    t = (uint16_t)(((uint16_t)(title_y + title_y) - col) >> 1);
    a0 += (int16_t)(uint16_t)(t * 0xa0u);
    t = (uint16_t)(0xa0 - (uint16_t)((uint32_t)col * 10));
    col = t & 0xf;
    t = (uint16_t)((t - col) >> 3);
    a0 += (int16_t)t;
    mask = &squeeze_masks[(int16_t)title_scale];

    for (n = 0; n < 20; n++) {
        int32_t idx = find_glyph(*s++, title_font_chars);

        if (idx >= 0) {
            uint8_t *keep = a0;
            const uint16_t *g = title_font + ((uint16_t)idx * 0xb0u) / 2;
            uint16_t m = *mask;
            uint16_t rows = 0x15;

            for (;;) {
                uint16_t bit = m >> 15;

                m = (uint16_t)((m << 1) | bit);
                if (bit == 0) {
                    g += 4;
                } else {
                    uint32_t p1 = g[0], p2 = g[1], p3 = g[2], p4 = g[3];
                    uint16_t mm = *mask;
                    int16_t sh;
                    int i;

                    g += 4;
                    for (i = 0; i < 16; i++) {
                        uint16_t carry = mm >> 15;

                        mm = (uint16_t)(mm << 1);
                        if (carry) {
                            p1 <<= 1; p2 <<= 1; p3 <<= 1; p4 <<= 1;
                        } else {
                            p1 = (p1 & 0xffff0000u) | (uint16_t)(p1 << 1);
                            p2 = (p2 & 0xffff0000u) | (uint16_t)(p2 << 1);
                            p3 = (p3 & 0xffff0000u) | (uint16_t)(p3 << 1);
                            p4 = (p4 & 0xffff0000u) | (uint16_t)(p4 << 1);
                        }
                    }
                    sh = (int16_t)(0x10 - col - title_scale);
                    if (sh >= 0) {
                        SETW(a0, 0x28, WORD(a0, 0x28) | (uint16_t)((p2 >> 16) << sh));
                        SETW(a0, 0x50, WORD(a0, 0x50) | (uint16_t)((p3 >> 16) << sh));
                        SETW(a0, 0x78, WORD(a0, 0x78) | (uint16_t)((p4 >> 16) << sh));
                        SETW(a0, 0, WORD(a0, 0) | (uint16_t)((p1 >> 16) << sh));
                        a0 += 2;
                    } else {
                        sh += 0x10;
                        p1 = (p1 << sh) | (p1 >> (32 - sh));
                        p2 = (p2 << sh) | (p2 >> (32 - sh));
                        p3 = (p3 << sh) | (p3 >> (32 - sh));
                        p4 = (p4 << sh) | (p4 >> (32 - sh));
                        SETW(a0, 0x28, WORD(a0, 0x28) | (uint16_t)p2);
                        SETW(a0, 0x50, WORD(a0, 0x50) | (uint16_t)p3);
                        SETW(a0, 0x78, WORD(a0, 0x78) | (uint16_t)p4);
                        SETW(a0, 0, WORD(a0, 0) | (uint16_t)p1);
                        a0 += 2;
                        SETW(a0, 0, (uint16_t)(p1 >> 16));
                        SETW(a0, 0x28, (uint16_t)(p2 >> 16));
                        SETW(a0, 0x50, (uint16_t)(p3 >> 16));
                        SETW(a0, 0x78, (uint16_t)(p4 >> 16));
                    }
                    a0 += 0x9e;
                }
                if ((int32_t)(uintptr_t)a0 >= (int32_t)(uintptr_t)a6)
                    break;
                if (rows-- == 0)
                    break;
            }
            a0 = keep;
        }
        col = (uint16_t)(col + title_scale);
        if ((int16_t)col >= 0x10) {
            a0 += 2;
            col = (uint16_t)(col - 0x10);
        }
    }
}

/* orig $00854C: moves the 48 stars toward the viewer (z drops by 0x200, a star that passes is thrown again) and draws them
 * in the screen; a star that lands on a drawn pixel is marked hidden (bit 15 of z) */
void stars_update(void)
{
    int i;

    for (i = 0; i < 48; i++) {
        struct Star *s = &stars[i];
        int32_t x = s->x, y = s->y;
        int16_t z = (int16_t)(((uint16_t)s->z & 0x7fff) - 0x200);
        uint16_t px, py, mask, c;
        uint8_t *a1;
        int d;

        if (!(z > 0x200))
            goto regen;
    again:
        s->z = (uint16_t)z;
        px = (uint16_t)(divs_low((int32_t)((uint32_t)x << 4), z) + 0xa0);
        if (px >= 0x140)
            goto regen;
        py = (uint16_t)(divs_low((int32_t)((uint32_t)y << 4), z) + 0x64);
        if (py >= 0xc8)
            goto regen;
        goto plot;
    regen:
        {
            uint32_t r = random_next();

            y = (int16_t)r;
            s->y = (int16_t)r;
            r = random_next();
            s->x = (int16_t)r;
            x = (int16_t)r;
            z = 0x1fff;
        }
        goto again;
    plot:
        a1 = draw_screen + (int16_t)(uint16_t)(py * 0xa0u);
        d = px & 0xf;
        a1 += (int16_t)(uint16_t)((px - d) >> 3);
        mask = (uint16_t)(0x8000 >> d);
        if ((WORD(a1, 0) | WORD(a1, 0x28) | WORD(a1, 0x50) | WORD(a1, 0x78)) & mask) {
            s->z |= (int16_t)0x8000;
            continue;
        }
        c = star_colors[(((uint16_t)z << 6) | ((uint16_t)z >> 10)) & 7];
        if (c & 1) SETW(a1, 0, WORD(a1, 0) | mask);
        if (c & 2) SETW(a1, 0x28, WORD(a1, 0x28) | mask);
        if (c & 4) SETW(a1, 0x50, WORD(a1, 0x50) | mask);
        if (c & 8) SETW(a1, 0x78, WORD(a1, 0x78) | mask);
    }
}

/* orig $00861C: erases the visible stars from the screen (clears their pixel in the 4 planes) */
void stars_erase(void)
{
    int i;

    for (i = 0; i < 48; i++) {
        struct Star *s = &stars[i];
        uint16_t px, py, t;
        uint8_t *a1;
        int d;
        uint16_t keep;

        if (s->z < 0)
            continue;
        px = (uint16_t)(divs_low((int32_t)((uint32_t)(int32_t)s->x << 4), s->z) + 0xa0);
        py = (uint16_t)(divs_low((int32_t)((uint32_t)(int32_t)s->y << 4), s->z) + 0x64);
        a1 = draw_screen + (int16_t)(uint16_t)(py * 0xa0u);
        d = px & 0xf;
        t = (uint16_t)((px - d) >> 3);
        a1 += (int16_t)t;
        keep = (uint16_t)((0x7fff >> d) | (0x7fff << (16 - d)));
        SETW(a1, 0, WORD(a1, 0) & keep);
        SETW(a1, 0x28, WORD(a1, 0x28) & keep);
        SETW(a1, 0x50, WORD(a1, 0x50) & keep);
        SETW(a1, 0x78, WORD(a1, 0x78) & keep);
    }
}

/* orig $008092: draws the logo (gfx, rows + 1 rows of width + 1 pixels, 4 planes interleaved per 16 pixels) scaled by title_scale / 16
 * around the screen centre: squeeze_masks[title_scale] picks the rows and the columns kept (one bit per row, one per column, rotated left).
 * Each register of the original holds the output word under construction in its high half and a marker bit that leaves by the carry of
 * add.l when 16 pixels are in; a dropped column shifts only the low half (add.w). */
void draw_logo_scaled(const uint16_t *gfx, int16_t x, int16_t y, int16_t width, int16_t rows)
{
    const uint16_t *a0 = gfx;
    uint8_t *a1 = draw_screen;
    uint16_t px = (uint16_t)((int16_t)(uint16_t)(int32_t)((int32_t)x * (int16_t)title_scale) >> 4) + 0xa0;
    uint16_t py = (uint16_t)((int16_t)(uint16_t)(int32_t)((int32_t)y * (int16_t)title_scale) >> 4) + 0x64;
    uint16_t sh = px & 0xf;
    uint16_t cnt = (uint16_t)rows;
    uint16_t mask = squeeze_masks[(int16_t)title_scale];
    uint16_t rowpat = mask, colpat = mask;
    uint32_t marker;

    a1 += (int16_t)(uint16_t)((int32_t)(int16_t)py * 0xa0);
    a1 += (int16_t)(uint16_t)((uint16_t)(px - sh) >> 3);
    marker = 0x10000u << sh;
    do {
        uint32_t r0 = marker, r1 = marker, r2 = marker, r3 = marker;
        uint16_t d7 = (uint16_t)width;
        uint16_t bit = rowpat >> 15;

        rowpat = (uint16_t)((rowpat << 1) | bit);
        if (bit == 0) {
            a0 += (8 + ((uint16_t)width & 0xfff0) / 2) / 2;
        } else {
            uint8_t *a2 = a1;
            uint16_t d6 = 0;
            uint8_t carry;

            for (;;) {
                d6--;
                if (d6 == 0xffff) {
                    r0 = (r0 & 0xffff0000u) | *a0++;
                    r1 = (r1 & 0xffff0000u) | *a0++;
                    r2 = (r2 & 0xffff0000u) | *a0++;
                    r3 = (r3 & 0xffff0000u) | *a0++;
                    d6 = 0xf;
                }
                bit = colpat >> 15;
                colpat = (uint16_t)((colpat << 1) | bit);
                if (bit == 0) {
                    r0 = (r0 & 0xffff0000u) | (uint16_t)(r0 << 1);
                    r1 = (r1 & 0xffff0000u) | (uint16_t)(r1 << 1);
                    r2 = (r2 & 0xffff0000u) | (uint16_t)(r2 << 1);
                    r3 = (r3 & 0xffff0000u) | (uint16_t)(r3 << 1);
                    if (--d7 != 0xffff)
                        continue;
                    break;
                }
                carry = (uint8_t)(r3 >> 31);
                r0 <<= 1; r1 <<= 1; r2 <<= 1; r3 <<= 1;
                if (!carry) {
                    if (--d7 != 0xffff)
                        continue;
                    break;
                }
                wr16(a2 + 0x28, (uint16_t)(r1 >> 16));
                wr16(a2 + 0x50, (uint16_t)(r2 >> 16));
                wr16(a2 + 0x78, (uint16_t)(r3 >> 16));
                wr16(a2, (uint16_t)(r0 >> 16));
                a2 += 2;
                r0 = 0x10000u | (r0 & 0xffff);
                r1 = 0x10000u | (r1 & 0xffff);
                r2 = 0x10000u | (r2 & 0xffff);
                r3 = 0x10000u | (r3 & 0xffff);
                if (--d7 != 0xffff)
                    continue;
                break;
            }
            do {
                carry = (uint8_t)(r3 >> 31);
                r0 <<= 1; r1 <<= 1; r2 <<= 1; r3 <<= 1;
            } while (!carry);
            wr16(a2 + 0x28, (uint16_t)(r1 >> 16));
            wr16(a2 + 0x50, (uint16_t)(r2 >> 16));
            wr16(a2 + 0x78, (uint16_t)(r3 >> 16));
            wr16(a2, (uint16_t)(r0 >> 16));
            a1 += 0xa0;
        }
        cnt--;
    } while ((int16_t)cnt >= 0);
}

/* orig $0081EA: the text squeezes back out (scale 16 down to 1) over the logo and stars until a key ends it */
void title_fade_out(void)
{
    title_scale = 0x10;
    while (!key_pressed) {
        clear_screen();
        draw_logo_scaled(title_gfx, -0x70, -0x50, 0xcf, 0x35);
        stars_update();
        swap_screens_200();
        title_scale--;
        if (title_scale == 0)
            break;
    }
}

/* orig $008190: stars for 34 frames, then the logo grows in (scale 1 to 15) */
void title_fade_in(void)
{
    uint16_t n = 0x22;

    title_scale = 1;
    do {
        clear_screen();
        stars_update();
        swap_screens_200();
        if (key_pressed)
            break;
    } while (--n != 0);
    for (;;) {
        if (key_pressed)
            return;
        clear_screen();
        draw_logo_scaled(title_gfx, -0x70, -0x50, 0xcf, 0x35);
        stars_update();
        swap_screens_200();
        title_scale++;
        if ((int16_t)title_scale >= 0x10)
            return;
    }
}

/* orig $007DE2: clears the screen and draws the logo (54 rows of 13 groups) at its place */
void title_draw_logo(void)
{
    uint8_t *a0;
    const uint16_t *g = title_gfx;
    int row, k;

    clear_screen();
    a0 = draw_screen + 0xc86;
    for (row = 0; row < 54; row++) {
        for (k = 0; k < 13; k++) {
            SETW(a0, 2 * k, *g++);
            SETW(a0, 2 * k + 0x28, *g++);
            SETW(a0, 2 * k + 0x50, *g++);
            SETW(a0, 2 * k + 0x78, *g++);
        }
        a0 += 0xa0;
    }
}

/* orig $0084AE: plays a table of squeeze steps over the text (a0) with stars and the title palette until a negative step */
void show_text_sequence(uint16_t row, const uint8_t *text, const uint16_t *steps)
{
    title_rows = row;
    title_steps_ptr = (uint16_t *)steps;
    title_text = text;
    title_index = 0;
    for (;;) {
        uint16_t off;

        clear_screen();
        off = title_index;
        steps = title_steps_ptr;
        if (text_wait_key && steps[off / 2] == 0x11) {
            if (key_pressed == 0)
                goto no_advance;
            key_pressed = 0;
        }
        title_index += 2;
    no_advance:
        title_scale = steps[off / 2];
        if ((int16_t)title_scale < 0)
            return;
        if (title_scale != 0) {
            int32_t prod = (int32_t)(int16_t)(uint16_t)(title_rows - 0x64) * (int16_t)title_scale;

            title_y = (int16_t)(((int16_t)(uint16_t)prod >> 4) + 0x64);
            draw_text_squeezed(title_text);
        }
        stars_update();
        swap_screens_200();
        set_palette(title_palette);
    }
}

/* orig $008442: saves the palette, runs two set-up routines and throws 48 new stars */
void title_stars_start(void)
{
    int i;

    for (i = 0; i < 16; i++)
        palette_saved[i] = palette_shadow[i];
    clear_palette();
    work_area_save();
    for (i = 0; i < 48; i++) {
        stars[i].x = (int16_t)random_next();
        stars[i].y = (int16_t)random_next();
        stars[i].z = (int16_t)(random_next() & 0x1fff);
    }
}

/* orig $00847E: the saved palette back, the work area, the dot stars */
void title_restore(void)
{
    set_palette(palette_saved);
    work_area_restore();
    stars_init_dots();
}

/* orig $00842A: a text page of the intro over the starfield, then the old palette and the dot stars */
void title_message(const uint8_t *text)
{
    title_stars_start();
    show_text_sequence(0x64, text, title_script);
    title_restore();
}

/* orig $007D9E: 48 dot stars: screen offset (row * 160 + even byte), bit mask, 0 (same records as the 3D stars) */
void stars_init_dots(void)
{
    int i;

    for (i = 0; i < 48; i++) {
        uint16_t x = (uint16_t)(random_below(0x14) * 2);
        uint16_t off = (uint16_t)((uint16_t)(random_below(0xc0) * 0xa0u) + x);
        uint16_t bit = (uint16_t)(1u << (random_next() & 0xf));

        stars[i].x = (int16_t)off;
        stars[i].y = (int16_t)bit;
        stars[i].z = 0;
    }
}

/* orig $008220: the credits of the title: pages of three text lines squeezed in and out over the logo and stars;
 * a key (key_pressed) ends it */
void title_credits(void)
{
    int i;
    uint16_t d0;

    for (i = 0; i < 16; i++)
        palette_saved[i] = palette_shadow[i];
    music_start(0);
    music_volume_hi = 0x3f;
    title_text = title_lines;
    title_index = 0;
    title_y = 0x78;
    title_phase = 0;
    for (i = 0; i < 48; i++) {
        stars[i].x = (int16_t)random_next();
        stars[i].y = (int16_t)random_next();
        stars[i].z = (int16_t)(random_next() & 0x1fff);
    }
    set_palette(title_palette);
    title_fade_in();
    if (key_pressed)
        goto abort;
l82aa:
    title_draw_logo();
l82ae:
    title_y = 0x78;
    if (title_phase == 1) {
        draw_text_row(title_text);
        title_y = 0x96;
    }
    d0 = title_index;
    title_index += 2;
    title_scale = title_steps[d0 / 2];
    if (title_scale == 0)
        goto frame;
    if ((int16_t)title_scale > 0) {
        const uint8_t *a1 = title_text;

        if (title_phase == 1)
            a1 += 0x14;
        draw_text_squeezed(a1);
        goto frame;
    }
    title_phase++;
    if (title_phase != 3)
        goto l82ae;
    title_phase = 0;
    title_index = 0;
    title_text += 0x28;
    if (title_text != ready_text)
        goto l82ae;
    title_fade_out();
    if (key_pressed)
        goto abort;
    hiscore_show();
    if (key_pressed)
        goto abort;
    title_fade_in();
    if (key_pressed)
        goto abort;
    title_text = title_lines;
    title_index = 0;
    goto l82aa;
frame:
    stars_update();
    if (key_pressed) {
        key_pressed = 0;
        music_silence();
        title_fade_out();
        goto restore;
    }
    swap_screens_200();
    goto l82aa;
abort:
    music_silence();
restore:
    key_pressed = 0;
    set_palette(palette_saved);
}
