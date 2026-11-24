#include "stars.h"
#include "game_all.h"
#include "amiga/hw.h"
#ifdef GAME_SDL
#include "sdl/presentation.h"
#endif

/* The starfield of the game keeps in each record of `stars` (struct Star of title): x = offset in the screen, y = bit mask, z = fractional line. */
#define STAR_OFF(s) ((s)->x)
#define STAR_MASK(s) ((s)->y)
#define STAR_POS(s) ((s)->z)

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
        uint16_t *dst;
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
        pixel = px & 0xf;
        dst = (uint16_t *)(xenon2.draw_screen + (int16_t)(uint16_t)(py * 0xa0u) + (int16_t)(uint16_t)((px - pixel) >> 3));
        mask = (uint16_t)(0x8000 >> pixel);
        if ((dst[0] | dst[SCREEN_PLANE_WORDS] | dst[2 * SCREEN_PLANE_WORDS] | dst[3 * SCREEN_PLANE_WORDS]) & mask) {
            s->z |= (int16_t)0x8000;
            continue;
        }
        color = xenon2.star_colors[(((uint16_t)z << 6) | ((uint16_t)z >> 10)) & 7];
        if (color & 1) dst[0] = dst[0] | mask;
        if (color & 2) dst[SCREEN_PLANE_WORDS] = dst[SCREEN_PLANE_WORDS] | mask;
        if (color & 4) dst[2 * SCREEN_PLANE_WORDS] = dst[2 * SCREEN_PLANE_WORDS] | mask;
        if (color & 8) dst[3 * SCREEN_PLANE_WORDS] = dst[3 * SCREEN_PLANE_WORDS] | mask;
    }
}

/* orig $00861C: erases the visible stars from the screen (clears their pixel in the 4 planes) */
void stars_erase(void)
{
    int i;

    for (i = 0; i < 48; i++) {
        struct Star *s = &xenon2.stars[i];
        uint16_t px, py, byte_off;
        uint16_t *dst;
        int pixel;
        uint16_t keep;

        if (s->z < 0)
            continue;
        px = (uint16_t)(divs_low((int32_t)((uint32_t)(int32_t)s->x << 4), s->z) + 0xa0);
        py = (uint16_t)(divs_low((int32_t)((uint32_t)(int32_t)s->y << 4), s->z) + 0x64);
        pixel = px & 0xf;
        byte_off = (uint16_t)((px - pixel) >> 3);
        dst = (uint16_t *)(xenon2.draw_screen + (int16_t)(uint16_t)(py * 0xa0u) + (int16_t)byte_off);
        keep = (uint16_t)((0x7fff >> pixel) | (0x7fff << (16 - pixel)));
        dst[0] = dst[0] & keep;
        dst[SCREEN_PLANE_WORDS] = dst[SCREEN_PLANE_WORDS] & keep;
        dst[2 * SCREEN_PLANE_WORDS] = dst[2 * SCREEN_PLANE_WORDS] & keep;
        dst[3 * SCREEN_PLANE_WORDS] = dst[3 * SCREEN_PLANE_WORDS] & keep;
    }
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

/* orig $EDE, $0022DA: scroll the starfield: each of the 48 stars moves by a step that grows with its index (speed = scroll speed);
 * the fractional line (z) wraps at 0x180 and moves the offset by one line (0xA0 bytes), the offset wraps at 0x7800. */
void stars_scroll(uint16_t speed)
{
    uint16_t vel = *(const uint16_t *)((const uint8_t *)(xenon2.star_speed + 16) + (int16_t)(uint16_t)(speed * 2));
    uint16_t step = (uint16_t)(speed << 3);

    for (int i = 0; i < 48; i++) {
        struct Star *s = &xenon2.stars[i];
        uint16_t frac = STAR_POS(s) + vel;

        while ((int16_t)frac < 0) {
            STAR_OFF(s) -= SCREEN_ROW_BYTES;
            if ((int16_t)STAR_OFF(s) < 0)
                STAR_OFF(s) += SCREEN_ROWS * SCREEN_ROW_BYTES;
            frac += 0x180;
        }
        while ((int16_t)frac >= 0x180) {
            STAR_OFF(s) += SCREEN_ROW_BYTES;
            if ((int16_t)STAR_OFF(s) >= SCREEN_ROWS * SCREEN_ROW_BYTES)
                STAR_OFF(s) -= SCREEN_ROWS * SCREEN_ROW_BYTES;
            frac -= 0x180;
        }
        STAR_POS(s) = frac;
        vel += step;
    }
}

/* orig $00233A: draw the starfield into the draw screen: a star is drawn only where its four planes are empty under the mask;
 * four groups of 12 stars in four colours (planes 2; 0+2; 1+2; 0+1+2). */
void stars_draw(void)
{
    uint8_t *screen = xenon2.draw_screen;

    stars_scroll(xenon2.scroll_speed);
    for (int g = 0; g < 4; g++) {
        for (int i = 0; i < 12; i++) {
            struct Star *s = &xenon2.stars[g * 12 + i];
            uint16_t *p = (uint16_t *)(screen + (int16_t)STAR_OFF(s));
            uint16_t mask = STAR_MASK(s);
            uint16_t planes = p[0] | p[SCREEN_PLANE_WORDS] | p[2 * SCREEN_PLANE_WORDS] | p[3 * SCREEN_PLANE_WORDS];

#ifdef GAME_SDL
            if (presentation_capturing) {
                int offset = STAR_OFF(s);
                presentation_star(g * 12 + i, (offset % SCREEN_ROW_BYTES) * 8, offset / SCREEN_ROW_BYTES, mask);
            }
#endif
            if (planes & mask)
                continue;
            if (g == 1 || g == 3)
                p[0] = p[0] | mask;
            if (g == 2 || g == 3)
                p[SCREEN_PLANE_WORDS] = p[SCREEN_PLANE_WORDS] | mask;
            p[2 * SCREEN_PLANE_WORDS] = p[2 * SCREEN_PLANE_WORDS] | mask;
        }
    }
}
