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
void guest_state(host_state_visit visit, void *arg)
{
    visit(&xenon, sizeof xenon, arg);
    visit((void *)host_custom_raw, sizeof host_custom_raw, arg);
    visit((void *)host_cia_raw, sizeof host_cia_raw, arg);
}

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
static int blob_raw;   /* 1: the shop's blob is in $54E00-$73E00 (it overlays the level's): the members of the level that lie there are not converted;
                        * 2: the blob of a level after the first: the members of level 1 beyond the header ($54E40 on) are not converted */
#define BLOB_LO 0x54e00u
#define BLOB_HI 0x74000u
#define BLOB_LEVEL_LO 0x54e40u

static int blob_skips(uint32_t m)
{
    return (blob_raw == 1 && m >= BLOB_LO && m < BLOB_HI) || (blob_raw == 2 && m >= BLOB_LEVEL_LO && m < BLOB_HI);
}

int guest_level_table(void) { return blob_raw == 2 ? 2 : 0; }
extern void *guest_code(uint32_t address);

static void missing_code(void) {}

static void *guest_missing_code(uint32_t address)
{
    static uint32_t seen[256];
    static unsigned n;

    for (unsigned i = 0; i < n; i++)
        if (seen[i] == address)
            return (void *)missing_code;
    if (n < sizeof seen / sizeof seen[0])
        seen[n++] = address;
    fprintf(stderr, "xenon2-sdl: the level's code at $%06X has no C\n", address);
    return (void *)missing_code;
}

void *guest_code_stub(uint32_t address)
{
    void *f = guest_code(address);

    if (!f && address >= BLOB_LO && address < BLOB_HI && blob_raw == 2)
        return guest_missing_code(address);
    return f;
}

/* the shop (pkg_i) switches it on when its blob is loaded over the level's and off when the level's is loaded back */
void guest_blob_raw(int on) { blob_raw = on; }

void guest_load_range(uint32_t address, const uint8_t *image, uint32_t length)
{
    for (unsigned i = 0; i < sizeof guest_members / sizeof guest_members[0]; i++) {
        uint32_t m = guest_members[i].address, size = guest_members[i].size, elem = guest_members[i].elem;

        if (blob_skips(m))
            continue;
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
                    v = (uint32_t)(uintptr_t)(guest_members[i].ptr == 2 ? guest_code_stub(v) : guest_ptr(v));
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
    printf("game: vbl %u scroll %04x cur_player %u v_0430 %04x v_0d84 %02x v_0a02 %02x v_0e04 %u objects at %08x score digits", (unsigned)xenon.vbl_count, (unsigned)xenon.v_0cd0, (unsigned)xenon.cur_player, (unsigned)xenon.v_0430, (unsigned)xenon.v_0d84, (unsigned)xenon.v_0a02, (unsigned)xenon.v_0e04,
           (unsigned)(uintptr_t)xenon.obj_list[0].next);
    for (int i = 0; i < 7; i++)
        printf(" %u", (unsigned)xenon.v_0d7c[i]);
    printf("  lists:");
    for (int l = 0; l < 5; l++) {
        int n = 0;
        Object *o = xenon.obj_list[l].next;

        while (o && o != &xenon.obj_list[l] && n < 500) {
            n++;
            o = o->next;
        }
        printf(" %d", n);
    }
    printf("  enemies (x,y,type):");
    {
        Object *o = xenon.obj_list[2].next;

        for (int n = 0; o && o != &xenon.obj_list[2] && n < 4; n++, o = o->next)
            printf(" (%d,%d,%x)", (int)(int16_t)o->x, (int)(int16_t)o->y, (unsigned)o->type);
    }
    printf("\n");
}

/* ---- the memory as the original had it, for comparing the port with the original (tools/compare_state.py) ------------------------------------------------------------
 * The whole block (raw buffers keep their bytes) with every named member converted back to the original's byte order and addresses. */
extern void guest_unswap_record(const char *type, void *record);
extern uint32_t guest_code_address(const void *function);

unsigned host_vbl_count(void) { return xenon.vbl_count; }

static uint8_t guest_high[0x80000];   /* the original's memory from $80000 to $FFFFF (the lockstep gives it); zeros otherwise */

void guest_load_high(const uint8_t *bytes) { memcpy(guest_high, bytes, sizeof guest_high); }

uint16_t guest_high_word(uint32_t address)
{
    address -= 0x80000;
    return address + 2 <= sizeof guest_high ? (uint16_t)(guest_high[address] << 8 | guest_high[address + 1]) : 0;
}

void guest_set_stick(uint8_t value) { xenon.v_0a00 = value; }

void host_export_ram_buf(uint8_t *ram)   /* ram: 0x80000 bytes */
{
    memcpy(ram, &xenon, sizeof xenon);
    for (unsigned i = 0; i < sizeof guest_members / sizeof guest_members[0]; i++) {
        uint8_t *m = ram + guest_members[i].address;
        uint32_t size = guest_members[i].size, elem = guest_members[i].elem;

        if (blob_skips(guest_members[i].address))
            continue;
        if (guest_members[i].rec) {
            unsigned rsize = guest_record_size(guest_members[i].rec);

            for (uint32_t o = 0; rsize && o + rsize <= size; o += rsize)
                guest_unswap_record(guest_members[i].rec, m + o);
        } else if (elem > 1) {
            for (uint32_t o = 0; o + elem <= size; o += elem) {
                uint32_t v = 0;

                if (elem == 2) {
                    uint16_t w;

                    memcpy(&w, m + o, 2);
                    v = w;
                } else {
                    memcpy(&v, m + o, 4);
                    if (guest_members[i].ptr == 1 && v)
                        v = guest_address((void *)(uintptr_t)v);
                    else if (guest_members[i].ptr == 2 && v)
                        v = guest_code_address((void *)(uintptr_t)v);
                }
                if (elem == 2) {
                    m[o] = (uint8_t)(v >> 8);
                    m[o + 1] = (uint8_t)v;
                } else {
                    m[o] = (uint8_t)(v >> 24);
                    m[o + 1] = (uint8_t)(v >> 16);
                    m[o + 2] = (uint8_t)(v >> 8);
                    m[o + 3] = (uint8_t)v;
                }
            }
        }
    }
}

int host_export_ram(const char *path)
{
    uint8_t *ram = malloc(sizeof xenon);
    FILE *f;

    if (!ram)
        return -1;
    host_export_ram_buf(ram);
    f = fopen(path, "wb");
    if (!f) {
        free(ram);
        return -1;
    }
    fwrite(ram, 1, 0x80000, f);   /* the 512 KB the original had */
    fclose(f);
    free(ram);
    return 0;
}

/* the memory as the original had it (big endian, 0x80000 bytes) into the block: the raw bytes, then the named members converted (the dev build starts the port from the
 * original's state this way) */
void host_import_ram_buf(const uint8_t *ram)
{
    memcpy(&xenon, ram, sizeof xenon);
    blob_raw = be16(ram + 0xe04) > 1 ? 2 : 0;   /* (v_0e04: the level) */
    guest_load_range(0, ram, 0x80000);
}
