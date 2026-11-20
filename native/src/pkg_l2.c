/* pkg_l2: level 2's own code (the blob at $54E00 while level 2 is loaded).  The blob is raw big-endian memory read and written with
 * sh_r16 and the like at the addresses it has in the original (guest_blob_raw 2: level 1's members beyond the header do not describe it);
 * a function has `level2 $ADDR` for its address (not `orig $`: level 1's code is at the same addresses).  The addresses of code with no C
 * yet resolve to a function that does nothing (guest_missing_code); the ones this file names itself go through GFN(). */
#include "pkg_d.h"
#include "pkg_i.h"
#include "game_all.h"

#define FN(f) ((obj_fn)(f))
#define W(a) sh_r16(a)
#define B(a) sh_r8(a)
#define PLAYER_X (PLAYER->x)
#define PLAYER_Y (PLAYER->y)

/* the object from the free list put last in the list whose sentinel is `list` */
static Object *alloc_in(Object *list)
{
    Object *o = obj_free_pop(), *tail = list->prev;

    list->prev = o;
    o->prev = tail;
    o->next = list;
    tail->next = o;
    return o;
}

static void kill_thunk(Object *o)
{
    o->type = 4;
    o->fn_update = (obj_fn)free_object;
    o->fn_draw = (obj_fn)obj_nop_thunk;
    o->fn_hit = (void *)obj_nop_thunk;
    o->flag34 = 0;
    o->flag60 = 0;
}

static void copy2(uint8_t *dst, const uint8_t *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
}

static void copy4(uint8_t *dst, const uint8_t *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}

/* the animation header at an address of the blob */
static void set_anim(Object *o, uint32_t address)
{
    anim_header_set(o, (const uint8_t *)GPTR(address));
}

static uint32_t ror5(uint16_t *pair)
{
    uint32_t l = ((uint32_t)pair[0] << 16) | pair[1];

    l = (l >> 5) | (l << 27);
    pair[0] = (uint16_t)(l >> 16);
    pair[1] = (uint16_t)l;
    return l;
}

/* ---- the entries the engine calls ($54E0C..$54E2C) ---- */

/* level2 $054E0C, $056F54: a box (x0, y0)-(x1, y1) meets one of the blocks that break (level_buf: four groups by the quarter of the
 * screen the box is in, a count and entries of state, x, y, the cell that was there); returns level_obj with the entry in its $16, or 0 */
Object *l2_collision_hook(int16_t d0, int16_t d1, int16_t d2, int16_t d3)
{
    const uint8_t *a1;
    uint16_t n;
    uint32_t d4 = 0;

    d1 = (int16_t)(d1 + v_0cd0);
    d3 = (int16_t)(d3 + v_0cd0);
    if (d1 >= 0x1b0)
        return 0;
    if (d1 <= 0x80)
        d4 |= 4;
    if (d0 <= 0x80)
        d4 |= 8;
    a1 = level_buf + sh_r32(0x56f24 + d4);
    n = rd16(a1);
    a1 += 2;
    for (; n != 0; n--, a1 += 8) {
        int16_t x, y;

        if (rd16(a1) == 0)
            continue;
        x = (int16_t)rd16(a1 + 2);
        if (x > d2)
            return 0;
        y = (int16_t)rd16(a1 + 4);
        if (box_overlap(d0, d1, d2, d3, x, y, (int16_t)(x + 0xf), (int16_t)(y + 0xf))) {
            level_obj.gfx = a1;
            return &level_obj;
        }
    }
    return 0;
}

/* level2 $056FA8: the hit routine of level_obj: the block is broken (its entry's state 0, its cell of the map cleared) and bursts */
void l2_block_hit(Object *a0, Object *a1, uint16_t d0)
{
    uint8_t *e = (uint8_t *)level_obj.gfx;
    uint16_t x = rd16(e + 2), y = rd16(e + 4);
    uint16_t cell;

    wr16(e, 0);
    cell = (uint16_t)((uint16_t)(y >> 3) * 0x14 + (x >> 3));
    wr16(level_init_src + cell, 0);
    spawn_effect((int16_t)(x + 8), (int16_t)(y - v_0cd0 + 8), anim_270e, draw_bob_masked);
}

/* level2 $054E10, $055D8A: level events by d0 (a1 = the record that fired): d0 0 is the wave of $326C, 1 to 6 formations */
void l2_event(uint16_t d0, const uint8_t *a1)
{
    static const uint32_t forms[6] = { 0x54e82, 0x54e9c, 0x54eb6, 0x54ed0, 0x54eea, 0x54f04 };

    if (d0 == 0) {
        spawn_wave_326c(0, a1);   /* $E5A */
        return;
    }
    if (d0 <= 6)
        spawn_formation(0, a1, (const uint8_t *)GPTR(forms[d0 - 1]));   /* $E60 */
}

/* level2 $054E18, $055B62: level hooks by d0 (a1 = the record that fired, d2 its scroll value; w0c of the record says which side): 0 remembers the record,
 * 1 a turret $E8/$EC, 2 a marker $F0/$F4, 3 a launcher $100/$104, 4 and 5 markers $FC and $F8 */
void l2_hook(uint16_t d0, const uint8_t *a1, uint16_t d2)
{
    int flag = rd16(a1 + 0xc) != 0;
    Object *a0;

    switch (d0) {
    case 0:
        note_spawn_record(a1, d2);   /* $EA2 */
        return;
    case 1:
        a0 = spawn_marker_enemy(flag ? 0xe8 : 0xec, a1, (void (*)(void))l2_turret_update);
        a0->fn_hit = (void *)hit_tile;
        a0->energy = (int16_t)W(0x54e4e);
        a0->hit[0] = 0x3e8;
        a0->hit[2] = 0x3e8;
        a0->gfx = GPTR(flag ? 0x559c8 : 0x559e6);
        a0->anim = (const uint8_t *)GPTR(flag ? 0x559c4 : 0x559c6);
        copy2(a0->v46, (const uint8_t *)a0->gfx);
        a0->v42 = 0x64;
        a0->v3e = 0;
        a0->v40 = 0;
        return;
    case 2:
        a0 = spawn_marker_front(flag ? 0xf0 : 0xf4, a1, (void (*)(void))l2_front_update);
        copy4(a0->v46, (const uint8_t *)GPTR(flag ? 0x55890 : 0x558b8));
        copy4(a0->v46 + 0x28, (const uint8_t *)GPTR(flag ? 0x55894 : 0x558bc));
        return;
    case 3:
        a0 = alloc_in(LIST_C);
        a0->type = flag ? 0x100 : 0x104;
        a0->fn_update = FN(l2_launcher_update);
        a0->fn_draw = draw_bob_masked;
        set_anim(a0, flag ? 0x55d12 : 0x55cd0);
        SET_V5E(a0, 0);
        a0->fn_hit = (void *)hit_enemy;
        a0->energy = (int16_t)W(0x54e50);
        a0->flag35 = 0;
        a0->v42 = 0xc8;
        a0->x = (int16_t)(rd16(a1 + 8) - 8);
        a0->y = (int16_t)(rd16(a1 + 10) - 8 - v_0cd0);
        a0->v28 = -1;
        a0->v2a = (int16_t)(rd16(a1 + 4) << 4);
        a0->v30 = a0->v2a;
        a0->v44 = 0;
        bbox_from_frame(a0);
        return;
    case 4:
        a0 = spawn_marker_front(0xfc, a1, (void (*)(void))l2_marker_fc_update);
        a0->v30 = 1;
        copy2(a0->v46, (const uint8_t *)GPTR(0x5567a));
        return;
    case 5:
        a0 = spawn_marker_front(0xf8, a1, (void (*)(void))l2_marker_f8_update);
        a0->v30 = 1;
        return;
    }
}

/* level2 $054E24, $055B1E: the level's scroll events: the walls of debris while the first fight is not won, the stop of the scroll while it is */
void l2_scroll(void)
{
    if (v_0d90 == 0) {
        if ((int16_t)v_0cd0 >= 0x9d0 && (int16_t)v_0cd0 <= 0xb50)
            l2_debris_walls();
    } else if ((int16_t)v_0cd0 > 0x9f0) {
        v_0cd0 = 0x9f0;
        v_0cea = 0x9f0;
        v_0cce = 0x18d8;
        v_0cd6 = 0;
    }
    v_0d8d = 0;
    v_0d8e = 0;
}

/* level2 $054E2C, $054E68: a change of players: the table of blocks and the second player's copy exchange */
void l2_swap_hook(void)
{
    uint8_t *a0 = level_buf, *a1 = (uint8_t *)GPTR(0x3eb8c);
    int i;

    for (i = 0; i < 0xb4; i++, a0 += 2, a1 += 2) {
        uint8_t t0 = a0[0], t1 = a0[1];

        a0[0] = a1[0];
        a0[1] = a1[1];
        a1[0] = t0;
        a1[1] = t1;
    }
}

/* level2 $054E20, $05705E: level 2 starts: its data is copied once (and the table of blocks), the boss, the three gates and the walls' state are made */
void l2_init(void)
{
    static const uint16_t gate_at[3][2] = { { 9, 0xb8 }, { 4, 0xa4 }, { 0xe, 0xa4 } };
    static const uint16_t groups[4] = { 0, 0x62, 0xc4, 0x116 };
    Object *a0;
    int i, d7;

    if (level_init_src == (uint8_t *)GPTR(0x71a12)) {
        for (i = 11999; i >= 0; i--)
            ((uint8_t *)GPTR(0x7d120))[i] = ((const uint8_t *)GPTR(0x71a12))[i];
        level_init_src = (uint8_t *)GPTR(0x7d120);
        for (i = 359; i >= 0; i--)
            ((uint8_t *)GPTR(0x7d120 - 360))[i] = ((const uint8_t *)GPTR(0x71a12 - 360))[i];
        level_buf = (uint8_t *)GPTR(0x7d120 - 360);
    }
    v_0ce8 = 0x9e0;
    v_0cea = 0x11ff;
    v_0d90 = 0;
    a0 = alloc_in(LIST_C);
    a0->type = 0x50;
    a0->fn_update = FN(l2_boss_update);
    a0->fn_draw = FN(l2_boss_draw);
    a0->fn_hit = (void *)l2_boss_hit;
    SET_V5E(a0, 0);
    a0->flag60 = -1;
    a0->flag61 = -1;
    a0->y = 0x60;
    a0->energy = (int16_t)W(0x54e5e);
    a0->v28 = -1;
    a0->v2e = 0;
    a0->v3e = 0;
    a0->v30 = 0;
    a0->v44 = 0;
    for (d7 = 2; d7 >= 0; d7--) {
        uint16_t x = gate_at[2 - d7][0], y = gate_at[2 - d7][1];

        a0 = alloc_in(LIST_C);
        a0->type = 0x54;
        a0->fn_update = FN(l2_gate_update);
        a0->fn_draw = (obj_fn)obj_nop_thunk;
        a0->fn_hit = (void *)l2_gate_hit;
        a0->energy = (int16_t)W(0x54e60);
        a0->flag60 = -1;
        a0->flag61 = -1;
        SET_V5E(a0, 0);
        a0->x = (int16_t)x;
        a0->y = (int16_t)y;
        a0->gfx = level_init_src + (uint16_t)(((uint16_t)(y * 0x14) + x) * 2);
        a0->v28 = 0;
        a0->v30 = (int16_t)d7;
    }
    v_0d5c = 3;
    v_0d5e = 0;
    sh_w8(0xd5f, 0);
    v_0d8d = 0;
    v_0d8e = 0;
    v_0d8f = 2;
    for (i = 0; i < 8; i++)
        v_0d26[i] = 0;
    for (i = 0; i < 4; i++) {
        uint8_t *g = level_buf + groups[i];
        uint16_t n = rd16(g);

        for (g += 2; n != 0; n--, g += 8)
            wr16(g, 1);
    }
    level_obj.type = 0;
    level_obj.prev = &level_obj;
    level_obj.next = &level_obj;
    level_obj.fn_update = (obj_fn)obj_nop_thunk;
    level_obj.fn_draw = (obj_fn)obj_nop_thunk;
    level_obj.fn_hit = (void *)l2_block_hit;
    v_0d26[16] = 0x45d2;
    v_0d26[17] = 0x72ba;
    v_0d26[18] = 0xaa6a;
    v_0d26[19] = 0x4a56;
    v_0424 = (uint16_t)(v_0e02 * 1);
    v_0426 = (uint16_t)(v_0e02 * 2);
    for (i = 0; i < 0xb4; i++) {
        ((uint8_t *)GPTR(0x3eb8c))[2 * i] = level_buf[2 * i];
        ((uint8_t *)GPTR(0x3eb8c))[2 * i + 1] = level_buf[2 * i + 1];
    }
}

/* ---- the turrets, launchers and what the hooks put into the level ---- */

/* level2 $056596: a piece of the marker $F0 / $F4: every 16 steps it turns towards the player (eight directions), moves by the table,
 * and on the player's box hurts him; after 80 steps (v28), or when it leaves the screen, it is gone, and in the first case, or when the
 * player is shielded, it bursts */
void l2_front_piece_update(Object *a0)
{
    static const int16_t dx[8] = { -4, -5, -4, 0, 4, 5, 4, 0 };
    static const int16_t dy[8] = { -4, 0, 4, 5, 4, 0, -4, -5 };
    int burst = 0;

    anim_step(a0);
    a0->v28++;
    if (a0->v28 >= 0x50) {
        burst = 1;
    } else {
        int16_t x, y;

        if ((a0->v28 & 0xf) == 0) {
            uint16_t d = direction8((int16_t)(PLAYER_X - a0->x), (int16_t)(PLAYER_Y - a0->y));

            a0->v2a = (int16_t)(~d & 7);
            set_anim(a0, sh_r32(0x56576 + ((uint16_t)a0->v2a << 2)));
        }
        x = (int16_t)(dx[a0->v2a] + a0->x);
        if ((uint16_t)x >= 0x140) {
            kill_thunk(a0);
            return;
        }
        a0->x = x;
        y = (int16_t)(dy[a0->v2a] + a0->y + v_0cd8);
        if ((uint16_t)y >= 0xc0) {
            kill_thunk(a0);
            return;
        }
        a0->y = y;
        bbox_from_frame(a0);
        if (!point_in_player_box(x, y))
            return;
        if (v_0cc8 != 0) {
            burst = 1;
        } else {
            PLAYER->fn_draw = draw_bob_or_first;
            player_damage(a0, 4);
            kill_thunk(a0);
            return;
        }
    }
    if (burst) {
        const uint8_t *f = (const uint8_t *)a0->gfx;
        int16_t ex = (int16_t)(((int16_t)(a0->x - (int16_t)rd16(f)) * 2 + (int16_t)rd16(f + 4)) >> 1);
        int16_t ey = (int16_t)(((int16_t)(a0->y - (int16_t)rd16(f + 2)) * 2 + (int16_t)rd16(f + 6)) >> 1);

        spawn_effect(ex, ey, anim_270e, draw_bob_masked);
    }
    kill_thunk(a0);
}

/* level2 $056144: the flying enemy $E0 / $E4: it comes down the screen to y $32, drifts towards the player's x and, within 10 of it,
 * dives (v28 1) and rises again; the offsets are from a table indexed by the frame and the object's address */
void l2_flyer_update(Object *a0)
{
    static const int16_t wobble[8] = { 0, -2, -4, -2, 0, 2, 4, 2 };
    uint16_t phase = (uint16_t)(v_0cdc + (uint16_t)GADDR(a0));
    int16_t d0;

    anim_step(a0);
    if (a0->v28 == 0) {
        if (a0->y > 0x32) {
            a0->y -= 5;
            a0->x = (int16_t)(a0->x + wobble[(phase >> 2) & 7]);
            bbox_from_frame(a0);
            return;
        }
        d0 = (int16_t)(a0->x - PLAYER_X);
        if (d0 < 0)
            d0 = (int16_t)-d0;
        if (d0 < 10) {
            a0->v28 = 1;
            set_anim(a0, a0->type == 0xe0 ? 0x55032 : 0x55050);
            bbox_from_frame(a0);
            return;
        }
        a0->x = (int16_t)(a0->x - (a0->x >= PLAYER_X ? 4 : -4));
        d0 = (int16_t)(wobble[(phase & 0xe) >> 1] + a0->y);
        if (d0 > 0x32)
            d0 = 0x32;
        a0->y = d0;
        bbox_from_frame(a0);
        return;
    }
    a0->y += 0xc;
    if (a0->y >= 0xa0) {
        a0->v28 = 0;
        set_anim(a0, a0->type == 0xe0 ? 0x5501a : 0x5504a);
    }
    bbox_from_frame(a0);
}

/* level2 $05533E: the marker $F8: as $05550E, but its picture blinks (v28 0 / 1) until the scroll is near, its steps use a table of
 * two longs, and at step 4 it launches the flyer $E0 */
void l2_marker_f8_update(Object *a0)
{
    int16_t d0;
    const uint8_t *a2;

    if ((int16_t)((int16_t)v_0cea + 0xd0) <= a0->y) {
        kill_thunk(a0);
        return;
    }
    d0 = (int16_t)(a0->y - v_0cd0);
    if (d0 >= 0xb4)
        return;
    if (a0->v28 >= 5) {
        if (a0->v30 == 0) {
            kill_thunk(a0);
            return;
        }
        if (a0->v28 >= 9) {
            a0->v28 = 0;
            a0->v30--;
        }
    }
    if ((uint16_t)a0->v28 <= 1) {
        if (d0 >= 0x64)
            a0->v28 = 2;
        else if ((v_0cdc & 3) == 0)
            a0->v28 ^= 1;
    }
    a2 = (const uint8_t *)GPTR(0x554c6 + ((uint16_t)a0->v28 << 3));
    copy4(a0->v46, a2);
    copy4(a0->v46 + 0x28, a2 + 4);
    if ((uint16_t)a0->v28 < 2)
        return;
    if (v_0cdc & 3)
        return;
    a0->v28++;
    if (a0->v28 != 4)
        return;
    {
        Object *s = alloc_in(LIST_C);

        s->type = 0xe0;
        s->fn_update = (obj_fn)l2_flyer_update;
        s->fn_draw = draw_bob_masked;
        s->fn_hit = (void *)hit_enemy;
        set_anim(s, 0x5501a);
        SET_V5E(s, 0);
        s->x = (int16_t)(a0->x + 0x10);
        s->y = (int16_t)(a0->y - v_0cd0 + 0x10);
        s->energy = (int16_t)W(0x54e42);
        s->flag35 = (int8_t)0xff;
        s->v42 = 0x12c;
        s->v28 = 0;
        s->v44 = 0;
        bbox_from_frame(s);
    }
}

/* level2 $05550E: the marker $FC: gone when the scroll has passed it; its picture steps (v28) and at step 4 it launches the flyer $E4;
 * v30 counts the launches left */
void l2_marker_fc_update(Object *a0)
{
    int16_t d0;

    if ((int16_t)((int16_t)v_0cea + 0xd0) <= a0->y) {
        kill_thunk(a0);
        return;
    }
    d0 = (int16_t)(a0->y - v_0cd0);
    if (d0 >= 0xb4)
        return;
    if (a0->v28 >= 5) {
        if (a0->v30 == 0) {
            kill_thunk(a0);
            return;
        }
        if (a0->v28 >= 9) {
            a0->v28 = 0;
            a0->v30--;
        }
    }
    if ((uint16_t)a0->v28 <= 1) {
        if (d0 < 0x64)
            return;
        a0->v28 = 2;
    }
    copy2(a0->v46, (const uint8_t *)GPTR(0x5567a + ((uint16_t)a0->v28 << 1)));
    if (v_0cdc & 3)
        return;
    a0->v28++;
    if (a0->v28 != 4)
        return;
    {
        Object *s = alloc_in(LIST_C);

        s->type = 0xe4;
        s->fn_update = (obj_fn)l2_flyer_update;
        s->fn_draw = draw_bob_masked;
        s->fn_hit = (void *)hit_enemy;
        set_anim(s, 0x5504a);
        SET_V5E(s, 0);
        s->x = a0->x;
        s->y = (int16_t)(a0->y - v_0cd0);
        s->energy = (int16_t)W(0x54e44);
        s->flag35 = 0;
        s->v42 = 0x96;
        s->v28 = 0;
        s->v44 = 0;
        bbox_from_frame(s);
    }
}

/* level2 $056FEC: the first block that is broken (its entry's state 0) and meets the box of a0 comes back: state 1, its cell of the map
 * written again */
static void l2_restore_blocks(Object *a0)
{
    int16_t d0 = a0->hit[0], d1 = (int16_t)(a0->hit[1] + v_0cd0), d2 = a0->hit[2], d3 = (int16_t)(a0->hit[3] + v_0cd0);
    uint32_t d4 = 0;
    const uint8_t *a1;
    uint16_t n;

    if (d1 <= 0x80)
        d4 |= 4;
    if (d0 <= 0x80)
        d4 |= 8;
    a1 = level_buf + sh_r32(0x56f24 + d4);
    n = rd16(a1);
    a1 += 2;
    for (; n != 0; n--, a1 += 8) {
        uint8_t *e = (uint8_t *)a1;
        int16_t x, y;

        if (rd16(e) != 0)
            continue;
        x = (int16_t)rd16(e + 2);
        y = (int16_t)rd16(e + 4);
        if (!box_overlap(d0, d1, d2, d3, x, y, (int16_t)(x + 0xf), (int16_t)(y + 0xf)))
            continue;
        wr16(e, 1);
        wr16(level_init_src + (uint16_t)(((uint16_t)y >> 3) * 0x14 + ((uint16_t)x >> 3)), rd16(e + 6));
        return;
    }
}

/* level2 $0563E0: the creature's bolt ($110): it turns (v2a, eight directions) towards v28, which is -1 when it flies on; it shoots
 * every so often, moves one step by the table, and at the corners of the table of $056524 takes a new direction; it breaks blocks back
 * into place */
void l2_roamer_shot_update(Object *a0)
{
    static const int16_t dx[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    static const int16_t dy[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
    int16_t x, y, d0;
    uint8_t carry;
    int n;

    anim_step(a0);
    a0->y = (int16_t)(a0->y + v_0cd8);
    if (a0->v28 >= 0) {
        if (!(v_0cdc & 1)) {
            bbox_from_frame(a0);
            return;
        }
        d0 = (int16_t)(a0->v28 - a0->v2a);
        if (d0 != 0) {
            if (d0 < 0)
                d0 += 8;
            a0->v2a = (int16_t)((a0->v2a + (d0 > 4 ? -1 : 1)) & 7);
            set_anim(a0, sh_r32(0x563c0 + ((uint16_t)a0->v2a << 2)));
            bbox_from_frame(a0);
            return;
        }
        a0->v28 = -1;
    }
    if ((uint16_t)a0->y < 0xc0) {
        uint16_t sum = (uint16_t)(a0->v5e_b[0] + B(0x54e59));

        a0->v5e_b[0] = (uint8_t)sum;
        carry = sum > 0xff;
        if (carry) {
            uint16_t d3 = W(0x54e5a);
            uint16_t angle;
            const void *gfx;

            a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
            gfx = GPTR(sh_r32(sh_r32(0x6c89c + (uint32_t)d3 * 4)));
            angle = direction8((int16_t)(PLAYER_X - a0->x), (int16_t)(PLAYER_Y - a0->y));
            spawn_enemy_shot((uint16_t)a0->x, (uint16_t)a0->y, angle, W(0x54e5c), gfx);
        }
    }
    x = (int16_t)(a0->x + dx[a0->v2a]);
    y = (int16_t)(a0->y + dy[a0->v2a]);
    a0->x = x;
    a0->y = y;
    y = (int16_t)(y + v_0cd0);
    for (n = 0; n < 8; n++) {
        uint32_t entry = 0x56526 + (uint32_t)n * 10;

        if ((int16_t)W(entry) == x && (int16_t)W(entry + 2) == y) {
            uint16_t r;

            do
                r = (uint16_t)(random_next() & 3);
            while (r == 0);
            a0->v28 = (int16_t)W(entry + 4 + (uint32_t)(r - 1) * 2);
            bbox_from_frame(a0);
            return;
        }
    }
    bbox_from_frame(a0);
    l2_restore_blocks(a0);
}

/* level2 $05623E: the creature that crosses the screen between the walls (v2a its direction, v28 0 moving, 1 at a wall, 2 shooting): at the
 * frame $5B5D6 of its animation it launches the bolt $110 in its direction and is gone */
void l2_roamer_update(Object *a0)
{
    static const int16_t dx[8] = { 0, 3, 4, 3, 0, -3, -4, -3 };
    static const int16_t dy[8] = { -4, -3, 0, 3, 4, 3, 0, -3 };

    anim_step(a0);
    a0->y = (int16_t)(a0->y + v_0cd8);
    if (a0->v28 == 0) {
        int16_t x = (int16_t)(a0->x + dx[a0->v2a]);
        int16_t y = (int16_t)(a0->y + dy[a0->v2a]);

        if (x <= 0x74) {
            x = 0x74;
            a0->v28 = 1;
            a0->v2a = 4;
        }
        if (x >= 0xcc) {
            x = 0xcc;
            a0->v28 = 1;
            a0->v2a = 0;
        }
        y = (int16_t)(y + v_0cd0);
        if (y <= 0x6c) {
            y = 0x6c;
            a0->v28 = 1;
            a0->v2a = 6;
        }
        if (y >= 0x154) {
            y = 0x154;
            a0->v28 = 1;
            a0->v2a = 2;
        }
        a0->x = x;
        a0->y = (int16_t)(y - v_0cd0);
        return;
    }
    if (a0->v28 == 1) {
        if ((random_next() & 7) == 0) {
            a0->v28 = 2;
            set_anim(a0, 0x5513a);
        }
        return;
    }
    if (a0->gfx == GPTR(0x5b5d6)) {
        Object *s = alloc_in(LIST_C);

        s->type = 0x110;
        s->fn_update = (obj_fn)l2_roamer_shot_update;
        s->fn_draw = draw_bob_masked;
        s->fn_hit = (void *)hit_enemy;
        s->energy = (int16_t)W(0x54e56);
        s->flag35 = 0;
        SET_V5E(s, 0);
        s->v42 = 0x1e;
        s->v44 = 0;
        set_anim(s, sh_r32(0x563c0 + ((uint16_t)a0->v2a << 2)));
        s->x = a0->x;
        s->y = a0->y;
        s->v2a = a0->v2a;
        s->v28 = -1;
        bbox_from_frame(s);
        kill_thunk(a0);
    }
}

/* level2 $05568C: the marker $F0 / $F4: waits for the scroll, then its picture takes 20 steps (v28) from a table, and it bursts into
 * eight pieces ($108, update $056596) */
void l2_front_update(Object *a0)
{
    int i;

    if (a0->v28 == 0) {
        if ((int16_t)(a0->y - v_0cd0) >= (int16_t)W(0x54e62))
            a0->v28 = 1;
        return;
    }
    {
        uint32_t table = a0->type == 0xf0 ? 0x557e8 : 0x5583c;
        const uint8_t *a2 = (const uint8_t *)GPTR(sh_r32(table + ((uint16_t)a0->v28 << 2)));

        copy4(a0->v46, a2);
        copy4(a0->v46 + 0x28, a2 + 4);
    }
    a0->v28++;
    if (a0->v28 < 0x15)
        return;
    for (i = 7; i >= 0; i--) {
        uint32_t entry = 0x557a8 + (uint32_t)(7 - i) * 8;
        Object *s = alloc_in(LIST_C);

        s->type = 0x108;
        s->fn_update = (obj_fn)l2_front_piece_update;
        s->fn_draw = draw_bob_masked;
        s->fn_hit = (void *)hit_explode;
        set_anim(s, sh_r32(entry));
        SET_V5E(s, 0);
        s->x = (int16_t)(a0->x + 0x10 + (int16_t)W(entry + 4));
        s->y = (int16_t)(a0->y - v_0cd0 + 0x10 + (int16_t)W(entry + 6));
        s->v2a = (int16_t)i;
        s->energy = 1;
        s->flag35 = 0;
        s->v42 = 0x14;
        s->v28 = (int16_t)(random_next() & 0x1f);
        s->v44 = 0;
        bbox_from_frame(s);
    }
    kill_thunk(a0);
}


/* level2 $0558E0: update of the turret $E8 / $EC: its box, the cells it opens and closes (v28 is the step), and at step 7 a shot */
void l2_turret_update(Object *a0)
{
    int16_t d0, d1, d2;
    uint32_t pic;

    if ((int16_t)(v_0cea + 0xd0) < a0->y) {
        kill_thunk(a0);
        return;
    }
    d0 = (int16_t)(a0->x - 8);
    a0->hit[0] = d0;
    a0->hit[2] = (int16_t)(d0 + 0x14);
    d0 = (int16_t)(a0->y - v_0cd0 + 4);
    a0->hit[1] = d0;
    a0->hit[3] = (int16_t)(d0 + 8);
    if (a0->v28 == 0) {
        if ((uint8_t)random_next() < B(0x54e47))
            a0->v28 = 1;
        return;
    }
    copy2(a0->v46, (const uint8_t *)GPTR((a0->type == 0xe8 ? 0x559c8 : 0x559e6) + 2 * (uint16_t)a0->v28));
    a0->v28++;
    if (a0->v28 != 7) {
        if (a0->v28 == 0xf)
            a0->v28 = 0;
        return;
    }
    d0 = (int16_t)(a0->x + 8);
    d1 = (int16_t)(a0->y - v_0cd0 + 8);
    pic = sh_r32(sh_r32(0x6c89c + 4 * (uint32_t)W(0x54e48)));
    d2 = a0->type == 0xec ? 2 : 6;
    spawn_enemy_shot((uint16_t)d0, (uint16_t)d1, (uint16_t)d2, W(0x54e4a), GPTR(pic));
}

/* level2 $055A04: update of the launcher $100 / $104: it moves up and down (v28 is its step, v30 its place between 0 and v2a); when the
 * player is level with it (now and then) it goes into list B and fires a shot */
void l2_launcher_update(Object *a0)
{
    int16_t d0, d1, d2;

    if ((int16_t)(v_0cea + 0xd0 - v_0cd0) < a0->y) {
        kill_thunk(a0);
        return;
    }
    anim_step(a0);
    a0->y = (int16_t)(a0->y + v_0cd8);
    if (a0->v28 == 0)
        return;
    d0 = (int16_t)(a0->y - PLAYER_Y);
    if (d0 <= 0xa && d0 >= -0x1e && (uint8_t)random_next() < 0x1e) {
        Object *tail;

        a0->v28 = 0;
        a0->next->prev = a0->prev;
        a0->prev->next = a0->next;
        tail = LIST_B->prev;
        LIST_B->prev = a0;
        a0->prev = tail;
        a0->next = LIST_B;
        tail->next = a0;
        set_anim(a0, a0->type == 0x100 ? 0x55d42 : 0x55d00);
        d0 = (int16_t)(a0->x + 0xb);
        d1 = (int16_t)(a0->y + 7);
        d2 = 6;
        if (a0->type != 0x100) {
            d2 = 2;
            d0 = (int16_t)(d0 - 8);
        }
        spawn_enemy_shot((uint16_t)d0, (uint16_t)d1, (uint16_t)d2, W(0x54e4c), GPTR(0x5ccf6));
        return;
    }
    d0 = a0->v28;
    a0->y = (int16_t)(a0->y + d0);
    a0->v30 = (int16_t)(a0->v30 + d0);
    if (a0->v30 == 0 || a0->v30 == a0->v2a)
        a0->v28 = (int16_t)-a0->v28;
    bbox_from_frame(a0);
}

/* the end of $055DEC, $055E0E and $055E28: the object a1 bursts into a wreck (animation a2) that flies away, the object goes */
static void l2_wreck(Object *a1, uint32_t a2)
{
    Object *n = alloc_in(LIST_B);

    n->type = a1->type;
    n->fn_update = a1->fn_update;
    n->fn_draw = draw_bob_masked;
    n->x = a1->x;
    n->y = a1->y;
    set_anim(n, a2);
    n->v28 = (int16_t)0x8000;
    n->v2a = 0;
    n->v2c = (int16_t)(random_next() & 0xff);
    kill_thunk(a1);
}

/* level2 $055DEC: hit routine of a piece that lights a bit of the gates' state when it goes */
void l2_hit_a(Object *a0, Object *a1, uint16_t d0)
{
    a1->fn_draw = draw_bob_or_first;
    if ((int16_t)d0 < a1->energy) {
        hit_enemy(a0, a1, d0);
        return;
    }
    v_0d5e &= (uint8_t)~(1 << (a1->v30 & 7));
    l2_wreck(a1, 0x55062);
}

/* level2 $055E0E: hit routine of a piece */
void l2_hit_b(Object *a0, Object *a1, uint16_t d0)
{
    a1->fn_draw = draw_bob_or_first;
    if ((int16_t)d0 < a1->energy) {
        hit_enemy(a0, a1, d0);
        return;
    }
    l2_wreck(a1, 0x5509e);
}

/* level2 $055E28: hit routine of a piece */
void l2_hit_c(Object *a0, Object *a1, uint16_t d0)
{
    a1->fn_draw = draw_bob_or_first;
    if ((int16_t)d0 < a1->energy) {
        hit_enemy(a0, a1, d0);
        return;
    }
    l2_wreck(a1, 0x550da);
}

/* ---- the walls of debris ---- */

/* orig $05605A (level2): a wall of debris d6 (0 left, 1 right) comes in: twelve pieces on one path of the table at $55F56 chosen by the generator
 * at $D46 (d6 0) or $D4A (d6 1), one after another (the wait in $28 counts down by ten) */
static void l2_debris_wall(uint16_t d6)
{
    uint32_t r;
    uint16_t d0, d1, d2, d3, k;
    const uint8_t *a1, *a2;
    Object *a0;

    if (v_0d5c == 1) {
        r = ror5(&v_0d26[d6 ? 18 : 16]) & 6;
    } else if (v_0d8f != 0) {
        v_0d8f--;
        r = ror5(&v_0d26[d6 ? 18 : 16]) & 6;
    } else {
        r = (ror5(&v_0d26[d6 ? 18 : 16]) & 6) + 8;
    }
    a1 = (const uint8_t *)GPTR(0x55f56 + (uint16_t)(((uint16_t)r + d6) * 12));
    d0 = rd16(a1);
    d1 = (uint16_t)(rd16(a1 + 2) - v_0cda - v_0cd0);
    d2 = rd16(a1 + 4);
    if (v_0d26[d2 - 1] == 0)
        v_0d26[d2 - 1] = 0x18;
    d3 = rd16(a1 + 6);
    a1 = (const uint8_t *)GPTR(rd32(a1 + 8));
    d2 = 0;
    a2 = (const uint8_t *)GPTR(0x55ec4);
    for (k = rd16(a2), a2 += 2; k != 0xffff; k--) {
        a0 = alloc_in(LIST_C);
        a0->type = rd16(a2);
        a0->fn_update = FN(l2_piece_update);
        a0->fn_draw = (obj_fn)obj_nop_thunk;
        a0->fn_hit = GFN(rd32(a2 + 2));
        a0->gfx = GPTR(rd32(a2 + 6));
        a0->v42 = (int16_t)rd16(a2 + 10);
        a0->v2e = (int16_t)d3;
        a0->energy = (int16_t)W(0x54e52);
        SET_V5E(a0, 0);
        a0->flag35 = 0;
        a0->anim_delay = 0;
        a0->v46 = (uint8_t *)a1;
        a0->ptr4a = a1;
        a0->x = (int16_t)d0;
        a0->y = (int16_t)d1;
        a0->v44 = 0;
        a0->v28 = (int16_t)d2;
        a0->v54 = (int16_t)W(0x54e54);
        a0->v30 = (int16_t)d6;
        d2 = (uint16_t)(d2 - 0xa);
        bbox_from_frame(a0);
        a2 += 12;
    }
    v_0d5e |= (uint8_t)(1 << (d6 & 7));
}

/* level2 $056016: the walls of debris that are not in the field (v_0d8d, v_0d8e say a piece of each moved this frame) are made */
void l2_debris_walls(void)
{
    if (v_0d90 != 0)
        return;
    if (v_0d8d == 0) {
        l2_debris_wall(0);
        v_0d8d = 0xff;
    }
    if (v_0d8e == 0) {
        l2_debris_wall(1);
        v_0d8e = 0xff;
    }
}

/* level2 $0566CE: update of a piece of a wall of debris ($114 to $11C): v28 $8000 is a wreck flying off in the direction of v2c; otherwise
 * it follows its path (a wait while v28 is negative), is drawn behind the map while the path is inside it, and its picture goes by the direction */
void l2_piece_update(Object *a0)
{
    const uint8_t *a1;
    uint16_t d2;

    if (a0->v28 == (int16_t)0x8000) {
        int16_t d0, d1;

        anim_step(a0);
        d2 = (uint16_t)a0->v2c;
        d1 = (int16_t)(v_0f60[d2] >> 2);
        d0 = (int16_t)(v_0f60[(uint16_t)(d2 + 0x40)] >> 2);
        d0 = (int16_t)(d0 + a0->x);
        d1 = (int16_t)(d1 + a0->y);
        if ((uint16_t)d0 >= 0x140 || (uint16_t)d1 >= 0xc0) {
            kill_thunk(a0);
            return;
        }
        a0->x = d0;
        a0->y = d1;
        a0->fn_draw = draw_bob_masked;
        return;
    }
    a0->y = (int16_t)(a0->y + v_0cd8);
    (&v_0d8d)[(uint16_t)a0->v30] = 0xff;
    enemy_update_no_anim(a0);
    if (a0->type == 4) {
        uint16_t *c = &v_0d26[(uint16_t)a0->v2e - 1];

        if (*c == 0)
            *c = 0x18;
        return;
    }
    if (a0->fn_draw == (obj_fn)obj_nop_thunk) {
        if (a0->v28 >= 0)
            a0->fn_draw = draw_bob_behind_map;
    } else if (a0->fn_draw == draw_bob_behind_map) {
        a1 = a0->ptr4a;
        if (rd16(a1) != 0 && a1 - 10 != a0->v46)
            a0->fn_draw = draw_bob_masked;
    } else if (rd16(a0->ptr4a) == 0) {
        a0->fn_draw = draw_bob_behind_map;
    }
    d2 = (uint16_t)(((uint16_t)(a0->v2c + 0x10) >> 3) & 0x1c);
    a0->gfx = GPTR(sh_r32((a0->type == 0x118 ? 0x5681a : a0->type == 0x11c ? 0x5683a : 0x567fa) + d2));
    bbox_from_frame(a0);
}

/* ---- the first fight: three gates and the boss ---- */

/* level2 $056A4A: update of a gate ($54, v30 its number): its box once the player can reach it, its cells open and close (v28 is the step);
 * the gate 2 also patches the map by the counters in v_0d26 (eight records at $56B58) */
void l2_gate_update(Object *a0)
{
    int hide, step;
    int16_t d0, px = PLAYER_X;

    if ((int16_t)v_0ccc < 0 && (int16_t)v_0cea <= 0xb40)
        v_0cea = 0xb40;
    if (a0->v30 == 2)
        hide = v_0d5c != 1;
    else if (a0->v30 == 1)
        hide = px >= 0xa0 || v_0d5e == 3;
    else
        hide = px < 0xa0 || v_0d5e == 3;
    if (hide) {
        a0->hit[0] = 0x3e8;
        a0->hit[2] = 0x3e8;
        step = 1;
    } else {
        d0 = (int16_t)(a0->x << 4);
        a0->hit[0] = d0;
        a0->hit[2] = (int16_t)(d0 + 0xf);
        d0 = (int16_t)((a0->y << 4) - v_0cd0);
        a0->hit[1] = d0;
        a0->hit[3] = (int16_t)(d0 + 0xf);
        step = a0->v28 != 4 || v_0d5e == 3;
    }
    if (step && !(v_0cdc & 1))
        a0->v28 = (int16_t)((a0->v28 + 1) & 7);
    d0 = a0->v28;
    if (d0 > 4)
        d0 = (int16_t)(8 - d0);
    copy2((uint8_t *)a0->gfx, (const uint8_t *)GPTR(0x56b4e + 2 * d0));
    if (a0->v30 == 2) {
        const uint8_t *a1 = (const uint8_t *)GPTR(0x56b58);
        uint16_t *a4 = v_0d26;
        int i;

        for (i = 7; i >= 0; i--, a4++, a1 += 0x12) {
            uint8_t *a2;
            const uint8_t *a3;

            if (*a4 == 0)
                continue;
            a2 = level_init_src + (int16_t)rd16(a1);
            (*a4)--;
            a3 = a1 + ((*a4 & 2) ? 0xa : 2);
            copy4(a2, a3);
            copy4(a2 + 0x28, a3 + 4);
        }
    }
}

/* level2 $0568C6: the draw of a gate that was hit: its cells (by its number) flash once */
void l2_gate_flash(Object *a0)
{
    const uint8_t *cells = (const uint8_t *)GPTR(0x56926);
    uint16_t rows = 1;
    int16_t d2 = (int16_t)((a0->y << 4) - v_0cd0 - 0x10);
    uint16_t d3 = (uint16_t)((a0->x << 3) - 0x10);

    a0->fn_draw = (obj_fn)obj_nop_thunk;
    if (a0->v30 != 0) {
        d3 = (uint16_t)(d3 + 0x10);
        cells = (const uint8_t *)GPTR(0x5691a);
        if (a0->v30 != 1) {
            cells = (const uint8_t *)GPTR(0x56932);
            rows = 2;
            d2 = (int16_t)(d2 - 0x10);
            d3 = (uint16_t)(d3 - 8);
        }
    }
    draw_tile_grid_flash(cells, 2, rows, d2, d3);
}

/* level2 $056944: hit routine of a gate: when it is destroyed its cell is cleared, the counter of gates goes down; with one left the
 * scroll is held and with none the first fight is won: everything but the boss goes, the limit of the scroll is lifted */
void l2_gate_hit(Object *a0, Object *a1, uint16_t d0)
{
    uint16_t old;
    Object *e;
    int i;

    if (v_0d5e == 3)
        return;
    a1->fn_draw = FN(l2_gate_flash);
    old = (uint16_t)a1->energy;
    a1->energy = (int16_t)(old - d0);
    if (old > d0)
        return;
    v_0d5e = 3;
    wr16((uint8_t *)a1->gfx, 0x8e45);
    spawn_effect((int16_t)(a1->hit[0] + 8), (int16_t)(a1->hit[1] + 8), anim_270e, draw_bob_masked);
    kill_thunk(a1);
    v_0d5c--;
    if (v_0d5c == 1) {
        v_0ccc = 0xffff;
        v_0d26[16] = 0xd251;
        v_0d26[17] = 0x35d3;
        v_0d26[18] = 0x894d;
        v_0d26[19] = 0x5d53;
    }
    if (v_0d5c != 0)
        return;
    v_0ccc = 1;
    for (i = 0; i < 0x1cc * 2; i++)
        level_init_src[0x1950 + i] = 0;
    for (e = LIST_HEAD(LIST_ENEMY); e->type != 0;) {
        Object *next = owner_chain_next(e);

        if (e->type != 0x50)
            apply_damage_max(a0, e);
        e = next;
    }
    v_0d8d = 0;
    v_0d8e = 0;
    for (i = 0; i < 16; i++)
        v_043c[i] = 0;
    debris_burst(a0, 0x27, 0, 0, 0x140, 0xc0, v_0f64);
    spawn_bonuses(4);
    v_0ce8 = 0;
    v_0d90 = 0xff;
}

/* level2 $05685A: hit routine of the boss: it flashes; when its energy is gone it bursts and the level is won */
void l2_boss_hit(Object *a0, Object *a1, uint16_t d0)
{
    uint16_t old;

    a1->fn_draw = FN(l2_boss_flash);
    old = (uint16_t)a1->energy;
    a1->energy = (int16_t)(old - d0);
    if (old > d0)
        return;
    kill_thunk(a1);
    spawn_bonuses(9);
    debris_burst(a0, 0x1d, 0x50, (uint16_t)(a1->y - v_0cd0), 0xa0, 0x70, v_0f64);
    free_object_chain(LIST_HEAD(LIST_ENEMY));
    v_0a07 = 0xff;
}

/* level2 $056ED0: the draw of the boss: its picture of cells while the scroll is near the end */
void l2_boss_draw(Object *a0)
{
    if ((int16_t)v_0cd0 >= 0x160)
        return;
    draw_tile_grid((const uint8_t *)GPTR(0x5725e), 5, 8, (int16_t)(a0->y - v_0cd0), 0x38);
}

/* level2 $056EF6: the draw of the boss for the frame after a hit */
void l2_boss_flash(Object *a0)
{
    a0->fn_draw = FN(l2_boss_draw);
    if ((int16_t)v_0cd0 >= 0x160)
        return;
    draw_tile_grid_flash((const uint8_t *)GPTR(0x5725e), 5, 8, (int16_t)(a0->y - v_0cd0), 0x38);
}

/* d0 as the original's code leaves it when the boss does not move and many enemies are about: whatever the caller had in the register.  The
 * lockstep gives the value the original had (l2_stale_d0); without it a value from the scroll. */
uint32_t l2_stale_d0;
int l2_stale_d0_set;

/* level2 $056E56: the boss at rest: a pause before the next move (a random number of frames), or, with 16 enemies or more, a move whose
 * direction and length come from d0 as it stands */
uint32_t l2_boss_rest_d0(void)
{
    uint32_t d0 = l2_stale_d0_set ? l2_stale_d0 : ((uint32_t)v_0cd0 << 3) ^ v_0cdc;

    l2_stale_d0_set = 0;
    return d0;
}

/* level2 $056DD8: the boss moves up or down (v2a a step, v2e the frames left) between y $60 and $C0 */
static void l2_boss_move(Object *a0)
{
    int16_t d0;

    if (a0->v2e == 0) {
        uint32_t r;
        int16_t d1;

        if ((int16_t)v_04e2 < 0x10) {
            r = random_next();
            if (!(r & 0x2000)) {
                a0->v3e = (int16_t)((r & 7) + 2);
                return;
            }
        } else {
            r = l2_boss_rest_d0();
        }
        d1 = (r & 0x4000) ? -3 : 3;
        a0->v2e = (int16_t)((r & 7) + 8);
        a0->v2a = d1;
        return;
    }
    if (B(0x19f9e + 0x1794) == 0)
        v_0414 = 3;
    a0->v2e--;
    d0 = (int16_t)(a0->v2a + a0->y);
    if (d0 < 0x60) {
        d0 = 0x60;
        a0->v2e = 0;
    }
    if (d0 > 0xc0) {
        d0 = 0xc0;
        a0->v2e = 0;
    }
    a0->y = d0;
    d0 = (int16_t)((v_0cdc & 1) * 4);
    copy4((uint8_t *)GPTR(0x572b2), (const uint8_t *)GPTR(sh_r32(0x56e46 + d0)));
    copy4((uint8_t *)GPTR(0x572b2 + 0xc), (const uint8_t *)GPTR(sh_r32(0x56e46 + d0)) + 4);
    copy4((uint8_t *)GPTR(0x572ba), (const uint8_t *)GPTR(sh_r32(0x56e4e + d0)));
    copy4((uint8_t *)GPTR(0x572ba + 0xc), (const uint8_t *)GPTR(sh_r32(0x56e4e + d0)) + 4);
}

/* level2 $056BE8: update of the boss ($50): it waits for the player to come into reach (v28 negative), then counts v28 down; it shoots now
 * and then (a ring of sixteen shots one time in four, else one); while its v3e counts down it changes the cells of its picture.  The moves from
 * $56DD8 on are not written: they test bits of d0 that the original leaves from the shot routine */
void l2_boss_update(Object *a0)
{
    int16_t d0, d5;
    Object *s;
    uint16_t sum;

    if ((int16_t)v_0cea <= 0x120)
        v_0cea = 0x120;
    if (a0->v2e == 0) {
        if (a0->v28 < 0) {
            a0->hit[0] = 0x3e8;
            a0->hit[2] = 0x3e8;
            d0 = (int16_t)(PLAYER_Y + v_0cd0);
            if (d0 > 0xb0)
                return;
            d0 = PLAYER_X;
            if (d0 < 0 || d0 > 0x13f)
                return;
            a0->v2e = 0x3e8;
            a0->v2a = 3;
            a0->v28 = 0x154;
            return;
        }
        a0->v28--;
        if (a0->v28 < 0) {
            a0->v3e = 0;
            a0->v2e = 0x3e8;
            a0->v2a = -3;
        }
    }
    sum = (uint16_t)(a0->v5e_b[0] + B(0x54e65));
    a0->v5e_b[0] = (uint8_t)sum;
    if (sum > 0xff) {
        int16_t y = (int16_t)(a0->y - v_0cd0 + 0x70);

        a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
        if ((random_next() & 3) != 0) {
            s = spawn_enemy_shot(0xa0, (uint16_t)y, 4, W(0x54e66), 0);
            set_anim(s, 0x56e8e);
        } else {
            int d7;

            for (d7 = 15; d7 >= 0; d7--) {
                s = spawn_enemy_shot(0xa0, (uint16_t)y, (uint16_t)d7, W(0x54e66), 0);
                set_anim(s, 0x56eac);
            }
        }
    }
    d5 = (int16_t)(a0->y - v_0cd0 + 0x70);
    a0->hit[0] = 0x98;
    a0->hit[1] = d5;
    a0->hit[2] = 0xa7;
    a0->hit[3] = (int16_t)(d5 + 0xa);
    if ((int16_t)v_0cd0 >= 0x160)
        return;
    if (a0->v3e != 0) {
        const uint8_t *a2;

        a0->v30 = (int16_t)((a0->v30 + 4) & 0xc);
        a2 = (const uint8_t *)GPTR(sh_r32(0x56c44 + (uint16_t)a0->v30));
        copy4((uint8_t *)GPTR(0x572aa), a2);
        copy4((uint8_t *)GPTR(0x572aa + 0xc), a2 + 4);
        a0->v3e--;
        if (a0->v3e != 0)
            return;
        s = alloc_in(LIST_C);
        s->type = 0x10c;
        s->fn_update = (obj_fn)l2_roamer_update;
        s->fn_draw = draw_bob_masked;
        s->fn_hit = (void *)obj_nop_thunk;
        s->hit[0] = 0x3e8;
        s->hit[2] = 0x3e8;
        s->x = 0xa0;
        s->y = (int16_t)(a0->y - v_0cd0 + 0x80);
        set_anim(s, 0x55116);
        s->v28 = 0;
        s->v2a = (int16_t)(random_next() & 7);
        return;
    }
    l2_boss_move(a0);
}

/* ---- the entries by the level in v_0e04: level 1 and level 2 are written, the others are not ---- */

Object *level_collision(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Object *miss)
{
    if (v_0e04 == 2)
        return l2_collision_hook(x0, y0, x1, y1);
    if (v_0e04 == 1)
        return level1_collision_hook() ? miss : 0;
    return 0;
}

void level_event(uint16_t d0, const uint8_t *a1)
{
    if (v_0e04 == 2)
        l2_event(d0, a1);
    else if (v_0e04 == 1)
        level1_event(d0, a1);
}

void level_hook(uint16_t d0, const uint8_t *a1, uint16_t d2)
{
    if (v_0e04 == 2)
        l2_hook(d0, a1, d2);
    else if (v_0e04 == 1)
        level1_hook(d0, a1, d2);
}

void level_scroll(void)
{
    if (v_0e04 == 2)
        l2_scroll();
    else if (v_0e04 == 1)
        level1_scroll();
}

void level_swap_hook(void)
{
    if (v_0e04 == 2)
        l2_swap_hook();
    else if (v_0e04 == 1)
        level1_swap_hook();
}

void level_init(void)
{
    if (v_0e04 == 2)
        l2_init();
    else if (v_0e04 == 1)
        level1_init();
}
