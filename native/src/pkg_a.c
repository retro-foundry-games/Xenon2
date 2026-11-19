#include "pkg_a.h"

typedef uint16_t u16a __attribute__((may_alias));

#define W(p) (*(u16a *)(p))
#define BLIT_WAIT() while (custom->dmaconr & DMAF_BLTBUSY) ;

/* inlined: $400 (bra.w to the start-up code) */
/* inlined: $E0C, $E12, $E18, $E2A, $E54, $E5A (jmp thunks of the table at $E0C: callers call the target) */

/* orig $FEE, $FF2, $1002 and $100E: take an object out of its list and put it on the free list */
void free_object(Obj *obj)
{
    Obj *next = obj->next, *prev = obj->prev;

    next->prev = prev;
    prev->next = next;
    obj->next = free_obj;
    free_obj = obj;
    obj->type = 0;
}

/* The clip of a bob (a picture with a mask) to the screen: header words dx, dy, width, height, then rows of 4 plane words and a mask word. */
typedef struct {
    const uint8_t *src;     /* a0 */
    uint8_t *dst;           /* a1 */
    const uint8_t *mask;    /* a2 */
    int16_t shift;          /* d0: x & 15 */
    int16_t width;          /* d1 */
    int16_t right;          /* d2: width + shift */
    int16_t xend;           /* d3 */
    int16_t xstart;         /* d4 */
    uint16_t size;          /* d6: blitter size */
    int16_t stride;         /* d7: bytes to the next plane word */
} Clip;

static uint16_t lsr_w(uint16_t v, int16_t count)
{
    count &= 63;
    return count >= 16 ? 0 : (uint16_t)(v >> count);
}

/* inlined: $107E, $1084, $1096, $109A, $10A8, $10B4, $10C4, $10C8, $10CE, $10D6, $10DA, $10E0, $10E8, $10F2
 * (the clip; returns 0 where the original returns from its caller) */
static int clip_bob(Clip *c, int16_t x, int16_t y)
{
    const uint8_t *a0 = c->src;
    int16_t d0 = x, d1 = y, d3, d4, d5, d6, d7;
    int32_t sum;

    d0 = (int16_t)(d0 - (int16_t)W(a0)); a0 += 2;
    if (d0 >= 0x140)
        return 0;
    d1 = (int16_t)(d1 - (int16_t)W(a0)); a0 += 2;
    d5 = (int16_t)(clip_bottom - d1);
    if (d5 < 0)
        return 0;
    d7 = d1;
    if (d7 < 0)
        d1 = 0;
    d3 = d0;
    c->dst = draw_screen + (int16_t)row_offset[(uint16_t)d1];
    d1 = d0;
    d0 &= 0xf;
    d1 = (int16_t)(d1 - d0);
    d1 >>= 3;
    c->dst += d1;
    d1 = (int16_t)W(a0); a0 += 2;
    d4 = d3;
    sum = (int32_t)d3 + d1;
    d3 = (int16_t)sum;
    if (sum <= 0)
        return 0;
    d6 = (int16_t)W(a0); a0 += 2;
    if (d6 > d5)
        d6 = d5;
    if (-(int32_t)d7 > 0) {
        d7 = (int16_t)-d7;
        d6 = (int16_t)(d6 - d7);
        if (d6 < 0)
            return 0;
        d7 = (int16_t)(d7 + d7);
        d5 = d7;
        d7 = (int16_t)(d7 + d7);
        d7 = (int16_t)(d7 + d7);
        d7 = (int16_t)(d7 + d5);
        a0 += d7;
        if (d1 > 0x10)
            a0 += d7;
    }
    c->src = a0;
    c->shift = d0;
    c->width = d1;
    c->right = (int16_t)(d1 + d0);
    c->xend = d3;
    c->xstart = d4;
    c->size = (uint16_t)((uint16_t)(d6 + 1) << 6);
    return 1;
}

/* inlined: $10F6, $1104, $1110, $1114, $1122, $1142, $1146, $114A, $114E, $117E, $1180, $118E, $119A, $11A8, $11AC, $11BA, $11D2, $11D8, $11F4,
 * $1210, $124C: the blitter's masks and modulos for the first and last word of the bob; HLE: blitter */
static void blit_edges(Clip *c)
{
    int16_t d0 = c->shift, d1 = c->width, d2 = c->right, d3 = c->xend, d4 = c->xstart;
    uint16_t m;

    if (d1 > 0x10)
        goto wide;
    c->mask = c->src + 8;
    c->stride = 2;
    if (d3 <= 0x140 && d2 > 0x10) {
        m = lsr_w(0xffff, (int16_t)-d4);
        if (-(int32_t)d4 <= 0)
            m = 0xffff;
        custom->bltafwm = m;
        custom->bltalwm = 0;
        custom->bltamod = 6;
        custom->bltbmod = 6;
        custom->bltcmod = 0x9c;
        custom->bltdmod = 0x9c;
        c->size += 2;
        return;
    }
    custom->bltalwm = (uint16_t)(0xffffu << (d0 & 63));
    custom->bltafwm = 0xffff;
    custom->bltamod = 8;
    custom->bltbmod = 8;
    custom->bltcmod = 0x9e;
    custom->bltdmod = 0x9e;
    c->size += 1;
    return;

wide:
    c->stride = 4;
    c->mask = c->src + 0x10;
    if (d3 > 0x140) {
        custom->bltalwm = (uint16_t)(0xffffu << (d0 & 63));
        if (d4 >= 0x130) {
            custom->bltafwm = 0xffff;
            custom->bltamod = 0x12;
            custom->bltbmod = 0x12;
            custom->bltcmod = 0x9e;
            custom->bltdmod = 0x9e;
            c->size += 1;
            return;
        }
        goto first_mask;
    }
    if (d4 <= -16) {
        c->src += 2;
        c->dst += 2;
        c->mask += 2;
        d4 = (int16_t)(d4 + 0x10);
        custom->bltafwm = lsr_w(0xffff, (int16_t)-d4);
        custom->bltalwm = 0;
        custom->bltamod = 0x10;
        custom->bltbmod = 0x10;
        custom->bltcmod = 0x9c;
        custom->bltdmod = 0x9c;
        c->size += 2;
        return;
    }
    if (d2 > 0x20) {
        m = 0xffff;
        if (-(int32_t)d4 > 0)
            m = lsr_w(0xffff, (int16_t)-d4);
        custom->bltafwm = m;
        custom->bltalwm = 0;
        custom->bltamod = 0xe;
        custom->bltbmod = 0xe;
        custom->bltcmod = 0x9a;
        custom->bltdmod = 0x9a;
        c->size += 3;
        return;
    }
    custom->bltalwm = 0xffff;
first_mask:
    m = 0xffff;
    if (-(int32_t)d4 > 0)
        m = lsr_w(0xffff, (int16_t)-d4);
    custom->bltafwm = m;
    custom->bltamod = 0x10;
    custom->bltbmod = 0x10;
    custom->bltcmod = 0x9c;
    custom->bltdmod = 0x9c;
    c->size += 2;
}

/* orig $129A: draw a bob (masked, cookie-cut) at x, y; gfx = its header; HLE: blitter */
HLE_WEAK void draw_bob_masked_at(uint16_t x, uint16_t y, const uint8_t *gfx)
{
    Clip c;
    uint16_t d0;
    const uint8_t *a0;
    uint8_t *a1;

    c.src = gfx;
    if (!clip_bob(&c, (int16_t)x, (int16_t)y))
        return;
    blit_edges(&c);
    d0 = (uint16_t)c.shift;
    d0 = (uint16_t)((d0 >> 4) | (d0 << 12));
    custom->bltcon1 = d0;
    custom->bltcon0 = d0 | 0x0fca;
    a0 = c.src;
    a1 = c.dst;
    for (int plane = 0; plane < 4; plane++) {
        custom->bltapt = ADDR(c.mask);
        custom->bltbpt = ADDR(a0);
        custom->bltcpt = ADDR(a1);
        custom->bltdpt = ADDR(a1);
        custom->bltsize = c.size;
        a0 += c.stride;
        a1 += 0x28;
        BLIT_WAIT();
    }
}

/* orig $128E, $133E, $1350, $135C and $1366: draw the bob of an object, masked; HLE: blitter */
HLE_WEAK void draw_bob_masked(Obj *obj)
{
    draw_bob_masked_at(obj->w20, obj->w24, obj->gfx);
}

/* orig $137C, $1384, $139E, $13AA, $13B4, $13BE, $13D0, $13DA, $13EA, $1404, $1414, $142C and $1436: draw a bob by OR-ing its mask
 * into planes 0-2 and clearing it in plane 3 (a flash); HLE: blitter */
HLE_WEAK void draw_bob_or_at(uint16_t x, uint16_t y, const uint8_t *gfx)
{
    Clip c;
    uint16_t d0;
    uint8_t *a1;

    c.src = gfx;
    if (!clip_bob(&c, (int16_t)x, (int16_t)y))
        return;
    blit_edges(&c);
    d0 = (uint16_t)c.shift;
    d0 = (uint16_t)((d0 >> 4) | (d0 << 12));
    d0 |= 0x0bfa;
    custom->bltcon0 = d0;
    custom->bltcon1 = 0;
    a1 = c.dst;
    for (int plane = 0; plane < 4; plane++) {
        if (plane == 3)
            custom->bltcon0 = (uint16_t)((d0 & 0xff00) | 0x0a);
        custom->bltapt = ADDR(c.mask);
        custom->bltcpt = ADDR(a1);
        custom->bltdpt = ADDR(a1);
        custom->bltsize = c.size;
        a1 += 0x28;
        BLIT_WAIT();
    }
}

/* orig $1370 and $1378: draw the bob of an object as above */
HLE_WEAK void draw_bob_or(Obj *obj)
{
    draw_bob_or_at(obj->w20, obj->w24, obj->gfx);
}

/* orig $1368: the object is drawn masked from now on, this time as above */
HLE_WEAK void draw_bob_or_first(Obj *obj)
{
    obj->fn_b = (void (*)(void))draw_bob_masked;
    draw_bob_or(obj);
}

/* orig $15CE: draw a zero-terminated string with the font into the screen at dest (2 bytes a character); HLE: blitter */
HLE_WEAK void draw_text(const uint8_t *text, uint8_t *dest)
{
    for (;;) {
        uint8_t c = *text++;

        if (c == 0)
            return;
        if (c != 0x20) {
            const uint8_t *p = text_chars;   /* the search of $7EF8 */
            uint16_t index = 0xffff;
            const uint8_t *glyph;

            while (*p) {
                if (*p++ == c) {
                    index = (uint16_t)(p - text_chars - 1);
                    break;
                }
            }
            glyph = font_gfx + (int16_t)(uint16_t)(index << 7);
            custom->bltcon0 = 0x09f0;
            custom->bltcon1 = 0;
            custom->bltamod = 0;
            custom->bltdmod = 0x26;
            custom->bltafwm = 0xffff;
            custom->bltalwm = 0xffff;
            custom->bltapt = ADDR(glyph);
            custom->bltdpt = ADDR(dest);
            custom->bltsize = 0x1001;
            BLIT_WAIT();
        }
        dest += 2;
    }
}

/* orig $1636: draw value as 7 decimal digits (8 lines each, 1 byte a digit) at dest in this screen and in the other one; the
 * unexecuted part is read from the listing */
void draw_number(uint32_t value, uint8_t *dest)
{
    uint8_t *other = dest - 0x8700;
    const uint32_t *p = pow10;

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
        g = &digit_gfx[(uint16_t)(digit << 5)];
        for (int row = 0; row < 8; row++) {
            for (int plane = 0; plane < 4; plane++) {
                uint8_t b = *g++;

                dest[row * 0xa0 + plane * 0x28] = b;
                other[row * 0xa0 + plane * 0x28] = b;
            }
        }
        dest += 1;
        other += 1;
    }
}

/* orig $16BE: draw the energy bar (cc6 of 39 cells) into the two screens; flag a6b says it is to be redrawn */
void redraw_energy_bar(void)
{
    uint8_t *a0, *a1;
    uint16_t d0, d2, d3;

    if (!v_0a6b)
        return;
    v_0a6b = 0;
    d0 = v_0cc6;
    if ((int16_t)d0 > 0x27)
        d0 = 0x27;
    a0 = &screen_b[0x79ea];
    a1 = &screen_a[0x79ea];
    d2 = 0x10;
    if (cur_player) {
        a0 = &screen_b[0x79f6];
        a1 = &screen_a[0x79f6];
        d2 = 2;
    }
    d3 = (uint16_t)~d2;
    for (int cell = 0; cell < 0x27; cell++) {
        int16_t d4 = 0;

        if (d0 != 0) {
            d0--;
            do {
                W(a0 + d4) &= d3;
                W(a0 + 0x28 + d4) |= d2;
                W(a0 + 0x50 + d4) |= d2;
                W(a0 + 0x78 + d4) |= d2;
                W(a1 + d4) &= d3;
                W(a1 + 0x28 + d4) |= d2;
                W(a1 + 0x50 + d4) |= d2;
                W(a1 + 0x78 + d4) |= d2;
                d4 = (int16_t)(d4 + 0xa0);
            } while (d4 <= 0x140);
        } else {
            do {
                W(a0 + d4) &= d3;
                W(a0 + 0x28 + d4) &= d3;
                W(a0 + 0x50 + d4) &= d3;
                W(a0 + 0x78 + d4) &= d3;
                W(a1 + d4) &= d3;
                W(a1 + 0x28 + d4) &= d3;
                W(a1 + 0x50 + d4) &= d3;
                W(a1 + 0x78 + d4) &= d3;
                d4 = (int16_t)(d4 + 0xa0);
            } while (d4 <= 0x140);
        }
        {
            uint16_t carry = d2 & 1;

            d2 = (uint16_t)((d2 >> 1) | (d2 << 15));
            if (carry) {
                a0 += 2;
                a1 += 2;
            }
        }
        d3 = (uint16_t)~d2;
    }
}

/* orig $176E: copy 8 lines of 4 plane bytes of the digit glyph cca (0-9, 32 bytes each) into both screens */
void draw_status_digit(void)
{
    int16_t d0 = (int16_t)v_0cca;
    const uint8_t *a0;
    uint8_t *a1 = &screen_b[0x78b1], *a2 = &screen_a[0x78b1];

    if (d0 > 9)
        d0 = 9;
    a0 = &digit_gfx[0x140] + (int16_t)(uint16_t)(d0 << 5);
    if (cur_player) {
        a1 = &screen_b[0x78b6];
        a2 = &screen_a[0x78b6];
    }
    for (int row = 0; row < 8; row++) {
        for (int plane = 0; plane < 4; plane++) {
            uint8_t b = *a0++;

            a1[plane * 0x28] = b;
            a2[plane * 0x28] = b;
        }
        a1 += 0xa0;
        a2 += 0xa0;
    }
}

/* one tile of the background: a plain one is copied, one with bit 15 set is cookie-cut with its mask into the 4 planes */
static void put_tile(const uint8_t *tiles, uint16_t cell, uint8_t *dest, uint16_t rows_size, int partial)
{
    const uint8_t *a3;

    if (!(cell & 0x8000)) {
        a3 = tiles + ((uint32_t)cell << 4);
        custom->bltcon0 = 0x09f0;
        custom->bltcon1 = 0;
        custom->bltamod = 0;
        custom->bltdmod = 0x26;
        custom->bltafwm = 0xffff;
        custom->bltalwm = 0xffff;
        custom->bltapt = ADDR(a3);
        custom->bltdpt = ADDR(dest);
        custom->bltsize = partial ? (uint16_t)((uint16_t)(rows_size << 2) - 3) : 0x1001;
        BLIT_WAIT();
        return;
    }
    a3 = tiles + ((uint32_t)(cell & 0x7fff) << 4);
    {
        const uint8_t *a6 = a3 + 2;

        custom->bltcon0 = 0x0fca;
        custom->bltcon1 = 0;
        custom->bltamod = 8;
        custom->bltbmod = 8;
        custom->bltcmod = 0x9e;
        custom->bltdmod = 0x9e;
        for (int plane = 0; plane < 4; plane++) {
            custom->bltapt = ADDR(a3);
            custom->bltbpt = ADDR(a6);
            custom->bltcpt = ADDR(dest);
            custom->bltdpt = ADDR(dest);
            custom->bltsize = partial ? rows_size : 0x0401;
            dest += 0x28;
            a6 += 2;
            BLIT_WAIT();
        }
    }
}

/* orig $17D6: draw the background: the picture scrolled by the strip at $436, then the map's tiles over it; HLE: blitter */
HLE_WEAK void draw_background(void)
{
    uint16_t scroll = (uint16_t)(v_0436 >> 16);
    uint16_t d1 = (uint16_t)(0xc0 - scroll);
    uint8_t *a0 = map_image + (int16_t)(uint16_t)(scroll * 0x50u);
    uint8_t *a1 = draw_screen, *a4, *a5;
    const uint16_t *cells;
    const uint8_t *tiles;
    int16_t d2, rows;

    custom->bltapt = ADDR(a0);
    custom->bltdpt = ADDR(a1);
    custom->bltamod = 0;
    custom->bltdmod = 0x50;
    custom->bltafwm = 0xffff;
    custom->bltalwm = 0xffff;
    custom->bltcon0 = 0x09f0;
    custom->bltcon1 = 0;
    custom->bltsize = (uint16_t)((uint16_t)(d1 << 6) + 0x28);
    BLIT_WAIT();
    d1 = scroll;
    if (d1) {
        custom->bltapt = ADDR(map_image);
        custom->bltsize = (uint16_t)((uint16_t)(d1 << 6) + 0x28);
        BLIT_WAIT();
    }
    a1 += 0x50;
    custom->bltdpt = ADDR(a1);
    custom->bltcon0 = 0x01f0;
    custom->bltadat = 0;
    custom->bltsize = 0x3028;
    BLIT_WAIT();
    if (v_0d85)
        return;

    cells = (const uint16_t *)((const uint8_t *)level_init_src + (int16_t)v_0cce);
    rows = 0xc;
    d2 = (int16_t)-(int16_t)v_0cd6;
    a0 = draw_screen;
    a4 = a0 + 0x7800;
    a5 = a0 + 0x6ea0;
    tiles = tile_gfx;
    custom->bltafwm = 0xffff;
    custom->bltalwm = 0xffff;
    while (d2 <= -16) {
        cells += 20;
        d2 = (int16_t)(d2 + 0x10);
        if (rows-- == 0)
            return;
    }
    if (d2 >= 0xc0)
        return;
    a0 += (int16_t)(uint16_t)((int32_t)d2 * 0xa0);
    for (;;) {
        if ((int32_t)ADDR(a0) >= (int32_t)ADDR(a5)) {
            uint32_t d7 = ADDR(a4) - ADDR(a0);
            uint16_t size;
            uint8_t *col = a0;

            if (d7 / 0xa0 <= 0xffff)
                d7 = ((d7 % 0xa0) << 16) | (d7 / 0xa0);
            size = (uint16_t)((uint16_t)((uint16_t)d7 << 6) + 1);
            for (int n = 0; n < 20; n++, col += 2) {
                uint16_t cell = *cells++;
                uint16_t t;

                if (cell == 0)
                    continue;
                if (!(cell & 0x8000)) {
                    t = (uint16_t)(size << 2);
                    put_tile(tiles, cell, col, t, 1);
                    size = (uint16_t)((uint16_t)(t - 3 + 3) >> 2);
                } else
                    put_tile(tiles, cell, col, size, 1);
            }
            return;
        }
        {
            uint8_t *col = a0;

            for (int n = 0; n < 20; n++, col += 2) {
                uint16_t cell = *cells++;

                if (cell != 0)
                    put_tile(tiles, cell, col, 0, 0);
            }
        }
        a0 += 0xa00;
        if ((int32_t)ADDR(a0) >= (int32_t)ADDR(a4))
            return;
        if (rows-- == 0)
            return;
    }
}
