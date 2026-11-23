/* The game's drawing routines that program the blitter on the Amiga (amiga/draw.c), done here on the pictures themselves: the planes of the screen are
 * words of 16 pixels (plane p of line y at y * SCREEN_ROW_BYTES + p * SCREEN_PLANE_BYTES), a bob is a picture with a mask (struct Pic, then rows of
 * four plane words and a mask word, a second set of words for a bob wider than 16), a tile is 16 pixels by 16 lines.  What a blit would write outside the
 * screen is not written. */
#include <stdint.h>
#include <string.h>

#include "game.h"
#include "draw.h"
#include "game_all.h"

static uint16_t *screen_word(int line, int plane, int x)
{
    return (uint16_t *)((uint8_t *)xenon2.draw_screen + line * SCREEN_ROW_BYTES + plane * SCREEN_PLANE_BYTES + (x >> 4) * 2);
}

/* the pixel at screen column x of a line takes the bits of the four planes (bit 0 of planes[]: plane 0), or, for a flash, is set in planes 0 to 2 and cleared in plane 3 */
static void put_pixel(int line, int x, const uint16_t *planes, int bit, int flash)
{
    uint16_t m = (uint16_t)(0x8000u >> (x & 15));

    for (int plane = 0; plane < 4; plane++) {
        uint16_t *w = screen_word(line, plane, x);
        int set = flash ? plane < 3 : (planes[plane] >> bit) & 1;

        *w = set ? (uint16_t)(*w | m) : (uint16_t)(*w & ~m);
    }
}

/* the part of a bob that is on the screen: its rows first..last (of the picture) at line top + row, the left column of the picture; 0 when nothing of it is */
struct Bob {
    const struct Pic *pic;
    int left, top, first, last, row_bytes;
};

static int bob_clip(struct Bob *b, const uint16_t *gfx, int16_t x, int16_t y, int with_map)
{
    int16_t left, top, room, lines;

    b->pic = (const struct Pic *)gfx;
    left = (int16_t)(x - (int16_t)b->pic->hot_x);
    if (left >= 0x140)
        return 0;
    top = (int16_t)(y - (int16_t)b->pic->hot_y);
    if (with_map)
        map_tile_mask2((uint16_t)left, (uint16_t)top, b->pic->height);
    room = (int16_t)(xenon2.clip_bottom - top);
    if (room < 0 || left + (int16_t)b->pic->width <= 0)
        return 0;
    lines = (int16_t)b->pic->height;
    if (lines > room)
        lines = room;
    b->first = top < 0 ? -top : 0;
    b->last = lines;
    if (b->first > b->last)
        return 0;
    b->left = left;
    b->top = top;
    b->row_bytes = b->pic->width > 16 ? 20 : 10;
    return 1;
}

static const uint8_t *bob_row(const struct Bob *b, int row)
{
    return (const uint8_t *)(b->pic + 1) + row * b->row_bytes;
}

/* the words of a bob's row: the planes and the mask of word w (a bob wider than 16 has two words, the planes four bytes apart) */
static uint16_t bob_plane(const struct Bob *b, const uint8_t *row, int plane, int w)
{
    return *(const uint16_t *)(row + (b->row_bytes == 20 ? plane * 4 + w * 2 : plane * 2));
}

static uint16_t bob_mask(const struct Bob *b, const uint8_t *row, int w)
{
    return *(const uint16_t *)(row + (b->row_bytes == 20 ? 16 + w * 2 : 8));
}

static void draw_bob(const uint16_t *gfx, int16_t x, int16_t y, int flash)
{
    struct Bob b;

    if (!bob_clip(&b, gfx, x, y, 0))
        return;
    for (int r = b.first; r <= b.last; r++) {
        const uint8_t *row = bob_row(&b, r);

        for (int w = 0; w < (b.row_bytes == 20 ? 2 : 1); w++) {
            uint16_t mask = bob_mask(&b, row, w), planes[4];

            for (int p = 0; p < 4; p++)
                planes[p] = bob_plane(&b, row, p, w);
            for (int i = 0; i < 16; i++) {
                int px = b.left + w * 16 + i;

                if ((mask & (0x8000 >> i)) && px >= 0 && px < 320)
                    put_pixel(b.top + r, px, planes, 15 - i, flash);
            }
        }
    }
}

/* orig $E90, $129A: draw a bob (masked, cookie-cut) at x, y; gfx = its header */
void draw_bob_masked_at(uint16_t x, uint16_t y, const uint16_t *gfx)
{
    draw_bob(gfx, (int16_t)x, (int16_t)y, 0);
}

/* orig $F14, $137C, $1384, $139E, $13AA, $13B4, $13BE, $13D0, $13DA, $13EA, $1404, $1414, $142C and $1436: draw a bob as a flash: its mask is set in
 * planes 0-2 and cleared in plane 3 */
void draw_bob_or_at(uint16_t x, uint16_t y, const uint16_t *gfx)
{
    draw_bob(gfx, (int16_t)x, (int16_t)y, 1);
}

/* orig $EBA, $1438: draw the bob of an object masked, and hidden where the map's tiles under it are solid (a bob of up to 16 pixels: tile_mask_buf holds
 * what is visible of each row, in two words from the column of the cell the bob starts in) */
void draw_bob_behind_map(Object *obj)
{
    struct Bob b;
    int shift;

    if (!bob_clip(&b, (const uint16_t *)obj->gfx, obj->x, obj->y, 1))
        return;
    shift = b.left & 15;
    for (int r = b.first; r <= b.last; r++) {
        const uint8_t *row = bob_row(&b, r);
        uint16_t *seen = &xenon2.tile_mask_buf[2 * (r - b.first)];
        uint16_t mask = bob_mask(&b, row, 0), planes[4];

        seen[0] = (uint16_t)((mask >> shift) & ~seen[0]);
        seen[1] = (uint16_t)(((uint32_t)mask << (16 - shift) & 0xffff) & ~seen[1]);
        for (int p = 0; p < 4; p++)
            planes[p] = bob_plane(&b, row, p, 0);
        for (int i = 0; i < 16; i++) {
            int j = shift + i, px = b.left + i;

            if (px < 0 || px >= 320 || !(seen[j >> 4] & (0x8000 >> (j & 15))))
                continue;
            put_pixel(b.top + r, px, planes, 15 - i, 0);
        }
    }
}

/* the glyph or tile of 16 lines of four planes (64 words) put in the 16 pixels at byte column col of the screen, as far as `lines` lines */
static void put_block(const uint16_t *src, int col, int line, int lines)
{
    for (int r = 0; r < lines; r++)
        for (int p = 0; p < 4; p++) {
            if (line + r >= 0)
                *(uint16_t *)((uint8_t *)xenon2.draw_screen + (line + r) * SCREEN_ROW_BYTES + p * SCREEN_PLANE_BYTES + col) = *src;
            src++;
        }
}

/* orig $15CE: draw a zero-terminated string with the font into the screen at dest (2 bytes a character) */
void draw_text(const uint8_t *text, uint8_t *dest)
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
            for (int i = 0; i < 64; i++)
                *(uint16_t *)(dest + i * 40) = glyph[i];
        }
        dest += 2;
    }
}

/* orig $17D6: draw the background: the picture scrolled by the strip at $436 in planes 0 and 1 (the rest of the planes cleared), then the map's tiles over it */
void draw_background(void)
{
    uint16_t scroll = (uint16_t)(xenon2.background_scroll >> 16);
    uint8_t *screen = xenon2.draw_screen;

    for (int line = 0; line < SCREEN_ROWS; line++) {
        const uint8_t *src = xenon2.map_image + ((line + scroll) % SCREEN_ROWS) * MAP_BLIT_BYTES;

        memcpy(screen + line * SCREEN_ROW_BYTES, src, MAP_BLIT_BYTES);
        memset(screen + line * SCREEN_ROW_BYTES + MAP_BLIT_BYTES, 0, MAP_BLIT_BYTES);
    }
    if (xenon2.skip_map_draw)
        return;
    draw_tile_grid((const uint16_t *)(xenon2.level_init_src + (int16_t)xenon2.map_row_offset), 0x13, 0xc, (int16_t)-(int16_t)xenon2.scroll_fine, 0);
}

/* a tile of the map at byte column col and line y: a plain cell is the 16 by 16 pixels as they are; a cell with bit 15 has a mask word before each line's plane words, and
 * is cookie-cut with it (a flash: set in planes 0-2 and cleared in plane 3 where its mask is set; a plain cell of a flash is the fixed block at $1EA2) */
static void put_tile(const uint8_t *tiles, uint16_t cell, int col, int line, int lines, int flash)
{
    const uint8_t *tile;

    if (!(cell & 0x8000)) {
        put_block(flash ? (const uint16_t *)xenon2.tile_mask_block : (const uint16_t *)(tiles + ((uint32_t)cell << 4)), col, line, lines);
        return;
    }
    tile = tiles + ((uint32_t)(cell & 0x7fff) << 4);
    for (int r = 0; r < lines; r++) {
        const uint16_t *row = (const uint16_t *)(tile + r * 10);

        if (line + r < 0)
            continue;
        for (int p = 0; p < 4; p++) {
            uint16_t *w = (uint16_t *)((uint8_t *)xenon2.draw_screen + (line + r) * SCREEN_ROW_BYTES + p * SCREEN_PLANE_BYTES + col);

            if (flash)
                *w = p < 3 ? (uint16_t)(*w | row[0]) : (uint16_t)(*w & ~row[0]);
            else
                *w = (uint16_t)((row[1 + p] & row[0]) | (*w & ~row[0]));
        }
    }
}

/* rows of cells (words: 0 nothing, bit 15 a tile with a mask, else a plain tile) put on the screen at line y and byte x / 4; cols + 1 cells a row, rows + 1 rows of
 * 16 lines; what is below the last line of the screen is not drawn */
static void tile_grid(const uint16_t *cells, uint16_t cols, int16_t rows, int16_t y, uint16_t x, int flash)
{
    const uint8_t *tiles = xenon2.tile_gfx;
    int col0 = (uint16_t)(x >> 2);

    while (y <= -16) {
        cells += (int16_t)cols + 1;
        y = (int16_t)(y + 0x10);
        if (rows-- == 0)
            return;
    }
    if (y >= SCREEN_ROWS)
        return;
    for (;;) {
        int lines = y + 16 > SCREEN_ROWS ? SCREEN_ROWS - y : 16;

        for (int n = 0; n <= (int)cols; n++) {
            uint16_t cell = *cells++;

            if (cell != 0)
                put_tile(tiles, cell, col0 + 2 * n, y, lines, flash);
        }
        y = (int16_t)(y + 16);
        if (y >= SCREEN_ROWS || rows-- == 0)
            return;
    }
}

/* orig $E8A, $18B6: draw a block of map cells (cols + 1 by rows + 1) at line y and byte x / 4 of the screen */
void draw_tile_grid(const uint16_t *cells, uint16_t cols, uint16_t rows, int16_t y, uint16_t x)
{
    tile_grid(cells, cols, (int16_t)rows, y, x, 0);
}

/* orig $E96, $1BD0: the same with the cells drawn as flashes */
void draw_tile_grid_flash(const uint16_t *cells, uint16_t cols, uint16_t rows, int16_t y, uint16_t x)
{
    tile_grid(cells, cols, (int16_t)rows, y, x, 1);
}
