#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "draw.h"
#include "host.h"
#include "presentation.h"
#include "stars.h"

enum { TOP = 0x3a - 0x2c, VBL_NS = 20000000 };

static uint16_t map[14][MAP_COLS], strip[SCREEN_ROWS][MAP_BLIT_WORDS], tiles[160];
static const struct {
    struct Pic pic;
    uint16_t words[5];
} bob = { {0, 0, 16, 0}, {0xffff, 0xffff, 0xffff, 0xffff, 0xc000} };
static uint32_t original_rgb[DISPLAY_W * DISPLAY_H], smooth_rgb[DISPLAY_W * DISPLAY_H];

static unsigned pixel(const uint16_t *picture, int x, int y)
{
    unsigned value = 0;
    for (int p = 0; p < 4; p++)
        value |= (picture[y * SCREEN_ROW_WORDS + p * SCREEN_PLANE_WORDS + x / 16] >> (15 - (x & 15)) & 1u) << p;
    return value;
}

static void show(uint16_t *screen)
{
    for (int p = 0; p < 4; p++)
        custom->bplpt[p] = (uintptr_t)(screen + p * SCREEN_PLANE_WORDS);
}

static void gameplay_frame(uint16_t *screen, unsigned tick, int step, int x, int reuse)
{
    Object *ship = &xenon2.obj_pool[0];
    Object *hidden = &xenon2.obj_pool[1];
    const uint16_t *pic = (const uint16_t *)&bob;

    xenon2.draw_screen = (uint8_t *)screen;
    xenon2.scroll_pos = 100 - step;
    xenon2.scroll_fine = 8 - step;
    xenon2.background_scroll = (uint32_t)((SCREEN_ROWS + 1 - step) % SCREEN_ROWS) << 16;
    xenon2.stars[0].x = (SCREEN_ROWS + 2 - step) % SCREEN_ROWS * SCREEN_ROW_BYTES + 30;
    xenon2.stars[0].y = 0x8000;
    presentation_begin();
    draw_background();
    stars_draw();
    if (reuse)
        presentation_reuse(ship);
    presentation_owner(ship);
    draw_bob_masked_at(x, 20, pic);
    presentation_owner(hidden);
    hidden->x = 16;
    hidden->y = 8 + step;
    hidden->gfx = (void *)pic;
    draw_bob_behind_map(hidden);
    presentation_owner(&xenon2.shots[0]);
    presentation_draw(PRESENT_SHOT, x, 30);
    draw_screen_word(screen + 30 * SCREEN_ROW_WORDS + x / 16, 0x8000u >> (x & 15), 0x8000u >> (x & 15));
    presentation_owner(NULL);
    presentation_pin(1);
    draw_bob_masked_at(80, 40, pic);
    presentation_pin(0);
    presentation_seal();
    show(screen);
    presentation_flip(screen, tick, (uint64_t)tick * VBL_NS);
}

int main(void)
{
    xenon2.map_image = (uint8_t *)strip;
    xenon2.level_init_src = (uint8_t *)map;
    xenon2.tile_gfx = (uint8_t *)tiles;
    xenon2.clip_bottom = SCREEN_ROWS - 1;
    map[1][1] = 0x8001;
    for (int y = 0; y < 16; y++) {
        tiles[8 + y * 5] = 0x8000;
        tiles[9 + y * 5] = 0x8000;
    }
    strip[49][0] = 0x8000;
    strip[16][1] = 0x8000;
    custom->bplcon0 = 0x4000;
    custom->ddfstrt = 0x38;
    custom->ddfstop = 0xd0;
    custom->diwstrt = 0x3a81;
    custom->diwstop = 0x02c1;
    custom->bpl1mod = custom->bpl2mod = 0x78;
    custom->cop1lc = (uintptr_t)xenon2.copper_list_a;
    xenon2.copper_list_a[0] = 0xffff;
    xenon2.copper_list_a[1] = 0xfffe;
    for (int i = 0; i < 16; i++)
        custom->color[i] = i;
    xenon2.panel_b[0] = 0x8000;

    presentation_enable(1);
    gameplay_frame(xenon2.screen_a, 5, 0, 11, 0);
    assert(!presentation_picture(5 * VBL_NS));
    gameplay_frame(xenon2.screen_b, 10, 4, 19, 0);
    const uint16_t *middle = presentation_picture(10 * VBL_NS + 50 * 1000 * 1000);
    assert(middle);
    assert(pixel(middle, 15, 20) == 15 && pixel(middle, 16, 20) == 15 && pixel(middle, 19, 20) == 0);
    assert(pixel(middle, 15, 30) == 1 && pixel(middle, 19, 30) == 0);
    assert(pixel(middle, 16, 10) == 1 && pixel(middle, 17, 10) == 15);
    assert(pixel(middle, 16, 26) == 0 && pixel(middle, 16, 25) == 1);
    assert(pixel(middle, 0, 50) == 1 && pixel(middle, 0, 52) == 0);
    assert(pixel(middle, 240, 0) == 4 && pixel(middle, 240, 190) == 0);
    assert(pixel(middle, 80, 40) == 15 && pixel(middle, 80, 38) == 0);
    display_render_interpolated(smooth_rgb, 10 * VBL_NS + 50 * 1000 * 1000);
    assert(smooth_rgb[(TOP + 20) * DISPLAY_W + 15] == 255);
    assert(smooth_rgb[(TOP + SCREEN_ROWS) * DISPLAY_W] == 17);

    uint64_t before_deadline = 15 * VBL_NS - 1;
    assert(memcmp(xenon2.screen_b, presentation_picture(before_deadline), sizeof xenon2.screen_b) == 0);
    display_render(original_rgb);
    display_render_interpolated(smooth_rgb, before_deadline);
    assert(memcmp(original_rgb, smooth_rgb, sizeof original_rgb) == 0);
    assert(!presentation_picture(15 * VBL_NS));

    gameplay_frame(xenon2.screen_a, 15, 4, 100, 1);
    middle = presentation_picture(15 * VBL_NS);
    assert(middle && pixel(middle, 100, 20) == 15 && pixel(middle, 19, 20) == 0);
    presentation_flip(xenon2.screen_b, 20, 20 * VBL_NS);
    assert(!presentation_picture(20 * VBL_NS));
    gameplay_frame(xenon2.screen_a, 25, 4, 100, 0);
    xenon2.draw_screen = (uint8_t *)xenon2.screen_b;
    presentation_begin();
    draw_background();
    xenon2.scroll_pos -= 16;
    presentation_seal();
    presentation_flip(xenon2.screen_b, 30, 30 * VBL_NS);
    assert(!presentation_picture(30 * VBL_NS));
    puts("Interpolated draws, scroll layers, occlusion, HUD and original raster passed.");
    return 0;
}
