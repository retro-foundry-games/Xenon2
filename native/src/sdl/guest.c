/* The game's memory and the chips' registers.
 *
 * The memory is one block, struct Xenon2 (game.h): the members sit at the offsets of the original's addresses, so
 * an address of the original is an offset into the block.  The registers of the custom chips and of the CIAs are objects of their own. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "host.h"

struct Xenon2 xenon2 __attribute__((aligned(16)));
volatile uint8_t host_custom_raw[0x1000] __attribute__((aligned(4)));
volatile uint8_t host_cia_raw[0x2000];

#include "guest_members.h"

/* an address of the original's memory is an offset into struct Xenon2; a value that is already an address of the block (a pointer the C made) stays */
void guest_state(host_state_visit visit, void *arg)
{
    visit(&xenon2, sizeof xenon2, arg);
    visit((void *)host_custom_raw, sizeof host_custom_raw, arg);
    visit((void *)host_cia_raw, sizeof host_cia_raw, arg);
}

/* the words of an object that one kind keeps a pointer in: the loader converts the record's own pointer members, the kinds' words only here */
static void object_pointers_to_host(Object *o)
{
    if (o->type == 0x40 && o->link.ptr)
        o->link.ptr = guest_ptr((uint32_t)(uintptr_t)o->link.ptr);
}

static void object_pointers_to_guest(Object *o)
{
    if (o->type == 0x40 && o->link.ptr)
        o->link.ptr = (Object *)(uintptr_t)guest_address(o->link.ptr);
}

void *guest_ptr(uint32_t address)
{
    uint32_t base = (uint32_t)(uintptr_t)&xenon2;

    if (address >= base && address < base + sizeof xenon2)
        return (void *)(uintptr_t)address;
    if (address < sizeof xenon2)
        return (uint8_t *)&xenon2 + address;
    return 0;
}

uint32_t guest_address(const void *p)
{
    return (uint32_t)((const uint8_t *)p - (const uint8_t *)&xenon2);
}

static uint16_t be16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

/* the swaps of the record types' fields (generated: tools/gen_records.py), 0 until a record type has fields to swap */
extern void *guest_code(uint32_t address);   /* code_table.c (generated): the C function for an address of the original's code */
extern void guest_swap_record(const char *type, void *record);
extern unsigned guest_record_size(const char *type);

/* put the bytes of the original's memory at ADDRESS (as they are in the image: big endian) into the members that cover them, in this machine's byte
 * order; pointers (addresses of the original's memory) become pointers into the block */
static int blob_raw;   /* 1: the shop's blob (overlays the level's; the flat members that lie in $54E00-$73E00 are not loaded), 0, 2 to 5: the blob of that level
                        * (0 is level 1): its members (types_blobs.h, guest_blob.h) are converted */
#define BLOB_LO 0x54e00u
#define BLOB_HI 0x74000u
#define BLOB_LEVEL_LO 0x54e40u

static int blob_skips(uint32_t m)
{
    return blob_raw == 1 && m >= BLOB_LO && m < BLOB_HI;
}

int guest_level_table(void) { return blob_raw >= 2 ? blob_raw : 0; }
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

    if (!f && address >= BLOB_LO && address < BLOB_HI && blob_raw >= 2)
        return guest_missing_code(address);
    return f;
}

/* the shop (shop) switches it on when its blob is loaded over the level's and off when the level's is loaded back */
void guest_blob_raw(int on) { blob_raw = on; }

extern uint32_t guest_code_address(const void *function);
struct blob_member { uint32_t offset, count, size, pointer; };
#include "guest_blob.h"

static const struct blob_member *blob_list(unsigned *n)
{
    switch (blob_raw) {
    case 0: *n = sizeof blob_members_1 / sizeof blob_members_1[0]; return blob_members_1;
    case 1: *n = sizeof blob_members_shop / sizeof blob_members_shop[0]; return blob_members_shop;
    case 2: *n = sizeof blob_members_2 / sizeof blob_members_2[0]; return blob_members_2;
    case 3: *n = sizeof blob_members_3 / sizeof blob_members_3[0]; return blob_members_3;
    case 4: *n = sizeof blob_members_4 / sizeof blob_members_4[0]; return blob_members_4;
    case 5: *n = sizeof blob_members_5 / sizeof blob_members_5[0]; return blob_members_5;
    }
    *n = 0;
    return 0;
}

/* the members of the loaded level's blob that the load covers (the image holds ADDRESS.., big endian) into their own bytes of the block */
static void blob_load(uint32_t address, const uint8_t *image, uint32_t length)
{
    unsigned n;
    const struct blob_member *l = blob_list(&n);

    for (unsigned i = 0; i < n; i++) {
        uint32_t a = BLOB_LEVEL_LO + l[i].offset;

        if (a < address || a + l[i].count * l[i].size > address + length)
            continue;
        for (uint32_t k = 0; k < l[i].count; k++) {
            const uint8_t *src = image + (a - address) + k * l[i].size;
            uint8_t *dst = (uint8_t *)&xenon2 + a + k * l[i].size;

            if (l[i].size == 2) {
                uint16_t v = be16(src);

                memcpy(dst, &v, 2);
            } else {
                uint32_t v = be32(src);

                if (l[i].pointer == 1 && v)
                    v = (uint32_t)(uintptr_t)guest_ptr(v);
                else if (l[i].pointer == 2 && v)
                    v = (uint32_t)(uintptr_t)guest_code_stub(v);
                memcpy(dst, &v, 4);
            }
        }
    }
}

/* and back: the same members of the block into the original's memory ram */
static void blob_save(uint8_t *ram)
{
    unsigned n;
    const struct blob_member *l = blob_list(&n);

    for (unsigned i = 0; i < n; i++) {
        for (uint32_t k = 0; k < l[i].count; k++) {
            uint32_t a = BLOB_LEVEL_LO + l[i].offset + k * l[i].size;
            const uint8_t *src = (const uint8_t *)&xenon2 + a;
            uint8_t *dst = ram + a;

            if (l[i].size == 2) {
                uint16_t v;

                memcpy(&v, src, 2);
                dst[0] = (uint8_t)(v >> 8);
                dst[1] = (uint8_t)v;
            } else {
                uint32_t v;

                memcpy(&v, src, 4);
                if (l[i].pointer == 1 && v)
                    v = guest_address((void *)(uintptr_t)v);
                else if (l[i].pointer == 2 && v)
                    v = guest_code_address((void *)(uintptr_t)v);
                dst[0] = (uint8_t)(v >> 24);
                dst[1] = (uint8_t)(v >> 16);
                dst[2] = (uint8_t)(v >> 8);
                dst[3] = (uint8_t)v;
            }
        }
    }
}

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
{
                guest_swap_record(guest_members[i].rec, dst + o);
                if (!strcmp(guest_members[i].rec, "Object"))
                    object_pointers_to_host((Object *)(dst + o));
            }
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
    blob_load(address, image, length);
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
    memset(&xenon2, 0, sizeof xenon2);
    /* the whole image as it is (big endian): the bytes that no member names are still the original's memory, read as bytes by the C that computes addresses
     * from a base; the named members are then converted over it */
    memcpy((uint8_t *)&xenon2 + GAME_IMAGE_ADDRESS, image, GAME_IMAGE_SIZE);
    guest_load_range(GAME_IMAGE_ADDRESS, image, GAME_IMAGE_SIZE);
    free(image);
    return 0;
}

void host_irq_dispatch(int level)
{
    if (level >= 1 && level <= 7 && xenon2.irq_vectors[level - 1])
        xenon2.irq_vectors[level - 1]();
}

extern void game_entry(void) __attribute__((weak));   /* mainloop: the start-up flow of the original ($400) */

void host_game_start(void)
{
    if (game_entry)
        game_entry();
    else
        fprintf(stderr, "xenon2-sdl: the game's entry (game_entry) is not there\n");
}

/* ---- the memory as the original had it, for comparing the port with the original (tools/compare_state.py) ------------------------------------------------------------
 * The whole block (raw buffers keep their bytes) with every named member converted back to the original's byte order and addresses. */
extern void guest_unswap_record(const char *type, void *record);
extern uint32_t guest_code_address(const void *function);

unsigned host_vbl_count(void) { return xenon2.vbl_count; }

static uint8_t guest_high[0x80000];   /* the original's memory from $80000 to $FFFFF (the lockstep gives it); zeros otherwise */

void guest_load_high(const uint8_t *bytes) { memcpy(guest_high, bytes, sizeof guest_high); }

/* a pointer past the end of the port's memory stands for the original's memory there: LENGTH bytes at P lie beyond it */
int guest_beyond(const void *p, size_t length)
{
    return (const uint8_t *)p + length > (const uint8_t *)&xenon2 + sizeof xenon2;
}

const uint8_t *guest_high_ptr(const void *p)   /* 0 outside $80000..$FFFFF */
{
    uint32_t address = guest_address(p) - 0x80000;

    return address < sizeof guest_high ? guest_high + address : 0;
}

uint16_t guest_high_word(const void *p)
{
    uint32_t address = guest_address(p) - 0x80000;

    return address + 2 <= sizeof guest_high ? (uint16_t)(guest_high[address] << 8 | guest_high[address + 1]) : 0;
}

extern uint32_t l2_stale_d0;   /* level2.c */
extern int l2_stale_d0_set;

void guest_set_stale(uint32_t value)
{
    l2_stale_d0 = value;
    l2_stale_d0_set = 1;
}

static int stick_poked;
static uint8_t stick_saved;

/* The lockstep sets the stick byte for the routine that is entered; the interrupt's decode takes the byte it had before (guest_stick_restore). */
void guest_edge_apply(uint8_t value);

void guest_set_stick(uint8_t value)
{
    if (!stick_poked)
        stick_saved = xenon2.joy_stick;
    stick_poked = 1;
    xenon2.joy_stick = value;
    guest_edge_apply(value);
}

static int a6a_set;
static uint8_t a6a_value;

/* The lockstep gives the byte the original's update_all copied into fire_press: the press its interrupt saw lands in a different place of the frame than the port's. */
void guest_set_a6a(uint8_t value)
{
    a6a_set = 1;
    a6a_value = value;
}

uint8_t guest_a6a(uint8_t a01)
{
    if (!a6a_set)
        return a01;
    a6a_set = 0;
    return a6a_value;
}

void guest_set_shield(uint16_t frames)
{
    xenon2.shield_timer = frames;
}

void guest_stick_restore(void)
{
    if (stick_poked)
        xenon2.joy_stick = stick_saved;
    stick_poked = 0;
}

void host_export_ram_buf(uint8_t *ram)   /* ram: 0x80000 bytes */
{
    memcpy(ram, &xenon2, sizeof xenon2);
    for (unsigned i = 0; i < sizeof guest_members / sizeof guest_members[0]; i++) {
        uint8_t *m = ram + guest_members[i].address;
        uint32_t size = guest_members[i].size, elem = guest_members[i].elem;

        if (blob_skips(guest_members[i].address))
            continue;
        if (guest_members[i].rec) {
            unsigned rsize = guest_record_size(guest_members[i].rec);

            for (uint32_t o = 0; rsize && o + rsize <= size; o += rsize)
{
                if (!strcmp(guest_members[i].rec, "Object"))
                    object_pointers_to_guest((Object *)(m + o));
                guest_unswap_record(guest_members[i].rec, m + o);
            }
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
    blob_save(ram);
}

/* the memory as the original had it (big endian, 0x80000 bytes) into the block: the raw bytes, then the named members converted (the dev build starts the port from the
 * original's state this way) */
void host_import_ram_buf(const uint8_t *ram)
{
    memcpy(&xenon2, ram, sizeof xenon2);
    if (blob_raw != 1)   /* (the shop's blob stays while the shop runs) */
        blob_raw = be16(ram + 0xe04) > 1 ? be16(ram + 0xe04) : 0;   /* (level_number: the level) */
    guest_load_range(0, ram, 0x80000);
}
