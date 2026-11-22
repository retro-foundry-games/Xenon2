/* level2: level 2's own code (the blob at $54E40 while level 2 is loaded).  The blob is level2_data (struct Level2Data, types_blobs.h): its integer and
 * pointer members are converted when it is loaded, the bytes (cells, animations, pictures) stay big endian (guest_blob_raw 2);
 * a function has `level2 $ADDR` for its address (not `orig $`: level 1's code is at the same addresses).  The addresses of code with no C
 * yet resolve to a function that does nothing (guest_missing_code); the ones this file names itself go through GFN(). */
#include "weapons.h"
#include "shop.h"
#include "game_all.h"



/* the scratch fields of Object, by the kind that uses them */



/* the scratch fields of Object, by the kind that uses them */


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

/* the animation header at an address of the blob */
static void set_anim(Object *o, const uint8_t *anim)
{
    anim_header_set(o, anim);
}

static uint32_t ror5(uint32_t *seed)
{
    *seed = (*seed >> 5) | (*seed << 27);
    return *seed;
}

/* the table of blocks that break: four groups (by the quarter of the screen), each a count and that many entries (the words stay big endian) */
struct L2Block { RawWord state, x, y, cell; };
struct L2Group { RawWord count; struct L2Block entries[]; };

/* ---- the entries the engine calls ($54E0C..$54E2C) ---- */

/* level2 $054E0C, $056F54: a box (x0, y0)-(x1, y1) meets one of the blocks that break (l2_block_table: four groups by the quarter of the
 * screen the box is in, a count and entries of state, x, y, the cell that was there); returns l2_block_object with the entry in its $16, or 0 */
Object *l2_collision_hook(int16_t box_x0, int16_t box_y0, int16_t box_x1, int16_t box_y1)
{
    const struct L2Block *entry;
    uint16_t n;
    uint32_t quarter = 0;

    box_y0 = (int16_t)(box_y0 + xenon2.scroll_pos);
    box_y1 = (int16_t)(box_y1 + xenon2.scroll_pos);
    if (box_y0 >= 0x1b0)
        return 0;
    if (box_y0 <= 0x80)
        quarter |= 4;
    if (box_x0 <= 0x80)
        quarter |= 8;
    {
        const struct L2Group *group = (const struct L2Group *)(xenon2.l2_block_table + xenon2.level2_data.block_group_offsets[quarter / 4]);

        n = rd16(group->count.b);
        entry = group->entries;
    }
    for (; n != 0; n--, entry++) {
        int16_t x, y;

        if (rd16(entry->state.b) == 0)
            continue;
        x = (int16_t)rd16(entry->x.b);
        if (x > box_x1)
            return 0;
        y = (int16_t)rd16(entry->y.b);
        if (box_overlap(box_x0, box_y0, box_x1, box_y1, x, y, (int16_t)(x + 0xf), (int16_t)(y + 0xf))) {
            xenon2.l2_block_object.gfx = entry;
            return &xenon2.l2_block_object;
        }
    }
    return 0;
}

/* level2 $056FA8: the hit routine of l2_block_object: the block is broken (its entry's state 0, its cell of the map cleared) and bursts */
void l2_block_hit(Object *attacker, Object *target, uint16_t damage)
{
    struct L2Block *e = (struct L2Block *)xenon2.l2_block_object.gfx;
    uint16_t x = rd16(e->x.b), y = rd16(e->y.b);
    uint16_t cell;

    wr16(e->state.b, 0);
    cell = (uint16_t)((uint16_t)(y >> 3) * 0x14 + (x >> 3));
    wr16(xenon2.level_init_src + cell, 0);
    spawn_effect((int16_t)(x + 8), (int16_t)(y - xenon2.scroll_pos + 8), xenon2.anim_explosion, draw_bob_masked);
}

/* level2 $054E10, $055D8A: level events by event (rec = the record that fired): event 0 is the wave of $326C, 1 to 6 formations */
void l2_event(uint16_t event, const struct SpawnRec *rec)
{
    const struct SpawnDef *forms[6] = {
        &xenon2.level2_data.formation_1.defs, &xenon2.level2_data.formation_2.defs, &xenon2.level2_data.formation_3.defs,
        &xenon2.level2_data.formation_4.defs, &xenon2.level2_data.formation_5.defs, &xenon2.level2_data.formation_6.defs };

    if (event == 0) {
        spawn_wave_326c(0, rec);   /* $E5A */
        return;
    }
    if (event <= 6)
        spawn_formation(0, rec, forms[event - 1]);   /* $E60 */
}

/* level2 $054E18, $055B62: level hooks by hook (rec = the record that fired, rec_scroll its scroll value; w0c of the record says which side): 0 remembers the record,
 * 1 a turret $E8/$EC, 2 a marker $F0/$F4, 3 a launcher $100/$104, 4 and 5 markers $FC and $F8 */
void l2_hook(uint16_t hook, const struct SpawnRec *rec, uint16_t rec_scroll)
{
    int flag = rec->steps != 0;
    Object *obj;

    switch (hook) {
    case 0:
        note_spawn_record(rec, rec_scroll);   /* $EA2 */
        return;
    case 1:
        obj = spawn_marker_enemy(flag ? 0xe8 : 0xec, rec, (void (*)(void))l2_turret_update);
        obj->fn_hit = (void *)hit_tile;
        obj->energy = (int16_t)xenon2.level2_data.hook_energy;
        obj->hit[0] = 0x3e8;
        obj->hit[2] = 0x3e8;
        obj->gfx = (void *)(flag ? xenon2.level2_data.hook_picture_a : xenon2.level2_data.hook_picture_b);
        obj->anim = flag ? xenon2.level2_data.hook_anim_a : xenon2.level2_data.hook_anim_b;
        obj->map[0] = *(const Cell *)obj->gfx;
        obj->score = 0x64;
        obj->tile.cols = 0;
        obj->tile.rows = 0;
        return;
    case 2:
        obj = spawn_marker_front(flag ? 0xf0 : 0xf4, rec, (void (*)(void))l2_front_update);
        {
            const Cell *top = flag ? xenon2.level2_data.hook_cells_a : xenon2.level2_data.hook_cells_b;
            const Cell *bottom = flag ? xenon2.level2_data.hook_cells2_a : xenon2.level2_data.hook_cells2_b;

            obj->map[0] = top[0];
            obj->map[1] = top[1];
            obj->map[MAP_COLS] = bottom[0];
            obj->map[MAP_COLS + 1] = bottom[1];
        }
        return;
    case 3:
        obj = alloc_in(LIST_C);
        obj->type = flag ? 0x100 : 0x104;
        obj->fn_update = (obj_fn)l2_launcher_update;
        obj->fn_draw = draw_bob_masked;
        set_anim(obj, flag ? xenon2.level2_data.hook_fire_anim_a : xenon2.level2_data.hook_fire_anim_b);
        obj->shot_count = obj->shot_rate = 0;
        obj->fn_hit = (void *)hit_enemy;
        obj->energy = (int16_t)xenon2.level2_data.hook_energy_2;
        obj->big = 0;
        obj->score = 0xc8;
        obj->x = (int16_t)(rec->x - 8);
        obj->y = (int16_t)(rec->y - 8 - xenon2.scroll_pos);
        obj->launcher.step = -1;
        obj->launcher.range = (obj->launcher.range & 0xffffu) | (uint32_t)(uint16_t)((int16_t)(rec->count << 4)) << 16;
        obj->launcher.pos = (int16_t)(obj->launcher.range >> 16);
        obj->kind = 0;
        bbox_from_frame(obj);
        return;
    case 4:
        obj = spawn_marker_front(0xfc, rec, (void (*)(void))l2_marker_fc_update);
        obj->marker.launches = 1;
        obj->map[0] = xenon2.level2_data.hook_words[0];
        return;
    case 5:
        obj = spawn_marker_front(0xf8, rec, (void (*)(void))l2_marker_f8_update);
        obj->marker.launches = 1;
        return;
    }
}

/* level2 $054E24, $055B1E: the level's scroll events: the walls of debris while the first fight is not won, the stop of the scroll while it is */
void l2_scroll(void)
{
    if (xenon2.l2_first_fight_won == 0) {
        if ((int16_t)xenon2.scroll_pos >= 0x9d0 && (int16_t)xenon2.scroll_pos <= 0xb50)
            l2_debris_walls();
    } else if ((int16_t)xenon2.scroll_pos > 0x9f0) {
        xenon2.scroll_pos = 0x9f0;
        xenon2.scroll_max = 0x9f0;
        xenon2.map_row_offset = 0x18d8;
        xenon2.scroll_fine = 0;
    }
    xenon2.l2_left_wall_made = 0;
    xenon2.l2_right_wall_made = 0;
}

/* level2 $054E2C, $054E68: a change of players: the table of blocks and the second player's copy exchange */
void l2_swap_hook(void)
{
    uint8_t *blocks = xenon2.l2_block_table, *saved = xenon2.l2_blocks_player2;
    int i;

    for (i = 0; i < 0xb4; i++, blocks += 2, saved += 2) {
        uint8_t t0 = blocks[0], t1 = blocks[1];

        blocks[0] = saved[0];
        blocks[1] = saved[1];
        saved[0] = t0;
        saved[1] = t1;
    }
}

/* level2 $054E20, $05705E: level 2 starts: its data is copied once (and the table of blocks), the boss, the three gates and the walls' state are made */
void l2_init(void)
{
    static const uint16_t gate_at[3][2] = { { 9, 0xb8 }, { 4, 0xa4 }, { 0xe, 0xa4 } };
    static const uint16_t groups[4] = { 0, 0x62, 0xc4, 0x116 };
    Object *obj;
    int i, gate;

    if (xenon2.level_init_src != xenon2.level_data_dst) {
        for (i = 11999; i >= 0; i--)
            xenon2.level_data_dst[i] = xenon2.level_init_src[i];
        for (i = 359; i >= 0; i--)
            xenon2.l2_blocks[i] = (xenon2.level_init_src - 360)[i];
        xenon2.level_init_src = xenon2.level_data_dst;
        xenon2.l2_block_table = xenon2.l2_blocks;
    }
    xenon2.scroll_min = 0x9e0;
    xenon2.scroll_max = 0x11ff;
    xenon2.l2_first_fight_won = 0;
    obj = alloc_in(LIST_C);
    obj->type = 0x50;
    obj->fn_update = (obj_fn)l2_boss_update;
    obj->fn_draw = (obj_fn)l2_boss_draw;
    obj->fn_hit = (void *)l2_boss_hit;
    obj->shot_count = obj->shot_rate = 0;
    obj->persists = -1;
    obj->screen_fixed = -1;
    obj->y = 0x60;
    obj->energy = (int16_t)xenon2.level2_data.init_energy;
    obj->l2_boss.wait = -1;
    obj->l2_boss.frames = 0;
    obj->l2_boss_b.flash = 0;
    obj->l2_boss.cell = 0;
    obj->kind = 0;
    for (gate = 2; gate >= 0; gate--) {
        uint16_t x = gate_at[2 - gate][0], y = gate_at[2 - gate][1];

        obj = alloc_in(LIST_C);
        obj->type = 0x54;
        obj->fn_update = (obj_fn)l2_gate_update;
        obj->fn_draw = (obj_fn)obj_nop_thunk;
        obj->fn_hit = (void *)l2_gate_hit;
        obj->energy = (int16_t)xenon2.level2_data.init_energy_2;
        obj->persists = -1;
        obj->screen_fixed = -1;
        obj->shot_count = obj->shot_rate = 0;
        obj->x = (int16_t)x;
        obj->y = (int16_t)y;
        obj->gfx = xenon2.level_init_src + (uint16_t)(((uint16_t)(y * 0x14) + x) * 2);
        obj->gate.step = 0;
        obj->gate.number = (int16_t)gate;
    }
    xenon2.l2_gates_left = 3;
    xenon2.l2_walls_made = 0;
    xenon2.l2_walls_spare = 0;
    xenon2.l2_left_wall_made = 0;
    xenon2.l2_right_wall_made = 0;
    xenon2.l2_debris_waves = 2;
    for (i = 0; i < 8; i++)
        xenon2.level_vars[i] = 0;
    for (i = 0; i < 4; i++) {
        uint8_t *g = xenon2.l2_block_table + groups[i];
        uint16_t n = rd16(g);

        for (g += 2; n != 0; n--, g += 8)
            wr16(g, 1);
    }
    xenon2.l2_block_object.type = 0;
    xenon2.l2_block_object.prev = &xenon2.l2_block_object;
    xenon2.l2_block_object.next = &xenon2.l2_block_object;
    xenon2.l2_block_object.fn_update = (obj_fn)obj_nop_thunk;
    xenon2.l2_block_object.fn_draw = (obj_fn)obj_nop_thunk;
    xenon2.l2_block_object.fn_hit = (void *)l2_block_hit;
    xenon2.unit_seed[0] = 0x45d272ba;
    xenon2.unit_seed[1] = 0xaa6a4a56;
    xenon2.difficulty_x1 = (uint16_t)(xenon2.difficulty * 1);
    xenon2.difficulty_x2 = (uint16_t)(xenon2.difficulty * 2);
    for (i = 0; i < 0xb4; i++) {
        xenon2.l2_blocks_player2[2 * i] = xenon2.l2_block_table[2 * i];
        xenon2.l2_blocks_player2[2 * i + 1] = xenon2.l2_block_table[2 * i + 1];
    }
}

/* ---- the turrets, launchers and what the hooks put into the level ---- */

/* level2 $056596: a piece of the marker $F0 / $F4: every 16 steps it turns towards the player (eight directions), moves by the table,
 * and on the player's box hurts him; after 80 steps (v28), or when it leaves the screen, it is gone, and in the first case, or when the
 * player is shielded, it bursts */
void l2_front_piece_update(Object *part)
{
    static const int16_t dx[8] = { -4, -5, -4, 0, 4, 5, 4, 0 };
    static const int16_t dy[8] = { -4, 0, 4, 5, 4, 0, -4, -5 };
    int burst = 0;

    anim_step(part);
    part->front.tick++;
    if (part->front.tick >= 0x50) {
        burst = 1;
    } else {
        int16_t x, y;

        if ((part->front.tick & 0xf) == 0) {
            uint16_t d = direction8((int16_t)(PLAYER_X - part->x), (int16_t)(PLAYER_Y - part->y));

            part->front.dir = (part->front.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)(~d & 7)) << 16;
            set_anim(part, xenon2.level2_data.front_anims_by_dir[(uint16_t)(int16_t)(part->front.dir >> 16)]);
        }
        x = (int16_t)(dx[(int16_t)(part->front.dir >> 16)] + part->x);
        if ((uint16_t)x >= 0x140) {
            kill_thunk(part);
            return;
        }
        part->x = x;
        y = (int16_t)(dy[(int16_t)(part->front.dir >> 16)] + part->y + xenon2.scroll_speed);
        if ((uint16_t)y >= 0xc0) {
            kill_thunk(part);
            return;
        }
        part->y = y;
        bbox_from_frame(part);
        if (!point_in_player_box(x, y))
            return;
        if (xenon2.shield_timer != 0) {
            burst = 1;
        } else {
            PLAYER->fn_draw = draw_bob_or_first;
            player_damage(part, 4);
            kill_thunk(part);
            return;
        }
    }
    if (burst) {
        const struct Pic *f = part->gfx;
        int16_t ex = (int16_t)(((int16_t)(part->x - (int16_t)rd16(f->hot_x.b)) * 2 + (int16_t)rd16(f->width.b)) >> 1);
        int16_t ey = (int16_t)(((int16_t)(part->y - (int16_t)rd16(f->hot_y.b)) * 2 + (int16_t)rd16(f->height.b)) >> 1);

        spawn_effect(ex, ey, xenon2.anim_explosion, draw_bob_masked);
    }
    kill_thunk(part);
}

/* level2 $056144: the flying enemy $E0 / $E4: it comes down the screen to y $32, drifts towards the player's x and, within 10 of it,
 * dives (v28 1) and rises again; the offsets are from a table indexed by the frame and the object's address */
void l2_flyer_update(Object *enemy)
{
    static const int16_t wobble[8] = { 0, -2, -4, -2, 0, 2, 4, 2 };
    uint16_t phase = (uint16_t)(xenon2.frame_tick + (uint16_t)GADDR(enemy));
    int16_t dist, new_y;

    anim_step(enemy);
    if (enemy->flyer.dive == 0) {
        if (enemy->y > 0x32) {
            enemy->y -= 5;
            enemy->x = (int16_t)(enemy->x + wobble[(phase >> 2) & 7]);
            bbox_from_frame(enemy);
            return;
        }
        dist = (int16_t)(enemy->x - PLAYER_X);
        if (dist < 0)
            dist = (int16_t)-dist;
        if (dist < 10) {
            enemy->flyer.dive = 1;
            set_anim(enemy, enemy->type == 0xe0 ? xenon2.level2_data.flyer_anim_e0 : xenon2.level2_data.flyer_anim_other);
            bbox_from_frame(enemy);
            return;
        }
        enemy->x = (int16_t)(enemy->x - (enemy->x >= PLAYER_X ? 4 : -4));
        new_y = (int16_t)(wobble[(phase & 0xe) >> 1] + enemy->y);
        if (new_y > 0x32)
            new_y = 0x32;
        enemy->y = new_y;
        bbox_from_frame(enemy);
        return;
    }
    enemy->y += 0xc;
    if (enemy->y >= 0xa0) {
        enemy->flyer.dive = 0;
        set_anim(enemy, enemy->type == 0xe0 ? xenon2.level2_data.flyer_anim2_e0 : xenon2.level2_data.flyer_anim2_other);
    }
    bbox_from_frame(enemy);
}


/* level2 $05533E: the marker $F8: as $05550E, but its picture blinks (v28 0 / 1) until the scroll is near, its steps use a table of
 * two longs, and at step 4 it launches the flyer $E0 */
void l2_marker_f8_update(Object *marker)
{
    int16_t rel_y;
    const Cell *cells;

    if ((int16_t)((int16_t)xenon2.scroll_max + 0xd0) <= marker->y) {
        kill_thunk(marker);
        return;
    }
    rel_y = (int16_t)(marker->y - xenon2.scroll_pos);
    if (rel_y >= 0xb4)
        return;
    if (marker->marker_f8.blink >= 5) {
        if (marker->marker.launches == 0) {
            kill_thunk(marker);
            return;
        }
        if (marker->marker_f8.blink >= 9) {
            marker->marker_f8.blink = 0;
            marker->marker.launches--;
        }
    }
    if ((uint16_t)marker->marker_f8.blink <= 1) {
        if (rel_y >= 0x64)
            marker->marker_f8.blink = 2;
        else if ((xenon2.frame_tick & 3) == 0)
            marker->marker_f8.blink ^= 1;
    }
    cells = xenon2.level2_data.marker_f8_cells + ((uint16_t)marker->marker_f8.blink << 2);
    marker->map[0] = cells[0];
    marker->map[1] = cells[1];
    marker->map[MAP_COLS] = cells[2];
    marker->map[MAP_COLS + 1] = cells[3];
    if ((uint16_t)marker->marker_f8.blink < 2)
        return;
    if (xenon2.frame_tick & 3)
        return;
    marker->marker_f8.blink++;
    if (marker->marker_f8.blink != 4)
        return;
    {
        Object *flyer = alloc_in(LIST_C);

        flyer->type = 0xe0;
        flyer->fn_update = (obj_fn)l2_flyer_update;
        flyer->fn_draw = draw_bob_masked;
        flyer->fn_hit = (void *)hit_enemy;
        set_anim(flyer, xenon2.level2_data.flyer_anim2_e0);
        flyer->shot_count = flyer->shot_rate = 0;
        flyer->x = (int16_t)(marker->x + 0x10);
        flyer->y = (int16_t)(marker->y - xenon2.scroll_pos + 0x10);
        flyer->energy = (int16_t)xenon2.level2_data.marker_f8_energy;
        flyer->big = (int8_t)0xff;
        flyer->score = 0x12c;
        flyer->marker_f8.blink = 0;
        flyer->kind = 0;
        bbox_from_frame(flyer);
    }
}


/* level2 $05550E: the marker $FC: gone when the scroll has passed it; its picture steps (v28) and at step 4 it launches the flyer $E4;
 * v30 counts the launches left */
void l2_marker_fc_update(Object *marker)
{
    int16_t rel_y;

    if ((int16_t)((int16_t)xenon2.scroll_max + 0xd0) <= marker->y) {
        kill_thunk(marker);
        return;
    }
    rel_y = (int16_t)(marker->y - xenon2.scroll_pos);
    if (rel_y >= 0xb4)
        return;
    if (marker->marker_fc.step >= 5) {
        if (marker->marker_fc.launches == 0) {
            kill_thunk(marker);
            return;
        }
        if (marker->marker_fc.step >= 9) {
            marker->marker_fc.step = 0;
            marker->marker_fc.launches--;
        }
    }
    if ((uint16_t)marker->marker_fc.step <= 1) {
        if (rel_y < 0x64)
            return;
        marker->marker_fc.step = 2;
    }
    marker->map[0] = xenon2.level2_data.hook_words[(uint16_t)marker->marker_fc.step];
    if (xenon2.frame_tick & 3)
        return;
    marker->marker_fc.step++;
    if (marker->marker_fc.step != 4)
        return;
    {
        Object *flyer = alloc_in(LIST_C);

        flyer->type = 0xe4;
        flyer->fn_update = (obj_fn)l2_flyer_update;
        flyer->fn_draw = draw_bob_masked;
        flyer->fn_hit = (void *)hit_enemy;
        set_anim(flyer, xenon2.level2_data.flyer_anim2_other);
        flyer->shot_count = flyer->shot_rate = 0;
        flyer->x = marker->x;
        flyer->y = (int16_t)(marker->y - xenon2.scroll_pos);
        flyer->energy = (int16_t)xenon2.level2_data.marker_fc_energy;
        flyer->big = 0;
        flyer->score = 0x96;
        flyer->marker_fc.step = 0;
        flyer->kind = 0;
        bbox_from_frame(flyer);
    }
}

/* level2 $056FEC: the first block that is broken (its entry's state 0) and meets the box of bolt comes back: state 1, its cell of the map
 * written again */
static void l2_restore_blocks(Object *bolt)
{
    int16_t box_x0 = bolt->hit[0], box_y0 = (int16_t)(bolt->hit[1] + xenon2.scroll_pos), box_x1 = bolt->hit[2], box_y1 = (int16_t)(bolt->hit[3] + xenon2.scroll_pos);
    uint32_t quarter = 0;
    struct L2Block *entry;
    uint16_t n;

    if (box_y0 <= 0x80)
        quarter |= 4;
    if (box_x0 <= 0x80)
        quarter |= 8;
    {
        struct L2Group *group = (struct L2Group *)(xenon2.l2_block_table + xenon2.level2_data.block_group_offsets[quarter / 4]);

        n = rd16(group->count.b);
        entry = group->entries;
    }
    for (; n != 0; n--, entry++) {
        struct L2Block *e = entry;
        int16_t x, y;

        if (rd16(e->state.b) != 0)
            continue;
        x = (int16_t)rd16(e->x.b);
        y = (int16_t)rd16(e->y.b);
        if (!box_overlap(box_x0, box_y0, box_x1, box_y1, x, y, (int16_t)(x + 0xf), (int16_t)(y + 0xf)))
            continue;
        wr16(e->state.b, 1);
        wr16(xenon2.level_init_src + (uint16_t)(((uint16_t)y >> 3) * 0x14 + ((uint16_t)x >> 3)), rd16(e->cell.b));
        return;
    }
}


/* level2 $0563E0: the creature's bolt ($110): it turns (v2a, eight directions) towards v28, which is -1 when it flies on; it shoots
 * every so often, moves one step by the table, and at the corners of the table of $056524 takes a new direction; it breaks blocks back
 * into place */
void l2_roamer_shot_update(Object *bolt)
{
    static const int16_t dx[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    static const int16_t dy[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
    int16_t x, y, diff;
    uint8_t carry;
    int n;

    anim_step(bolt);
    bolt->y = (int16_t)(bolt->y + xenon2.scroll_speed);
    if (bolt->bolt.target >= 0) {
        if (!(xenon2.frame_tick & 1)) {
            bbox_from_frame(bolt);
            return;
        }
        diff = (int16_t)(bolt->bolt.target - (int16_t)(bolt->bolt.dir >> 16));
        if (diff != 0) {
            if (diff < 0)
                diff += 8;
            bolt->bolt.dir = (bolt->bolt.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)(((int16_t)(bolt->bolt.dir >> 16) + (diff > 4 ? -1 : 1)) & 7)) << 16;
            set_anim(bolt, xenon2.level2_data.bolt_anims_by_dir[(uint16_t)(int16_t)(bolt->bolt.dir >> 16)]);
            bbox_from_frame(bolt);
            return;
        }
        bolt->bolt.target = -1;
    }
    if ((uint16_t)bolt->y < 0xc0) {
        uint16_t sum = (uint16_t)(bolt->shot_count + xenon2.level2_data.roamer_shot_fire_rate);

        bolt->shot_count = (uint8_t)sum;
        carry = sum > 0xff;
        if (carry) {
            uint16_t param = xenon2.level2_data.roamer_shot_param;
            uint16_t angle;
            const void *gfx;

            bolt->shot_count = (uint8_t)(random_next() & 0x3f);
            gfx = GPTR(rd32(xenon2.level2_data.shot_gfx_table[param]));
            angle = direction8((int16_t)(PLAYER_X - bolt->x), (int16_t)(PLAYER_Y - bolt->y));
            spawn_enemy_shot((uint16_t)bolt->x, (uint16_t)bolt->y, angle, xenon2.level2_data.roamer_shot_shot_steps, gfx);
        }
    }
    x = (int16_t)(bolt->x + dx[(int16_t)(bolt->bolt.dir >> 16)]);
    y = (int16_t)(bolt->y + dy[(int16_t)(bolt->bolt.dir >> 16)]);
    bolt->x = x;
    bolt->y = y;
    y = (int16_t)(y + xenon2.scroll_pos);
    for (n = 0; n < 8; n++) {
        const struct Level2RoamerNode *entry = &xenon2.level2_data.roamer_shot_entries[n];

        if (entry->x == x && entry->y == y) {
            uint16_t r;

            do
                r = (uint16_t)(random_next() & 3);
            while (r == 0);
            bolt->bolt.target = (int16_t)entry->targets[r - 1];
            bbox_from_frame(bolt);
            return;
        }
    }
    bbox_from_frame(bolt);
    l2_restore_blocks(bolt);
}


/* level2 $05623E: the creature that crosses the screen between the walls (v2a its direction, v28 0 moving, 1 at a wall, 2 shooting): at the
 * frame $5B5D6 of its animation it launches the bolt $110 in its direction and is gone */
void l2_roamer_update(Object *roamer)
{
    static const int16_t dx[8] = { 0, 3, 4, 3, 0, -3, -4, -3 };
    static const int16_t dy[8] = { -4, -3, 0, 3, 4, 3, 0, -3 };

    anim_step(roamer);
    roamer->y = (int16_t)(roamer->y + xenon2.scroll_speed);
    if (roamer->roamer.state == 0) {
        int16_t x = (int16_t)(roamer->x + dx[(int16_t)(roamer->roamer.dir >> 16)]);
        int16_t y = (int16_t)(roamer->y + dy[(int16_t)(roamer->roamer.dir >> 16)]);

        if (x <= 0x74) {
            x = 0x74;
            roamer->roamer.state = 1;
            roamer->roamer.dir = (roamer->roamer.dir & 0xffffu) | (uint32_t)(uint16_t)(4) << 16;
        }
        if (x >= 0xcc) {
            x = 0xcc;
            roamer->roamer.state = 1;
            roamer->roamer.dir = (roamer->roamer.dir & 0xffffu) | (uint32_t)(uint16_t)(0) << 16;
        }
        y = (int16_t)(y + xenon2.scroll_pos);
        if (y <= 0x6c) {
            y = 0x6c;
            roamer->roamer.state = 1;
            roamer->roamer.dir = (roamer->roamer.dir & 0xffffu) | (uint32_t)(uint16_t)(6) << 16;
        }
        if (y >= 0x154) {
            y = 0x154;
            roamer->roamer.state = 1;
            roamer->roamer.dir = (roamer->roamer.dir & 0xffffu) | (uint32_t)(uint16_t)(2) << 16;
        }
        roamer->x = x;
        roamer->y = (int16_t)(y - xenon2.scroll_pos);
        return;
    }
    if (roamer->roamer.state == 1) {
        if ((random_next() & 7) == 0) {
            roamer->roamer.state = 2;
            set_anim(roamer, xenon2.level2_data.roamer_anim);
        }
        return;
    }
    if (roamer->gfx == (void *)xenon2.level2_data.roamer_picture) {
        Object *bolt = alloc_in(LIST_C);

        bolt->type = 0x110;
        bolt->fn_update = (obj_fn)l2_roamer_shot_update;
        bolt->fn_draw = draw_bob_masked;
        bolt->fn_hit = (void *)hit_enemy;
        bolt->energy = (int16_t)xenon2.level2_data.roamer_energy;
        bolt->big = 0;
        bolt->shot_count = bolt->shot_rate = 0;
        bolt->score = 0x1e;
        bolt->kind = 0;
        set_anim(bolt, xenon2.level2_data.bolt_anims_by_dir[(uint16_t)(int16_t)(roamer->roamer.dir >> 16)]);
        bolt->x = roamer->x;
        bolt->y = roamer->y;
        bolt->roamer.dir = (bolt->roamer.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)(roamer->roamer.dir >> 16)) << 16;
        bolt->roamer.state = -1;
        bbox_from_frame(bolt);
        kill_thunk(roamer);
    }
}


/* level2 $05568C: the marker $F0 / $F4: waits for the scroll, then its picture takes 20 steps (v28) from a table, and it bursts into
 * eight pieces ($108, update $056596) */
void l2_front_update(Object *marker)
{
    int i;

    if (marker->front.step == 0) {
        if ((int16_t)(marker->y - xenon2.scroll_pos) >= (int16_t)xenon2.level2_data.front_param)
            marker->front.step = 1;
        return;
    }
    {
        const uint8_t *const *table = marker->type == 0xf0 ? xenon2.level2_data.front_table_f0 : xenon2.level2_data.front_table_other;
        const Cell *cells = (const Cell *)table[(uint16_t)marker->front.step];

        marker->map[0] = cells[0];
        marker->map[1] = cells[1];
        marker->map[MAP_COLS] = cells[2];
        marker->map[MAP_COLS + 1] = cells[3];
    }
    marker->front.step++;
    if (marker->front.step < 0x15)
        return;
    for (i = 7; i >= 0; i--) {
        const struct Level2FrontEntry *entry = &xenon2.level2_data.front_entries[7 - i];
        Object *part = alloc_in(LIST_C);

        part->type = 0x108;
        part->fn_update = (obj_fn)l2_front_piece_update;
        part->fn_draw = draw_bob_masked;
        part->fn_hit = (void *)hit_explode;
        set_anim(part, entry->anim);
        part->shot_count = part->shot_rate = 0;
        part->x = (int16_t)(marker->x + 0x10 + entry->dx);
        part->y = (int16_t)(marker->y - xenon2.scroll_pos + 0x10 + entry->dy);
        part->front.dir = (part->front.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)i) << 16;
        part->energy = 1;
        part->big = 0;
        part->score = 0x14;
        part->front.step = (int16_t)(random_next() & 0x1f);
        part->kind = 0;
        bbox_from_frame(part);
    }
    kill_thunk(marker);
}


/* the turret's use of the scratch fields of Object */

/* level2 $0558E0: update of the turret $E8 / $EC: its box, the cells it opens and closes (v28 is the step), and at step 7 a shot */
void l2_turret_update(Object *turret)
{
    int16_t left, top, shot_x, shot_y, dir;
    uint32_t pic;

    if ((int16_t)(xenon2.scroll_max + 0xd0) < turret->y) {
        kill_thunk(turret);
        return;
    }
    left = (int16_t)(turret->x - 8);
    turret->hit[0] = left;
    turret->hit[2] = (int16_t)(left + 0x14);
    top = (int16_t)(turret->y - xenon2.scroll_pos + 4);
    turret->hit[1] = top;
    turret->hit[3] = (int16_t)(top + 8);
    if (turret->turret.step == 0) {
        if ((uint8_t)random_next() < xenon2.level2_data.turret_chance)
            turret->turret.step = 1;
        return;
    }
    turret->map[0] = ((const Cell *)(turret->type == 0xe8 ? xenon2.level2_data.hook_picture_a : xenon2.level2_data.hook_picture_b))[(uint16_t)turret->turret.step];
    turret->turret.step++;
    if (turret->turret.step != 7) {
        if (turret->turret.step == 0xf)
            turret->turret.step = 0;
        return;
    }
    shot_x = (int16_t)(turret->x + 8);
    shot_y = (int16_t)(turret->y - xenon2.scroll_pos + 8);
    pic = rd32(xenon2.level2_data.shot_gfx_table[xenon2.level2_data.turret_param]);
    dir = turret->type == 0xec ? 2 : 6;
    spawn_enemy_shot((uint16_t)shot_x, (uint16_t)shot_y, (uint16_t)dir, xenon2.level2_data.turret_shot_steps, GPTR(pic));
}

/* the launcher's use of the scratch fields of Object */

/* level2 $055A04: update of the launcher $100 / $104: it moves up and down (v28 is its step, v30 its place between 0 and v2a); when the
 * player is level with it (now and then) it goes into list B and fires a shot */
void l2_launcher_update(Object *launcher)
{
    int16_t dist, shot_x, shot_y, dir, step;

    if ((int16_t)(xenon2.scroll_max + 0xd0 - xenon2.scroll_pos) < launcher->y) {
        kill_thunk(launcher);
        return;
    }
    anim_step(launcher);
    launcher->y = (int16_t)(launcher->y + xenon2.scroll_speed);
    if (launcher->launcher.step == 0)
        return;
    dist = (int16_t)(launcher->y - PLAYER_Y);
    if (dist <= 0xa && dist >= -0x1e && (uint8_t)random_next() < 0x1e) {
        Object *tail;

        launcher->launcher.step = 0;
        launcher->next->prev = launcher->prev;
        launcher->prev->next = launcher->next;
        tail = LIST_B->prev;
        LIST_B->prev = launcher;
        launcher->prev = tail;
        launcher->next = LIST_B;
        tail->next = launcher;
        set_anim(launcher, launcher->type == 0x100 ? xenon2.level2_data.launcher_anim_100 : xenon2.level2_data.launcher_anim_other);
        shot_x = (int16_t)(launcher->x + 0xb);
        shot_y = (int16_t)(launcher->y + 7);
        dir = 6;
        if (launcher->type != 0x100) {
            dir = 2;
            shot_x = (int16_t)(shot_x - 8);
        }
        spawn_enemy_shot((uint16_t)shot_x, (uint16_t)shot_y, (uint16_t)dir, xenon2.level2_data.launcher_shot_steps, (void *)xenon2.level2_data.launcher_shot_picture);
        return;
    }
    step = launcher->launcher.step;
    launcher->y = (int16_t)(launcher->y + step);
    launcher->launcher.pos = (int16_t)(launcher->launcher.pos + step);
    if (launcher->launcher.pos == 0 || launcher->launcher.pos == (int16_t)(launcher->launcher.range >> 16))
        launcher->launcher.step = (int16_t)-launcher->launcher.step;
    bbox_from_frame(launcher);
}

/* the end of $055DEC, $055E0E and $055E28: the object obj bursts into a wreck (animation anim) that flies away, the object goes */
static void l2_wreck(Object *obj, const uint8_t *anim)
{
    Object *wreck = alloc_in(LIST_B);

    wreck->type = obj->type;
    wreck->fn_update = obj->fn_update;
    wreck->fn_draw = draw_bob_masked;
    wreck->x = obj->x;
    wreck->y = obj->y;
    set_anim(wreck, anim);
    wreck->piece.wait = (int16_t)0x8000;
    wreck->piece.dir = (uint16_t)((int16_t)(random_next() & 0xff));
    kill_thunk(obj);
}

/* level2 $055DEC: hit routine of a piece that lights a bit of the gates' state when it goes */
void l2_hit_a(Object *attacker, Object *target, uint16_t damage)
{
    target->fn_draw = draw_bob_or_first;
    if ((int16_t)damage < target->energy) {
        hit_enemy(attacker, target, damage);
        return;
    }
    xenon2.l2_walls_made &= (uint8_t)~(1 << (target->piece.wall & 7));
    l2_wreck(target, xenon2.level2_data.wreck_a);
}

/* level2 $055E0E: hit routine of a piece */
void l2_hit_b(Object *attacker, Object *target, uint16_t damage)
{
    target->fn_draw = draw_bob_or_first;
    if ((int16_t)damage < target->energy) {
        hit_enemy(attacker, target, damage);
        return;
    }
    l2_wreck(target, xenon2.level2_data.wreck_b);
}

/* level2 $055E28: hit routine of a piece */
void l2_hit_c(Object *attacker, Object *target, uint16_t damage)
{
    target->fn_draw = draw_bob_or_first;
    if ((int16_t)damage < target->energy) {
        hit_enemy(attacker, target, damage);
        return;
    }
    l2_wreck(target, xenon2.level2_data.wreck_c);
}

/* ---- the walls of debris ---- */

/* orig $05605A (level2): a wall of debris side (0 left, 1 right) comes in: twelve pieces on one path of the table at $55F56 chosen by the generator
 * at $D46 (side 0) or $D4A (side 1), one after another (the wait in $28 counts down by ten) */
static void l2_debris_wall(uint16_t side)
{
    uint32_t r;
    uint16_t x, y, index, wait, var, k;
    const uint8_t *path;
    const struct Level2DebrisWall *w;
    const struct Level2WallPiece *spec;
    Object *obj;

    if (xenon2.l2_gates_left == 1) {
        r = ror5(&xenon2.unit_seed[side ? 1 : 0]) & 6;
    } else if (xenon2.l2_debris_waves != 0) {
        xenon2.l2_debris_waves--;
        r = ror5(&xenon2.unit_seed[side ? 1 : 0]) & 6;
    } else {
        r = (ror5(&xenon2.unit_seed[side ? 1 : 0]) & 6) + 8;
    }
    w = &xenon2.level2_data.debris_wall_records[(uint16_t)(r + side)];
    x = w->x;
    y = (uint16_t)(w->y - xenon2.scroll_step - xenon2.scroll_pos);
    index = w->index;
    if (xenon2.level_vars[index - 1] == 0)
        xenon2.level_vars[index - 1] = 0x18;
    var = w->var;
    path = w->path;
    wait = 0;
    spec = xenon2.level2_data.wall_pieces.pieces;
    for (k = xenon2.level2_data.wall_pieces.last; k != 0xffff; k--) {
        obj = alloc_in(LIST_C);
        obj->type = spec->type;
        obj->fn_update = (obj_fn)l2_piece_update;
        obj->fn_draw = (obj_fn)obj_nop_thunk;
        obj->fn_hit = (void *)spec->hit;
        obj->gfx = (void *)spec->gfx;
        obj->score = (int16_t)spec->score;
        obj->piece.var = (int16_t)var;
        obj->energy = (int16_t)xenon2.level2_data.debris_wall_energy;
        obj->shot_count = obj->shot_rate = 0;
        obj->big = 0;
        obj->anim_delay = 0;
        obj->script = (uint8_t *)path;
        obj->cursor = (const struct PathCmd *)path;
        obj->x = (int16_t)x;
        obj->y = (int16_t)y;
        obj->kind = 0;
        obj->piece.wait = (int16_t)wait;
        obj->steps = (int16_t)xenon2.level2_data.debris_wall_steps;
        obj->piece.wall = (int16_t)side;
        wait = (uint16_t)(wait - 0xa);
        bbox_from_frame(obj);
        spec++;
    }
    xenon2.l2_walls_made |= (uint8_t)(1 << (side & 7));
}

/* level2 $056016: the walls of debris that are not in the field (l2_left_wall_made, l2_right_wall_made say a piece of each moved this frame) are made */
void l2_debris_walls(void)
{
    if (xenon2.l2_first_fight_won != 0)
        return;
    if (xenon2.l2_left_wall_made == 0) {
        l2_debris_wall(0);
        xenon2.l2_left_wall_made = 0xff;
    }
    if (xenon2.l2_right_wall_made == 0) {
        l2_debris_wall(1);
        xenon2.l2_right_wall_made = 0xff;
    }
}


/* level2 $0566CE: update of a piece of a wall of debris ($114 to $11C): v28 $8000 is a wreck flying off in the direction of v2c; otherwise
 * it follows its path (a wait while v28 is negative), is drawn behind the map while the path is inside it, and its picture goes by the direction */
void l2_piece_update(Object *part)
{
    const struct PathCmd *cursor;
    uint16_t angle, pic_index;

    if (part->piece.wait == (int16_t)0x8000) {
        int16_t new_x, new_y;

        anim_step(part);
        angle = (uint16_t)(int16_t)part->piece.dir;
        new_y = (int16_t)(xenon2.piece_directions[angle] >> 2);
        new_x = (int16_t)(xenon2.piece_directions[(uint16_t)(angle + 0x40)] >> 2);
        new_x = (int16_t)(new_x + part->x);
        new_y = (int16_t)(new_y + part->y);
        if ((uint16_t)new_x >= 0x140 || (uint16_t)new_y >= 0xc0) {
            kill_thunk(part);
            return;
        }
        part->x = new_x;
        part->y = new_y;
        part->fn_draw = draw_bob_masked;
        return;
    }
    part->y = (int16_t)(part->y + xenon2.scroll_speed);
    (&xenon2.l2_left_wall_made)[(uint16_t)part->piece.wall] = 0xff;
    enemy_update_no_anim(part);
    if (part->type == 4) {
        uint16_t *c = &xenon2.level_vars[(uint16_t)part->piece.var - 1];

        if (*c == 0)
            *c = 0x18;
        return;
    }
    if (part->fn_draw == (obj_fn)obj_nop_thunk) {
        if (part->piece.wait >= 0)
            part->fn_draw = draw_bob_behind_map;
    } else if (part->fn_draw == draw_bob_behind_map) {
        cursor = part->cursor;
        if (rd16(cursor->op.b) != 0 && cursor != (const struct PathCmd *)(&((const struct PathCmd *)part->script)->arg.move + 1))
            part->fn_draw = draw_bob_masked;
    } else if (rd16(part->cursor->op.b) == 0) {
        part->fn_draw = draw_bob_behind_map;
    }
    pic_index = (uint16_t)(((uint16_t)((int16_t)part->piece.dir + 0x10) >> 3) & 0x1c);
    part->gfx = (void *)(part->type == 0x118 ? xenon2.level2_data.piece_pictures_118 : part->type == 0x11c ? xenon2.level2_data.piece_pictures_11c : xenon2.level2_data.piece_pictures_other)[pic_index / 4];
    bbox_from_frame(part);
}

/* ---- the first fight: three gates and the boss ---- */

/* the gate's use of the scratch fields of Object */

/* level2 $056A4A: update of a gate ($54, v30 its number): its box once the player can reach it, its cells open and close (v28 is the step);
 * the gate 2 also patches the map by the counters in level_vars (eight records at $56B58) */
void l2_gate_update(Object *gate)
{
    int hide, step;
    int16_t edge, frame, px = PLAYER_X;

    if ((int16_t)xenon2.scroll_step_target < 0 && (int16_t)xenon2.scroll_max <= 0xb40)
        xenon2.scroll_max = 0xb40;
    if (gate->gate.number == 2)
        hide = xenon2.l2_gates_left != 1;
    else if (gate->gate.number == 1)
        hide = px >= 0xa0 || xenon2.l2_walls_made == 3;
    else
        hide = px < 0xa0 || xenon2.l2_walls_made == 3;
    if (hide) {
        gate->hit[0] = 0x3e8;
        gate->hit[2] = 0x3e8;
        step = 1;
    } else {
        edge = (int16_t)(gate->x << 4);
        gate->hit[0] = edge;
        gate->hit[2] = (int16_t)(edge + 0xf);
        edge = (int16_t)((gate->y << 4) - xenon2.scroll_pos);
        gate->hit[1] = edge;
        gate->hit[3] = (int16_t)(edge + 0xf);
        step = gate->gate.step != 4 || xenon2.l2_walls_made == 3;
    }
    if (step && !(xenon2.frame_tick & 1))
        gate->gate.step = (int16_t)((gate->gate.step + 1) & 7);
    frame = gate->gate.step;
    if (frame > 4)
        frame = (int16_t)(8 - frame);
    *(uint16_t *)((uint8_t *)gate->gfx) = *(const uint16_t *)(xenon2.level2_data.gate_number_cells + 2 * frame);
    if (gate->gate.number == 2) {
        const uint8_t *cells = xenon2.level2_data.gate_cells;
        uint16_t *counter = xenon2.level_vars;
        int i;

        for (i = 7; i >= 0; i--, counter++, cells += 0x12) {
            uint8_t *dst;
            const uint8_t *src;

            if (*counter == 0)
                continue;
            dst = xenon2.level_init_src + (int16_t)rd16(cells);
            (*counter)--;
            src = cells + ((*counter & 2) ? 0xa : 2);
            *(uint32_t *)(dst) = *(const uint32_t *)(src);
            *(uint32_t *)(dst + 0x28) = *(const uint32_t *)(src + 4);
        }
    }
}

/* level2 $0568C6: the draw of a gate that was hit: its cells (by its number) flash once */
void l2_gate_flash(Object *gate)
{
    const uint8_t *cells = xenon2.level2_data.gate_flash_cells_a;
    uint16_t rows = 1;
    int16_t y = (int16_t)((gate->y << 4) - xenon2.scroll_pos - 0x10);
    uint16_t x = (uint16_t)((gate->x << 3) - 0x10);

    gate->fn_draw = (obj_fn)obj_nop_thunk;
    if (gate->gate.number != 0) {
        x = (uint16_t)(x + 0x10);
        cells = xenon2.level2_data.gate_flash_cells_b;
        if (gate->gate.number != 1) {
            cells = xenon2.level2_data.gate_flash_cells_c;
            rows = 2;
            y = (int16_t)(y - 0x10);
            x = (uint16_t)(x - 8);
        }
    }
    draw_tile_grid_flash(cells, 2, rows, y, x);
}

/* level2 $056944: hit routine of a gate: when it is destroyed its cell is cleared, the counter of gates goes down; with one left the
 * scroll is held and with none the first fight is won: everything but the boss goes, the limit of the scroll is lifted */
void l2_gate_hit(Object *attacker, Object *gate, uint16_t damage)
{
    uint16_t old;
    Object *e;
    int i;

    if (xenon2.l2_walls_made == 3)
        return;
    gate->fn_draw = (obj_fn)l2_gate_flash;
    old = (uint16_t)gate->energy;
    gate->energy = (int16_t)(old - damage);
    if (old > damage)
        return;
    xenon2.l2_walls_made = 3;
    wr16((uint8_t *)gate->gfx, 0x8e45);
    spawn_effect((int16_t)(gate->hit[0] + 8), (int16_t)(gate->hit[1] + 8), xenon2.anim_explosion, draw_bob_masked);
    kill_thunk(gate);
    xenon2.l2_gates_left--;
    if (xenon2.l2_gates_left == 1) {
        xenon2.scroll_step_target = 0xffff;
        xenon2.unit_seed[0] = 0xd25135d3;
        xenon2.unit_seed[1] = 0x894d5d53;
    }
    if (xenon2.l2_gates_left != 0)
        return;
    xenon2.scroll_step_target = 1;
    for (i = 0; i < 0x1cc * 2; i++)
        xenon2.level_init_src[0x1950 + i] = 0;
    for (e = LIST_HEAD(LIST_ENEMY); e->type != 0;) {
        Object *next = owner_chain_next(e);

        if (e->type != 0x50)
            apply_damage_max(attacker, e);
        e = next;
    }
    xenon2.l2_left_wall_made = 0;
    xenon2.l2_right_wall_made = 0;
    for (i = 0; i < 16; i++)
        xenon2.pickup_groups[i] = 0;
    debris_burst(attacker, 0x27, 0, 0, 0x140, 0xc0, xenon2.big_explosion_anim);
    spawn_bonuses(4);
    xenon2.scroll_min = 0;
    xenon2.l2_first_fight_won = 0xff;
}

/* level2 $05685A: hit routine of the boss: it flashes; when its energy is gone it bursts and the level is won */
void l2_boss_hit(Object *attacker, Object *boss, uint16_t damage)
{
    uint16_t old;

    boss->fn_draw = (obj_fn)l2_boss_flash;
    old = (uint16_t)boss->energy;
    boss->energy = (int16_t)(old - damage);
    if (old > damage)
        return;
    kill_thunk(boss);
    spawn_bonuses(9);
    debris_burst(attacker, 0x1d, 0x50, (uint16_t)(boss->y - xenon2.scroll_pos), 0xa0, 0x70, xenon2.big_explosion_anim);
    free_object_chain(LIST_HEAD(LIST_ENEMY));
    xenon2.boss_dead = 0xff;
}

/* level2 $056ED0: the draw of the boss: its picture of cells while the scroll is near the end */
void l2_boss_draw(Object *boss)
{
    if ((int16_t)xenon2.scroll_pos >= 0x160)
        return;
    draw_tile_grid(xenon2.level2_data.boss_grid, 5, 8, (int16_t)(boss->y - xenon2.scroll_pos), 0x38);
}

/* level2 $056EF6: the draw of the boss for the frame after a hit */
void l2_boss_flash(Object *boss)
{
    boss->fn_draw = (obj_fn)l2_boss_draw;
    if ((int16_t)xenon2.scroll_pos >= 0x160)
        return;
    draw_tile_grid_flash(xenon2.level2_data.boss_grid, 5, 8, (int16_t)(boss->y - xenon2.scroll_pos), 0x38);
}

uint32_t l2_stale_d0;
int l2_stale_d0_set;

/* level2 $056E56: the boss at rest: a pause before the next move (a random number of frames), or, with 16 enemies or more, a move whose
 * direction and length come from d0 as the code before it left it (the lockstep hooks the entry and gives the value the original had) */
void l2_boss_rest(Object *boss)
{
    uint32_t r;
    int16_t step;

    if ((int16_t)xenon2.target_count < 0x10) {
        r = random_next();
        if (!(r & 0x2000)) {
            boss->l2_boss_b.flash = (int16_t)((r & 7) + 2);
            return;
        }
    } else {
        r = l2_stale_d0_set ? l2_stale_d0 : ((uint32_t)xenon2.scroll_pos << 3) ^ xenon2.frame_tick;
        l2_stale_d0_set = 0;
    }
    step = (r & 0x4000) ? -3 : 3;
    boss->l2_boss.frames = (int16_t)((r & 7) + 8);
    boss->l2_boss.step = (boss->l2_boss.step & 0xffffu) | (uint32_t)(uint16_t)(step) << 16;
}

/* the boss's use of the scratch fields of Object */

/* level2 $056DD8: the boss moves up or down (v2a a step, v2e the frames left) between y $60 and $C0 */
static void l2_boss_move(Object *boss)
{
    int16_t new_y, phase;

    if (boss->l2_boss.frames == 0) {
        l2_boss_rest(boss);
        return;
    }
    if (xenon2.sfx_channels[2].active == 0)
        xenon2.sfx_cmd = 3;
    boss->l2_boss.frames--;
    new_y = (int16_t)((int16_t)(boss->l2_boss.step >> 16) + boss->y);
    if (new_y < 0x60) {
        new_y = 0x60;
        boss->l2_boss.frames = 0;
    }
    if (new_y > 0xc0) {
        new_y = 0xc0;
        boss->l2_boss.frames = 0;
    }
    boss->y = new_y;
    phase = (int16_t)((xenon2.frame_tick & 1) * 4);
    *(uint32_t *)(&xenon2.level2_data.boss_cells[0x8]) = *(const uint32_t *)(xenon2.level2_data.boss_move_paths_a[phase / 4]);
    *(uint32_t *)(&xenon2.level2_data.boss_cells[0x14]) = *(const uint32_t *)(xenon2.level2_data.boss_move_paths_a[phase / 4] + 4);
    *(uint32_t *)(&xenon2.level2_data.boss_cells[0x10]) = *(const uint32_t *)(xenon2.level2_data.boss_move_paths_b[phase / 4]);
    *(uint32_t *)(&xenon2.level2_data.boss_cells[0x1c]) = *(const uint32_t *)(xenon2.level2_data.boss_move_paths_b[phase / 4] + 4);
}


/* level2 $056BE8: update of the boss ($50): it waits for the player to come into reach (v28 negative), then counts v28 down; it shoots now
 * and then (a ring of sixteen shots one time in four, else one); while its v3e counts down it changes the cells of its picture.  The moves from
 * $56DD8 on are not written: they test bits of d0 that the original leaves from the shot routine */
void l2_boss_update(Object *boss)
{
    int16_t player_y, player_x, box_y;
    Object *shot;
    uint16_t sum;

    if ((int16_t)xenon2.scroll_max <= 0x120)
        xenon2.scroll_max = 0x120;
    if (boss->l2_boss.frames == 0) {
        if (boss->l2_boss.wait < 0) {
            boss->hit[0] = 0x3e8;
            boss->hit[2] = 0x3e8;
            player_y = (int16_t)(PLAYER_Y + xenon2.scroll_pos);
            if (player_y > 0xb0)
                return;
            player_x = PLAYER_X;
            if (player_x < 0 || player_x > 0x13f)
                return;
            boss->l2_boss.frames = 0x3e8;
            boss->l2_boss.step = (boss->l2_boss.step & 0xffffu) | (uint32_t)(uint16_t)(3) << 16;
            boss->l2_boss.wait = 0x154;
            return;
        }
        boss->l2_boss.wait--;
        if (boss->l2_boss.wait < 0) {
            boss->l2_boss_b.flash = 0;
            boss->l2_boss.frames = 0x3e8;
            boss->l2_boss.step = (boss->l2_boss.step & 0xffffu) | (uint32_t)(uint16_t)(-3) << 16;
        }
    }
    sum = (uint16_t)(boss->shot_count + xenon2.level2_data.boss_fire_rate);
    boss->shot_count = (uint8_t)sum;
    if (sum > 0xff) {
        int16_t y = (int16_t)(boss->y - xenon2.scroll_pos + 0x70);

        boss->shot_count = (uint8_t)(random_next() & 0x3f);
        if ((random_next() & 3) != 0) {
            shot = spawn_enemy_shot(0xa0, (uint16_t)y, 4, xenon2.level2_data.boss_shot_steps, 0);
            set_anim(shot, xenon2.level2_data.boss_update_data);
        } else {
            int dir;

            for (dir = 15; dir >= 0; dir--) {
                shot = spawn_enemy_shot(0xa0, (uint16_t)y, (uint16_t)dir, xenon2.level2_data.boss_shot_steps, 0);
                set_anim(shot, xenon2.level2_data.boss_update_data_2);
            }
        }
    }
    box_y = (int16_t)(boss->y - xenon2.scroll_pos + 0x70);
    boss->hit[0] = 0x98;
    boss->hit[1] = box_y;
    boss->hit[2] = 0xa7;
    boss->hit[3] = (int16_t)(box_y + 0xa);
    if ((int16_t)xenon2.scroll_pos >= 0x160)
        return;
    if (boss->l2_boss_b.flash != 0) {
        const uint8_t *cells;

        boss->l2_boss.cell = (int16_t)((boss->l2_boss.cell + 4) & 0xc);
        cells = xenon2.level2_data.boss_cells_by_cell[(uint16_t)boss->l2_boss.cell / 4];
        *(uint32_t *)(&xenon2.level2_data.boss_cells[0]) = *(const uint32_t *)(cells);
        *(uint32_t *)(&xenon2.level2_data.boss_cells[0xc]) = *(const uint32_t *)(cells + 4);
        boss->l2_boss_b.flash--;
        if (boss->l2_boss_b.flash != 0)
            return;
        shot = alloc_in(LIST_C);
        shot->type = 0x10c;
        shot->fn_update = (obj_fn)l2_roamer_update;
        shot->fn_draw = draw_bob_masked;
        shot->fn_hit = (void *)obj_nop_thunk;
        shot->hit[0] = 0x3e8;
        shot->hit[2] = 0x3e8;
        shot->x = 0xa0;
        shot->y = (int16_t)(boss->y - xenon2.scroll_pos + 0x80);
        set_anim(shot, xenon2.level2_data.boss_update_data_4);
        shot->l2_boss.wait = 0;
        shot->l2_boss.step = (shot->l2_boss.step & 0xffffu) | (uint32_t)(uint16_t)((int16_t)(random_next() & 7)) << 16;
        return;
    }
    l2_boss_move(boss);
}
