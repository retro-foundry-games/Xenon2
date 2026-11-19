#ifndef PKG_I_H
#define PKG_I_H
/* The shop: the program the game loads from disk 2 (248 sectors from sector 1476) over the level's blob at $54E00 and calls (pkg_g shop_run).
 * Its code, its tables and its pictures are that blob; the C reads and writes it as the raw big-endian memory it is (GPTR, rd16/wr16: no member of
 * struct Xenon describes it, the level's members that lie there are left alone while it is loaded: guest_blob_raw).  A function has `shop $ADDR` for the
 * address it has in the original (not `orig $`: the level's code is at the same addresses, the code table is the level's). */
#include "game.h"
#include "pkg_a.h"

#ifdef GAME_SDL
void guest_blob_raw(int on);
uint32_t guest_address(const void *p);
#define GADDR(p) guest_address(p)
#else
#define guest_blob_raw(on) ((void)0)
#define GADDR(p) ((uint32_t)(uintptr_t)(p))
#endif

/* raw words, longs and bytes of the original's memory at an address of the blob */
static inline uint8_t sh_r8(uint32_t a) { return *(const uint8_t *)GPTR(a); }
static inline void sh_w8(uint32_t a, uint8_t v) { *(uint8_t *)GPTR(a) = v; }
static inline uint16_t sh_r16(uint32_t a) { return rd16(GPTR(a)); }
static inline void sh_w16(uint32_t a, uint16_t v) { wr16(GPTR(a), v); }
static inline uint32_t sh_r32(uint32_t a) { return rd32(GPTR(a)); }
static inline void sh_w32(uint32_t a, uint32_t v) { wr32(GPTR(a), v); }

void shop_main(void);                                   /* shop $54E00 */

/* the shop's sound (pkg_i2.c): $6477C, $647A6 and what they call */
void shop_irq_tick(void);                               /* shop $6477C */
void shop_sound_cmd(uint16_t d0);                       /* shop $647A6 */
void shop_sound_stop(void);                             /* shop $6478A */
void shop_sound_reset(void);                            /* shop $647B4 */
void shop_sound_tick(void);                             /* shop $64BBC */
void shop_sound_tables(void);                           /* shop $6492A */
void shop_sound_tables2(void);                          /* shop $64972 */
void shop_sound_delay(void);                            /* shop $64BB2 */

#endif
