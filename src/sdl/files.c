/* The disks: ADF images (880 KB, 512-byte sectors, track after track).  Disk 1 holds the game's program, disk 2 the level data. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host.h"

static uint8_t *disks[3];
static size_t disk_size[3];

static int load(int n, const char *folder, const char *const *names)
{
    for (; *names; names++) {
        char path[1024];
        FILE *f;

        snprintf(path, sizeof path, "%s/%s", folder, *names);
        f = fopen(path, "rb");
        if (!f)
            continue;
        enum { ADF_SIZE = 80 * 2 * 11 * 512 };
        long length = fseek(f, 0, SEEK_END) ? -1 : ftell(f);
        if (length != ADF_SIZE || fseek(f, 0, SEEK_SET)) {
            fprintf(stderr, "xenon2-sdl: disk %d must be an uncompressed 880 KB ADF.\n", n);
            fclose(f);
            return -1;
        }
        disks[n] = malloc(ADF_SIZE);
        if (!disks[n] || fread(disks[n], 1, ADF_SIZE, f) != ADF_SIZE) {
            fprintf(stderr, "xenon2-sdl: could not read disk %d.\n", n);
            free(disks[n]);
            disks[n] = NULL;
            fclose(f);
            return -1;
        }
        disk_size[n] = ADF_SIZE;
        fclose(f);
        return 0;
    }
    fprintf(stderr, "xenon2-sdl: could not find disk %d (xenon2-disk%d.adf) in %s.\n", n, n, folder);
    return -1;
}

int files_open(const char *folder)
{
    static const char *const one[] = {
        "xenon2-disk1.adf",
        "Xenon 2 - Megablast (1989)(Image Works)(Disk 1 of 2)[cr PNA].adf",
        "Xenon 2 - Megablast (1989)(Image Works)(Disk 1 of 2)[cr BS1][h Deejay99][t +3 BS1].adf",
        NULL
    };
    static const char *const two[] = { "xenon2-disk2.adf", "Xenon 2 - Megablast (1989)(Image Works)(Disk 2 of 2).adf", 0 };

    for (int n = 1; n <= 2; n++) {
        free(disks[n]);
        disks[n] = NULL;
        disk_size[n] = 0;
    }
    return load(1, folder, one) || load(2, folder, two) ? -1 : 0;
}

int files_read_sectors(int disk, uint32_t first, uint32_t count, void *dest)
{
    if (disk < 1 || disk > 2 || !disks[disk])
        return -1;
    size_t sectors = disk_size[disk] / 512;
    if (first > sectors || count > sectors - first)
        return -1;
    memcpy(dest, disks[disk] + (size_t)first * 512, (size_t)count * 512);
    return 0;
}

