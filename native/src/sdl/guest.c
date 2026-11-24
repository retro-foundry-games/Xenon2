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
#include "presentation.h"
#include "sound.h"

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
    if (o->type == 0x48) {   /* the target: the long in the words 30 and 32 (high first), each word loaded as it is */
        uint32_t a = (uint32_t)(uint16_t)o->drill.target_hi << 16 | (uint16_t)o->energy;
        Object *target = a ? (Object *)guest_ptr(a) : 0;

        if (!a || target)   /* (a debris of kind $48 keeps other words here) */
            o->drill_target.target = target;
    }
}

static void object_pointers_to_guest(Object *o)
{
    if (o->type == 0x40 && o->link.ptr)
        o->link.ptr = (Object *)(uintptr_t)guest_address(o->link.ptr);
    if (o->type == 0x48) {
        const uint8_t *t = (const uint8_t *)o->drill_target.target;

        if (!t || (t >= (const uint8_t *)&xenon2 && t < (const uint8_t *)&xenon2 + sizeof xenon2)) {
            uint32_t a = t ? guest_address(t) : 0;

            o->drill.target_hi = (int16_t)(a >> 16);
            o->energy = (int16_t)a;
        }
    }
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
                        * (0 is level 1): its members (level.h, levelN.h, shop.h, guest_blob.h) are converted */
#define BLOB_LO 0x54e00u
#define BLOB_HI 0x74000u
#define BLOB_LEVEL_LO 0x54e40u

static int blob_skips(uint32_t m)
{
    return blob_raw == 1 && m >= BLOB_LO && m < BLOB_HI;
}

int guest_level_table(void) { return blob_raw >= 2 ? blob_raw : 0; }
extern void *guest_code(uint32_t address);

/* what stands for the level's code that has no C: it takes the object like the update and draw routines it stands for (WebAssembly checks the signature of an indirect call) */
static void missing_code(void *object) { (void)object; }
int guest_is_missing_code(const void *function) { return function == (const void *)missing_code; }

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

struct blob_script { uint32_t offset, size; };
struct blob_script_pointers { uint32_t offset, count; };
static const struct blob_script_pointers *blob_script_pointers(unsigned *n)
{
    switch (blob_raw) {
    case 0: *n = sizeof blob_script_pointers_1 / sizeof blob_script_pointers_1[0]; return (const struct blob_script_pointers *)blob_script_pointers_1;
    case 2: *n = sizeof blob_script_pointers_2 / sizeof blob_script_pointers_2[0]; return (const struct blob_script_pointers *)blob_script_pointers_2;
    case 3: *n = sizeof blob_script_pointers_3 / sizeof blob_script_pointers_3[0]; return (const struct blob_script_pointers *)blob_script_pointers_3;
    case 4: *n = sizeof blob_script_pointers_4 / sizeof blob_script_pointers_4[0]; return (const struct blob_script_pointers *)blob_script_pointers_4;
    case 5: *n = sizeof blob_script_pointers_5 / sizeof blob_script_pointers_5[0]; return (const struct blob_script_pointers *)blob_script_pointers_5;
    }
    *n = 0;
    return 0;
}
static const struct blob_script *blob_scripts(unsigned *n)
{
    switch (blob_raw) {
    case 0: *n = sizeof blob_scripts_1 / sizeof blob_scripts_1[0]; return (const struct blob_script *)blob_scripts_1;
    case 2: *n = sizeof blob_scripts_2 / sizeof blob_scripts_2[0]; return (const struct blob_script *)blob_scripts_2;
    case 3: *n = sizeof blob_scripts_3 / sizeof blob_scripts_3[0]; return (const struct blob_script *)blob_scripts_3;
    case 4: *n = sizeof blob_scripts_4 / sizeof blob_scripts_4[0]; return (const struct blob_script *)blob_scripts_4;
    case 5: *n = sizeof blob_scripts_5 / sizeof blob_scripts_5[0]; return (const struct blob_script *)blob_scripts_5;
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

/* ---- the animation scripts: steps of a picture (a long) and a delay; the picture 0 then a routine (a long): "jump" (a long, the script to go on with) and
 * "free" end it.  The longs become pointers when the words that hold them are loaded (the sites are kept to put the addresses back on export). ---- */
#define SCRIPT_SITES 20000
static uint32_t script_site[SCRIPT_SITES];   /* the address of the long, shifted left by 2, with the kind: 1 data, 2 code */
static unsigned script_sites;
static uint8_t script_seen[0x80000 / 16];   /* a bit per word of the block: part of a script already made */

static int script_is_seen(uint32_t a) { return script_seen[a >> 4] >> ((a >> 1) & 7) & 1; }
static void script_mark(uint32_t a) { script_seen[a >> 4] |= (uint8_t)(1u << ((a >> 1) & 7)); }

static void script_forget(uint32_t address, uint32_t length)
{
    unsigned keep = 0;

    for (uint32_t a = address & ~1u; a < address + length && a < 0x80000; a += 2)
        script_seen[a >> 4] &= (uint8_t)~(1u << ((a >> 1) & 7));
    for (unsigned i = 0; i < script_sites; i++) {
        uint32_t a = script_site[i] >> 2;

        if (a + 4 <= address || a >= address + length)
            script_site[keep++] = script_site[i];
    }
    script_sites = keep;
}

static uint32_t script_long(uint32_t a)
{
    const uint16_t *w = (const uint16_t *)((const uint8_t *)&xenon2 + a);

    return (uint32_t)w[0] << 16 | w[1];
}

static void script_put(uint32_t a, void *pointer, unsigned kind)
{
    memcpy((uint8_t *)&xenon2 + a, &pointer, 4);
    if (script_sites < SCRIPT_SITES)
        script_site[script_sites++] = a << 2 | kind;
}

static void *script_data(uint32_t v)
{
    void *p = guest_ptr(v);

    return p ? p : (uint8_t *)&xenon2 + v;   /* (the original's memory beyond the block) */
}

extern void anim_op_jump(), anim_op_free();

/* the script at A, and what follows it up to END (the member's end); END 0: the script alone, up to the step that ends it */
static void script_walk(uint32_t a, uint32_t lo, uint32_t hi, uint32_t end)
{
    while (a >= lo && a + 6 <= hi && (!end || a + 6 <= end) && !script_is_seen(a)) {
        uint32_t pic = script_long(a);

        if (pic && (pic < 0x400 || pic >= 0x80000 || (pic & 1)))
            return;   /* (not a picture: the words that follow a script are data) */
        if (pic) {
            for (int k = 0; k < 6; k += 2)
                script_mark(a + k);
            script_put(a, script_data(pic), 1);
            a += 6;
            continue;
        }
        if (a + 8 > hi || (end && a + 8 > end))
            return;
        for (int k = 0; k < 8; k += 2)
            script_mark(a + k);
        void *code = guest_code_stub(script_long(a + 4));

        script_put(a + 4, code, 2);
        a += 8;
        if (code == (void *)anim_op_jump && a + 4 <= hi && (!end || a + 4 <= end)) {
            uint32_t target = script_long(a);

            script_mark(a);
            script_mark(a + 2);
            script_put(a, script_data(target), 1);
            a += 4;
            script_walk(target, lo, hi, 0);
        }
        if (!end || a >= end)
            return;
    }
}

static void scripts_load(uint32_t address, uint32_t length)
{
    script_forget(address, length);
    for (unsigned i = 0; i < sizeof guest_members / sizeof guest_members[0]; i++) {
        uint32_t m = guest_members[i].address;

        if (guest_members[i].elem == 2 && !guest_members[i].ptr && (!strncmp(guest_members[i].name, "anim_", 5) || !strcmp(guest_members[i].name, "option_anims")) && m >= address && m < address + length && !blob_skips(m))
            script_walk(m, address, address + length, m + guest_members[i].size);
    }
    if (blob_raw != 1) {
        unsigned n;
        const struct blob_script *l = blob_scripts(&n);

        for (unsigned i = 0; i < n; i++) {
            uint32_t m = BLOB_LEVEL_LO + l[i].offset;

            if (m >= address && m < address + length)
                script_walk(m, address, address + length, 0);
        }
        const struct blob_script_pointers *sp = blob_script_pointers(&n);

        for (unsigned i = 0; i < n; i++)
            for (uint32_t k = 0; k < sp[i].count; k++) {
                const uint8_t *site = (const uint8_t *)&xenon2 + BLOB_LEVEL_LO + sp[i].offset + 4 * k;
                const uint8_t *p;

                memcpy(&p, site, 4);
                if (p >= (const uint8_t *)&xenon2 && p < (const uint8_t *)&xenon2 + sizeof xenon2 && (uint32_t)(site - (const uint8_t *)&xenon2) >= address
                    && (uint32_t)(site - (const uint8_t *)&xenon2) < address + length)
                    {
                        uint32_t t = (uint32_t)(p - (const uint8_t *)&xenon2);

                        script_walk(t, address, address + length, 0);
                    }
            }
    }
}

static void scripts_save(uint8_t *ram)
{
    for (unsigned i = 0; i < script_sites; i++) {
        uint32_t a = script_site[i] >> 2, v;
        void *p;

        memcpy(&p, (const uint8_t *)&xenon2 + a, 4);
        v = (script_site[i] & 3) == 2 ? guest_code_address(p) : (uint32_t)((const uint8_t *)p - (const uint8_t *)&xenon2);
        ram[a] = (uint8_t)(v >> 24);
        ram[a + 1] = (uint8_t)(v >> 16);
        ram[a + 2] = (uint8_t)(v >> 8);
        ram[a + 3] = (uint8_t)v;
    }
}

/* ---- the sample bank: headers (a long and a word) between the samples, 20 instruments from the start and 18 effects from SAMPLE_EFFECTS ---- */
#define SAMPLE_BANK offsetof(struct Xenon2, sample_bank)
#define SAMPLE_BANK_END (SAMPLE_BANK + sizeof xenon2.sample_bank)
#define SAMPLE_EFFECTS (SAMPLE_BANK + SAMPLE_EFFECTS_OFFSET)
static const struct { uint32_t start; unsigned count; } sample_runs[] = { { SAMPLE_BANK, 20 }, { SAMPLE_EFFECTS, 18 } };

static int sample_headers(uint32_t address, const uint8_t *image, int convert)
{
    for (unsigned r = 0; r < 2; r++) {
        uint32_t a = sample_runs[r].start;
        uint32_t limit = r ? SAMPLE_BANK_END : SAMPLE_EFFECTS - SOUND_SILENCE_BYTES;

        for (unsigned i = 0; i < sample_runs[r].count; i++) {
            struct SampleHeader *h = (struct SampleHeader *)((uint8_t *)&xenon2 + a);
            if (limit - a < sizeof *h)
                return -1;
            const uint8_t *src = image + (a - address);
            uint32_t length = be32(src);
            uint16_t rate = be16(src + 4);

            if (length > limit - a - sizeof *h || (length & 1) || !rate)
                return -1;
            if (convert) {
                h->length = length;
                h->rate = rate;
            }
            a += sizeof *h + length;
        }
    }
    return 0;
}

static void samples_save(uint8_t *ram)
{
    for (unsigned r = 0; r < 2; r++) {
        uint32_t a = sample_runs[r].start;

        for (unsigned i = 0; i < sample_runs[r].count; i++) {
            struct SampleHeader h;

            memcpy(&h, ram + a, sizeof h);
            ram[a] = (uint8_t)(h.length >> 24);
            ram[a + 1] = (uint8_t)(h.length >> 16);
            ram[a + 2] = (uint8_t)(h.length >> 8);
            ram[a + 3] = (uint8_t)h.length;
            ram[a + 4] = (uint8_t)(h.rate >> 8);
            ram[a + 5] = (uint8_t)h.rate;
            a += sizeof h + h.length;
        }
    }
}

void guest_load_range(uint32_t address, const uint8_t *image, uint32_t length)
{
    for (unsigned i = 0; i < sizeof guest_members / sizeof guest_members[0]; i++) {
        uint32_t m = guest_members[i].address, size = guest_members[i].size, elem = guest_members[i].elem;

        if (blob_skips(m) || m >= 0x80000)
            continue;
        uint32_t lo = m > address ? m : address, hi = m + size < address + length ? m + size : address + length;

        if (lo >= hi)
            continue;
        /* records and bytes: a member is loaded in one piece; words and longwords: the elements the image covers */
        int partial = m < address || m + size > address + length;

        if (partial && (guest_members[i].rec || elem == 1 || ((lo - m) % elem) || ((hi - lo) % elem)))
            continue;
        if (partial)
            size = hi - lo;
        uint8_t *dst = (uint8_t *)guest_members[i].dst + (lo - m);
        const uint8_t *src = image + (lo - address);

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
    scripts_load(address, length);
    if (address <= SAMPLE_BANK && address + length >= SAMPLE_BANK_END && sample_headers(address, image, 1))
        fprintf(stderr, "xenon2-sdl: invalid sample bank in loaded memory.\n");
}

/* In the raw release, disk 1 from track 82 on is the image at $400. */
#define GAME_IMAGE_SECTOR (82 * 11)   /* disk offset $70C00 */
#define GAME_IMAGE_ADDRESS 0x400
#define GAME_IMAGE_SIZE 0x6b400

int guest_load_game_image(void)
{
    uint8_t sector[512];
    if (files_read_sectors(2, 0, 1, sector) || memcmp(sector + 2, "X2B ", 4)) {
        fprintf(stderr, "xenon2-sdl: disk 2 is missing the Xenon 2 game-disk signature. Check the disk order.\n");
        return -1;
    }

    uint8_t *image = malloc(GAME_IMAGE_SIZE);
    if (!image) {
        fprintf(stderr, "xenon2-sdl: could not allocate the image for disk 1.\n");
        return -1;
    }
    /* $400 starts with BRA.W on both releases. Check the audio headers
     * before interpreting any of the disk bytes as pointers or lengths. */
    int raw = files_read_sectors(1, GAME_IMAGE_SECTOR, GAME_IMAGE_SIZE / 512, image) == 0 &&
              be16(image) == 0x6000;
    if ((!raw && files_read_bs1_game(image, GAME_IMAGE_SIZE)) ||
        be16(image) != 0x6000 || sample_headers(GAME_IMAGE_ADDRESS, image, 0)) {
        free(image);
        fprintf(stderr, "xenon2-sdl: disk 1 has a damaged or unsupported game image. Use the PNA or BS1/Deejay99 disk 1.\n");
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

extern void game_entry(void) __attribute__((weak));   /* game.c: the start-up flow of the original ($400) */

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

void guest_load_high(const uint8_t *bytes)
{
    for (unsigned i = 0; i < sizeof xenon2.beyond_512k / sizeof xenon2.beyond_512k[0]; i++)
        xenon2.beyond_512k[i] = be16(bytes + 2 * i);
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

void guest_set_random(uint32_t a, uint32_t b)
{
    xenon2.random_state_a = a;
    xenon2.random_state_b = b;
}

void guest_stick_restore(void)
{
    if (stick_poked)
        xenon2.joy_stick = stick_saved;
    stick_poked = 0;
}

void host_export_ram_buf(uint8_t *ram)   /* ram: 0x80000 bytes */
{
    memcpy(ram, &xenon2, 0x80000);
    for (unsigned i = 0; i < sizeof guest_members / sizeof guest_members[0]; i++) {
        uint8_t *m = ram + guest_members[i].address;
        uint32_t size = guest_members[i].size, elem = guest_members[i].elem;

        if (blob_skips(guest_members[i].address) || guest_members[i].address >= 0x80000)
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
    scripts_save(ram);
    samples_save(ram);
}

/* the memory as the original had it (big endian, 0x80000 bytes) into the block: the raw bytes, then the named members converted (the dev build starts the port from the
 * original's state this way) */
void host_import_ram_buf(const uint8_t *ram)
{
    presentation_reset();
    memcpy(&xenon2, ram, 0x80000);
    if (blob_raw != 1)   /* (the shop's blob stays while the shop runs) */
        blob_raw = be16(ram + 0xe04) > 1 ? be16(ram + 0xe04) : 0;   /* (level_number: the level) */
    guest_load_range(0, ram, 0x80000);
}
