#include "pkg_e.h"
#include "game_all.h"
#include "amiga/hw.h"

/* inlined at its callers: the pair of list links of an append at the end of a list (before the head) */
static void list_append(Object *head, Object *o)
{
    Object *last = head->prev;

    head->prev = o;
    o->prev = last;
    o->next = head;
    last->next = o;
}

/* orig $006bc8 and $006bce: clears both screens (d0+1 longwords each; $6bc8 d0=$1f3f, $6bce d0=$1dff) */
static void clear_screens(uint16_t d0)
{
    uint32_t *a = (uint32_t *)screen_b;
    uint32_t *b = (uint32_t *)screen_a;
    uint32_t i;

    for (i = 0; i <= d0; i++) {
        if (i < 0x1e00) {
            a[i] = 0;
            b[i] = 0;
        } else {
            panel_b[i - 0x1e00] = 0;
            panel_a[i - 0x1e00] = 0;
        }
    }
}

/* orig $006bc8: clears $1F40 longwords of both screens */
void clear_screens_large(void)
{
    clear_screens(0x1f3f);
}

/* orig $006bce: clears $1E00 longwords of both screens */
void clear_screens_small(void)
{
    clear_screens(0x1dff);
}

/* orig $00668c: fills the score area of both screens at the current player's place with the first glyph rows */
void panel_clear_score(void)
{
    const uint8_t *a2 = digit_gfx;
    uint32_t off = cur_player ? 0x1e : 2;   /* from the panel's start */
    uint8_t *pb = (uint8_t *)panel_b;
    uint8_t *pa = (uint8_t *)panel_a;
    uint16_t first = cur_player ? 0 : 1;
    int row;

    for (row = 0; row < 8; row++) {
        uint16_t d4 = 0;
        do {
            uint8_t d1 = *a2++;
            uint16_t k;

            for (k = first; k < first + 7; k++) {
                pb[off + d4 + k] = d1;
                pa[off + d4 + k] = d1;
            }
            d4 += 0x28;
        } while ((int16_t)d4 <= 0x78);
        off += 0xa0;
    }
}

/* orig $006676: the score area for both players */
void panel_init(void)
{
    panel_clear_score();
    cur_player ^= 1;
    panel_clear_score();
    cur_player ^= 1;
}

/* orig $006766: counts the displayed score up towards the real one by 10, 100 or 1000 and draws the digit that changed */
void update_score_display(void)
{
    int32_t d0 = (int32_t)(v_0c92 - v_0c96);
    uint8_t *digit;
    uint32_t off;   /* from the panel's start */
    uint8_t *pb = (uint8_t *)panel_b;
    uint8_t *pa = (uint8_t *)panel_a;

    if (d0 == 0)
        return;
    if (d0 < 0) {
        v_0c96 = v_0c92;
        return;
    }
    if (d0 >= 0xc8) {
        if (d0 >= 0x7d0) {
            digit = &v_0d7c[3];
            off = 6;
            v_0c96 += 0x3e8;
        } else {
            digit = &v_0d7c[4];
            off = 7;
            v_0c96 += 0x64;
        }
    } else {
        digit = &v_0d7c[5];
        off = 8;
        v_0c96 += 0xa;
    }
    if (cur_player)
        off += 0x1b;
    for (;;) {
        const uint8_t *g;
        int row, k;

        (*digit)++;
        if ((int8_t)*digit >= 10)
            *digit = 0;
        g = digit_gfx + ((uint16_t)*digit << 5);
        for (row = 0; row < 8; row++) {
            for (k = 0; k < 4; k++) {
                uint8_t b = *g++;

                pb[off + row * 0xa0 + k * 0x28] = b;
                pa[off + row * 0xa0 + k * 0x28] = b;
            }
        }
        if (*digit != 0)
            return;
        digit--;
        off--;
        if (digit < v_0d7c)
            return;
    }
}

/* orig $006bea: sets up a level: flags, both screens' panel, the free list of objects, the shot list, the objects */
void init_level_state(void)
{
    uint16_t *a0, *a1;
    const uint16_t *a2 = v_19a9e;
    uint16_t g, c, i;
    Object *o;

    v_0a02 = 0;
    v_0a03[0] = 0xff;
    v_0e06 = 8;
    v_0e02 = 1;
    cur_player = 0;
    v_0a03[1] = (players == 2) ? 0xff : 0;
    v_0a03[2] = 0;
    v_0a03[3] = 0;
    clear_screens_small();
    a0 = (uint16_t *)panel_b;   /* what $6bce leaves in a0 and a1: the panels */
    a1 = (uint16_t *)panel_a;
    for (g = 0; g < 8; g++) {
        for (c = 0; c < 20; c++) {
            uint16_t d1;

            d1 = *a2++; a0[0] = d1; a1[0] = d1;
            d1 = *a2++; a0[20] = d1; a1[20] = d1;
            d1 = *a2++; a0[40] = d1; a1[40] = d1;
            d1 = *a2++; a0[60] = d1; a1[60] = d1;
            a0++;
            a1++;
        }
        a0 += 60;
        a1 += 60;
    }
    v_0a07 = 0;
    o = obj_pool;
    free_obj = o;
    for (i = 0; i < 158; i++) {
        Object *n = o + 1;

        o->type = 0;
        o->next = n;
        o = n;
    }
    o->type = 0;
    o->next = 0;
    for (i = 0; i < 80; i++) {
        shots[i].x = -1;
        shots[i].y = -1;
        shots[i].dx = -1;
        shots[i].dy = -1;
    }
    panel_init();
    init_objects();
    if (v_0a03[1]) {
        swap_player_state();
        init_objects();
        swap_player_state();
    }
}

/* orig $006990: empties the five object lists, puts in the player and its four options, clears the game's counters */
void init_objects(void)
{
    uint16_t i;
    Object *o;

    for (i = 0; i < 5; i++) {
        o = &obj_list[i];
        o->type = 0;
        o->prev = o;
        o->next = o;
        o->fn_update = obj_nop;
        o->fn_draw = obj_nop;
        o->fn_hit = obj_nop;
    }
    o = &player;
    list_append(&obj_list[0], o);
    o->type = 8;
    o->fn_update = player_update;
    o->fn_draw = draw_bob_masked;
    o->gfx = &player_gfx;
    o->anim_delay = 0;
    o->x = 0xa0;
    o->y = 0xb0;
    for (i = 0; i < 4; i++) {
        const uint16_t *rec = &ship_records[i * 12];   /* the record the table at $6CB2 points to */
        Object *h = &obj_list[0];
        Object *first;

        o = obj_free_pop();
        first = h->next;
        h->next = o;
        o->next = first;
        o->prev = h;
        first->prev = o;
        o->type = 0xc4;
        o->fn_update = (void (*)(void))option_update;   /* a0 is the object */
        o->fn_draw = obj_nop;
        o->gfx = (void *)(((uint32_t)rec[0] << 16) | rec[1]);
        o->anim_delay = rec[2];
        o->anim = &rec[3];
        o->x = 0xa0;
        o->y = 0xb0;
        o->v30 = 3 - i;
    }
    for (i = 0; i < 7; i++)
        v_0c66[i * 3] = 0;
    fn_004afc();
    for (i = 0; i < 14; i++)
        v_0caa[i] = 0;
    fn_00352e();
    v_0c60 = 8;
    v_0c62 = 8;
    v_0c64 = 1;
    v_042e = 0;
    v_0c90 = 0;
    v_0430 = 0;
    v_0432 = 0;
    v_0c92 = 0;
    v_0c96 = 0;
    v_0c9e = 0;
    v_0ca0 = 0;
    v_0434 = 0;
    v_0ca2 = 0;
    for (i = 0; i < 7; i++)
        v_0d7c[i] = 0;
    v_0ca4 = 0;
    v_0cc6 = 0x27;
    v_0cc8 = 0;
    v_0d83 = 0;
    v_0d84 = 0;
    v_0cca = 3;
    v_0ccc = 1;
    v_0d85 = 0;
    v_0cf0 = 3;
    fn_00176e();
    v_0a6b = 0xff;
    fn_0016be();
}

/* orig $0068b0: an option's update: follows the player at an offset given by v_0cde, fires when the button says so */
void option_update(Object *a0)
{
    uint16_t d0, d1;
    uint8_t bit;

    fn_002c5a(a0);
    d0 = player.x;
    d1 = (uint16_t)v_0cde;
    if ((int16_t)d1 < 0)
        d1 = (uint16_t)-(int16_t)d1;
    d1 += d1;
    if (a0->v30 & 1)
        d1 += 0xe;
    d0 += (uint16_t)v_6894[d1 / 2];
    a0->x = d0;
    a0->y = player.y;
    a0->fn_draw = obj_nop;
    if (v_0430)
        return;
    bit = (a0->v30 < 2) ? 1 : 2;
    if (v_0a00 & bit)
        a0->fn_draw = draw_bob_masked;
}

/* orig $00615a: makes a shot object ($5850) for the object a0; d0 and d1 go into its $2A and $2E, d2/d3 are offsets from the player;
 * a2 points at its graphics pointer (the next ones when a0's $42 is 1 or more) */
void spawn_shot_object(Object *a0, uint16_t d0, uint16_t d1, uint16_t d2, uint16_t d3, void *const *a2)
{
    Object *a1;

    if (v_0414 < 0)
        v_0414 = 0x89;
    if (a0->v42 != 0) {
        if (a0->v42 != 1)
            a2++;
        a2++;
    }
    a1 = obj_free_pop();
    list_append(&obj_list[3], a1);
    a1->v2a = d0;
    a1->v2e = d1;
    a1->x = (uint16_t)(d2 + player.x);
    a1->y = (uint16_t)(d3 + player.y);
    a1->type = 0x10;
    a1->fn_update = shot_update;
    a1->fn_draw = draw_bob_masked;
    a1->gfx = *a2;
    a1->anim_delay = 0;
    a1->v28 = 0;
    a1->v42 = a0->v42;
}

/* orig $00644e: empties list 1 of its objects and restores its ends and the block at $C66 from their saved copies */
void reset_extra_objects(void)
{
    Object *a1 = obj_list[1].prev;
    uint16_t i;

    while (a1->type) {
        Object *p = a1->prev;

        fn_005c9e(a1);
        a1 = p;
    }
    obj_list[1].prev = v_0cf4;
    obj_list[1].next = v_0cf8;
    for (i = 0; i < 21; i++)
        v_0c66[i] = v_0cfc[i];
}

/* orig $006372: the per-frame counters: the panel's fade, the palette load, the two timers, the autofire counter */
void frame_update(void)
{
    if (v_0430 != 0 || v_0d84 != 0) {
        if (v_0e06 != 0x10)
            v_0e06++;
    } else if (v_0e06 != 0) {
        v_0e06--;
    }
    if (v_0c90 != 0) {
        v_0c90--;
        if (v_0c90 == 0) {
            const uint32_t *a0 = level_palette;
            uint16_t i;

            for (i = 0; i < 8; i++) {
                uint32_t d = a0[i];

                palette_shadow[i * 2] = (uint16_t)(d >> 16);
                palette_shadow[i * 2 + 1] = (uint16_t)d;
                d += d;
                custom->color[i * 2] = (uint16_t)(d >> 16);
                custom->color[i * 2 + 1] = (uint16_t)d;
            }
        }
    }
    if (v_0434 != 0) {
        v_0434--;
        fn_00635e(0x98, 8, v_0434, 0x88);
        if (v_0434 == 0) {
            v_0432 = 0xffff;
            v_0414 = 0x10;
        }
    }
    if (v_0ca2 != 0) {
        v_0ca2--;
        fn_00635e(0x98, 0xa0, v_0ca2, 0xaa);
        if (v_0ca2 == 0)
            reset_extra_objects();
    }
    if (v_0a00 & 0x80) {
        int16_t a = (int16_t)v_0c60;
        int16_t b = (int16_t)v_0c64;

        v_0c60 = (uint16_t)(a - b);
        if (!(a > b)) {
            v_0c60 += v_0c62;
            v_0a01 = 0xff;
        }
    } else {
        v_0c60 = v_0c62;
    }
}

/* orig $0061e6: moves the 80 shot dots and draws them into the screen being drawn */
void update_shots(void)
{
    uint16_t i;

    for (i = 0; i < 80; i++) {
        Shot *s = &shots[i];
        uint16_t d0, d1, x16;
        uint8_t *a1;
        uint32_t d2, d3;
        uint16_t lo2, hi2, lo3, hi3;

        if (s->x < 0)
            continue;
        d0 = (uint16_t)(s->x + s->dx);
        d1 = (uint16_t)(s->y + s->dy);
        if (d0 >= 0x13c || d1 >= 0xbc) {
            s->x = -1;
            continue;
        }
        s->x = d0;
        s->y = d1;
        if (!point_in_enemy(d0, d1)) {
            s->x = -1;
            continue;
        }
        a1 = draw_screen;
        a1 += (int16_t)row_offset[d1];
        x16 = d0 & 0xf;
        a1 += (uint16_t)((d0 ^ x16) >> 3);
#define W(off) (*(uint16_t *)(a1 + (off)))
        if ((int16_t)x16 <= 0xc) {
            lo2 = (uint16_t)(0xe000u >> x16 | 0xe000u << (16 - x16));
            lo3 = (uint16_t)(0x4000u >> x16 | 0x4000u << (16 - x16));
            hi2 = hi3 = 0;
        } else {
            d2 = 0xe000u;
            d3 = 0x4000u;
            d2 = d2 >> x16 | d2 << (32 - x16);
            d3 = d3 >> x16 | d3 << (32 - x16);
            lo2 = (uint16_t)d2;
            hi2 = (uint16_t)(d2 >> 16);
            lo3 = (uint16_t)d3;
            hi3 = (uint16_t)(d3 >> 16);
        }
        W(0) |= lo2;
        W(0x28) |= lo3;
        W(0x50) |= lo2;
        W(0x140) |= lo2;
        W(0x168) |= lo3;
        W(0x190) |= lo2;
        W(0x78) &= (uint16_t)~lo3;
        W(0x1b8) &= (uint16_t)~lo3;
        W(0xa0) |= lo2;
        W(0xc8) |= lo2;
        W(0xf0) |= lo2;
        W(0x118) &= (uint16_t)~lo2;
        if ((int16_t)x16 > 0xc) {
            W(2) |= hi2;
            W(0x2a) |= hi3;
            W(0x52) |= hi2;
            W(0x1e2) |= hi2;
            W(0x20a) |= hi3;
            W(0x232) |= hi2;
            W(0x7a) &= (uint16_t)~hi3;
            W(0x25a) &= (uint16_t)~hi3;
            W(0xa2) |= hi2;
            W(0xca) |= hi2;
            W(0xf2) |= hi2;
            W(0x142) |= hi2;
            W(0x16a) |= hi2;
            W(0x192) |= hi2;
            W(0x11a) &= (uint16_t)~hi2;
            W(0x1ba) &= (uint16_t)~hi2;
        }
#undef W
    }
}

/* orig $00683c: draws the objects of list 2 and counts those that are not of type $50 or $54 */
void draw_enemy_list(void)
{
    Object *a1 = obj_list[2].prev;
    uint16_t d1 = 0;
    uint16_t d2;

    while ((d2 = a1->type) != 0) {
        if (d2 != 0x50 && d2 != 0x54)
            d1++;
        a1 = owner_chain_next(a1);
    }
    v_04e2 = d1;
}

/* orig $006862: puts the objects of a chain (towards the previous ones) back on the free list */
void free_object_chain(Object *a1)
{
    while (a1->type) {
        Object *a2 = a1->prev;
        Object *a4 = a1->next;
        Object *a5 = a1->prev;

        a4->prev = a5;
        a5->next = a4;
        a1->next = free_obj;
        free_obj = a1;
        a1->type = 0;
        a1 = a2;
    }
}

/* orig $006d74: makes an object for each of the 7 queued (type, value) pairs; the value goes into the object's $42 */
void create_queued_objects(void)
{
    uint16_t i;

    for (i = 0; i < 7; i++) {
        uint16_t d1 = v_0caa[i * 2];
        uint16_t d2 = v_0caa[i * 2 + 1];

        if (d1 != 0) {
            Object *a1 = weapon_select(d1);

            a1->v42 = d2;
        }
    }
}

/* orig $006d22: empties the object lists 2 to 4, and the shot dots */
void clear_level_objects(void)
{
    uint16_t d7;
    Object *a1;
    uint16_t i;

    fn_019fe4();
    if (v_0ca2)
        reset_extra_objects();
    d7 = v_0ce6 - v_0cd0;
    fn_005d8c(obj_list[4].prev, d7);
    fn_005d8c(obj_list[3].prev, d7);
    fn_005d8c(obj_list[2].prev, d7);
    for (a1 = obj_list[1].prev; a1->type; a1 = obj_list[1].prev)
        fn_005c9e(a1);
    for (i = 0; i < 80; i++) {
        shots[i].x = -1;
        shots[i].y = -1;
        shots[i].dx = -1;
        shots[i].dy = -1;
    }
}

/* orig $006e70: exchanges the state of the two players: the block at $A76, the player object, the level data */
void swap_player_state(void)
{
    uint32_t *a = v_3ecf4;
    uint32_t *b = (uint32_t *)obj_list;   /* the block runs on over the variables that follow the lists */
    uint8_t *pa = (uint8_t *)&player;
    uint8_t *pb = v_0a08;
    uint32_t *la = level_init_src;
    uint32_t *lb = v_3bcac;
    uint16_t i;

    for (i = 0; i < 202; i++) {
        uint32_t t = a[i];

        a[i] = b[i];
        b[i] = t;
    }
    for (i = 0; i < 98; i++) {
        uint8_t t = pa[i];

        pa[i] = pb[i];
        pb[i] = t;
    }
    for (i = 0; i < 3000; i++) {
        uint32_t t = la[i];

        la[i] = lb[i];
        lb[i] = t;
    }
    cur_player ^= 1;
    fn_054e2c();
}

/* orig $006fe8: all 16 palette entries, the copy and the chip's, to black */
void clear_palette(void)
{
    uint16_t i;

    for (i = 0; i < 16; i++) {
        palette_shadow[i] = 0;
        custom->color[i] = 0;
    }
}

/* orig $006d9c: puts the player back at the start of a level section (scroll positions, the shot trails, the "PLAYER n" text) */
void respawn_player(void)
{
    uint16_t d0, i;

    v_0d84 = 0;
    player.x = v_0ce2;
    player.y = 0xb0;
    d0 = v_0ce6;
    v_0cd0 = d0;
    v_0cea = d0;
    d0++;
    v_0cd2 = d0;
    d0 += 0xbf;
    v_0cd4 = d0;
    v_0cd6 = v_0ce6 & 0xf;
    v_0cce = (uint16_t)((v_0ce6 >> 4) * 0x28);
    create_queued_objects();
    v_0c9a = v_0ca6;
    for (i = 0; i < 8; i++)
        v_043c[i] = 0;
    v_0430 = 0;
    v_042e = 0;
    v_0a6b = 0xff;
    v_0cde = 0;
    for (i = 0; i < 4; i++) {
        v_046c[i * 2] = player.x;
        v_046c[i * 2 + 1] = player.y;
    }
    for (i = 0; i < 17; i++) {
        v_047c[i * 3] = v_0cd0;
        v_047c[i * 3 + 1] = player.x;
        v_047c[i * 3 + 2] = player.y;
    }
    v_8836[18] = (uint8_t)(cur_player + 0x31);
    v_8428 = 0xff;
    fn_00842a(v_8836);
    v_8428 = 0;
    clear_palette();
    if (v_0ca2)
        fn_004a0a();
}

/* orig $006ec6: after a death: clears the level objects and, if the other player is in, gives the turn to him */
void switch_player(void)
{
    uint16_t d0;

    clear_level_objects();
    d0 = cur_player;
    if (v_0a03[d0]) {
        d0 ^= 1;
        if (v_0a03[d0]) {
            if (v_0a03[2 + d0] == 0)
                swap_player_state();
        }
    } else {
        swap_player_state();
        d0 = cur_player;
        if (v_0a03[2 + d0]) {
            v_0a07 = 0xff;
            if (v_0e04 == 5)
                v_0a6c = 0xff;
        }
    }
    respawn_player();
    v_0e06 = 8;
}

/* orig $006f1e: the scroll and timing variables at the start of a level */
void level_reset_scroll(void)
{
    v_0cce = 0x2d00;
    v_0cd6 = 0;
    v_0cd0 = 0x1200;
    v_0cd2 = 0x7fee;
    v_0cd4 = 0x7fee;
    v_0cee = 0x10;
    v_0ce2 = player.x;
    v_0ce4 = player.y;
    v_0ce6 = v_0cd0;
    v_0ca6 = v_0c9a;
    v_0cd8 = 1;
    v_0cda = 0;
    v_0cde = 0;
    fn_054e20();
    v_0ce0 = 0;
    v_042e = 0;
    v_0cdc = 0;
    v_0c9a = 0;
    v_0ca6 = 0;
    v_0ca4 = 0;
    v_0cec = 1;
    v_0cf2 = 0;
}

/* orig $006f94: copies the level's initial data in and resets the start state of each player */
void level_start(void)
{
    const uint32_t *a0 = level_init_src;
    uint16_t i;

    for (i = 0; i < 3000; i++)
        v_3bcac[i] = a0[i];
    v_0436 = 0;
    v_04e2 = 0;
    v_043a = 0;
    for (i = 0; i < 8; i++)
        v_043c[i] = 0;
    v_0a6c = 0;
    v_0a00 = 0;
    v_0a01 = 0;
    level_reset_scroll();
    if (players == 2) {
        swap_player_state();
        level_reset_scroll();
        swap_player_state();
    }
}
