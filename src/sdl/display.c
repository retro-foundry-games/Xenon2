/* The picture, from what the game leaves: the registers of the custom chips (set by the CPU) and the copper list that `cop1lc` points at (a list of
 * MOVE and WAIT instructions that, in this game, mostly sets the bitplane pointers).  No beam is emulated: the list is read as data once a frame,
 * and the bitplanes it names are decoded line by line with the registers as the list leaves them at that line.  The blitter is not emulated either:
 * the game's drawing routines that use it are replaced (HLE_WEAK in the game's C) and draw straight into the bitplanes. */
#include <stdint.h>
#include <string.h>

#include "amiga/hw.h"
#include "host.h"

#define CANVAS_V0 0x2c   /* the first line of the 256-line PAL window */
#define CANVAS_H0 0x81   /* the first lowres pixel of the 320-pixel window */

struct state {
    uint32_t pt[6];        /* where the next line of each bitplane is */
    uint16_t pt_hi[6];
    uint16_t bplcon0, bplcon1, bplcon2, mod1, mod2;
    uint16_t color[32];
    uint16_t ddfstrt, ddfstop, diwstrt, diwstop;
};

static uint32_t rgb_of(uint16_t c)
{
    return (uint32_t)((c >> 8) & 15) * 0x110000u + (uint32_t)((c >> 4) & 15) * 0x1100u + (uint32_t)(c & 15) * 0x11u;
}

static const uint8_t *plane_ptr(uint32_t address)
{
    return (const uint8_t *)guest_ptr(address);
}

static void draw_line(uint32_t *row, struct state *s, int vstart_is_first)
{
    int planes = (s->bplcon0 >> 12) & 7;
    int hires = (s->bplcon0 & 0x8000) != 0;
    int words = ((s->ddfstop - s->ddfstrt) >> (hires ? 2 : 3)) + (hires ? 2 : 1);
    int hstart = s->diwstrt & 0xff, hstop = (s->diwstop & 0xff) | 0x100;
    int first_x = s->ddfstrt * 2 + 17;   /* the hpos of the first pixel fetched (lowres pixels) */
    int delay1 = s->bplcon1 & 15, delay2 = (s->bplcon1 >> 4) & 15;
    int width = words * 16;
    int bytes = words * 2;
    uint32_t palette[32];

    (void)vstart_is_first;
    for (int i = 0; i < 32; i++)
        palette[i] = rgb_of(s->color[i]);
    if (planes > 6)
        planes = 6;
    for (int x = 0; x < width; x++) {
        int sx = hires ? x / 2 : x;
        int hpos = first_x + sx;
        int canvas_x = hpos - CANVAS_H0;
        unsigned index = 0;

        if (hpos < hstart || hpos >= hstop || canvas_x < 0 || canvas_x >= DISPLAY_W)
            continue;
        for (int p = 0; p < planes; p++) {
            int delay = (p & 1) ? delay2 : delay1;
            int bx = x - delay;
            const uint8_t *base = plane_ptr(s->pt[p]);

            if (bx < 0 || bx >= width || !base)
                continue;
            if ((base[bx >> 3] >> (7 - (bx & 7))) & 1)
                index |= 1u << p;
        }
        row[canvas_x] = palette[index & 31];
    }
    for (int p = 0; p < planes; p++)
        s->pt[p] += (uint32_t)bytes + (uint32_t)(int32_t)(int16_t)((p & 1) ? s->mod2 : s->mod1);
}

void display_render(uint32_t *rgb)
{
    volatile struct Custom *c = custom;
    struct state s;
    int vstart, vstop, line;
    const uint16_t *cop;

    memset(&s, 0, sizeof s);
    for (int p = 0; p < 6; p++)
        s.pt[p] = c->bplpt[p];
    s.bplcon0 = c->bplcon0;
    s.bplcon1 = c->bplcon1;
    s.bplcon2 = c->bplcon2;
    s.mod1 = c->bpl1mod;
    s.mod2 = c->bpl2mod;
    for (int i = 0; i < 32; i++)
        s.color[i] = c->color[i];
    s.ddfstrt = c->ddfstrt;
    s.ddfstop = c->ddfstop;
    s.diwstrt = c->diwstrt;
    s.diwstop = c->diwstop;
    for (int i = 0; i < DISPLAY_W * DISPLAY_H; i++)
        rgb[i] = rgb_of(s.color[0]);

    vstart = s.diwstrt >> 8;
    vstop = (s.diwstop >> 8) | ((s.diwstop & 0x8000) ? 0 : 0x100);
    line = vstart;

    /* the copper: apply the MOVEs, draw up to each WAIT's line */
    cop = (const uint16_t *)guest_ptr(c->cop1lc);
    for (int guard = 0; cop && guard < 4096; guard++, cop += 2) {
        uint16_t w1 = cop[0], w2 = cop[1];

        if (w1 & 1) {                        /* WAIT / SKIP / END */
            int v = w1 >> 8;

            if (w1 == 0xffff && w2 == 0xfffe)
                break;
            while (line < v && line < vstop) {
                if (line >= vstart && line - CANVAS_V0 >= 0 && line - CANVAS_V0 < DISPLAY_H)
                    draw_line(rgb + (line - CANVAS_V0) * DISPLAY_W, &s, 0);
                line++;
            }
            continue;
        }
        unsigned reg = w1 & 0x1fe;

        if (reg >= 0xe0 && reg < 0xf8) {      /* BPLxPTH / BPLxPTL */
            int p = (reg - 0xe0) / 4;

            if ((reg & 2) == 0)
                s.pt_hi[p] = w2;
            else
                s.pt[p] = ((uint32_t)s.pt_hi[p] << 16) | w2;
        } else if (reg >= 0x180 && reg < 0x1c0) {
            s.color[(reg - 0x180) / 2] = w2;
        } else if (reg == 0x100) {
            s.bplcon0 = w2;
        } else if (reg == 0x102) {
            s.bplcon1 = w2;
        } else if (reg == 0x104) {
            s.bplcon2 = w2;
        } else if (reg == 0x108) {
            s.mod1 = w2;
        } else if (reg == 0x10a) {
            s.mod2 = w2;
        } else if (reg == 0x92) {
            s.ddfstrt = w2;
        } else if (reg == 0x94) {
            s.ddfstop = w2;
        }
    }
    while (line < vstop) {
        if (line >= vstart && line - CANVAS_V0 >= 0 && line - CANVAS_V0 < DISPLAY_H)
            draw_line(rgb + (line - CANVAS_V0) * DISPLAY_W, &s, 0);
        line++;
    }
}

/* a text report of what the picture is made from (for the --status option): the registers, the copper list, and how much of each bitplane is not zero */
#include <stdio.h>
void display_status(void)
{
    volatile struct Custom *c = custom;
    const uint16_t *cop = (const uint16_t *)guest_ptr(c->cop1lc);

    printf("bplcon0 %04x diw %04x-%04x ddf %04x-%04x mod %d/%d cop1lc %08x\n", c->bplcon0, c->diwstrt, c->diwstop, c->ddfstrt, c->ddfstop,
           (int)(int16_t)c->bpl1mod, (int)(int16_t)c->bpl2mod, c->cop1lc);
    {
        extern unsigned host_blit_count;

        printf("blits so far: %u\n", host_blit_count);
    }
    for (int b = 0; b < 2; b++) {
        const uint8_t *p = (const uint8_t *)guest_ptr(b ? 0x4c700 : 0x44000);
        int nz = 0;

        for (int i = 0; p && i < 34560; i++)
            nz += p[i] != 0;
        printf("screen at %05x: %d of 34560 bytes not zero\n", b ? 0x4c700 : 0x44000, nz);
    }
    printf("colors:");
    for (int i = 0; i < 16; i++)
        printf(" %03x", c->color[i]);
    printf("\ncopper:");
    for (int i = 0; cop && i < 24; i++)
        printf(" %04x", cop[i]);
    printf("\n");
    if (cop) {
        uint32_t pt[4];

        for (int i = 0; i < 4; i++)
            pt[i] = ((uint32_t)cop[i * 4 + 1] << 16) | cop[i * 4 + 3];
        for (int i = 0; i < 4; i++) {
            const uint8_t *p = (const uint8_t *)guest_ptr(pt[i]);
            int nz = 0;

            for (int b = 0; p && b < 40 * 200; b++)
                nz += p[b] != 0;
            printf("plane %d at %08x: %d of 8000 bytes not zero\n", i, pt[i], nz);
        }
    }
}
