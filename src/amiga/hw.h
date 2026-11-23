/* The Amiga custom chips, as the programs of Xenon 2 use them: the register block at $DFF000. */
#ifndef XENON_HW_H
#define XENON_HW_H

#include <stddef.h>
#include <stdint.h>

/* a channel of the audio hardware, as it sits in the custom chip registers */
struct AudioChannel {
    uint32_t lc;           /* sample address */
    uint16_t len, per, vol, dat, pad[2];
};

struct Custom {
    uint16_t bltddat, dmaconr, vposr, vhposr, dskdatr, joy0dat, joy1dat, clxdat;      /* 000 */
    uint16_t adkconr, pot0dat, pot1dat, potgor, serdatr, dskbytr, intenar, intreqr;   /* 010 */
    uint32_t dskpt;                                                                   /* 020 */
    uint16_t dsklen, dskdat, refptr, vposw, vhposw, copcon, serdat, serper, potgo, joytest;
    uint16_t pad038[4];                                                               /* 038 */
    uint16_t bltcon0, bltcon1, bltafwm, bltalwm;                                      /* 040 */
    uint32_t bltcpt, bltbpt, bltapt, bltdpt;                                          /* 048 */
    uint16_t bltsize, bltcon0l, bltsizv, bltsizh;                                     /* 058 */
    uint16_t bltcmod, bltbmod, bltamod, bltdmod;                                      /* 060 */
    uint16_t pad068[4];                                                               /* 068 */
    uint16_t bltcdat, bltbdat, bltadat;                                               /* 070 */
    uint16_t pad076[4], dsksync;                                                      /* 076 */
    uint32_t cop1lc, cop2lc;                                                          /* 080 */
    uint16_t copjmp1, copjmp2, copins, diwstrt, diwstop, ddfstrt, ddfstop;            /* 088 */
    uint16_t dmacon, clxcon, intena, intreq, adkcon;                                  /* 096 */
    struct AudioChannel aud[4];                                                  /* 0A0 */
    uint32_t bplpt[6];                                                                /* 0E0 */
    uint16_t pad0f8[4];
    uint16_t bplcon0, bplcon1, bplcon2, bplcon3, bpl1mod, bpl2mod, pad10c[2];         /* 100 */
    uint16_t bpldat[6];                                                               /* 110 */
    uint16_t pad11c[2];
    uint32_t sprpt[8];                                                                /* 120 */
    uint16_t spr[8][4];                                                               /* 140 */
    uint16_t color[32];                                                               /* 180 */
};
_Static_assert(offsetof(struct Custom, dmaconr) == 0x002, "custom layout");
_Static_assert(offsetof(struct Custom, intreqr) == 0x01e, "custom layout");
_Static_assert(offsetof(struct Custom, bltcon0) == 0x040, "custom layout");
_Static_assert(offsetof(struct Custom, bltsize) == 0x058, "custom layout");
_Static_assert(offsetof(struct Custom, bltdmod) == 0x066, "custom layout");
_Static_assert(offsetof(struct Custom, bltcdat) == 0x070, "custom layout");
_Static_assert(offsetof(struct Custom, dsksync) == 0x07e, "custom layout");
_Static_assert(offsetof(struct Custom, dmacon) == 0x096, "custom layout");
_Static_assert(offsetof(struct Custom, aud) == 0x0a0, "custom layout");
_Static_assert(offsetof(struct Custom, bplpt) == 0x0e0, "custom layout");
_Static_assert(offsetof(struct Custom, bplcon0) == 0x100, "custom layout");
_Static_assert(offsetof(struct Custom, bpldat) == 0x110, "custom layout");
_Static_assert(offsetof(struct Custom, sprpt) == 0x120, "custom layout");
_Static_assert(offsetof(struct Custom, spr) == 0x140, "custom layout");
_Static_assert(offsetof(struct Custom, color) == 0x180, "custom layout");

#ifdef GAME_SDL   /* the host builds: the chips' registers are objects (sdl/guest.c), not pages at the Amiga's addresses */
extern volatile uint8_t host_custom_raw[0x1000];
extern volatile uint8_t host_cia_raw[0x2000];   /* the CIAs: $BFD000 on */
#define custom ((volatile struct Custom *)host_custom_raw)
#else
#define custom ((volatile struct Custom *)0xdff000)
#endif

/* The registers with SET/CLR semantics (bit 15 selects set or clear of the other bits): dmacon, intena, intreq, adkcon.  Write them only
 * with SETCLR_WRITE(dmacon, 0x8201): on the Amiga it is the plain write, in the SDL port the host applies it (starts and stops the audio
 * channels, enables the interrupts).  Read them (dmaconr, intenar, intreqr) as plain members. */
#ifdef GAME_SDL
void host_setclr_write(unsigned register_offset, uint16_t value);
#define SETCLR_WRITE(reg, value) host_setclr_write(offsetof(struct Custom, reg), (uint16_t)(value))
#else
#define SETCLR_WRITE(reg, value) (custom->reg = (uint16_t)(value))
#endif

/* Starting the blitter: the write to BLTSIZE.  On the Amiga it is the plain write; in the SDL port the host performs the whole operation (the area blit with the
 * registers as they are: shifts, masks, minterm, modulos) at once, so the blitter is never busy.  Write BLTSIZE only with BLTSIZE_WRITE(value). */
#ifdef GAME_SDL
void host_blit_start(uint16_t bltsize);
#define BLTSIZE_WRITE(value) host_blit_start((uint16_t)(value))
#else
#define BLTSIZE_WRITE(value) (custom->bltsize = (uint16_t)(value))
#endif

/* A function that programs a chip (the blitter, the disk DMA, ...) and is replaced in the SDL port by a high-level one with the same
 * signature in native/src/sdl/ is declared `HLE_WEAK void blit_xxx(...)` (and its comment says HLE: blitter): the C stays faithful for
 * the Amiga build and the oracle, and the port strong definition wins at link time. */
#ifdef GAME_SDL
#define HLE_WEAK __attribute__((weak))
#else
#define HLE_WEAK
#endif

/* A loop that waits for something an interrupt handler (or a chip) changes, as `while (!vblank_seen) HOST_IDLE();`: on the Amiga the
 * interrupt comes by itself and HOST_IDLE() is nothing; in the SDL port it runs the interrupts that are due (the vertical blank every
 * 20 ms of emulated time, with the picture and the sound) and waits for the clock. */
#ifdef GAME_SDL
void host_idle(void);
#define HOST_IDLE() host_idle()
#else
#define HOST_IDLE() ((void)0)
#endif

#define DMAF_SETCLR 0x8000
#define DMAF_BLITHOG 0x0400     /* blitter priority over the CPU */
#define DMAF_BLTBUSY 0x4000     /* in dmaconr */



/* The CIAs: one register is 0x100 apart at the 68000's addresses ($BFE001 + 0x100 * register for CIA A, $BFD000 + ... for CIA B). */
#ifdef GAME_SDL
#define CIA_BYTE(base, reg) (host_cia_raw[(base) - 0xbfd000 + 0x100 * (reg)])
#else
#define CIA_BYTE(base, reg) (*(volatile uint8_t *)((base) + 0x100 * (reg)))
#endif
#define CIAA_PRA  CIA_BYTE(0xbfe001, 0)   /* bits 6 and 7: the fire buttons, 0 when pressed */
#define CIAA_SDR  CIA_BYTE(0xbfe001, 0xc) /* the serial data register: the keyboard's key code */
#define CIAA_ICR  CIA_BYTE(0xbfe001, 0xd)
#define CIAA_CRA  CIA_BYTE(0xbfe001, 0xe) /* bit 7: the mains frequency (50 Hz) */
#define CIAB_ICR  CIA_BYTE(0xbfd000, 0xd)
#define CIAB_PRB  CIA_BYTE(0xbfd100, 0)   /* the drive select / motor / side / step lines */

#endif
