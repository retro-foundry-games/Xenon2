#include "draw.h"
#include "game_all.h"

#define BLIT_WAIT() while (custom->dmaconr & DMAF_BLTBUSY) ;

/* inlined: $400 (bra.w to the start-up code) */
/* inlined: $E0C, $E12, $E18, $E2A, $E54, $E5A (jmp thunks of the table at $E0C: callers call the target) */

/* orig $E42, $FEE, $FF2, $1002 and $100E: take an object out of its list and put it on the free list */
void free_object(Object *obj)
{
    Object *next = obj->next, *prev = obj->prev;

    next->prev = prev;
    prev->next = next;
    obj->next = xenon2.free_obj;
    xenon2.free_obj = obj;
    obj->type = 0;
}

/* The clip of a bob (a picture with a mask) to the screen: header words dx, dy, width, height, then rows of 4 plane words and a mask word. */
typedef struct {
    const uint8_t *src;
    uint8_t *dst;
    const uint8_t *mask;
    int16_t shift;          /* x & 15 */
    int16_t width;
    int16_t right;          /* width + shift */
    int16_t xend;
    int16_t xstart;
    uint16_t size;          /* blitter size */
    int16_t stride;         /* bytes to the next plane word */
} Clip;

static uint16_t lsr_w(uint16_t v, int16_t count)
{
    count &= 63;
    return count >= 16 ? 0 : (uint16_t)(v >> count);
}

/* inlined: $107E, $1084, $1096, $109A, $10A8, $10B4, $10C4, $10C8, $10CE, $10D6, $10DA, $10E0, $10E8, $10F2
 * (the clip; returns 0 where the original returns from its caller) */
static int clip_bob(Clip *c, int16_t x, int16_t y, int with_map)
{
    const struct Pic *pic = (const struct Pic *)c->src;
    const uint8_t *p;
    int16_t shift = x, row = y, col, width, xend, xstart, room, lines, top;
    int32_t sum;

    shift = (int16_t)(shift - (int16_t)pic->hot_x);
    if (shift >= 0x140)
        return 0;
    row = (int16_t)(row - (int16_t)pic->hot_y);
    if (with_map)   /* $105C: the masks of the map under the bob first (inlined: $2F66) */
        map_tile_mask2((uint16_t)shift, (uint16_t)row, pic->height);
    room = (int16_t)(xenon2.clip_bottom - row);
    if (room < 0)
        return 0;
    top = row;
    if (top < 0)
        row = 0;
    xend = shift;
    c->dst = (uint8_t *)xenon2.draw_screen + (int16_t)((uint16_t)((uint16_t)row) * SCREEN_ROW_BYTES);
    col = shift;
    shift &= 0xf;
    col = (int16_t)(col - shift);
    col >>= 3;
    c->dst += col;
    width = (int16_t)pic->width;
    xstart = xend;
    sum = (int32_t)xend + width;
    xend = (int16_t)sum;
    if (sum <= 0)
        return 0;
    lines = (int16_t)pic->height;
    p = (const uint8_t *)(pic + 1);
    if (lines > room)
        lines = room;
    if (-(int32_t)top > 0) {
        top = (int16_t)-top;
        lines = (int16_t)(lines - top);
        if (lines < 0)
            return 0;
        top = (int16_t)(top + top);
        room = top;
        top = (int16_t)(top + top);
        top = (int16_t)(top + top);
        top = (int16_t)(top + room);
        p += top;
        if (width > 0x10)
            p += top;
    }
    c->src = p;
    c->shift = shift;
    c->width = width;
    c->right = (int16_t)(width + shift);
    c->xend = xend;
    c->xstart = xstart;
    c->size = (uint16_t)((uint16_t)(lines + 1) << 6);
    return 1;
}

/* inlined: $10F6, $1104, $1110, $1114, $1122, $1142, $1146, $114A, $114E, $117E, $1180, $118E, $119A, $11A8, $11AC, $11BA, $11D2, $11D8, $11F4,
 * $1210, $124C: the blitter's masks and modulos for the first and last word of the bob; HLE: blitter */
static void blit_edges(Clip *c)
{
    int16_t shift = c->shift, width = c->width, right = c->right, xend = c->xend, xstart = c->xstart;
    uint16_t fwm;

    if (width > 0x10)
        goto wide;
    c->mask = c->src + 8;
    c->stride = 2;
    if (xend <= 0x140 && right > 0x10) {
        fwm = lsr_w(0xffff, (int16_t)-xstart);
        if (-(int32_t)xstart <= 0)
            fwm = 0xffff;
        custom->bltafwm = fwm;
        custom->bltalwm = 0;
        custom->bltamod = 6;
        custom->bltbmod = 6;
        custom->bltcmod = 0x9c;
        custom->bltdmod = 0x9c;
        c->size += 2;
        return;
    }
    custom->bltalwm = (uint16_t)(0xffffu << (shift & 63));
    custom->bltafwm = 0xffff;
    custom->bltamod = 8;
    custom->bltbmod = 8;
    custom->bltcmod = SCREEN_ROW_BYTES - 2;
    custom->bltdmod = SCREEN_ROW_BYTES - 2;
    c->size += 1;
    return;

wide:
    c->stride = 4;
    c->mask = c->src + 0x10;
    if (xend > 0x140) {
        custom->bltalwm = (uint16_t)(0xffffu << (shift & 63));
        if (xstart >= 0x130) {
            custom->bltafwm = 0xffff;
            custom->bltamod = 0x12;
            custom->bltbmod = 0x12;
            custom->bltcmod = SCREEN_ROW_BYTES - 2;
            custom->bltdmod = SCREEN_ROW_BYTES - 2;
            c->size += 1;
            return;
        }
        goto first_mask;
    }
    if (xstart <= -16) {
        c->src += 2;
        c->dst += 2;
        c->mask += 2;
        xstart = (int16_t)(xstart + 0x10);
        custom->bltafwm = lsr_w(0xffff, (int16_t)-xstart);
        custom->bltalwm = 0;
        custom->bltamod = 0x10;
        custom->bltbmod = 0x10;
        custom->bltcmod = 0x9c;
        custom->bltdmod = 0x9c;
        c->size += 2;
        return;
    }
    if (right > 0x20) {
        fwm = 0xffff;
        if (-(int32_t)xstart > 0)
            fwm = lsr_w(0xffff, (int16_t)-xstart);
        custom->bltafwm = fwm;
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
    fwm = 0xffff;
    if (-(int32_t)xstart > 0)
        fwm = lsr_w(0xffff, (int16_t)-xstart);
    custom->bltafwm = fwm;
    custom->bltamod = 0x10;
    custom->bltbmod = 0x10;
    custom->bltcmod = 0x9c;
    custom->bltdmod = 0x9c;
    c->size += 2;
}

/* orig $E90, $129A: draw a bob (masked, cookie-cut) at x, y; gfx = its header; HLE: blitter */
HLE_WEAK void draw_bob_masked_at(uint16_t x, uint16_t y, const uint16_t *gfx)
{
    Clip c;
    uint16_t bltcon;
    const uint8_t *src;
    uint8_t *dst;

    c.src = (const uint8_t *)gfx;
    if (!clip_bob(&c, (int16_t)x, (int16_t)y, 0))
        return;
    blit_edges(&c);
    bltcon = (uint16_t)c.shift;
    bltcon = (uint16_t)((bltcon >> 4) | (bltcon << 12));
    custom->bltcon1 = bltcon;
    custom->bltcon0 = bltcon | 0x0fca;
    src = c.src;
    dst = c.dst;
    for (int plane = 0; plane < 4; plane++) {
        custom->bltapt = ADDR(c.mask);
        custom->bltbpt = ADDR(src);
        custom->bltcpt = ADDR(dst);
        custom->bltdpt = ADDR(dst);
        BLTSIZE_WRITE(c.size);
        src += c.stride;
        dst += SCREEN_PLANE_BYTES;
        BLIT_WAIT();
    }
}

/* orig $E12, $128E, $133E, $1350, $135C and $1366: draw the bob of an object, masked; HLE: blitter */
HLE_WEAK void draw_bob_masked(Object *obj)
{
    draw_bob_masked_at(obj->x, obj->y, (const uint16_t *)obj->gfx);
}

/* orig $F14, $137C, $1384, $139E, $13AA, $13B4, $13BE, $13D0, $13DA, $13EA, $1404, $1414, $142C and $1436: draw a bob by OR-ing its mask
 * into planes 0-2 and clearing it in plane 3 (a flash); HLE: blitter */
HLE_WEAK void draw_bob_or_at(uint16_t x, uint16_t y, const uint16_t *gfx)
{
    Clip c;
    uint16_t bltcon;
    uint8_t *dst;

    c.src = (const uint8_t *)gfx;
    if (!clip_bob(&c, (int16_t)x, (int16_t)y, 0))
        return;
    blit_edges(&c);
    bltcon = (uint16_t)c.shift;
    bltcon = (uint16_t)((bltcon >> 4) | (bltcon << 12));
    bltcon |= 0x0bfa;
    custom->bltcon0 = bltcon;
    custom->bltcon1 = 0;
    dst = c.dst;
    for (int plane = 0; plane < 4; plane++) {
        if (plane == 3)
            custom->bltcon0 = (uint16_t)((bltcon & 0xff00) | 0x0a);
        custom->bltapt = ADDR(c.mask);
        custom->bltcpt = ADDR(dst);
        custom->bltdpt = ADDR(dst);
        BLTSIZE_WRITE(c.size);
        dst += SCREEN_PLANE_BYTES;
        BLIT_WAIT();
    }
}

/* orig $F08, $1370 and $1378: draw the bob of an object as above */
HLE_WEAK void draw_bob_or(Object *obj)
{
    draw_bob_or_at(obj->x, obj->y, (const uint16_t *)obj->gfx);
}

/* orig $E66, $1368: the object is drawn masked from now on, this time as above */
HLE_WEAK void draw_bob_or_first(Object *obj)
{
    obj->fn_draw = (obj_fn)draw_bob_masked;
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
            const uint8_t *p = xenon2.text_chars;   /* the search of $7EF8 */
            uint16_t index = 0xffff;
            const uint16_t *glyph;

            while (*p) {
                if (*p++ == c) {
                    index = (uint16_t)(p - xenon2.text_chars - 1);
                    break;
                }
            }
            glyph = xenon2.font_gfx + (int16_t)(uint16_t)(index << 6);
            custom->bltcon0 = 0x09f0;
            custom->bltcon1 = 0;
            custom->bltamod = 0;
            custom->bltdmod = 0x26;
            custom->bltafwm = 0xffff;
            custom->bltalwm = 0xffff;
            custom->bltapt = ADDR(glyph);
            custom->bltdpt = ADDR(dest);
            BLTSIZE_WRITE(0x1001);
            BLIT_WAIT();
        }
        dest += 2;
    }
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

/* one tile of the background: a plain one is copied, one with bit 15 set is cookie-cut with its mask into the 4 planes */
static void put_tile(const uint8_t *tiles, uint16_t cell, uint8_t *dest, uint16_t rows_size, int partial)
{
    const uint8_t *tile;

    if (!(cell & 0x8000)) {
        tile = tiles + ((uint32_t)cell << 4);
        custom->bltcon0 = 0x09f0;
        custom->bltcon1 = 0;
        custom->bltamod = 0;
        custom->bltdmod = 0x26;
        custom->bltafwm = 0xffff;
        custom->bltalwm = 0xffff;
        custom->bltapt = ADDR(tile);
        custom->bltdpt = ADDR(dest);
        BLTSIZE_WRITE(partial ? (uint16_t)((uint16_t)(rows_size << 2) - 3) : 0x1001);
        BLIT_WAIT();
        return;
    }
    tile = tiles + ((uint32_t)(cell & 0x7fff) << 4);
    {
        const uint8_t *tile_b = tile + 2;

        custom->bltcon0 = 0x0fca;
        custom->bltcon1 = 0;
        custom->bltamod = 8;
        custom->bltbmod = 8;
        custom->bltcmod = SCREEN_ROW_BYTES - 2;
        custom->bltdmod = SCREEN_ROW_BYTES - 2;
        for (int plane = 0; plane < 4; plane++) {
            custom->bltapt = ADDR(tile);
            custom->bltbpt = ADDR(tile_b);
            custom->bltcpt = ADDR(dest);
            custom->bltdpt = ADDR(dest);
            BLTSIZE_WRITE(partial ? rows_size : 0x0401);
            dest += SCREEN_PLANE_BYTES;
            tile_b += 2;
            BLIT_WAIT();
        }
    }
}

/* orig $17D6: draw the background: the picture scrolled by the strip at $436, then the map's tiles over it; HLE: blitter */
HLE_WEAK void draw_background(void)
{
    uint16_t scroll = (uint16_t)(xenon2.background_scroll >> 16);
    uint16_t lines = (uint16_t)(0xc0 - scroll);
    uint8_t *src = xenon2.map_image + (int16_t)(uint16_t)(scroll * MAP_BLIT_BYTES);
    uint8_t *dst = xenon2.draw_screen;

    custom->bltapt = ADDR(src);
    custom->bltdpt = ADDR(dst);
    custom->bltamod = 0;
    custom->bltdmod = MAP_BLIT_BYTES;
    custom->bltafwm = 0xffff;
    custom->bltalwm = 0xffff;
    custom->bltcon0 = 0x09f0;
    custom->bltcon1 = 0;
    BLTSIZE_WRITE((uint16_t)((uint16_t)(lines << 6) + MAP_BLIT_WORDS));
    BLIT_WAIT();
    lines = scroll;
    if (lines) {
        custom->bltapt = ADDR(xenon2.map_image);
        BLTSIZE_WRITE((uint16_t)((uint16_t)(lines << 6) + MAP_BLIT_WORDS));
        BLIT_WAIT();
    }
    dst += MAP_BLIT_BYTES;
    custom->bltdpt = ADDR(dst);
    custom->bltcon0 = 0x01f0;
    custom->bltadat = 0;
    BLTSIZE_WRITE(0x3028);
    BLIT_WAIT();
    if (xenon2.skip_map_draw)
        return;

    draw_tile_grid((const uint16_t *)(xenon2.level_init_src + (int16_t)xenon2.map_row_offset), 0x13, 0xc, (int16_t)-(int16_t)xenon2.scroll_fine, 0);
}

/* one tile as $1BD0 puts it: a plain cell is the fixed block at $1EA2, a cell with bit 15 is OR-ed into planes 0 to 2 by its mask and
 * cleared in plane 3 (a flash) */
static void put_tile_flash(const uint8_t *tiles, uint16_t cell, uint8_t *dest, uint16_t rows_size, int partial)
{
    const uint8_t *tile;

    if (!(cell & 0x8000)) {
        custom->bltcon0 = 0x09f0;
        custom->bltcon1 = 0;
        custom->bltamod = 0;
        custom->bltdmod = 0x26;
        custom->bltafwm = 0xffff;
        custom->bltalwm = 0xffff;
        custom->bltapt = ADDR(xenon2.tile_mask_block);
        custom->bltdpt = ADDR(dest);
        BLTSIZE_WRITE(partial ? (uint16_t)((uint16_t)(rows_size << 2) - 3) : 0x1001);
        BLIT_WAIT();
        return;
    }
    tile = tiles + ((uint32_t)(cell & 0x7fff) << 4);
    custom->bltcon0 = 0x0bfa;
    custom->bltcon1 = 0;
    custom->bltamod = 8;
    custom->bltcmod = SCREEN_ROW_BYTES - 2;
    custom->bltdmod = SCREEN_ROW_BYTES - 2;
    for (int plane = 0; plane < 4; plane++) {
        if (plane == 3)
            custom->bltcon0 = 0x0b0a;
        custom->bltapt = ADDR(tile);
        custom->bltcpt = ADDR(dest);
        custom->bltdpt = ADDR(dest);
        BLTSIZE_WRITE(partial ? rows_size : 0x0401);
        dest += SCREEN_PLANE_BYTES;
        BLIT_WAIT();
    }
}

typedef void (*tile_fn)(const uint8_t *tiles, uint16_t cell, uint8_t *dest, uint16_t rows_size, int partial);

/* rows of cells (words: 0 nothing, bit 15 a tile with a mask, else a plain tile) put on the screen at line y and byte x / 4; cols + 1
 * cells a row, rows + 1 rows of 16 lines; from the row that reaches line $6EA0 / $A0 on, one blit a cell as far as the end of the screen */
static void tile_grid(const uint16_t *cells, uint16_t cols, int16_t rows, int16_t y, uint16_t x, tile_fn put)
{
    uint8_t *dst = xenon2.draw_screen;
    uint8_t *screen_end = dst + SCREEN_ROWS * SCREEN_ROW_BYTES, *split = SCREEN_POS(dst, 177, 0);
    const uint8_t *tiles = xenon2.tile_gfx;
    uint16_t xb = (uint16_t)(x >> 2);

    dst += (int16_t)xb;
    custom->bltafwm = 0xffff;
    custom->bltalwm = 0xffff;
    while (y <= -16) {
        cells += (int16_t)cols + 1;
        y = (int16_t)(y + 0x10);
        if (rows-- == 0)
            return;
    }
    if (y >= 0xc0)
        return;
    dst += (int16_t)(uint16_t)((int32_t)y * SCREEN_ROW_BYTES);
    for (;;) {
        if ((int32_t)ADDR(dst) >= (int32_t)ADDR(split)) {
            uint32_t span = ADDR(screen_end) - ADDR(dst);
            uint16_t size;
            uint8_t *col = dst;

            span = (span & 0xffff0000u) | (uint16_t)(span + xb);
            if (span / 0xa0 <= 0xffff)
                span = ((span % 0xa0) << 16) | (span / 0xa0);
            size = (uint16_t)((uint16_t)((uint16_t)span << 6) + 1);
            for (int n = 0; n <= (int)cols; n++, col += 2) {
                uint16_t cell = *cells++;

                if (cell == 0)
                    continue;
                put(tiles, cell, col, size, 1);
                if (!(cell & 0x8000))
                    size = (uint16_t)((uint16_t)(size << 2) >> 2);   /* lsl.w #2 .. lsr.w #2 */
            }
            return;
        }
        {
            uint8_t *col = dst;

            for (int n = 0; n <= (int)cols; n++, col += 2) {
                uint16_t cell = *cells++;

                if (cell != 0)
                    put(tiles, cell, col, 0, 0);
            }
        }
        dst += 16 * SCREEN_ROW_BYTES;
        if ((int32_t)ADDR(dst) >= (int32_t)ADDR(screen_end))
            return;
        if (rows-- == 0)
            return;
    }
}

/* orig $E8A, $18B6: draw a block of map cells (cols + 1 by rows + 1) at line y and byte x / 4 of the screen; HLE: blitter */
HLE_WEAK void draw_tile_grid(const uint16_t *cells, uint16_t cols, uint16_t rows, int16_t y, uint16_t x)
{
    tile_grid(cells, cols, (int16_t)rows, y, x, put_tile);
}

/* orig $E96, $1BD0: the same with the cells drawn as flashes; HLE: blitter */
HLE_WEAK void draw_tile_grid_flash(const uint16_t *cells, uint16_t cols, uint16_t rows, int16_t y, uint16_t x)
{
    tile_grid(cells, cols, (int16_t)rows, y, x, put_tile_flash);
}

static void bob_behind_map(Object *obj)
{
    Clip c;
    uint16_t bltcon, size;
    const uint8_t *src;
    uint8_t *dst;

    c.src = (const uint8_t *)obj->gfx;
    if (!clip_bob(&c, obj->x, obj->y, 1))
        return;
    src = c.src;
    dst = c.dst;
    size = c.size;
    custom->bltapt = ADDR(src + 8);
    custom->bltcpt = ADDR(xenon2.tile_mask_buf);
    custom->bltdpt = ADDR(xenon2.tile_mask_buf);
    custom->bltamod = 6;
    custom->bltcmod = 0;
    custom->bltdmod = 0;
    custom->bltafwm = 0xffff;
    custom->bltalwm = 0;
    bltcon = (uint16_t)c.shift;
    custom->bltcon0 = (uint16_t)(((bltcon >> 4) | (bltcon << 12)) | 0x0b50);
    custom->bltcon1 = 0;
    BLTSIZE_WRITE((uint16_t)(size + 2));
    BLIT_WAIT();
    custom->bltafwm = 0xffff;
    custom->bltalwm = 0xffff;
    if (c.xend > 0x140)
        custom->bltalwm = 0;
    custom->bltamod = 0;
    custom->bltbmod = 6;
    custom->bltcmod = 0x9c;
    custom->bltdmod = 0x9c;
    bltcon = (uint16_t)((bltcon >> 4) | (bltcon << 12));
    custom->bltcon1 = bltcon;
    custom->bltcon0 = 0x0fca;
    for (int plane = 0; plane < 4; plane++) {
        custom->bltapt = ADDR(xenon2.tile_mask_buf);
        custom->bltbpt = ADDR(src);
        custom->bltcpt = ADDR(dst);
        custom->bltdpt = ADDR(dst);
        BLTSIZE_WRITE((uint16_t)(size + 2));
        src += 2;
        dst += SCREEN_PLANE_BYTES;
        BLIT_WAIT();
    }
}

/* orig $EBA, $1438: draw the bob of an object masked, and hidden where the map's tiles under it are solid; HLE: blitter */
HLE_WEAK void draw_bob_behind_map(Object *obj)
{
    bob_behind_map(obj);
}
