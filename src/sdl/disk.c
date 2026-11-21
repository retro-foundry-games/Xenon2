/* The game's own disk loader, replaced by a high-level one: the game's C (HLE_WEAK disk_read_sectors, menus) programs the disk DMA and decodes MFM; here the
 * sectors come straight from the ADF of the game disk (disk 2: the levels, the shop; disk 1 holds the program, loaded at the start) and are put into the
 * members of the game's memory that cover the destination.  There is no drive to put a disk in: the game's check of sector 0 always finds its disk. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void host_idle(void);

#include "game.h"
#include "host.h"

int host_disk_gate;      /* the lockstep: a load waits (host_disk_waiting) until the script releases it instead of taking the drive's time */
int host_disk_waiting;

int disk_read_sectors(uint16_t first_sector, uint16_t count, void *dest)
{
    uint8_t *tmp;
    int r;

    tmp = malloc((size_t)count * 512);
    if (!tmp)
        return -1;
    r = files_read_sectors(2, first_sector, count, tmp);
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
