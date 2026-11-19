/* The blitter: the operation the game's registers describe, done at once when BLTSIZE is written (hw.h BLTSIZE_WRITE).  Area mode only: the four channels
 * A, B, C, D, the barrel shifters, the first and last word masks, the minterm and the modulos, ascending.  No timing, no line mode, no fill mode (a game
 * that uses them says so here and the operation is skipped). */
#include <stdint.h>
#include <stdio.h>

#include "amiga/hw.h"
#include "host.h"

static uint16_t get_word(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static void put_word(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }

static uint16_t minterm(uint8_t lf, uint16_t a, uint16_t b, uint16_t c)
{
    uint16_t d = 0;

    if (lf & 0x80) d |= a & b & c;
    if (lf & 0x40) d |= a & b & (uint16_t)~c;
    if (lf & 0x20) d |= a & (uint16_t)~b & c;
    if (lf & 0x10) d |= a & (uint16_t)~b & (uint16_t)~c;
    if (lf & 0x08) d |= (uint16_t)~a & b & c;
    if (lf & 0x04) d |= (uint16_t)~a & b & (uint16_t)~c;
    if (lf & 0x02) d |= (uint16_t)~a & (uint16_t)~b & c;
    if (lf & 0x01) d |= (uint16_t)~a & (uint16_t)~b & (uint16_t)~c;
    return d;
}

void host_blit_start(uint16_t bltsize)
{
    volatile struct Custom *c = custom;
    uint16_t con0 = c->bltcon0, con1 = c->bltcon1;
    int use_a = (con0 >> 11) & 1, use_b = (con0 >> 10) & 1, use_c = (con0 >> 9) & 1, use_d = (con0 >> 8) & 1;
    int ash = con0 >> 12, bsh = con1 >> 12;
    int w = bltsize & 63, h = bltsize >> 6;
    uint8_t *ap = (uint8_t *)guest_ptr(c->bltapt), *bp = (uint8_t *)guest_ptr(c->bltbpt);
    uint8_t *cp = (uint8_t *)guest_ptr(c->bltcpt), *dp = (uint8_t *)guest_ptr(c->bltdpt);
    int16_t amod = (int16_t)c->bltamod, bmod = (int16_t)c->bltbmod, cmod = (int16_t)c->bltcmod, dmod = (int16_t)c->bltdmod;

    if (w == 0) w = 64;
    if (h == 0) h = 1024;
    if (con1 & 0x1b) {   /* line, descending, fill modes */
        static int said;

        if (!said++)
            fprintf(stderr, "xenon2-sdl: a blit in a mode this port does not do (bltcon1 = %04x)\n", con1);
        return;
    }
    for (int y = 0; y < h; y++) {
        uint16_t aold = 0, bold = 0;

        for (int x = 0; x < w; x++) {
            uint16_t a = c->bltadat, b = c->bltbdat, cc = c->bltcdat, d;
            uint32_t sa, sb;

            if (use_a) { a = ap ? get_word(ap) : 0; ap += 2; }
            if (use_b) { b = bp ? get_word(bp) : 0; bp += 2; }
            if (use_c) { cc = cp ? get_word(cp) : 0; cp += 2; }
            if (x == 0) a &= c->bltafwm;
            if (x == w - 1) a &= c->bltalwm;
            sa = ((uint32_t)aold << 16 | a) >> ash;
            sb = ((uint32_t)bold << 16 | b) >> bsh;
            aold = a;
            bold = b;
            d = minterm((uint8_t)con0, (uint16_t)sa, (uint16_t)sb, cc);
            if (use_d) { if (dp) put_word(dp, d); dp += 2; }
        }
        if (use_a) ap += amod;
        if (use_b) bp += bmod;
        if (use_c) cp += cmod;
        if (use_d) dp += dmod;
    }
    /* the pointers end where the hardware leaves them */
    if (use_a) c->bltapt = (uint32_t)(uintptr_t)ap;
    if (use_b) c->bltbpt = (uint32_t)(uintptr_t)bp;
    if (use_c) c->bltcpt = (uint32_t)(uintptr_t)cp;
    if (use_d) c->bltdpt = (uint32_t)(uintptr_t)dp;
}
