/* level2: level 2's own code (the blob at $54E00 while level 2 is loaded).  The blob is level2_blob (struct Level2Blob, blobs.h): its integer and
 * pointer members are converted when it is loaded, the bytes (cells, animations, pictures) stay big endian (guest_blob_raw 2);
 * a function has `level2 $ADDR` for its address (not `orig $`: level 1's code is at the same addresses).  The addresses of code with no C
 * yet resolve to a function that does nothing (guest_missing_code); the ones this file names itself go through GFN(). */
#include "weapons.h"
#include "shop.h"
#include "game_all.h"
#include "blobs.h"



/* the scratch fields of Object, by the kind that uses them */



/* the scratch fields of Object, by the kind that uses them */


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
    o->is_part = 0;
    o->persists = 0;
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
static void set_anim(Object *o, const uint8_t *anim)
{
    anim_header_set(o, anim);
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

/* level2 $054E0C, $056F54: a box (x0, y0)-(x1, y1) meets one of the blocks that break (l2_block_table: four groups by the quarter of the
 * screen the box is in, a count and entries of state, x, y, the cell that was there); returns l2_block_object with the entry in its $16, or 0 */
Object *l2_collision_hook(int16_t d0, int16_t d1, int16_t d2, int16_t d3)
{
    const uint8_t *a1;
    uint16_t n;
    uint32_t d4 = 0;

    d1 = (int16_t)(d1 + scroll_pos);
    d3 = (int16_t)(d3 + scroll_pos);
    if (d1 >= 0x1b0)
        return 0;
    if (d1 <= 0x80)
        d4 |= 4;
    if (d0 <= 0x80)
        d4 |= 8;
    a1 = l2_block_table + level2_blob.block_group_offsets[d4 / 4];
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
            l2_block_object.gfx = a1;
            return &l2_block_object;
        }
    }
    return 0;
}

/* level2 $056FA8: the hit routine of l2_block_object: the block is broken (its entry's state 0, its cell of the map cleared) and bursts */
void l2_block_hit(Object *a0, Object *a1, uint16_t d0)
{
    uint8_t *e = (uint8_t *)l2_block_object.gfx;
    uint16_t x = rd16(e + 2), y = rd16(e + 4);
    uint16_t cell;

    wr16(e, 0);
    cell = (uint16_t)((uint16_t)(y >> 3) * 0x14 + (x >> 3));
    wr16(level_init_src + cell, 0);
    spawn_effect((int16_t)(x + 8), (int16_t)(y - scroll_pos + 8), anim_explosion, draw_bob_masked);
}

/* level2 $054E10, $055D8A: level events by d0 (a1 = the record that fired): d0 0 is the wave of $326C, 1 to 6 formations */
void l2_event(uint16_t d0, const uint8_t *a1)
{
    const uint8_t *forms[6] = { level2_blob.formation_1, level2_blob.formation_2, level2_blob.formation_3, level2_blob.formation_4,
                                level2_blob.formation_5, level2_blob.formation_6 };

    if (d0 == 0) {
        spawn_wave_326c(0, a1);   /* $E5A */
        return;
    }
    if (d0 <= 6)
        spawn_formation(0, a1, forms[d0 - 1]);   /* $E60 */
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
        a0->energy = (int16_t)level2_blob.hook_energy;
        a0->hit[0] = 0x3e8;
        a0->hit[2] = 0x3e8;
        a0->gfx = (void *)(flag ? level2_blob.hook_picture_a : level2_blob.hook_picture_b);
        a0->anim = flag ? level2_blob.hook_anim_a : level2_blob.hook_anim_b;
        copy2(a0->script, (const uint8_t *)a0->gfx);
        a0->score = 0x64;
        a0->tile.cols = 0;
        a0->tile.rows = 0;
        return;
    case 2:
        a0 = spawn_marker_front(flag ? 0xf0 : 0xf4, a1, (void (*)(void))l2_front_update);
        copy4(a0->script, flag ? level2_blob.hook_cells_a : level2_blob.hook_cells_b);
        copy4(a0->script + 0x28, flag ? level2_blob.hook_cells2_a : level2_blob.hook_cells2_b);
        return;
    case 3:
        a0 = alloc_in(LIST_C);
        a0->type = flag ? 0x100 : 0x104;
        a0->fn_update = FN(l2_launcher_update);
        a0->fn_draw = draw_bob_masked;
        set_anim(a0, flag ? level2_blob.hook_fire_anim_a : level2_blob.hook_fire_anim_b);
        SET_V5E(a0, 0);
        a0->fn_hit = (void *)hit_enemy;
        a0->energy = (int16_t)level2_blob.hook_energy_2;
        a0->big = 0;
        a0->score = 0xc8;
        a0->x = (int16_t)(rd16(a1 + 8) - 8);
        a0->y = (int16_t)(rd16(a1 + 10) - 8 - scroll_pos);
        a0->launcher.step = -1;
        a0->launcher.range = (int16_t)(rd16(a1 + 4) << 4);
        a0->launcher.pos = a0->launcher.range;
        a0->kind = 0;
        bbox_from_frame(a0);
        return;
    case 4:
        a0 = spawn_marker_front(0xfc, a1, (void (*)(void))l2_marker_fc_update);
        a0->marker.launches = 1;
        copy2(a0->script, level2_blob.hook_words);
        return;
    case 5:
        a0 = spawn_marker_front(0xf8, a1, (void (*)(void))l2_marker_f8_update);
        a0->marker.launches = 1;
        return;
    }
}

/* level2 $054E24, $055B1E: the level's scroll events: the walls of debris while the first fight is not won, the stop of the scroll while it is */
void l2_scroll(void)
{
    if (l2_first_fight_won == 0) {
        if ((int16_t)scroll_pos >= 0x9d0 && (int16_t)scroll_pos <= 0xb50)
            l2_debris_walls();
    } else if ((int16_t)scroll_pos > 0x9f0) {
        scroll_pos = 0x9f0;
        scroll_max = 0x9f0;
        map_row_offset = 0x18d8;
        scroll_fine = 0;
    }
    l2_left_wall_made = 0;
    l2_right_wall_made = 0;
}

/* level2 $054E2C, $054E68: a change of players: the table of blocks and the second player's copy exchange */
void l2_swap_hook(void)
{
    uint8_t *a0 = l2_block_table, *a1 = (uint8_t *)GPTR(0x3eb8c);
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
        l2_block_table = (uint8_t *)GPTR(0x7d120 - 360);
    }
    scroll_min = 0x9e0;
    scroll_max = 0x11ff;
    l2_first_fight_won = 0;
    a0 = alloc_in(LIST_C);
    a0->type = 0x50;
    a0->fn_update = FN(l2_boss_update);
    a0->fn_draw = FN(l2_boss_draw);
    a0->fn_hit = (void *)l2_boss_hit;
    SET_V5E(a0, 0);
    a0->persists = -1;
    a0->screen_fixed = -1;
    a0->y = 0x60;
    a0->energy = (int16_t)level2_blob.init_energy;
    a0->l2_boss.wait = -1;
    a0->l2_boss.frames = 0;
    a0->l2_boss_b.flash = 0;
    a0->l2_boss.cell = 0;
    a0->kind = 0;
    for (d7 = 2; d7 >= 0; d7--) {
        uint16_t x = gate_at[2 - d7][0], y = gate_at[2 - d7][1];

        a0 = alloc_in(LIST_C);
        a0->type = 0x54;
        a0->fn_update = FN(l2_gate_update);
        a0->fn_draw = (obj_fn)obj_nop_thunk;
        a0->fn_hit = (void *)l2_gate_hit;
        a0->energy = (int16_t)level2_blob.init_energy_2;
        a0->persists = -1;
        a0->screen_fixed = -1;
        SET_V5E(a0, 0);
        a0->x = (int16_t)x;
        a0->y = (int16_t)y;
        a0->gfx = level_init_src + (uint16_t)(((uint16_t)(y * 0x14) + x) * 2);
        a0->gate.step = 0;
        a0->gate.number = (int16_t)d7;
    }
    l2_gates_left = 3;
    l2_walls_made = 0;
    sh_w8(0xd5f, 0);
    l2_left_wall_made = 0;
    l2_right_wall_made = 0;
    l2_debris_waves = 2;
    for (i = 0; i < 8; i++)
        level_vars[i] = 0;
    for (i = 0; i < 4; i++) {
        uint8_t *g = l2_block_table + groups[i];
        uint16_t n = rd16(g);

        for (g += 2; n != 0; n--, g += 8)
            wr16(g, 1);
    }
    l2_block_object.type = 0;
    l2_block_object.prev = &l2_block_object;
    l2_block_object.next = &l2_block_object;
    l2_block_object.fn_update = (obj_fn)obj_nop_thunk;
    l2_block_object.fn_draw = (obj_fn)obj_nop_thunk;
    l2_block_object.fn_hit = (void *)l2_block_hit;
    level_vars[16] = 0x45d2;
    level_vars[17] = 0x72ba;
    level_vars[18] = 0xaa6a;
    level_vars[19] = 0x4a56;
    difficulty_x1 = (uint16_t)(difficulty * 1);
    difficulty_x2 = (uint16_t)(difficulty * 2);
    for (i = 0; i < 0xb4; i++) {
        ((uint8_t *)GPTR(0x3eb8c))[2 * i] = l2_block_table[2 * i];
        ((uint8_t *)GPTR(0x3eb8c))[2 * i + 1] = l2_block_table[2 * i + 1];
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
    a0->front.tick++;
    if (a0->front.tick >= 0x50) {
        burst = 1;
    } else {
        int16_t x, y;

        if ((a0->front.tick & 0xf) == 0) {
            uint16_t d = direction8((int16_t)(PLAYER_X - a0->x), (int16_t)(PLAYER_Y - a0->y));

            a0->front.dir = (int16_t)(~d & 7);
            set_anim(a0, level2_blob.front_anims_by_dir[(uint16_t)a0->front.dir]);
        }
        x = (int16_t)(dx[a0->front.dir] + a0->x);
        if ((uint16_t)x >= 0x140) {
            kill_thunk(a0);
            return;
        }
        a0->x = x;
        y = (int16_t)(dy[a0->front.dir] + a0->y + scroll_speed);
        if ((uint16_t)y >= 0xc0) {
            kill_thunk(a0);
            return;
        }
        a0->y = y;
        bbox_from_frame(a0);
        if (!point_in_player_box(x, y))
            return;
        if (shield_timer != 0) {
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

        spawn_effect(ex, ey, anim_explosion, draw_bob_masked);
    }
    kill_thunk(a0);
}

/* level2 $056144: the flying enemy $E0 / $E4: it comes down the screen to y $32, drifts towards the player's x and, within 10 of it,
 * dives (v28 1) and rises again; the offsets are from a table indexed by the frame and the object's address */
void l2_flyer_update(Object *a0)
{
    static const int16_t wobble[8] = { 0, -2, -4, -2, 0, 2, 4, 2 };
    uint16_t phase = (uint16_t)(frame_tick + (uint16_t)GADDR(a0));
    int16_t d0;

    anim_step(a0);
    if (a0->flyer.dive == 0) {
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
            a0->flyer.dive = 1;
            set_anim(a0, a0->type == 0xe0 ? level2_blob.flyer_anim_e0 : level2_blob.flyer_anim_other);
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
        a0->flyer.dive = 0;
        set_anim(a0, a0->type == 0xe0 ? level2_blob.flyer_anim2_e0 : level2_blob.flyer_anim2_other);
    }
    bbox_from_frame(a0);
}


/* level2 $05533E: the marker $F8: as $05550E, but its picture blinks (v28 0 / 1) until the scroll is near, its steps use a table of
 * two longs, and at step 4 it launches the flyer $E0 */
void l2_marker_f8_update(Object *a0)
{
    int16_t d0;
    const uint8_t *a2;

    if ((int16_t)((int16_t)scroll_max + 0xd0) <= a0->y) {
        kill_thunk(a0);
        return;
    }
    d0 = (int16_t)(a0->y - scroll_pos);
    if (d0 >= 0xb4)
        return;
    if (a0->marker_f8.blink >= 5) {
        if (a0->marker.launches == 0) {
            kill_thunk(a0);
            return;
        }
        if (a0->marker_f8.blink >= 9) {
            a0->marker_f8.blink = 0;
            a0->marker.launches--;
        }
    }
    if ((uint16_t)a0->marker_f8.blink <= 1) {
        if (d0 >= 0x64)
            a0->marker_f8.blink = 2;
        else if ((frame_tick & 3) == 0)
            a0->marker_f8.blink ^= 1;
    }
    a2 = level2_blob.marker_f8_cells + ((uint16_t)a0->marker_f8.blink << 3);
    copy4(a0->script, a2);
    copy4(a0->script + 0x28, a2 + 4);
    if ((uint16_t)a0->marker_f8.blink < 2)
        return;
    if (frame_tick & 3)
        return;
    a0->marker_f8.blink++;
    if (a0->marker_f8.blink != 4)
        return;
    {
        Object *s = alloc_in(LIST_C);

        s->type = 0xe0;
        s->fn_update = (obj_fn)l2_flyer_update;
        s->fn_draw = draw_bob_masked;
        s->fn_hit = (void *)hit_enemy;
        set_anim(s, level2_blob.flyer_anim2_e0);
        SET_V5E(s, 0);
        s->x = (int16_t)(a0->x + 0x10);
        s->y = (int16_t)(a0->y - scroll_pos + 0x10);
        s->energy = (int16_t)level2_blob.marker_f8_energy;
        s->big = (int8_t)0xff;
        s->score = 0x12c;
        s->marker_f8.blink = 0;
        s->kind = 0;
        bbox_from_frame(s);
    }
}


/* level2 $05550E: the marker $FC: gone when the scroll has passed it; its picture steps (v28) and at step 4 it launches the flyer $E4;
 * v30 counts the launches left */
void l2_marker_fc_update(Object *a0)
{
    int16_t d0;

    if ((int16_t)((int16_t)scroll_max + 0xd0) <= a0->y) {
        kill_thunk(a0);
        return;
    }
    d0 = (int16_t)(a0->y - scroll_pos);
    if (d0 >= 0xb4)
        return;
    if (a0->marker_fc.step >= 5) {
        if (a0->marker_fc.launches == 0) {
            kill_thunk(a0);
            return;
        }
        if (a0->marker_fc.step >= 9) {
            a0->marker_fc.step = 0;
            a0->marker_fc.launches--;
        }
    }
    if ((uint16_t)a0->marker_fc.step <= 1) {
        if (d0 < 0x64)
            return;
        a0->marker_fc.step = 2;
    }
    copy2(a0->script, level2_blob.hook_words + ((uint16_t)a0->marker_fc.step << 1));
    if (frame_tick & 3)
        return;
    a0->marker_fc.step++;
    if (a0->marker_fc.step != 4)
        return;
    {
        Object *s = alloc_in(LIST_C);

        s->type = 0xe4;
        s->fn_update = (obj_fn)l2_flyer_update;
        s->fn_draw = draw_bob_masked;
        s->fn_hit = (void *)hit_enemy;
        set_anim(s, level2_blob.flyer_anim2_other);
        SET_V5E(s, 0);
        s->x = a0->x;
        s->y = (int16_t)(a0->y - scroll_pos);
        s->energy = (int16_t)level2_blob.marker_fc_energy;
        s->big = 0;
        s->score = 0x96;
        s->marker_fc.step = 0;
        s->kind = 0;
        bbox_from_frame(s);
    }
}

/* level2 $056FEC: the first block that is broken (its entry's state 0) and meets the box of a0 comes back: state 1, its cell of the map
 * written again */
static void l2_restore_blocks(Object *a0)
{
    int16_t d0 = a0->hit[0], d1 = (int16_t)(a0->hit[1] + scroll_pos), d2 = a0->hit[2], d3 = (int16_t)(a0->hit[3] + scroll_pos);
    uint32_t d4 = 0;
    const uint8_t *a1;
    uint16_t n;

    if (d1 <= 0x80)
        d4 |= 4;
    if (d0 <= 0x80)
        d4 |= 8;
    a1 = l2_block_table + level2_blob.block_group_offsets[d4 / 4];
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
    a0->y = (int16_t)(a0->y + scroll_speed);
    if (a0->bolt.target >= 0) {
        if (!(frame_tick & 1)) {
            bbox_from_frame(a0);
            return;
        }
        d0 = (int16_t)(a0->bolt.target - a0->bolt.dir);
        if (d0 != 0) {
            if (d0 < 0)
                d0 += 8;
            a0->bolt.dir = (int16_t)((a0->bolt.dir + (d0 > 4 ? -1 : 1)) & 7);
            set_anim(a0, level2_blob.bolt_anims_by_dir[(uint16_t)a0->bolt.dir]);
            bbox_from_frame(a0);
            return;
        }
        a0->bolt.target = -1;
    }
    if ((uint16_t)a0->y < 0xc0) {
        uint16_t sum = (uint16_t)(a0->v5e_b[0] + level2_blob.roamer_shot_fire_rate);

        a0->v5e_b[0] = (uint8_t)sum;
        carry = sum > 0xff;
        if (carry) {
            uint16_t d3 = level2_blob.roamer_shot_param;
            uint16_t angle;
            const void *gfx;

            a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
            gfx = GPTR(sh_r32(sh_r32(0x6c89c + (uint32_t)d3 * 4)));
            angle = direction8((int16_t)(PLAYER_X - a0->x), (int16_t)(PLAYER_Y - a0->y));
            spawn_enemy_shot((uint16_t)a0->x, (uint16_t)a0->y, angle, level2_blob.roamer_shot_shot_steps, gfx);
        }
    }
    x = (int16_t)(a0->x + dx[a0->bolt.dir]);
    y = (int16_t)(a0->y + dy[a0->bolt.dir]);
    a0->x = x;
    a0->y = y;
    y = (int16_t)(y + scroll_pos);
    for (n = 0; n < 8; n++) {
        const struct Level2RoamerNode *entry = &level2_blob.roamer_shot_entries[n];

        if (entry->x == x && entry->y == y) {
            uint16_t r;

            do
                r = (uint16_t)(random_next() & 3);
            while (r == 0);
            a0->bolt.target = (int16_t)entry->targets[r - 1];
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
    a0->y = (int16_t)(a0->y + scroll_speed);
    if (a0->roamer.state == 0) {
        int16_t x = (int16_t)(a0->x + dx[a0->roamer.dir]);
        int16_t y = (int16_t)(a0->y + dy[a0->roamer.dir]);

        if (x <= 0x74) {
            x = 0x74;
            a0->roamer.state = 1;
            a0->roamer.dir = 4;
        }
        if (x >= 0xcc) {
            x = 0xcc;
            a0->roamer.state = 1;
            a0->roamer.dir = 0;
        }
        y = (int16_t)(y + scroll_pos);
        if (y <= 0x6c) {
            y = 0x6c;
            a0->roamer.state = 1;
            a0->roamer.dir = 6;
        }
        if (y >= 0x154) {
            y = 0x154;
            a0->roamer.state = 1;
            a0->roamer.dir = 2;
        }
        a0->x = x;
        a0->y = (int16_t)(y - scroll_pos);
        return;
    }
    if (a0->roamer.state == 1) {
        if ((random_next() & 7) == 0) {
            a0->roamer.state = 2;
            set_anim(a0, level2_blob.roamer_anim);
        }
        return;
    }
    if (a0->gfx == (void *)level2_blob.roamer_picture) {
        Object *s = alloc_in(LIST_C);

        s->type = 0x110;
        s->fn_update = (obj_fn)l2_roamer_shot_update;
        s->fn_draw = draw_bob_masked;
        s->fn_hit = (void *)hit_enemy;
        s->energy = (int16_t)level2_blob.roamer_energy;
        s->big = 0;
        SET_V5E(s, 0);
        s->score = 0x1e;
        s->kind = 0;
        set_anim(s, level2_blob.bolt_anims_by_dir[(uint16_t)a0->roamer.dir]);
        s->x = a0->x;
        s->y = a0->y;
        s->roamer.dir = a0->roamer.dir;
        s->roamer.state = -1;
        bbox_from_frame(s);
        kill_thunk(a0);
    }
}


/* level2 $05568C: the marker $F0 / $F4: waits for the scroll, then its picture takes 20 steps (v28) from a table, and it bursts into
 * eight pieces ($108, update $056596) */
void l2_front_update(Object *a0)
{
    int i;

    if (a0->front.step == 0) {
        if ((int16_t)(a0->y - scroll_pos) >= (int16_t)level2_blob.front_param)
            a0->front.step = 1;
        return;
    }
    {
        const uint8_t *const *table = a0->type == 0xf0 ? level2_blob.front_table_f0 : level2_blob.front_table_other;
        const uint8_t *a2 = table[(uint16_t)a0->front.step];

        copy4(a0->script, a2);
        copy4(a0->script + 0x28, a2 + 4);
    }
    a0->front.step++;
    if (a0->front.step < 0x15)
        return;
    for (i = 7; i >= 0; i--) {
        const struct Level2FrontEntry *entry = &level2_blob.front_entries[7 - i];
        Object *s = alloc_in(LIST_C);

        s->type = 0x108;
        s->fn_update = (obj_fn)l2_front_piece_update;
        s->fn_draw = draw_bob_masked;
        s->fn_hit = (void *)hit_explode;
        set_anim(s, entry->anim);
        SET_V5E(s, 0);
        s->x = (int16_t)(a0->x + 0x10 + entry->dx);
        s->y = (int16_t)(a0->y - scroll_pos + 0x10 + entry->dy);
        s->front.dir = (int16_t)i;
        s->energy = 1;
        s->big = 0;
        s->score = 0x14;
        s->front.step = (int16_t)(random_next() & 0x1f);
        s->kind = 0;
        bbox_from_frame(s);
    }
    kill_thunk(a0);
}


/* the turret's use of the scratch fields of Object */

/* level2 $0558E0: update of the turret $E8 / $EC: its box, the cells it opens and closes (v28 is the step), and at step 7 a shot */
void l2_turret_update(Object *a0)
{
    int16_t d0, d1, d2;
    uint32_t pic;

    if ((int16_t)(scroll_max + 0xd0) < a0->y) {
        kill_thunk(a0);
        return;
    }
    d0 = (int16_t)(a0->x - 8);
    a0->hit[0] = d0;
    a0->hit[2] = (int16_t)(d0 + 0x14);
    d0 = (int16_t)(a0->y - scroll_pos + 4);
    a0->hit[1] = d0;
    a0->hit[3] = (int16_t)(d0 + 8);
    if (a0->turret.step == 0) {
        if ((uint8_t)random_next() < level2_blob.turret_chance)
            a0->turret.step = 1;
        return;
    }
    copy2(a0->script, (a0->type == 0xe8 ? level2_blob.hook_picture_a : level2_blob.hook_picture_b) + 2 * (uint16_t)a0->turret.step);
    a0->turret.step++;
    if (a0->turret.step != 7) {
        if (a0->turret.step == 0xf)
            a0->turret.step = 0;
        return;
    }
    d0 = (int16_t)(a0->x + 8);
    d1 = (int16_t)(a0->y - scroll_pos + 8);
    pic = sh_r32(sh_r32(0x6c89c + 4 * (uint32_t)level2_blob.turret_param));
    d2 = a0->type == 0xec ? 2 : 6;
    spawn_enemy_shot((uint16_t)d0, (uint16_t)d1, (uint16_t)d2, level2_blob.turret_shot_steps, GPTR(pic));
}

/* the launcher's use of the scratch fields of Object */

/* level2 $055A04: update of the launcher $100 / $104: it moves up and down (v28 is its step, v30 its place between 0 and v2a); when the
 * player is level with it (now and then) it goes into list B and fires a shot */
void l2_launcher_update(Object *a0)
{
    int16_t d0, d1, d2;

    if ((int16_t)(scroll_max + 0xd0 - scroll_pos) < a0->y) {
        kill_thunk(a0);
        return;
    }
    anim_step(a0);
    a0->y = (int16_t)(a0->y + scroll_speed);
    if (a0->launcher.step == 0)
        return;
    d0 = (int16_t)(a0->y - PLAYER_Y);
    if (d0 <= 0xa && d0 >= -0x1e && (uint8_t)random_next() < 0x1e) {
        Object *tail;

        a0->launcher.step = 0;
        a0->next->prev = a0->prev;
        a0->prev->next = a0->next;
        tail = LIST_B->prev;
        LIST_B->prev = a0;
        a0->prev = tail;
        a0->next = LIST_B;
        tail->next = a0;
        set_anim(a0, a0->type == 0x100 ? level2_blob.launcher_anim_100 : level2_blob.launcher_anim_other);
        d0 = (int16_t)(a0->x + 0xb);
        d1 = (int16_t)(a0->y + 7);
        d2 = 6;
        if (a0->type != 0x100) {
            d2 = 2;
            d0 = (int16_t)(d0 - 8);
        }
        spawn_enemy_shot((uint16_t)d0, (uint16_t)d1, (uint16_t)d2, level2_blob.launcher_shot_steps, (void *)level2_blob.launcher_shot_picture);
        return;
    }
    d0 = a0->launcher.step;
    a0->y = (int16_t)(a0->y + d0);
    a0->launcher.pos = (int16_t)(a0->launcher.pos + d0);
    if (a0->launcher.pos == 0 || a0->launcher.pos == a0->launcher.range)
        a0->launcher.step = (int16_t)-a0->launcher.step;
    bbox_from_frame(a0);
}

/* the end of $055DEC, $055E0E and $055E28: the object a1 bursts into a wreck (animation a2) that flies away, the object goes */
static void l2_wreck(Object *a1, const uint8_t *a2)
{
    Object *n = alloc_in(LIST_B);

    n->type = a1->type;
    n->fn_update = a1->fn_update;
    n->fn_draw = draw_bob_masked;
    n->x = a1->x;
    n->y = a1->y;
    set_anim(n, a2);
    n->piece.wait = (int16_t)0x8000;
    n->piece.dir_hi = 0;
    n->piece.dir = (int16_t)(random_next() & 0xff);
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
    l2_walls_made &= (uint8_t)~(1 << (a1->piece.wall & 7));
    l2_wreck(a1, level2_blob.wreck_a);
}

/* level2 $055E0E: hit routine of a piece */
void l2_hit_b(Object *a0, Object *a1, uint16_t d0)
{
    a1->fn_draw = draw_bob_or_first;
    if ((int16_t)d0 < a1->energy) {
        hit_enemy(a0, a1, d0);
        return;
    }
    l2_wreck(a1, level2_blob.wreck_b);
}

/* level2 $055E28: hit routine of a piece */
void l2_hit_c(Object *a0, Object *a1, uint16_t d0)
{
    a1->fn_draw = draw_bob_or_first;
    if ((int16_t)d0 < a1->energy) {
        hit_enemy(a0, a1, d0);
        return;
    }
    l2_wreck(a1, level2_blob.wreck_c);
}

/* ---- the walls of debris ---- */

/* orig $05605A (level2): a wall of debris d6 (0 left, 1 right) comes in: twelve pieces on one path of the table at $55F56 chosen by the generator
 * at $D46 (d6 0) or $D4A (d6 1), one after another (the wait in $28 counts down by ten) */
static void l2_debris_wall(uint16_t d6)
{
    uint32_t r;
    uint16_t d0, d1, d2, d3, k;
    const uint8_t *a1;
    const struct Level2DebrisWall *w;
    const struct Level2WallPiece *a2;
    Object *a0;

    if (l2_gates_left == 1) {
        r = ror5(&level_vars[d6 ? 18 : 16]) & 6;
    } else if (l2_debris_waves != 0) {
        l2_debris_waves--;
        r = ror5(&level_vars[d6 ? 18 : 16]) & 6;
    } else {
        r = (ror5(&level_vars[d6 ? 18 : 16]) & 6) + 8;
    }
    w = &level2_blob.debris_wall_records[(uint16_t)(r + d6)];
    d0 = w->x;
    d1 = (uint16_t)(w->y - scroll_step - scroll_pos);
    d2 = w->index;
    if (level_vars[d2 - 1] == 0)
        level_vars[d2 - 1] = 0x18;
    d3 = w->var;
    a1 = w->path;
    d2 = 0;
    a2 = level2_blob.wall_pieces.pieces;
    for (k = level2_blob.wall_pieces.last; k != 0xffff; k--) {
        a0 = alloc_in(LIST_C);
        a0->type = a2->type;
        a0->fn_update = FN(l2_piece_update);
        a0->fn_draw = (obj_fn)obj_nop_thunk;
        a0->fn_hit = (void *)a2->hit;
        a0->gfx = (void *)a2->gfx;
        a0->score = (int16_t)a2->score;
        a0->piece.var = (int16_t)d3;
        a0->energy = (int16_t)level2_blob.debris_wall_energy;
        SET_V5E(a0, 0);
        a0->big = 0;
        a0->anim_delay = 0;
        a0->script = (uint8_t *)a1;
        a0->cursor = a1;
        a0->x = (int16_t)d0;
        a0->y = (int16_t)d1;
        a0->kind = 0;
        a0->piece.wait = (int16_t)d2;
        a0->steps = (int16_t)level2_blob.debris_wall_steps;
        a0->piece.wall = (int16_t)d6;
        d2 = (uint16_t)(d2 - 0xa);
        bbox_from_frame(a0);
        a2++;
    }
    l2_walls_made |= (uint8_t)(1 << (d6 & 7));
}

/* level2 $056016: the walls of debris that are not in the field (l2_left_wall_made, l2_right_wall_made say a piece of each moved this frame) are made */
void l2_debris_walls(void)
{
    if (l2_first_fight_won != 0)
        return;
    if (l2_left_wall_made == 0) {
        l2_debris_wall(0);
        l2_left_wall_made = 0xff;
    }
    if (l2_right_wall_made == 0) {
        l2_debris_wall(1);
        l2_right_wall_made = 0xff;
    }
}


/* level2 $0566CE: update of a piece of a wall of debris ($114 to $11C): v28 $8000 is a wreck flying off in the direction of v2c; otherwise
 * it follows its path (a wait while v28 is negative), is drawn behind the map while the path is inside it, and its picture goes by the direction */
void l2_piece_update(Object *a0)
{
    const uint8_t *a1;
    uint16_t d2;

    if (a0->piece.wait == (int16_t)0x8000) {
        int16_t d0, d1;

        anim_step(a0);
        d2 = (uint16_t)a0->piece.dir;
        d1 = (int16_t)(piece_directions[d2] >> 2);
        d0 = (int16_t)(piece_directions[(uint16_t)(d2 + 0x40)] >> 2);
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
    a0->y = (int16_t)(a0->y + scroll_speed);
    (&l2_left_wall_made)[(uint16_t)a0->piece.wall] = 0xff;
    enemy_update_no_anim(a0);
    if (a0->type == 4) {
        uint16_t *c = &level_vars[(uint16_t)a0->piece.var - 1];

        if (*c == 0)
            *c = 0x18;
        return;
    }
    if (a0->fn_draw == (obj_fn)obj_nop_thunk) {
        if (a0->piece.wait >= 0)
            a0->fn_draw = draw_bob_behind_map;
    } else if (a0->fn_draw == draw_bob_behind_map) {
        a1 = a0->cursor;
        if (rd16(a1) != 0 && a1 - 10 != a0->script)
            a0->fn_draw = draw_bob_masked;
    } else if (rd16(a0->cursor) == 0) {
        a0->fn_draw = draw_bob_behind_map;
    }
    d2 = (uint16_t)(((uint16_t)(a0->piece.dir + 0x10) >> 3) & 0x1c);
    a0->gfx = (void *)(a0->type == 0x118 ? level2_blob.piece_pictures_118 : a0->type == 0x11c ? level2_blob.piece_pictures_11c : level2_blob.piece_pictures_other)[d2 / 4];
    bbox_from_frame(a0);
}

/* ---- the first fight: three gates and the boss ---- */

/* the gate's use of the scratch fields of Object */

/* level2 $056A4A: update of a gate ($54, v30 its number): its box once the player can reach it, its cells open and close (v28 is the step);
 * the gate 2 also patches the map by the counters in level_vars (eight records at $56B58) */
void l2_gate_update(Object *a0)
{
    int hide, step;
    int16_t d0, px = PLAYER_X;

    if ((int16_t)scroll_step_target < 0 && (int16_t)scroll_max <= 0xb40)
        scroll_max = 0xb40;
    if (a0->gate.number == 2)
        hide = l2_gates_left != 1;
    else if (a0->gate.number == 1)
        hide = px >= 0xa0 || l2_walls_made == 3;
    else
        hide = px < 0xa0 || l2_walls_made == 3;
    if (hide) {
        a0->hit[0] = 0x3e8;
        a0->hit[2] = 0x3e8;
        step = 1;
    } else {
        d0 = (int16_t)(a0->x << 4);
        a0->hit[0] = d0;
        a0->hit[2] = (int16_t)(d0 + 0xf);
        d0 = (int16_t)((a0->y << 4) - scroll_pos);
        a0->hit[1] = d0;
        a0->hit[3] = (int16_t)(d0 + 0xf);
        step = a0->gate.step != 4 || l2_walls_made == 3;
    }
    if (step && !(frame_tick & 1))
        a0->gate.step = (int16_t)((a0->gate.step + 1) & 7);
    d0 = a0->gate.step;
    if (d0 > 4)
        d0 = (int16_t)(8 - d0);
    copy2((uint8_t *)a0->gfx, level2_blob.gate_number_cells + 2 * d0);
    if (a0->gate.number == 2) {
        const uint8_t *a1 = level2_blob.gate_cells;
        uint16_t *a4 = level_vars;
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
    const uint8_t *cells = level2_blob.gate_flash_cells_a;
    uint16_t rows = 1;
    int16_t d2 = (int16_t)((a0->y << 4) - scroll_pos - 0x10);
    uint16_t d3 = (uint16_t)((a0->x << 3) - 0x10);

    a0->fn_draw = (obj_fn)obj_nop_thunk;
    if (a0->gate.number != 0) {
        d3 = (uint16_t)(d3 + 0x10);
        cells = level2_blob.gate_flash_cells_b;
        if (a0->gate.number != 1) {
            cells = level2_blob.gate_flash_cells_c;
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

    if (l2_walls_made == 3)
        return;
    a1->fn_draw = FN(l2_gate_flash);
    old = (uint16_t)a1->energy;
    a1->energy = (int16_t)(old - d0);
    if (old > d0)
        return;
    l2_walls_made = 3;
    wr16((uint8_t *)a1->gfx, 0x8e45);
    spawn_effect((int16_t)(a1->hit[0] + 8), (int16_t)(a1->hit[1] + 8), anim_explosion, draw_bob_masked);
    kill_thunk(a1);
    l2_gates_left--;
    if (l2_gates_left == 1) {
        scroll_step_target = 0xffff;
        level_vars[16] = 0xd251;
        level_vars[17] = 0x35d3;
        level_vars[18] = 0x894d;
        level_vars[19] = 0x5d53;
    }
    if (l2_gates_left != 0)
        return;
    scroll_step_target = 1;
    for (i = 0; i < 0x1cc * 2; i++)
        level_init_src[0x1950 + i] = 0;
    for (e = LIST_HEAD(LIST_ENEMY); e->type != 0;) {
        Object *next = owner_chain_next(e);

        if (e->type != 0x50)
            apply_damage_max(a0, e);
        e = next;
    }
    l2_left_wall_made = 0;
    l2_right_wall_made = 0;
    for (i = 0; i < 16; i++)
        pickup_groups[i] = 0;
    debris_burst(a0, 0x27, 0, 0, 0x140, 0xc0, big_explosion_anim);
    spawn_bonuses(4);
    scroll_min = 0;
    l2_first_fight_won = 0xff;
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
    debris_burst(a0, 0x1d, 0x50, (uint16_t)(a1->y - scroll_pos), 0xa0, 0x70, big_explosion_anim);
    free_object_chain(LIST_HEAD(LIST_ENEMY));
    boss_dead = 0xff;
}

/* level2 $056ED0: the draw of the boss: its picture of cells while the scroll is near the end */
void l2_boss_draw(Object *a0)
{
    if ((int16_t)scroll_pos >= 0x160)
        return;
    draw_tile_grid(level2_blob.boss_grid, 5, 8, (int16_t)(a0->y - scroll_pos), 0x38);
}

/* level2 $056EF6: the draw of the boss for the frame after a hit */
void l2_boss_flash(Object *a0)
{
    a0->fn_draw = FN(l2_boss_draw);
    if ((int16_t)scroll_pos >= 0x160)
        return;
    draw_tile_grid_flash(level2_blob.boss_grid, 5, 8, (int16_t)(a0->y - scroll_pos), 0x38);
}

uint32_t l2_stale_d0;
int l2_stale_d0_set;

/* level2 $056E56: the boss at rest: a pause before the next move (a random number of frames), or, with 16 enemies or more, a move whose
 * direction and length come from d0 as the code before it left it (the lockstep hooks the entry and gives the value the original had) */
void l2_boss_rest(Object *a0)
{
    uint32_t r;
    int16_t d1;

    if ((int16_t)target_count < 0x10) {
        r = random_next();
        if (!(r & 0x2000)) {
            a0->l2_boss_b.flash = (int16_t)((r & 7) + 2);
            return;
        }
    } else {
        r = l2_stale_d0_set ? l2_stale_d0 : ((uint32_t)scroll_pos << 3) ^ frame_tick;
        l2_stale_d0_set = 0;
    }
    d1 = (r & 0x4000) ? -3 : 3;
    a0->l2_boss.frames = (int16_t)((r & 7) + 8);
    a0->l2_boss.step = d1;
}

/* the boss's use of the scratch fields of Object */

/* level2 $056DD8: the boss moves up or down (v2a a step, v2e the frames left) between y $60 and $C0 */
static void l2_boss_move(Object *a0)
{
    int16_t d0;

    if (a0->l2_boss.frames == 0) {
        l2_boss_rest(a0);
        return;
    }
    if (B(0x19f9e + 0x1794) == 0)
        sfx_cmd = 3;
    a0->l2_boss.frames--;
    d0 = (int16_t)(a0->l2_boss.step + a0->y);
    if (d0 < 0x60) {
        d0 = 0x60;
        a0->l2_boss.frames = 0;
    }
    if (d0 > 0xc0) {
        d0 = 0xc0;
        a0->l2_boss.frames = 0;
    }
    a0->y = d0;
    d0 = (int16_t)((frame_tick & 1) * 4);
    copy4(&level2_blob.boss_cells[0x8], level2_blob.boss_move_paths_a[d0 / 4]);
    copy4(&level2_blob.boss_cells[0x14], level2_blob.boss_move_paths_a[d0 / 4] + 4);
    copy4(&level2_blob.boss_cells[0x10], level2_blob.boss_move_paths_b[d0 / 4]);
    copy4(&level2_blob.boss_cells[0x1c], level2_blob.boss_move_paths_b[d0 / 4] + 4);
}


/* level2 $056BE8: update of the boss ($50): it waits for the player to come into reach (v28 negative), then counts v28 down; it shoots now
 * and then (a ring of sixteen shots one time in four, else one); while its v3e counts down it changes the cells of its picture.  The moves from
 * $56DD8 on are not written: they test bits of d0 that the original leaves from the shot routine */
void l2_boss_update(Object *a0)
{
    int16_t d0, d5;
    Object *s;
    uint16_t sum;

    if ((int16_t)scroll_max <= 0x120)
        scroll_max = 0x120;
    if (a0->l2_boss.frames == 0) {
        if (a0->l2_boss.wait < 0) {
            a0->hit[0] = 0x3e8;
            a0->hit[2] = 0x3e8;
            d0 = (int16_t)(PLAYER_Y + scroll_pos);
            if (d0 > 0xb0)
                return;
            d0 = PLAYER_X;
            if (d0 < 0 || d0 > 0x13f)
                return;
            a0->l2_boss.frames = 0x3e8;
            a0->l2_boss.step = 3;
            a0->l2_boss.wait = 0x154;
            return;
        }
        a0->l2_boss.wait--;
        if (a0->l2_boss.wait < 0) {
            a0->l2_boss_b.flash = 0;
            a0->l2_boss.frames = 0x3e8;
            a0->l2_boss.step = -3;
        }
    }
    sum = (uint16_t)(a0->v5e_b[0] + level2_blob.boss_fire_rate);
    a0->v5e_b[0] = (uint8_t)sum;
    if (sum > 0xff) {
        int16_t y = (int16_t)(a0->y - scroll_pos + 0x70);

        a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
        if ((random_next() & 3) != 0) {
            s = spawn_enemy_shot(0xa0, (uint16_t)y, 4, level2_blob.boss_shot_steps, 0);
            set_anim(s, level2_blob.boss_update_data);
        } else {
            int d7;

            for (d7 = 15; d7 >= 0; d7--) {
                s = spawn_enemy_shot(0xa0, (uint16_t)y, (uint16_t)d7, level2_blob.boss_shot_steps, 0);
                set_anim(s, level2_blob.boss_update_data_2);
            }
        }
    }
    d5 = (int16_t)(a0->y - scroll_pos + 0x70);
    a0->hit[0] = 0x98;
    a0->hit[1] = d5;
    a0->hit[2] = 0xa7;
    a0->hit[3] = (int16_t)(d5 + 0xa);
    if ((int16_t)scroll_pos >= 0x160)
        return;
    if (a0->l2_boss_b.flash != 0) {
        const uint8_t *a2;

        a0->l2_boss.cell = (int16_t)((a0->l2_boss.cell + 4) & 0xc);
        a2 = level2_blob.boss_cells_by_cell[(uint16_t)a0->l2_boss.cell / 4];
        copy4(&level2_blob.boss_cells[0], a2);
        copy4(&level2_blob.boss_cells[0xc], a2 + 4);
        a0->l2_boss_b.flash--;
        if (a0->l2_boss_b.flash != 0)
            return;
        s = alloc_in(LIST_C);
        s->type = 0x10c;
        s->fn_update = (obj_fn)l2_roamer_update;
        s->fn_draw = draw_bob_masked;
        s->fn_hit = (void *)obj_nop_thunk;
        s->hit[0] = 0x3e8;
        s->hit[2] = 0x3e8;
        s->x = 0xa0;
        s->y = (int16_t)(a0->y - scroll_pos + 0x80);
        set_anim(s, level2_blob.boss_update_data_4);
        s->l2_boss.wait = 0;
        s->l2_boss.step = (int16_t)(random_next() & 7);
        return;
    }
    l2_boss_move(a0);
}
