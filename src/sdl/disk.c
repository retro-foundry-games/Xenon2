/* The game's own disk loader, replaced by a high-level one: the game's C (HLE_WEAK disk_read_sectors, menus) programs the disk DMA and decodes MFM; here the
 * sectors come straight from the ADF of the disk in the drive and are put into the members of the game's memory that cover the destination.
 *
 * The game asks for its game disk in DF0 (sector 0 must hold "X2B "); the disk in the drive is disk 1 when the program starts.  The port puts disk 2 in at
 * the first ask, so the game never shows INSERT GAME DISK; the dev build waits for the ask number host_disk_swap_ask, as the original's run does. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void host_idle(void);

#include "game.h"
#include "host.h"

int host_disk_swap_ask = 1;
int host_disk_gate;      /* the lockstep: a load waits (host_disk_waiting) until the script releases it instead of taking the drive's time */
int host_disk_waiting;
static int asked_for_sector_0;

void disk_state(host_state_visit visit, void *arg)
{
    visit(&host_disk_swap_ask, sizeof host_disk_swap_ask, arg);
    visit(&asked_for_sector_0, sizeof asked_for_sector_0, arg);
}

int disk_read_sectors(uint16_t first_sector, uint16_t count, void *dest)
{
    uint8_t *tmp;
    int r;

    if (first_sector == 0 && count == 1 && files_current_disk() == 1 && ++asked_for_sector_0 >= host_disk_swap_ask)
        files_insert(2);
    tmp = malloc((size_t)count * 512);
    if (!tmp)
        return -1;
    r = files_read_sectors(files_current_disk(), first_sector, count, tmp);
    if (r == 0) {
        /* the bytes as they are (big endian) into the memory, then the members that cover them in the host's byte order */
        memcpy(dest, tmp, (size_t)count * 512);
        guest_load_range(guest_address(dest), tmp, (uint32_t)count * 512);
    }
    free(tmp);   /* (before the waiting: no heap object is live in the game's stack while a state can be taken) */
    if (host_disk_gate) {
        host_disk_waiting = 1;
        while (host_disk_waiting)
            host_idle();
    }
    return r;
}
