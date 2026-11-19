/* The game's memory and the chips' registers.
 *
 * The memory is one block, struct Xenon (game_state.h): with MATCH_MEMORY_LAYOUT the members sit at the offsets of the original's addresses, so
 * an address of the original is an offset into the block.  The registers of the custom chips and of the CIAs are objects of their own. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GAME_NO_MEMBER_MACROS   /* this file names the members itself (guest_members.h) */
#include "game.h"
#include "host.h"

struct Xenon xenon __attribute__((aligned(16)));
volatile uint8_t host_custom_raw[0x1000] __attribute__((aligned(4)));
volatile uint8_t host_cia_raw[0x2000];

#include "guest_members.h"

#ifndef MATCH_MEMORY_LAYOUT
#error "the SDL port keeps the original's layout (MATCH_MEMORY_LAYOUT) for now"
#endif

/* an address of the original's memory is an offset into struct Xenon; a value that is already an address of the block (a pointer the C made) stays */
void *guest_ptr(uint32_t address)
{
    uint32_t base = (uint32_t)(uintptr_t)&xenon;

    if (address >= base && address < base + sizeof xenon)
        return (void *)(uintptr_t)address;
    if (address < sizeof xenon)
        return (uint8_t *)&xenon + address;
    return 0;
}

uint32_t guest_address(const void *p)
{
    return (uint32_t)((const uint8_t *)p - (const uint8_t *)&xenon);
}

static uint16_t be16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

/* the swaps of the record types' fields (generated: tools/gen_records.py), 0 until a record type has fields to swap */
extern void *guest_code(uint32_t address);   /* code_table.c (generated): the C function for an address of the original's code */
extern void guest_swap_record(const char *type, void *record);
extern unsigned guest_record_size(const char *type);

/* put the bytes of the original's memory at ADDRESS (as they are in the image: big endian) into the members that cover them, in this machine's byte
 * order; pointers (addresses of the original's memory) become pointers into the block */
void guest_load_range(uint32_t address, const uint8_t *image, uint32_t length)
{
    for (unsigned i = 0; i < sizeof guest_members / sizeof guest_members[0]; i++) {
        uint32_t m = guest_members[i].address, size = guest_members[i].size, elem = guest_members[i].elem;
        uint32_t lo = m > address ? m : address, hi = m + size < address + length ? m + size : address + length;

        if (lo >= hi)
            continue;
        /* whole elements only: a member is loaded in one piece (the image covers all of it or none) */
        if (m < address || m + size > address + length)
            continue;
        uint8_t *dst = (uint8_t *)guest_members[i].dst;
        const uint8_t *src = image + (m - address);

        if (guest_members[i].rec) {
            unsigned rsize = guest_record_size(guest_members[i].rec);

            memcpy(dst, src, size);
            for (uint32_t o = 0; rsize && o + rsize <= size; o += rsize)
                guest_swap_record(guest_members[i].rec, dst + o);
        } else if (elem == 1) {
            memcpy(dst, src, size);
        } else {
            for (uint32_t o = 0; o + elem <= size; o += elem) {
                uint32_t v = elem == 2 ? be16(src + o) : be32(src + o);

                if (guest_members[i].ptr && v)
                    v = (uint32_t)(uintptr_t)(guest_members[i].ptr == 2 ? guest_code(v) : guest_ptr(v));
                if (elem == 2)
                    *(uint16_t *)(dst + o) = (uint16_t)v;
                else
                    *(uint32_t *)(dst + o) = v;
            }
        }
    }
}

/* disk 1 from track 82 on is the game's program, data and variables, as the loader of disk 1 puts them at $400 ($6B400 bytes) */
#define GAME_IMAGE_SECTOR (82 * 11)   /* disk offset $70C00 */
#define GAME_IMAGE_ADDRESS 0x400
#define GAME_IMAGE_SIZE 0x6b400

int guest_load_game_image(void)
{
    uint8_t *image = malloc(GAME_IMAGE_SIZE);

    if (!image || files_read_sectors(1, GAME_IMAGE_SECTOR, GAME_IMAGE_SIZE / 512, image) != 0) {
        free(image);
        fprintf(stderr, "xenon2-sdl: cannot read the game's image from disk 1\n");
        return -1;
    }
    memset(&xenon, 0, sizeof xenon);
    /* the whole image as it is (big endian): the bytes that no member names are still the original's memory, read as bytes by the C that computes addresses
     * from a base; the named members are then converted over it */
    memcpy((uint8_t *)&xenon + GAME_IMAGE_ADDRESS, image, GAME_IMAGE_SIZE);
    guest_load_range(GAME_IMAGE_ADDRESS, image, GAME_IMAGE_SIZE);
    free(image);
    return 0;
}

void host_irq_dispatch(int level)
{
    if (level >= 1 && level <= 7 && xenon.irq_vectors[level - 1])
        xenon.irq_vectors[level - 1]();
}

extern void game_entry(void) __attribute__((weak));   /* pkg_b: the start-up flow of the original ($400) */

void host_game_start(void)
{
    if (game_entry)
        game_entry();
    else
        fprintf(stderr, "xenon2-sdl: the game's entry (game_entry) is not there\n");
}

/* a few variables of the game, for the headless tests (--status): the frame count, the level's scroll, the score digits, the list head of the objects */
void host_game_status(void)
{
    printf("game: vbl %u scroll %04x cur_player %u objects at %08x score digits", (unsigned)xenon.vbl_count, (unsigned)xenon.v_0cd0, (unsigned)xenon.cur_player,
           (unsigned)(uintptr_t)xenon.obj_list[0].next);
    for (int i = 0; i < 7; i++)
        printf(" %u", (unsigned)xenon.v_0d7c[i]);
    printf("\n");
}
