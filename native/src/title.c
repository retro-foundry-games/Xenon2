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
        uint32_t v = ((uint32_t)src[i] << 16) | src[i + 1];

        xenon2.palette_shadow[i] = src[i];
        xenon2.palette_shadow[i + 1] = src[i + 1];
        v += v;
        custom->color[i] = (uint16_t)(v >> 16);
        custom->color[i + 1] = (uint16_t)v;
    }
}

/* orig $007EB6: clears the screen being drawn into (32000 bytes) */
void clear_screen(void)
{
    uint32_t *p = (uint32_t *)(xenon2.draw_screen + 0x7d00);
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
    uint8_t *out = dst;
    uint16_t n = count;

    do {
        int32_t idx = find_glyph(*s++, xenon2.title_font_chars);

        if (idx >= 0) {
            const uint16_t *g = xenon2.title_font + ((uint16_t)idx * 0xb0u) / 2;
            int row;

            for (row = 0; row < 22; row++) {
                SETW(out, 0, *g++);
                SETW(out, 0x28, *g++);
                SETW(out, 0x50, *g++);
                SETW(out, 0x78, *g++);
                out += 0xa0;
            }
            out -= 0xdc0;
        }
        out += 2;
    } while (n-- != 0);
}

/* orig $007F12: a line of text (20 characters) in the row title_y - 8 */
void draw_text_row(const uint8_t *s)
{
    uint16_t row_y = (uint16_t)(xenon2.title_y - 8);

    draw_text_row_at(xenon2.draw_screen + (int16_t)(uint16_t)(row_y * 0xa0u), s, 0x13);
}

/* orig $007F64: a line of text squeezed by the step title_scale: the columns of each character the mask keeps are compacted,
 * the rows the rotating mask drops are left out; from step 16 on the line is drawn plain */
void draw_text_squeezed(const uint8_t *s)
{
    uint8_t *out, *end;
    uint16_t col, offset;
    const uint16_t *mask;
    int n;

    if ((int16_t)xenon2.title_scale >= 0x10) {
        draw_text_row(s);
        return;
    }
    out = xenon2.draw_screen;
    end = out + 0x7d00;
    col = xenon2.title_scale;
    offset = (uint16_t)(((uint16_t)(xenon2.title_y + xenon2.title_y) - col) >> 1);
    out += (int16_t)(uint16_t)(offset * 0xa0u);
    offset = (uint16_t)(0xa0 - (uint16_t)((uint32_t)col * 10));
    col = offset & 0xf;
    offset = (uint16_t)((offset - col) >> 3);
    out += (int16_t)offset;
    mask = &xenon2.squeeze_masks[(int16_t)xenon2.title_scale];

    for (n = 0; n < 20; n++) {
        int32_t idx = find_glyph(*s++, xenon2.title_font_chars);

        if (idx >= 0) {
            uint8_t *keep = out;
            const uint16_t *g = xenon2.title_font + ((uint16_t)idx * 0xb0u) / 2;
            uint16_t row_mask = *mask;
            uint16_t rows = 0x15;

            for (;;) {
                uint16_t bit = row_mask >> 15;

                row_mask = (uint16_t)((row_mask << 1) | bit);
                if (bit == 0) {
                    g += 4;
                } else {
                    uint32_t plane1 = g[0], plane2 = g[1], plane3 = g[2], plane4 = g[3];
                    uint16_t col_mask = *mask;
                    int16_t shift;
                    int i;

                    g += 4;
                    for (i = 0; i < 16; i++) {
                        uint16_t carry = col_mask >> 15;

                        col_mask = (uint16_t)(col_mask << 1);
                        if (carry) {
                            plane1 <<= 1; plane2 <<= 1; plane3 <<= 1; plane4 <<= 1;
                        } else {
                            plane1 = (plane1 & 0xffff0000u) | (uint16_t)(plane1 << 1);
                            plane2 = (plane2 & 0xffff0000u) | (uint16_t)(plane2 << 1);
                            plane3 = (plane3 & 0xffff0000u) | (uint16_t)(plane3 << 1);
                            plane4 = (plane4 & 0xffff0000u) | (uint16_t)(plane4 << 1);
                        }
                    }
                    shift = (int16_t)(0x10 - col - xenon2.title_scale);
                    if (shift >= 0) {
                        SETW(out, 0x28, WORD(out, 0x28) | (uint16_t)((plane2 >> 16) << shift));
                        SETW(out, 0x50, WORD(out, 0x50) | (uint16_t)((plane3 >> 16) << shift));
                        SETW(out, 0x78, WORD(out, 0x78) | (uint16_t)((plane4 >> 16) << shift));
                        SETW(out, 0, WORD(out, 0) | (uint16_t)((plane1 >> 16) << shift));
                        out += 2;
                    } else {
                        shift += 0x10;
                        plane1 = (plane1 << shift) | (plane1 >> (32 - shift));
                        plane2 = (plane2 << shift) | (plane2 >> (32 - shift));
                        plane3 = (plane3 << shift) | (plane3 >> (32 - shift));
                        plane4 = (plane4 << shift) | (plane4 >> (32 - shift));
                        SETW(out, 0x28, WORD(out, 0x28) | (uint16_t)plane2);
                        SETW(out, 0x50, WORD(out, 0x50) | (uint16_t)plane3);
                        SETW(out, 0x78, WORD(out, 0x78) | (uint16_t)plane4);
                        SETW(out, 0, WORD(out, 0) | (uint16_t)plane1);
                        out += 2;
                        SETW(out, 0, (uint16_t)(plane1 >> 16));
                        SETW(out, 0x28, (uint16_t)(plane2 >> 16));
                        SETW(out, 0x50, (uint16_t)(plane3 >> 16));
                        SETW(out, 0x78, (uint16_t)(plane4 >> 16));
                    }
                    out += 0x9e;
                }
                if ((int32_t)(uintptr_t)out >= (int32_t)(uintptr_t)end)
                    break;
                if (rows-- == 0)
                    break;
            }
            out = keep;
        }
        col = (uint16_t)(col + xenon2.title_scale);
        if ((int16_t)col >= 0x10) {
            out += 2;
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
        struct Star *s = &xenon2.stars[i];
        int32_t x = s->x, y = s->y;
        int16_t z = (int16_t)(((uint16_t)s->z & 0x7fff) - 0x200);
        uint16_t px, py, mask, color;
        uint8_t *dst;
        int pixel;

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
            uint32_t rnd = random_next();

            y = (int16_t)rnd;
            s->y = (int16_t)rnd;
            rnd = random_next();
            s->x = (int16_t)rnd;
            x = (int16_t)rnd;
            z = 0x1fff;
        }
        goto again;
    plot:
        dst = xenon2.draw_screen + (int16_t)(uint16_t)(py * 0xa0u);
        pixel = px & 0xf;
        dst += (int16_t)(uint16_t)((px - pixel) >> 3);
        mask = (uint16_t)(0x8000 >> pixel);
        if ((WORD(dst, 0) | WORD(dst, 0x28) | WORD(dst, 0x50) | WORD(dst, 0x78)) & mask) {
            s->z |= (int16_t)0x8000;
            continue;
        }
        color = xenon2.star_colors[(((uint16_t)z << 6) | ((uint16_t)z >> 10)) & 7];
        if (color & 1) SETW(dst, 0, WORD(dst, 0) | mask);
        if (color & 2) SETW(dst, 0x28, WORD(dst, 0x28) | mask);
        if (color & 4) SETW(dst, 0x50, WORD(dst, 0x50) | mask);
        if (color & 8) SETW(dst, 0x78, WORD(dst, 0x78) | mask);
    }
}

/* orig $00861C: erases the visible stars from the screen (clears their pixel in the 4 planes) */
void stars_erase(void)
{
    int i;

    for (i = 0; i < 48; i++) {
        struct Star *s = &xenon2.stars[i];
        uint16_t px, py, byte_off;
        uint8_t *dst;
        int pixel;
        uint16_t keep;

        if (s->z < 0)
            continue;
        px = (uint16_t)(divs_low((int32_t)((uint32_t)(int32_t)s->x << 4), s->z) + 0xa0);
        py = (uint16_t)(divs_low((int32_t)((uint32_t)(int32_t)s->y << 4), s->z) + 0x64);
        dst = xenon2.draw_screen + (int16_t)(uint16_t)(py * 0xa0u);
        pixel = px & 0xf;
        byte_off = (uint16_t)((px - pixel) >> 3);
        dst += (int16_t)byte_off;
        keep = (uint16_t)((0x7fff >> pixel) | (0x7fff << (16 - pixel)));
        SETW(dst, 0, WORD(dst, 0) & keep);
        SETW(dst, 0x28, WORD(dst, 0x28) & keep);
        SETW(dst, 0x50, WORD(dst, 0x50) & keep);
        SETW(dst, 0x78, WORD(dst, 0x78) & keep);
    }
}

/* orig $008092: draws the logo (gfx, rows + 1 rows of width + 1 pixels, 4 planes interleaved per 16 pixels) scaled by title_scale / 16
 * around the screen centre: squeeze_masks[title_scale] picks the rows and the columns kept (one bit per row, one per column, rotated left).
 * Each register of the original holds the output word under construction in its high half and a marker bit that leaves by the carry of
 * add.l when 16 pixels are in; a dropped column shifts only the low half (add.w). */
void draw_logo_scaled(const uint16_t *gfx, int16_t x, int16_t y, int16_t width, int16_t rows)
{
    const uint16_t *src = gfx;
    uint8_t *dst = xenon2.draw_screen;
    uint16_t px = (uint16_t)((int16_t)(uint16_t)(int32_t)((int32_t)x * (int16_t)xenon2.title_scale) >> 4) + 0xa0;
    uint16_t py = (uint16_t)((int16_t)(uint16_t)(int32_t)((int32_t)y * (int16_t)xenon2.title_scale) >> 4) + 0x64;
    uint16_t shift = px & 0xf;
    uint16_t cnt = (uint16_t)rows;
    uint16_t mask = xenon2.squeeze_masks[(int16_t)xenon2.title_scale];
    uint16_t rowpat = mask, colpat = mask;
    uint32_t marker;

    dst += (int16_t)(uint16_t)((int32_t)(int16_t)py * 0xa0);
    dst += (int16_t)(uint16_t)((uint16_t)(px - shift) >> 3);
    marker = 0x10000u << shift;
    do {
        uint32_t w0 = marker, w1 = marker, w2 = marker, w3 = marker;
        uint16_t cols_left = (uint16_t)width;
        uint16_t bit = rowpat >> 15;

        rowpat = (uint16_t)((rowpat << 1) | bit);
        if (bit == 0) {
            src += (8 + ((uint16_t)width & 0xfff0) / 2) / 2;
        } else {
            uint8_t *out = dst;
            uint16_t bits_left = 0;
            uint8_t carry;

            for (;;) {
                bits_left--;
                if (bits_left == 0xffff) {
                    w0 = (w0 & 0xffff0000u) | *src++;
                    w1 = (w1 & 0xffff0000u) | *src++;
                    w2 = (w2 & 0xffff0000u) | *src++;
                    w3 = (w3 & 0xffff0000u) | *src++;
                    bits_left = 0xf;
                }
                bit = colpat >> 15;
                colpat = (uint16_t)((colpat << 1) | bit);
                if (bit == 0) {
                    w0 = (w0 & 0xffff0000u) | (uint16_t)(w0 << 1);
                    w1 = (w1 & 0xffff0000u) | (uint16_t)(w1 << 1);
                    w2 = (w2 & 0xffff0000u) | (uint16_t)(w2 << 1);
                    w3 = (w3 & 0xffff0000u) | (uint16_t)(w3 << 1);
                    if (--cols_left != 0xffff)
                        continue;
                    break;
                }
                carry = (uint8_t)(w3 >> 31);
                w0 <<= 1; w1 <<= 1; w2 <<= 1; w3 <<= 1;
                if (!carry) {
                    if (--cols_left != 0xffff)
                        continue;
                    break;
                }
                wr16(out + 0x28, (uint16_t)(w1 >> 16));
                wr16(out + 0x50, (uint16_t)(w2 >> 16));
                wr16(out + 0x78, (uint16_t)(w3 >> 16));
                wr16(out, (uint16_t)(w0 >> 16));
                out += 2;
                w0 = 0x10000u | (w0 & 0xffff);
                w1 = 0x10000u | (w1 & 0xffff);
                w2 = 0x10000u | (w2 & 0xffff);
                w3 = 0x10000u | (w3 & 0xffff);
                if (--cols_left != 0xffff)
                    continue;
                break;
            }
            do {
                carry = (uint8_t)(w3 >> 31);
                w0 <<= 1; w1 <<= 1; w2 <<= 1; w3 <<= 1;
            } while (!carry);
            wr16(out + 0x28, (uint16_t)(w1 >> 16));
            wr16(out + 0x50, (uint16_t)(w2 >> 16));
            wr16(out + 0x78, (uint16_t)(w3 >> 16));
            wr16(out, (uint16_t)(w0 >> 16));
            dst += 0xa0;
        }
        cnt--;
    } while ((int16_t)cnt >= 0);
}

/* orig $0081EA: the text squeezes back out (scale 16 down to 1) over the logo and stars until a key ends it */
void title_fade_out(void)
{
    xenon2.title_scale = 0x10;
    while (!xenon2.key_pressed) {
        clear_screen();
        draw_logo_scaled(xenon2.title_gfx, -0x70, -0x50, 0xcf, 0x35);
        stars_update();
        swap_screens_200();
        xenon2.title_scale--;
        if (xenon2.title_scale == 0)
            break;
    }
}

/* orig $008190: stars for 34 frames, then the logo grows in (scale 1 to 15) */
void title_fade_in(void)
{
    uint16_t n = 0x22;

    xenon2.title_scale = 1;
    do {
        clear_screen();
        stars_update();
        swap_screens_200();
        if (xenon2.key_pressed)
            break;
    } while (--n != 0);
    for (;;) {
        if (xenon2.key_pressed)
            return;
        clear_screen();
        draw_logo_scaled(xenon2.title_gfx, -0x70, -0x50, 0xcf, 0x35);
        stars_update();
        swap_screens_200();
        xenon2.title_scale++;
        if ((int16_t)xenon2.title_scale >= 0x10)
            return;
    }
}

/* orig $007DE2: clears the screen and draws the logo (54 rows of 13 groups) at its place */
void title_draw_logo(void)
{
    uint8_t *dst;
    const uint16_t *g = xenon2.title_gfx;
    int row, k;

    clear_screen();
    dst = xenon2.draw_screen + 0xc86;
    for (row = 0; row < 54; row++) {
        for (k = 0; k < 13; k++) {
            SETW(dst, 2 * k, *g++);
            SETW(dst, 2 * k + 0x28, *g++);
            SETW(dst, 2 * k + 0x50, *g++);
            SETW(dst, 2 * k + 0x78, *g++);
        }
        dst += 0xa0;
    }
}

/* orig $0084AE: plays a table of squeeze steps over the text with stars and the title palette until a negative step */
void show_text_sequence(uint16_t row, const uint8_t *text, const uint16_t *steps)
{
    xenon2.title_rows = row;
    xenon2.title_steps_ptr = (uint16_t *)steps;
    xenon2.title_text = text;
    xenon2.title_index = 0;
    for (;;) {
        uint16_t off;

        clear_screen();
        off = xenon2.title_index;
        steps = xenon2.title_steps_ptr;
        if (xenon2.text_wait_key && steps[off / 2] == 0x11) {
            if (xenon2.key_pressed == 0)
                goto no_advance;
            xenon2.key_pressed = 0;
        }
        xenon2.title_index += 2;
    no_advance:
        xenon2.title_scale = steps[off / 2];
        if ((int16_t)xenon2.title_scale < 0)
            return;
        if (xenon2.title_scale != 0) {
            int32_t prod = (int32_t)(int16_t)(uint16_t)(xenon2.title_rows - 0x64) * (int16_t)xenon2.title_scale;

            xenon2.title_y = (int16_t)(((int16_t)(uint16_t)prod >> 4) + 0x64);
            draw_text_squeezed(xenon2.title_text);
        }
        stars_update();
        swap_screens_200();
        set_palette(xenon2.title_palette);
    }
}

/* orig $008442: saves the palette, runs two set-up routines and throws 48 new stars */
void title_stars_start(void)
{
    int i;

    for (i = 0; i < 16; i++)
        xenon2.palette_saved[i] = xenon2.palette_shadow[i];
    clear_palette();
    work_area_save();
    for (i = 0; i < 48; i++) {
        xenon2.stars[i].x = (int16_t)random_next();
        xenon2.stars[i].y = (int16_t)random_next();
        xenon2.stars[i].z = (int16_t)(random_next() & 0x1fff);
    }
}

/* orig $00847E: the saved palette back, the work area, the dot stars */
void title_restore(void)
{
    set_palette(xenon2.palette_saved);
    work_area_restore();
    stars_init_dots();
}

/* orig $00842A: a text page of the intro over the starfield, then the old palette and the dot stars */
void title_message(const uint8_t *text)
{
    title_stars_start();
    show_text_sequence(0x64, text, xenon2.title_script);
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

        xenon2.stars[i].x = (int16_t)off;
        xenon2.stars[i].y = (int16_t)bit;
        xenon2.stars[i].z = 0;
    }
}

/* orig $008220: the credits of the title: pages of three text lines squeezed in and out over the logo and stars;
 * a key (key_pressed) ends it */
void title_credits(void)
{
    int i;
    uint16_t step_off;

    for (i = 0; i < 16; i++)
        xenon2.palette_saved[i] = xenon2.palette_shadow[i];
    music_start(0);
    xenon2.music_volume_hi = 0x3f;
    xenon2.title_text = xenon2.title_lines;
    xenon2.title_index = 0;
    xenon2.title_y = 0x78;
    xenon2.title_phase = 0;
    for (i = 0; i < 48; i++) {
        xenon2.stars[i].x = (int16_t)random_next();
        xenon2.stars[i].y = (int16_t)random_next();
        xenon2.stars[i].z = (int16_t)(random_next() & 0x1fff);
    }
    set_palette(xenon2.title_palette);
    title_fade_in();
    if (xenon2.key_pressed)
        goto abort;
l82aa:
    title_draw_logo();
l82ae:
    xenon2.title_y = 0x78;
    if (xenon2.title_phase == 1) {
        draw_text_row(xenon2.title_text);
        xenon2.title_y = 0x96;
    }
    step_off = xenon2.title_index;
    xenon2.title_index += 2;
    xenon2.title_scale = xenon2.title_steps[step_off / 2];
    if (xenon2.title_scale == 0)
        goto frame;
    if ((int16_t)xenon2.title_scale > 0) {
        const uint8_t *line = xenon2.title_text;

        if (xenon2.title_phase == 1)
            line += 0x14;
        draw_text_squeezed(line);
        goto frame;
    }
    xenon2.title_phase++;
    if (xenon2.title_phase != 3)
        goto l82ae;
    xenon2.title_phase = 0;
    xenon2.title_index = 0;
    xenon2.title_text += 0x28;
    if (xenon2.title_text != xenon2.ready_text)
        goto l82ae;
    title_fade_out();
    if (xenon2.key_pressed)
        goto abort;
    hiscore_show();
    if (xenon2.key_pressed)
        goto abort;
    title_fade_in();
    if (xenon2.key_pressed)
        goto abort;
    xenon2.title_text = xenon2.title_lines;
    xenon2.title_index = 0;
    goto l82aa;
frame:
    stars_update();
    if (xenon2.key_pressed) {
        xenon2.key_pressed = 0;
        music_silence();
        title_fade_out();
        goto restore;
    }
    swap_screens_200();
    goto l82aa;
abort:
    music_silence();
restore:
    xenon2.key_pressed = 0;
    set_palette(xenon2.palette_saved);
}
