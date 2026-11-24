/* Disk 1 of the BS1/Deejay99 release is an AmigaDOS OFS disk. Its Xenon2
 * executable unpacks backwards at $30000, then copies $3FC00 bytes from
 * there to $400 (bootstrap $700E0). Read those game bytes without running
 * the Amiga executable or its trainer. */
#include <stdlib.h>
#include <string.h>

#include "host.h"

enum {
    SECTOR_SIZE = 512, OFS_ROOT = 880, OFS_DATA = 24, OFS_PAYLOAD = SECTOR_SIZE - OFS_DATA,
    HUNK_CODE = 36, PACK_START = 0x136, GAME_BYTES = 0x3fc00, UNPACKED_BYTES = 0x464ca
};

static uint32_t big_endian_long(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static uint8_t *read_executable(size_t *length)
{
    uint8_t block[SECTOR_SIZE];
    if (files_read_sectors(1, 0, 1, block) || memcmp(block, "DOS\0", 4))
        return NULL;

    for (uint32_t file = 2; file < 2 * OFS_ROOT; file++) {
        if (files_read_sectors(1, file, 1, block))
            return NULL;
        if (big_endian_long(block) != 2 || big_endian_long(block + 4) != file ||
            big_endian_long(block + 500) != OFS_ROOT || big_endian_long(block + 508) != (uint32_t)-3 ||
            block[432] != 6 || memcmp(block + 433, "Xenon2", 6))
            continue;

        size_t size = big_endian_long(block + 324);
        uint32_t sector = big_endian_long(block + 16);
        if (size <= HUNK_CODE + PACK_START || size > 2 * OFS_ROOT * OFS_PAYLOAD)
            return NULL;
        uint8_t *bytes = malloc(size);
        if (!bytes)
            return NULL;
        for (size_t offset = 0, sequence = 1; offset < size; sequence++) {
            size_t count = size - offset;
            if (count > OFS_PAYLOAD)
                count = OFS_PAYLOAD;
            if (!sector || files_read_sectors(1, sector, 1, block) || big_endian_long(block) != 8 ||
                big_endian_long(block + 4) != file || big_endian_long(block + 8) != sequence ||
                big_endian_long(block + 12) != count) {
                free(bytes);
                return NULL;
            }
            memcpy(bytes + offset, block + OFS_DATA, count);
            offset += count;
            sector = big_endian_long(block + 16);
        }
        if (sector) {
            free(bytes);
            return NULL;
        }
        *length = size;
        return bytes;
    }
    return NULL;
}

struct Bits {
    const uint8_t *stream;
    size_t remaining;
    uint32_t word;
    int exhausted;
};

/* The unpacker's reader at $116 shifts bits out low first, then refills
 * backwards through the big-endian words. Each refill supplies a sentinel. */
static unsigned read_bits(struct Bits *bits, unsigned count)
{
    unsigned value = 0;

    while (count--) {
        unsigned bit = bits->word & 1;
        bits->word >>= 1;
        if (!bits->word) {
            if (bits->remaining < 4) {
                bits->exhausted = 1;
                return 0;
            }
            bits->remaining -= 4;
            uint32_t word = big_endian_long(bits->stream + bits->remaining);
            bit = word & 1;
            bits->word = (word >> 1) | UINT32_C(0x80000000);
        }
        value = (value << 1) | bit;
    }
    return value;
}

static unsigned literal_count(struct Bits *bits)
{
    unsigned count = read_bits(bits, 3);
    if (count != 7)
        return count;
    if (!read_bits(bits, 1))
        return read_bits(bits, 4) + 7;
    count = read_bits(bits, 10);
    return count ? count : read_bits(bits, 18);
}

/* Match lengths and offset widths from the unpacker's table at $80–$F6. */
static unsigned match_count(struct Bits *bits, unsigned *width)
{
    unsigned kind = read_bits(bits, 2);
    unsigned count = kind + 2;

    if (kind == 0) {
        *width = 8;
    } else if (kind == 1) {
        *width = read_bits(bits, 1) ? 8 : 14;
    } else {
        if (kind == 3) {
            unsigned extra = read_bits(bits, 2);
            if (extra == 3)
                count = read_bits(bits, 8);
            else if (extra == 2)
                count = read_bits(bits, 2) + 7;
            else
                count = extra + 5;
        }
        *width = 16;
        if (read_bits(bits, 1))
            *width = read_bits(bits, 1) ? 8 : 12;
    }
    return count;
}

static int unpack(const uint8_t *stream, size_t length, uint8_t *output, size_t size)
{
    struct Bits bits = { stream, length - 8, big_endian_long(stream + length - 8), 0 };
    size_t remaining = size;

    while (remaining) {
        unsigned literals = literal_count(&bits);
        if (bits.exhausted || literals > remaining)
            return -1;
        while (literals--)
            output[--remaining] = read_bits(&bits, 8);
        if (bits.exhausted)
            return -1;
        if (!remaining)
            break;

        unsigned width;
        unsigned count = match_count(&bits, &width);
        unsigned offset = read_bits(&bits, width);
        if (bits.exhausted || !count || count > remaining || !offset || offset > size - remaining)
            return -1;
        while (count--) {
            remaining--;
            output[remaining] = output[remaining + offset];
        }
    }
    return bits.remaining ? -1 : 0;
}

int files_read_bs1_game(uint8_t *image, size_t size)
{
    /* HUNK_HEADER and the unpacker entry. Its ADDA immediate gives the
     * packed size, and the final long is the decompressed length. */
    static const uint8_t signature[] = {
        0x41, 0xfa, 0x01, 0x34, 0xd1, 0xfc, 0x00, 0x03, 0x22, 0x60,
        0x43, 0xf9, 0x00, 0x03, 0x00, 0x00, 0x24, 0x60, 0xd5, 0xc9
    };
    size_t file_size = 0;
    uint8_t *file = read_executable(&file_size);
    int result = -1;

    if (file && big_endian_long(file) == 0x3f3 && big_endian_long(file + 32) == 0xc8e7 &&
        !memcmp(file + HUNK_CODE, signature, sizeof signature) && size >= GAME_BYTES) {
        const uint8_t *code = file + HUNK_CODE;
        size_t packed_size = big_endian_long(code + 6);
        if (packed_size >= 8 && packed_size <= file_size - HUNK_CODE - PACK_START &&
            big_endian_long(code + PACK_START + packed_size - 4) == UNPACKED_BYTES) {
            uint8_t *decoded = malloc(UNPACKED_BYTES);
            if (decoded && !unpack(code + PACK_START, packed_size, decoded, UNPACKED_BYTES)) {
                memcpy(image, decoded, GAME_BYTES);
                memset(image + GAME_BYTES, 0, size - GAME_BYTES);
                result = 0;
            }
            free(decoded);
        }
    }
    free(file);
    return result;
}
