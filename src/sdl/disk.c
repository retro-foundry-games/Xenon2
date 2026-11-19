/* The game's own disk loader, replaced by a high-level one: the game's C (HLE_WEAK disk_read_sectors, pkg_g) programs the disk DMA and decodes MFM; here the
 * sectors come straight from the ADF of the disk in the drive and are put into the members of the game's memory that cover the destination.
 *
 * The game asks for its game disk in DF0 (sector 0 must hold "X2B "); the disk in the drive is disk 1 when the program starts.  As the real machine
 * wanted the other disk put in by hand, the port puts disk 2 in after the game has asked twice (--manual-disk: never, F9 swaps). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "game.h"
#include "host.h"

int host_manual_disk;
static int asked_for_sector_0;

int disk_read_sectors(uint16_t first_sector, uint16_t count, void *dest)
{
    uint8_t *tmp;
    int r;

    if (first_sector == 0 && count == 1 && files_current_disk() == 1 && !host_manual_disk && ++asked_for_sector_0 >= 2)
        files_insert(2);
    tmp = malloc((size_t)count * 512);
    if (!tmp)
        return -1;
    r = files_read_sectors(files_current_disk(), first_sector, count, tmp);
    if (r == 0)
        guest_load_range(guest_address(dest), tmp, (uint32_t)count * 512);
    free(tmp);
    return r;
}
