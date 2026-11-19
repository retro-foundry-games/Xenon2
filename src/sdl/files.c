/* The disks: ADF images (880 KB, 512-byte sectors, track after track).  Disk 1 holds the game's program, disk 2 the level data; the
 * game asks for disk 2 in the drive when it starts a level and `files_insert` puts it there. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host.h"

static uint8_t *disks[3];
static size_t disk_size[3];
static int current = 1;

static int load(int n, const char *folder, const char *const *names)
{
    for (; *names; names++) {
        char path[1024];
        FILE *f;

        snprintf(path, sizeof path, "%s/%s", folder, *names);
        f = fopen(path, "rb");
        if (!f)
            continue;
        fseek(f, 0, SEEK_END);
        disk_size[n] = (size_t)ftell(f);
        rewind(f);
        disks[n] = malloc(disk_size[n]);
        if (!disks[n] || fread(disks[n], 1, disk_size[n], f) != disk_size[n]) {
            fclose(f);
            return -1;
        }
        fclose(f);
        return 0;
    }
    return -1;
}

int files_open(const char *folder)
{
    static const char *const one[] = { "xenon2-disk1.adf", "Xenon 2 - Megablast (1989)(Image Works)(Disk 1 of 2)[cr PNA].adf", 0 };
    static const char *const two[] = { "xenon2-disk2.adf", "Xenon 2 - Megablast (1989)(Image Works)(Disk 2 of 2).adf", 0 };

    if (load(1, folder, one) || load(2, folder, two)) {
        fprintf(stderr, "xenon2-sdl: need the two disk images (xenon2-disk1.adf, xenon2-disk2.adf) in %s\n", folder);
        return -1;
    }
    return 0;
}

int files_read_sectors(int disk, uint32_t first, uint32_t count, void *dest)
{
    if (disk < 1 || disk > 2 || !disks[disk] || (size_t)(first + count) * 512 > disk_size[disk])
        return -1;
    memcpy(dest, disks[disk] + (size_t)first * 512, (size_t)count * 512);
    return 0;
}

int files_current_disk(void) { return current; }
void files_insert(int disk) { current = disk; }
