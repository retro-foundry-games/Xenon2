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
#else
#define guest_blob_raw(on) ((void)0)
#endif

void shop_main(void);   /* shop $54E00 */

#endif
