/* level5: level 5's own code (the blob at $54E40 while level 5 is loaded), written like level4.c; a function has `level5 $ADDR` for its address. */
#include "weapons.h"
#include "shop.h"
#include "game_all.h"
#include <string.h>


void l5_unit_update(Object *obj);

void l5_beam_draw(Object *obj);
void l5_beam_update(Object *obj);
void l5_boss2_dead(Object *obj);
void l5_boss2_draw(Object *obj);
void l5_boss2_flash(Object *obj);
void l5_boss_draw(Object *obj);
void l5_bridge_flash(Object *obj);
void l5_bridge_hit(Object *shot, Object *obj, uint16_t damage);
void l5_bridge_update(Object *obj);
void l5_cannon_update(Object *obj);
void l5_gunner_update(Object *obj);
void l5_gun_update(Object *obj);
void l5_missile_update(Object *obj);
void l5_pellet_update(Object *obj);
void l5_unit_flash(Object *obj);
void l5_unit_hit(Object *shot, Object *obj, uint16_t damage);
void l5_wall_flash_a(Object *obj);
void l5_wall_flash_b(Object *obj);

/* the scratch fields of Object, by the kind that uses them */


/* the scratch fields of Object, by the kind that uses them */


/* the scratch fields of Object, by the kind that uses them */



static Object *alloc_in(Object *list)
{
    Object *o = obj_free_pop(), *tail = list->prev;

    list->prev = o;
    o->prev = tail;
    o->next = list;
    tail->next = o;
    return o;
}

static Object *insert_head(Object *list)
{
    Object *o = obj_free_pop(), *old = list->next;

    list->next = o;
    o->next = old;
    o->prev = list;
    old->prev = o;
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

static void set_anim(Object *o, const uint16_t *anim)
{
    anim_header_set(o, anim);
}

/* the byte at b grows by add; returns the carry */
static int add_carry(uint8_t *b, uint8_t add)
{
    uint16_t sum = (uint16_t)(*b + add);

    *b = (uint8_t)sum;
    return sum > 0xff;
}

/* the picture of the shot by the table at $72EC8 */
static const void *shot_gfx(uint16_t index)
{
    return xenon2.level5_data.shot_gfx_table[index]->pic;
}

/* the object follows its target ($564C4) */
static void follow_target_offset(Object *obj)
{
    Object *target = obj->target;

    obj->x = (int16_t)(target->x + obj->part_b.dx);
    obj->y = (int16_t)(target->y + obj->part_b.dy);
}

/* the burst at the hot spot of the picture of o (the end of the hits: $55D1C... $561F4) */
static void explode_at(const Object *o, const uint16_t *anim)
{
    const struct Pic *g = o->gfx;
    int16_t x = (int16_t)(o->x - (int16_t)g->hot_x);
    int16_t y = (int16_t)(o->y - (int16_t)g->hot_y);

    x = (int16_t)(x + x);
    y = (int16_t)(y + y);
    x = (int16_t)(x + (int16_t)g->width);
    y = (int16_t)(y + (int16_t)g->height);
    spawn_effect((int16_t)(x >> 1), (int16_t)(y >> 1), anim, draw_bob_masked);
}

static int dies(Object *obj, uint16_t damage)
{
    uint16_t old = (uint16_t)obj->energy;

    obj->energy = (int16_t)(old - damage);
    return old <= damage;
}

void l5_spawn_gunners(const struct SpawnRec *record);
void l5_spawn_cannon(const struct SpawnRec *record);
void l5_spawn_boss1(uint16_t slot_value);
void l5_spawn_boss2(const struct SpawnRec *record);
void l5_spawn_gates(const struct SpawnRec *record);
void l5_spawn_gun(const struct SpawnRec *record);
void l5_spawn_bridge(const struct SpawnRec *record);
void l5_spawn_group(const struct SpawnRec *record);
Object *l5_spawn_unit(const struct SpawnRec *record);

/* level5 $054E10, $057568: level events by event: 0 the wave of $326C, 1 to 8 formations */
void l5_event(uint16_t event, const struct SpawnRec *record)
{
    if (event == 0)
        spawn_wave_326c(0, record);   /* $E5A */
    else if (event < 9)
        spawn_formation(0, record, &xenon2.level5_data.formations[event - 1].defs);
}

/* level5 $054E18, $0551AA: the hooks by hook */
void l5_hook(uint16_t hook, const struct SpawnRec *record, uint16_t slot_value)
{
    switch (hook) {
    case 0:
        note_spawn_record(record, slot_value);   /* $EA2 */
        break;
    case 1:
        l5_spawn_gates(record);
        break;
    case 2:
        l5_spawn_gun(record);
        break;
    case 3:
        l5_spawn_unit(record);
        break;
    case 4:
        l5_spawn_bridge(record);
        break;
    case 5:
        l5_spawn_boss2(record);
        break;
    case 6:
        l5_spawn_boss1(slot_value);
        break;
    case 7:
        l5_spawn_gunners(record);
        break;
    case 8:
        l5_spawn_cannon(record);
        break;
    case 9:
        l5_spawn_group(record);
        break;
    }
}

/* level5 $054E2C, $054E80: the two limits are kept for the swap */
void l5_swap_hook(void)
{
    xenon2.level5_data.events_b[0].scroll = (int16_t)xenon2.l5_limit_a;
    xenon2.level5_data.events_a[0].scroll = (int16_t)xenon2.l5_limit_b;
}

/* level5 $054E20, $0575EA: the data is copied once (backwards: the copy lies above its source) */
void l5_init(void)
{
    int i;

    if (xenon2.level_init_src != (uint8_t *)xenon2.level_data_dst) {
        for (i = 5999; i >= 0; i--)
            xenon2.level_data_dst[i] = xenon2.level5_data.data_src[i];
        xenon2.level_init_src = (uint8_t *)xenon2.level_data_dst;
    }
    xenon2.scroll_min = 0;
    xenon2.scroll_max = 0x11ff;
    xenon2.l5_boss1_spawned = 0;
    xenon2.l5_gunners_spawned = 0;
    xenon2.l5_limit_a = 0x1200;
    xenon2.l5_limit_b = 0x11a0;
    xenon2.level5_data.boss1_pieces[0].energy = xenon2.level5_data.init_patch;
    xenon2.level5_data.boss2_pieces[5].energy = xenon2.level5_data.init_patch_2;
    xenon2.l5_group_done[0] = 0;
    xenon2.l5_group_done[1] = 0;
    xenon2.difficulty_x1 = (uint16_t)(xenon2.difficulty * 0xc);
    xenon2.difficulty_x2 = (uint16_t)(xenon2.difficulty * 0xe);
}

/* ---- the gunners ($551E8): ten of them from a formation table ---- */

/* level5 $0551E8: a formation of ten that fly the path of the table by the record's type */
void l5_spawn_gunners(const struct SpawnRec *record)
{
    const struct PathCmd *path = (const struct PathCmd *)xenon2.level5_data.gunner_paths[record->index];
    int16_t wait = 0;
    int count;

    for (count = 9; count >= 0; count--) {
        Object *obj = insert_head(LIST_C);

        obj->type = 0xdc;
        obj->fn_update = (obj_fn)l5_gunner_update;
        obj->fn_draw = (obj_fn)draw_bob_masked;
        obj->fn_hit = (void *)hit_enemy;
        obj->gfx = (void *)xenon2.level5_data.gunner_picture;
        obj->anim_delay = 0;
        obj->x = (int16_t)path->arg.pos.x;
        obj->y = (int16_t)(path->arg.pos.y - xenon2.scroll_pos);
        obj->energy = (int16_t)xenon2.level5_data.gunners_energy;
        obj->big = 0;
        obj->path.angle = 0;
        obj->turn = 0;
        obj->turn_step = 0;
        obj->script = (uint8_t *)path;
        obj->cursor = (const struct PathCmd *)(&path->arg.pos + 1);
        obj->path.wait = wait;
        wait = (int16_t)(wait - (int16_t)xenon2.level5_data.gunners_param);
        obj->hit[0] = 0x3e8;
        obj->hit[2] = 0x3e8;
        obj->score = 0xc8;
        obj->kind = 0;
        obj->steps = (int16_t)xenon2.level5_data.gunners_steps;
        obj->shot_count = (uint8_t)(count * 0x19);
    }
}

/* level5 $0552DA: a gunner: it flies its script ($EF6), its picture follows the direction, and now and then it fires all round */
void l5_gunner_update(Object *obj)
{
    uint16_t old, frame, old_frame;

    obj->y = (int16_t)(obj->y + xenon2.scroll_speed);
    if (obj->y >= 0x190) {
        kill_thunk(obj);
        return;
    }
    old = (uint16_t)(int16_t)obj->gunner.phase;
    obj_script_run(obj);
    frame = (uint16_t)(((uint16_t)((int16_t)obj->gunner.phase + 0x10) >> 5) & 7);
    old_frame = (uint16_t)(((uint16_t)(old + 0x10) >> 5) & 7);
    if (frame != old_frame)
        set_anim(obj, xenon2.level5_data.gunner_anims[frame]);
    if (add_carry(&obj->shot_count, xenon2.level5_data.gunner_fire_rate)) {
        int dir;

        obj->shot_count = 0;
        for (dir = 7; dir >= 0; dir--)
            spawn_enemy_shot((uint16_t)obj->x, (uint16_t)obj->y, (uint16_t)dir, xenon2.level5_data.gunner_shot_steps, shot_gfx(xenon2.level5_data.gunner_shot_steps_2));
    }
    bbox_from_frame(obj);
}

/* ---- the cannons ($55498) ---- */

/* level5 $055498: a cannon at the side that goes up and down; the record's $C tells the side */
void l5_spawn_cannon(const struct SpawnRec *record)
{
    uint16_t type = record->steps ? 0x104 : 0x100;
    Object *obj = spawn_marker_enemy(type, record, (void (*)(void))(obj_fn)l5_cannon_update);
    const uint16_t *anim = xenon2.level5_data.cannon_anim_a;

    obj->y = (int16_t)(obj->y - xenon2.scroll_pos);
    obj->fn_draw = (obj_fn)draw_bob_masked;
    obj->fn_hit = (void *)hit_enemy;
    obj->energy = (int16_t)xenon2.level5_data.cannon_energy;
    obj->big = 0xff;
    obj->hit[0] = 0x3e8;
    obj->hit[2] = 0x3e8;
    obj->score = 0xfa;
    if (obj->type != 0x100) {
        obj->x = (int16_t)(obj->x + 0x10);
        anim = xenon2.level5_data.cannon_anim_b;
    }
    set_anim(obj, anim);
    obj->kind = 0;
    obj->cannon.bob = (obj->cannon.bob & 0xffffu) | (uint32_t)(uint16_t)(0x14) << 16;
}

/* the pellet: type $E4 of the cannon and the unit ($55682) */
static void spawn_pellet(int16_t x, int16_t y, uint16_t dir)
{
    Object *pellet = alloc_in(LIST_C);

    pellet->type = 0xe4;
    pellet->fn_update = (obj_fn)l5_pellet_update;
    pellet->fn_draw = (obj_fn)draw_bob_masked;
    pellet->fn_hit = (void *)hit_enemy;
    pellet->x = x;
    pellet->y = y;
    pellet->pellet.dir = (pellet->pellet.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)dir) << 16;
    set_anim(pellet, xenon2.level5_data.pellet_anims_by_dir[dir]);
    pellet->energy = (int16_t)xenon2.level5_data.pellet_energy;
    pellet->big = 0;
    pellet->score = 0x64;
    pellet->pellet.tick = (int16_t)(random_next() & 0x1f);
    pellet->kind = 0;
    pellet->hit[0] = 0x3e8;
    pellet->hit[2] = 0x3e8;
}


/* level5 $0555A6: the cannon: it moves down by the scroll and slowly up and down by v2a, and fires pellets */
void l5_cannon_update(Object *obj)
{
    int16_t world_y;

    obj->y = (int16_t)(obj->y + xenon2.scroll_speed);
    if (obj->y >= 0x190) {
        kill_thunk(obj);
        return;
    }
    anim_step(obj);
    world_y = (int16_t)xenon2.scroll_pos;
    if ((int16_t)(obj->cannon.bob >> 16) >= 0) {
        obj->y = (int16_t)(obj->y + 1);
        world_y = (int16_t)(world_y + obj->y);
        if (world_y >= 0xf20) {
            obj->cannon.bob = (obj->cannon.bob & 0xffffu) | (uint32_t)(uint16_t)(-1) << 16;
            set_anim(obj, obj->type == 0x100 ? xenon2.level5_data.cannon_anim_100 : xenon2.level5_data.cannon_anim_other);
        }
    } else {
        obj->y = (int16_t)(obj->y - 1);
        world_y = (int16_t)(world_y + obj->y);
        if (world_y <= 0xe30) {
            obj->cannon.bob = (obj->cannon.bob & 0xffffu) | (uint32_t)(uint16_t)(1) << 16;
            set_anim(obj, obj->type == 0x100 ? xenon2.level5_data.cannon_anim_a : xenon2.level5_data.cannon_anim_b);
        }
    }
    if (add_carry(&obj->shot_count, xenon2.level5_data.cannon_fire_rate)) {
        int16_t x = (int16_t)(obj->x + 0x20);
        uint16_t dir = 2;

        obj->shot_count = (uint8_t)(random_next() & 0x3f);
        if (obj->type != 0x100) {
            dir = 6;
            x = (int16_t)(x - 0x40);
        }
        spawn_pellet(x, obj->y, dir);
    }
    bbox_from_frame(obj);
}

/* level5 $05581E: the pellet: it turns towards the player every eighth frame and goes; it ends in a burst */
void l5_pellet_update(Object *obj)
{
    int16_t dx, dy;
    uint16_t tick, turn;

    anim_step(obj);
    obj->pellet.tick = (int16_t)(obj->pellet.tick + 1);
    tick = (uint16_t)obj->pellet.tick;
    if ((int16_t)tick < (int16_t)xenon2.level5_data.pellet_param) {
        if ((tick & 7) == 0) {
            turn = direction8((int16_t)(PLAYER->x - obj->x), (int16_t)(PLAYER->y - obj->y));
            turn = (uint16_t)(turn - (uint16_t)(int16_t)(obj->pellet.dir >> 16));
            if (turn != 0) {
                if ((int16_t)turn < 0)
                    turn = (uint16_t)(turn + 8);
                obj->pellet.dir = (obj->pellet.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)((int16_t)(obj->pellet.dir >> 16) + 1)) << 16;
                if (turn >= 4 && (turn != 4 || !((uint16_t)obj->x & 1)))
                    obj->pellet.dir = (obj->pellet.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)((int16_t)(obj->pellet.dir >> 16) - 2)) << 16;
                obj->pellet.dir = (obj->pellet.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)((int16_t)(obj->pellet.dir >> 16) & 7)) << 16;
                set_anim(obj, xenon2.level5_data.pellet_anims_by_dir[(int16_t)(obj->pellet.dir >> 16)]);
            }
        }
        dx = xenon2.level5_data.pellet_dx_by_dir[(int16_t)(obj->pellet.dir >> 16)];
        dy = (int16_t)(xenon2.level5_data.pellet_dy_by_dir[(int16_t)(obj->pellet.dir >> 16)] + xenon2.scroll_speed);
        obj->x = (int16_t)(obj->x + dx);
        obj->y = (int16_t)(obj->y + dy);
        bbox_from_frame(obj);
        if (!point_in_player_box(dx, dy))
            return;
        if (xenon2.shield_timer == 0) {
            PLAYER->fn_draw = (obj_fn)draw_bob_or_first;
            player_damage(obj, 4);
            kill_thunk(obj);
            return;
        }
    }
    explode_at(obj, xenon2.anim_explosion);
    kill_thunk(obj);
}

/* ---- the big boss of 22 pieces ($55BFA) ---- */

/* level5 $055BFA: the boss: its picture is cells in the level's data, and the 22 pieces hang on the first */
void l5_spawn_boss1(uint16_t slot_value)
{
    Cell *map;
    const struct Level5Boss1Piece *rec = xenon2.level5_data.boss1_pieces;
    Object *root = 0;
    int count, i;

    if (xenon2.l5_boss1_spawned)
        return;
    xenon2.l5_boss1_spawned = 0xff;
    xenon2.scroll_y = 0x1a0;
    xenon2.player_x_saved = 0xa0;
    xenon2.player_y_saved = 0xb0;
    latch_slots(slot_value);
    xenon2.level5_data.events_b[0].scroll = (int16_t)0xffff;
    xenon2.level5_data.events_a[0].scroll = (int16_t)0xffff;
    xenon2.l5_limit_a = 0xffff;
    xenon2.l5_limit_b = 0xffff;
    map = (Cell *)xenon2.level_init_src;
    memset(map, 0, MAP_ROW(6));
    map += 6 * MAP_COLS;
    for (count = 0; count < 18; count++) {
        memset(map, 0, 3 * sizeof(Cell));
        for (i = 0; i < 15; i++)
            map[3 + i] = xenon2.level5_data.boss1_cells[15 * count + i];
        memset(&map[18], 0, 2 * sizeof(Cell));
        map += MAP_COLS;
    }
    memset(map, 0, MAP_ROW(15));
    xenon2.map_row_offset = MAP_ROW(26);
    xenon2.scroll_pos = 0x1a0;
    xenon2.scroll_max = 0x1a0;
    for (count = 21; count >= 0; count--, rec++) {
        Object *obj = insert_head(LIST_C);

        if (root == 0)
            root = obj;
        obj->type = rec->type;
        obj->fn_update = (obj_fn)rec->update;
        obj->fn_draw = (obj_fn)rec->draw;
        obj->fn_hit = (void *)rec->hit;
        set_anim(obj, rec->anim);
        obj->x = 0x30;
        obj->y = (int16_t)0xfec0;
        obj->part_b.dx = rec->dx;
        obj->part_b.dy = rec->dy;
        obj->boss.phase = 0;
        obj->persists = 0xff;
        obj->boss.step = (obj->boss.step & 0xffffu) | (uint32_t)(uint16_t)(4) << 16;
        obj->energy = rec->energy;
        obj->big = rec->big;
        obj->hit[0] = 0x3e8;
        obj->hit[2] = 0x3e8;
        obj->target = root;
        obj->kind = 0;
    }
    xenon2.l5_boss_pieces_left = 0x12;
}

/* level5 $055D1C: the flash of the boss, one frame */
void l5_boss_flash(Object *obj)
{
    obj->fn_draw = (obj_fn)l5_boss_draw;
    draw_tile_grid_flash(xenon2.level5_data.boss1_cells, 0xe, 0x11, obj->y, (uint16_t)((uint16_t)obj->x >> 1));
}

/* level5 $055D3C: a gun of the boss: it hangs on the boss and its box is a bar */
void l5_boss_gun(Object *obj)
{
    int16_t x, y;

    follow_target_offset(obj);
    x = obj->x;
    y = obj->y;
    obj->hit[0] = x;
    obj->hit[1] = y;
    x = (int16_t)(x + obj->energy);
    y = (int16_t)((y & 0xff00) | (uint8_t)((uint8_t)y + obj->big));
    obj->hit[2] = x;
    obj->hit[3] = y;
}

/* level5 $055D62: a piece of the boss that animates */
void l5_boss_piece(Object *obj)
{
    anim_step(obj);
    follow_target_offset(obj);
    bbox_from_frame(obj);
}

/* level5 $055D6E: the last piece: it has a box when the others are gone */
void l5_boss_last(Object *obj)
{
    follow_target_offset(obj);
    if (xenon2.l5_boss_pieces_left == 0)
        bbox_from_frame(obj);
}

/* level5 $055D7E: the boss: the scroll stops at its place, it shifts left and right, and it shakes the screen and fires */
void l5_boss_update(Object *obj)
{
    xenon2.scroll_max = 0x1a0;
    if (xenon2.scroll_pos == 0) {
        scroll_pos_update(xenon2.scroll_step);
        stars_scroll(xenon2.scroll_step);
    }
    obj->y = (int16_t)(obj->y + xenon2.scroll_speed);
    if (obj->boss.phase == 0) {
        if (!add_carry(&obj->shot_count, xenon2.level5_data.boss_fire_rate))
            return;
        obj->shot_count = (uint8_t)(random_next() & 0x3f);
        xenon2.sfx_cmd2 = -1;
        obj->boss.phase = (xenon2.frame_tick & 1) ? 2 : -2;
        return;
    }
    if (obj->boss.phase > 0) {
        obj->boss.phase = (int16_t)(obj->boss.phase + 2);
        if (obj->boss.phase == 0x14) {
            xenon2.sfx_cmd2 = 0x13;
            return;
        }
        if (obj->boss.phase == 0x28)
            goto fire;
        if (obj->boss.phase == 0x3c)
            obj->boss.phase = 0;
        return;
    }
    obj->boss.phase = (int16_t)(obj->boss.phase - 2);
    if (obj->boss.phase == (int16_t)0xffec) {
        xenon2.sfx_cmd2 = 0x13;
        return;
    }
    if (obj->boss.phase == (int16_t)0xffd8)
        goto fire;
    if (obj->boss.phase == (int16_t)0xffc4)
        obj->boss.phase = 0;
    return;
fire:
    {
        int16_t x = (int16_t)(obj->x + 0x77), y = 0x65;
        Object *missile;

        sfx_stop_all();
        if (obj->boss.phase < 0)
            y = 0xf5;
        y = (int16_t)(y + obj->y);
        missile = insert_head(LIST_C);
        missile->type = 0xec;
        missile->fn_update = (obj_fn)l5_missile_update;
        missile->fn_draw = (obj_fn)draw_bob_masked;
        missile->fn_hit = (void *)hit_enemy;
        missile->x = x;
        missile->y = y;
        missile->missile.dir = (missile->missile.dir & 0xffffu) | (uint32_t)(uint16_t)(0) << 16;
        set_anim(missile, xenon2.level5_data.anim_boss_update);
        missile->energy = (int16_t)xenon2.level5_data.boss_energy;
        missile->big = 0;
        missile->score = 0x96;
        missile->boss.phase = 1;
        missile->kind = 0;
        missile->hit[0] = 0x3e8;
        missile->hit[2] = 0x3e8;
    }
}


/* level5 $055EDA: the arms of the boss (v28 their stroke): the pictures are drawn at its side */
void l5_boss_draw(Object *obj)
{
    int16_t y = 0x55;
    int16_t stroke = obj->l5_arm.stroke;
    int16_t x;

    if (stroke == 0)
        return;
    if (stroke < 0) {
        y = 0xe5;
        stroke = (int16_t)-stroke;
    }
    stroke = (int16_t)(stroke & 0xfffc);
    x = (int16_t)(obj->x + 0x69);
    y = (int16_t)(y + obj->y);
    draw_bob_masked_at((uint16_t)x, (uint16_t)y, xenon2.level5_data.boss_draw_table[(uint16_t)stroke / 4]);
    if (stroke < 0x14 || stroke >= 0x28)
        return;
    x = (int16_t)(x + 0xe);
    y = (int16_t)(y + 0x10);
    draw_bob_masked_at((uint16_t)x, (uint16_t)y, xenon2.level5_data.boss_arm2_pictures[(uint16_t)stroke / 4 - 5]);
}

/* level5 $05605E: the missile of the boss: it turns to the player by eighths, and goes; it hits the player or the screen's edge */
void l5_missile_update(Object *obj)
{
    int16_t dx, dy;
    uint16_t dir;

    anim_step(obj);
    obj->missile.tick = (int16_t)(obj->missile.tick + 1);
    if ((obj->missile.tick & 7) == 0) {
        dir = direction8((int16_t)(PLAYER->x - obj->x), (int16_t)(PLAYER->y - obj->y));
        dir = (uint16_t)(dir - (uint16_t)(int16_t)(obj->missile.dir >> 16));
        if (dir != 0) {
            if ((int16_t)dir < 0)
                dir = (uint16_t)(dir + 8);
            obj->missile.dir = (obj->missile.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)((int16_t)(obj->missile.dir >> 16) + 1)) << 16;
            if (dir >= 4)
                obj->missile.dir = (obj->missile.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)((int16_t)(obj->missile.dir >> 16) - 2)) << 16;
            obj->missile.dir = (obj->missile.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)((int16_t)(obj->missile.dir >> 16) & 7)) << 16;
            set_anim(obj, xenon2.level5_data.missile_anims_by_dir[(int16_t)(obj->missile.dir >> 16)]);
        }
    }
    dx = xenon2.level5_data.pellet_dx_by_dir[(int16_t)(obj->missile.dir >> 16)];
    dy = xenon2.level5_data.pellet_dy_by_dir[(int16_t)(obj->missile.dir >> 16)];
    if (xenon2.l5_boss_pieces_left == 0) {
        dx = (int16_t)(dx + dx);
        dy = (int16_t)(dy + dy);
    }
    obj->x = (int16_t)(obj->x + dx);
    obj->y = (int16_t)(obj->y + dy);
    bbox_from_frame(obj);
    if (!player_hits(obj))
        return;
    if (xenon2.shield_timer == 0) {
        PLAYER->fn_draw = (obj_fn)draw_bob_or_first;
        player_damage(obj, 4);
    }
    kill_thunk(obj);
}

/* level5 $05612A: a gun on the boss: its picture at $6158C at the frame 1 fires a beam; now and then it begins to fire */
void l5_turret_update(Object *obj)
{
    anim_step(obj);
    follow_target_offset(obj);
    if (obj->gfx == &xenon2.level5_data.turret_fire_picture && obj->anim_delay == 1) {
        Object *beam = alloc_in(LIST_B);

        beam->type = 0x110;
        beam->fn_update = (obj_fn)l5_beam_update;
        beam->fn_draw = (obj_fn)l5_beam_draw;
        beam->x = (int16_t)(obj->x - 2);
        beam->y = (int16_t)(obj->y - 0x18);
        beam->beam.length = 0;
        beam->beam.dy = (beam->beam.dy & 0xffffu) | (uint32_t)(uint16_t)((int16_t)-(int16_t)xenon2.level5_data.turret_param) << 16;
    }
    if (obj->anim_delay == 0 && obj->y >= 0 && add_carry(&obj->shot_count, xenon2.level5_data.turret_fire_rate)) {
        obj->shot_count = (uint8_t)(random_next() & 0x3f);
        obj->anim_delay = 1;
    }
    bbox_from_frame(obj);
}

/* level5 $0561D2: the hit of a piece of the boss: the burst and the score when its energy is gone */
void l5_piece_hit(Object *shot, Object *obj, uint16_t damage)
{
    (void)shot;
    obj->fn_draw = (obj_fn)draw_bob_or_first;
    if (!dies(obj, damage))
        return;
    xenon2.l5_boss_pieces_left = (uint16_t)(xenon2.l5_boss_pieces_left - 1);
    xenon2.player_score += 0xc8;
    explode_at(obj, obj->big ? xenon2.big_explosion_anim : xenon2.anim_explosion);
    if (obj->type != 0x114) {
        kill_thunk(obj);
    } else {
        obj->fn_update = (obj_fn)l5_boss2_dead;
        obj->hit[0] = 0x3e8;
        obj->hit[2] = 0x3e8;
        obj->fn_hit = (void *)obj_nop_thunk;
    }
}

/* level5 $0562A6: the hit of the heart of the boss: its picture follows its energy; at the end the level's objects go */
void l5_heart_hit(Object *shot, Object *heart, uint16_t damage)
{
    Object *boss = heart->target;

    heart->fn_draw = (obj_fn)draw_bob_or_first;
    boss->fn_draw = (obj_fn)l5_boss_flash;
    if (!dies(boss, damage)) {
        uint32_t index = ((uint32_t)(uint16_t)boss->energy * 0xb) / xenon2.level5_data.boss1_pieces[0].energy;

        heart->gfx = (void *)xenon2.level5_data.heart_pictures[index];
        return;
    }
    memset(xenon2.level_init_src + MAP_ROW(12), 0, MAP_ROW(18));
    free_object_chain(LIST_C->prev);
    spawn_bonuses(9);
    debris_burst(shot, 0x27, 0, 0, 0x140, 0xc0, xenon2.big_explosion_anim);
    xenon2.boss_dead = 0xff;
    memset(xenon2.level_init_src, 0, 1560);
}

/* ---- the second boss of ten pieces ($5642A) ---- */

/* level5 $05642A: the second boss */
void l5_spawn_boss2(const struct SpawnRec *record)
{
    const struct Level5Boss2Piece *rec = xenon2.level5_data.boss2_pieces;
    Object *root = 0;
    int16_t y;
    int count;

    if (xenon2.l5_gunners_spawned)
        return;
    xenon2.l5_gunners_spawned = 0xff;
    y = (int16_t)(record->y - 8 - xenon2.scroll_pos);
    for (count = 9; count >= 0; count--, rec++) {
        Object *obj = insert_head(LIST_C);

        if (root == 0)
            root = obj;
        obj->type = rec->type;
        obj->fn_update = (obj_fn)rec->update;
        obj->fn_draw = (obj_fn)rec->draw;
        obj->fn_hit = (void *)rec->hit;
        obj->gfx = (void *)rec->gfx;
        obj->anim_delay = 0;
        obj->persists = 0xff;
        obj->x = 0x70;
        obj->y = y;
        obj->part_b.dx = rec->dx;
        obj->part_b.dy = rec->dy;
        obj->mover.tick = 0;
        obj->mover.step = (obj->mover.step & 0xffffu) | (uint32_t)(uint16_t)(4) << 16;
        obj->energy = (int16_t)rec->energy;
        obj->hit[0] = 0x3e8;
        obj->hit[2] = 0x3e8;
        obj->target = root;
        obj->kind = 0;
    }
}

/* level5 $0564E2: the big wheel: its picture turns by its energy */
void l5_wheel_update(Object *obj)
{
    int16_t angle;

    follow_target_offset(obj);
    angle = (int16_t)(0x100 - obj->energy + obj->wheel.angle);
    if (angle >= 0x600)
        angle = (int16_t)(angle - 0x600);
    obj->wheel.angle = angle;
    obj->gfx = (void *)xenon2.level5_data.wheel_pictures[((uint16_t)angle >> 8) & 0xff];
    bbox_from_frame(obj);
}

/* level5 $056526: the hit of the wheel: at the end the boss goes with its burst and bonuses */
void l5_wheel_hit(Object *shot, Object *wheel, uint16_t damage)
{
    Object *boss = wheel->target;
    int i;

    boss->fn_draw = (obj_fn)l5_boss2_flash;
    if (!dies(wheel, damage))
        return;
    free_object_chain(LIST_C->prev);
    debris_burst(shot, 0x13, 0x70, (uint16_t)(0x8d0 - xenon2.scroll_pos), 0x60, 0xa0, xenon2.big_explosion_anim);
    spawn_bonuses(4);
    for (i = 0; i < 10; i++)
        memset(xenon2.level_init_src + MAP_CELL(141, 6) + MAP_ROW(i), 0, 8 * sizeof(Cell));
    xenon2.player_score += 0x5dc;
}

/* level5 $056594: a small turning part: its picture by the scroll */
void l5_spinner_update(Object *obj)
{
    const uint8_t *const *table = obj->type == 0x118 ? xenon2.level5_data.spinner_source_118 : xenon2.level5_data.spinner_source_other;

    follow_target_offset(obj);
    obj->gfx = (void *)table[(uint16_t)(obj->y + xenon2.scroll_pos) & 3];
}

/* level5 $0565E2: a turret of the boss: it turns to the player and fires */
void l5_boss2_turret(Object *obj)
{
    uint16_t dir;

    follow_target_offset(obj);
    dir = direction8((int16_t)(PLAYER->x - obj->x), (int16_t)(PLAYER->y - obj->y));
    obj->l5_turret.dir = (obj->l5_turret.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)dir) << 16;
    obj->gfx = (void *)xenon2.level5_data.boss2_turret_pictures[dir];
    if (add_carry(&obj->shot_count, xenon2.level5_data.boss2_turret_fire_rate)) {
        obj->shot_count = (uint8_t)(random_next() & 0x3f);
        spawn_enemy_shot((uint16_t)(obj->x + 8), (uint16_t)(obj->y + 8), (uint16_t)(int16_t)(obj->l5_turret.dir >> 16), xenon2.level5_data.boss2_turret_shot_steps, shot_gfx(xenon2.level5_data.boss2_turret_shot_steps_2));
    }
    bbox_from_frame(obj);
}

/* level5 $056678: the hit of the turret: it stops with a burst */
void l5_boss2_turret_hit(Object *shot, Object *obj, uint16_t damage)
{
    (void)shot;
    obj->fn_draw = (obj_fn)draw_bob_or_first;
    if (!dies(obj, damage))
        return;
    obj->fn_update = (obj_fn)l5_boss2_dead;
    obj->hit[0] = 0x3e8;
    obj->hit[2] = 0x3e8;
    obj->fn_hit = (void *)obj_nop_thunk;
    xenon2.player_score += 0xc8;
    spawn_effect((int16_t)(obj->x + 8), (int16_t)(obj->y + 8), xenon2.anim_explosion, draw_bob_masked);
}

/* level5 $0566CE: the part that is dead */
void l5_boss2_dead(Object *obj)
{
    follow_target_offset(obj);
    obj->gfx = (void *)xenon2.level5_data.boss2_dead_picture;
    obj->hit[0] = 0x3e8;
    obj->hit[2] = 0x3e8;
}

/* level5 $0566E8: the mover of the second boss: it goes up and down by random, fires its beam and its pellets */
void l5_boss2_mover(Object *obj)
{
    int16_t range;

    if ((int16_t)xenon2.scroll_max < 0x920)
        xenon2.scroll_max = 0x920;
    obj->y = (int16_t)(obj->y + xenon2.scroll_speed);
    if ((int16_t)(obj->mover.step >> 16) != 0) {
        xenon2.sfx_cmd = 0x16;
        if ((int16_t)(obj->mover.step >> 16) > 0) {
            obj->y = (int16_t)(obj->y + 1);
            obj->mover.step = (obj->mover.step & 0xffffu) | (uint32_t)(uint16_t)((int16_t)((int16_t)(obj->mover.step >> 16) - 1)) << 16;
        } else {
            obj->y = (int16_t)(obj->y - 1);
            obj->mover.step = (obj->mover.step & 0xffffu) | (uint32_t)(uint16_t)((int16_t)((int16_t)(obj->mover.step >> 16) + 1)) << 16;
        }
    } else {
        uint32_t r = random_next();

        if ((uint8_t)r < 0xf) {
            if (r & 1) {
                range = (int16_t)(0x910 - obj->y - xenon2.scroll_pos);
                if (range != 0)
                    obj->mover.step = (obj->mover.step & 0xffffu) | (uint32_t)(uint16_t)((int16_t)(random_below((uint16_t)range) + 1)) << 16;
            } else {
                range = (int16_t)(obj->y + xenon2.scroll_pos - 0x8d0);
                if (range != 0)
                    obj->mover.step = (obj->mover.step & 0xffffu) | (uint32_t)(uint16_t)((int16_t)-(int16_t)(random_below((uint16_t)range) + 1)) << 16;
            }
        }
    }
    if (obj->mover.tick == 0) {
        if (!add_carry(&obj->shot_count, xenon2.level5_data.boss2_mover_fire_rate))
            goto pellets;
        obj->shot_count = (uint8_t)(random_next() & 0x3f);
    }
    obj->mover.tick = (int16_t)(obj->mover.tick + 1);
    if (obj->mover.tick == 9) {
        obj->mover.tick = 0;
    } else if (obj->mover.tick == 3) {
        Object *beam;

        xenon2.sfx_cmd = 0x84;
        beam = alloc_in(LIST_B);
        beam->type = 0x110;
        beam->fn_update = (obj_fn)l5_beam_update;
        beam->fn_draw = (obj_fn)l5_beam_draw;
        beam->x = 0x96;
        beam->y = (int16_t)(obj->y + 0x30);
        beam->mover.tick = 0;
        beam->mover.step = (beam->mover.step & 0xffffu) | (uint32_t)(uint16_t)((int16_t)xenon2.level5_data.turret_param) << 16;
    }
pellets:
    if (add_carry(&obj->shot_rate, xenon2.level5_data.boss2_mover_fire_rate_2)) {
        obj->shot_rate = (uint8_t)(random_next() & 0x3f);
        spawn_pellet(0x6e, (int16_t)(obj->y + 0x30), 6);
        spawn_pellet(0xd2, (int16_t)(obj->y + 0x30), 2);
    }
}

/* level5 $056840: the flash of the mover, one frame */
void l5_boss2_flash(Object *obj)
{
    obj->fn_draw = (obj_fn)l5_boss2_draw;
    draw_tile_grid_flash(xenon2.level5_data.boss2_flash_grid, 5, 5, obj->y, 0x38);
}


/* level5 $05685A: the cells of the mover: three of them change with v28 */
void l5_boss2_draw(Object *obj)
{
    uint32_t offset = (uint32_t)(uint16_t)obj->mover.cell * 6;

    *(uint32_t *)(&xenon2.level5_data.boss2_draw_data[0]) = *(const uint32_t *)(xenon2.level5_data.boss2_draw_rows + offset);
    *(uint32_t *)(&xenon2.level5_data.boss2_draw_data[6]) = *(const uint32_t *)(xenon2.level5_data.boss2_draw_rows + 2 + offset);
    *(uint32_t *)(&xenon2.level5_data.boss2_draw_data[12]) = *(const uint32_t *)(xenon2.level5_data.boss2_draw_rows + 4 + offset);
    draw_tile_grid(xenon2.level5_data.boss2_flash_grid, 5, 5, obj->y, 0x38);
}

/* level5 $056940: the beam: it grows to its length, then it goes; it hurts the player */
void l5_beam_update(Object *obj)
{
    int16_t y;

    obj->hit[0] = obj->x;
    obj->hit[2] = (int16_t)(obj->x + 0xb);
    if (obj->beam.length != 0x30) {
        obj->beam.length = (int16_t)(obj->beam.length + (int16_t)xenon2.level5_data.turret_param);
        if (obj->beam.length > 0x30)
            obj->beam.length = 0x30;
        obj->y = (int16_t)(obj->y + xenon2.scroll_speed);
        if ((int16_t)(obj->beam.dy >> 16) < 0)
            obj->y = (int16_t)(obj->y - (int16_t)xenon2.level5_data.turret_param);
    } else {
        obj->y = (int16_t)(obj->y + (int16_t)(obj->beam.dy >> 16));
        y = obj->y;
        if ((int16_t)(obj->beam.dy >> 16) >= 0) {
            if (y >= 0xc0)
                goto gone;
        } else {
            y = (int16_t)(y + obj->beam.length);
            if (y < 0)
                goto gone;
        }
    }
    obj->hit[1] = obj->y;
    obj->hit[3] = (int16_t)(obj->y + obj->beam.length);
    if (!player_hits(obj))
        return;
    if (xenon2.shield_timer == 0) {
        player_damage(obj, 6);
        PLAYER->fn_draw = (obj_fn)draw_bob_or_first;
    }
gone:
    kill_thunk(obj);
}

/* level5 $056A10: the beam's heads and the bits of its body in the screen's planes */
void l5_beam_draw(Object *obj)
{
    uint8_t *screen = xenon2.draw_screen;
    uint16_t *dst, *limit = (uint16_t *)(screen + SCREEN_ROWS * SCREEN_ROW_BYTES);
    uint32_t pat0 = 0xfff00000u, pat1 = 0xbfd00000u, pat2 = 0xfff00000u, pat3 = 0xc0300000u;
    uint16_t shift;
    uint32_t base0, base1, base2, base3, base4, base5;
    int16_t y, length;

    if ((int16_t)(obj->beam.dy >> 16) < 0 || obj->beam.length == 0x30)
        draw_bob_masked_at((uint16_t)obj->x, (uint16_t)obj->y, xenon2.level5_data.beam_top_picture);
    if ((int16_t)(obj->beam.dy >> 16) >= 0 || obj->beam.length == 0x30)
        draw_bob_masked_at((uint16_t)obj->x, (uint16_t)(obj->y + obj->beam.length - 1), xenon2.level5_data.beam_end_picture);
    dst = (uint16_t *)(screen + (((uint16_t)obj->x >> 3) & 0xfffe));
    shift = (uint16_t)obj->x & 0xf;
    pat0 >>= shift;
    pat1 >>= shift;
    pat2 >>= shift;
    pat3 >>= shift;
    pat0 = ~pat0;
    base0 = ((pat2 >> 16) << 16) | (pat3 >> 16);
    base1 = ((pat1 >> 16) << 16) | (pat2 >> 16);
    base2 = ((pat0 >> 16) << 16) | (pat0 >> 16);
    base3 = ((pat2 & 0xffff) << 16) | (pat3 & 0xffff);
    base4 = ((pat1 & 0xffff) << 16) | (pat2 & 0xffff);
    base5 = ((pat0 & 0xffff) << 16) | (pat0 & 0xffff);
    length = obj->beam.length;
    y = obj->y;
    if (y < 0) {
        length = (int16_t)(length + y);
        if (length < 0)
            return;
        y = 0;
    }
    dst += (uint32_t)((uint16_t)y * SCREEN_ROW_WORDS);
    pat0 = base0;
    pat1 = base1;
    pat2 = base2;
    pat3 = base3;
    {
        uint32_t pat4 = base4, pat5 = base5;

        while (dst < limit && length != 0) {
            uint16_t m2 = (uint16_t)pat2, h1 = (uint16_t)(pat1 >> 16), l1 = (uint16_t)pat1, h0 = (uint16_t)(pat0 >> 16), l0 = (uint16_t)pat0;
            int p;

            for (p = 0; p < 4; p++)
                dst[SCREEN_PLANE_WORDS * p] = (uint16_t)(dst[SCREEN_PLANE_WORDS * p] & m2);
            dst[0] = (uint16_t)(dst[0] | h1);
            dst[SCREEN_PLANE_WORDS] = (uint16_t)(dst[SCREEN_PLANE_WORDS] | l1);
            dst[2 * SCREEN_PLANE_WORDS] = (uint16_t)(dst[2 * SCREEN_PLANE_WORDS] | h0);
            dst[3 * SCREEN_PLANE_WORDS] = (uint16_t)(dst[3 * SCREEN_PLANE_WORDS] | l0);
            for (p = 0; p < 4; p++)
                dst[1 + SCREEN_PLANE_WORDS * p] = (uint16_t)(dst[1 + SCREEN_PLANE_WORDS * p] & (uint16_t)pat5);
            dst[1] = (uint16_t)(dst[1] | (uint16_t)(pat4 >> 16));
            dst[1 + SCREEN_PLANE_WORDS] = (uint16_t)(dst[1 + SCREEN_PLANE_WORDS] | (uint16_t)pat4);
            dst[1 + 2 * SCREEN_PLANE_WORDS] = (uint16_t)(dst[1 + 2 * SCREEN_PLANE_WORDS] | (uint16_t)(pat3 >> 16));
            dst[1 + 3 * SCREEN_PLANE_WORDS] = (uint16_t)(dst[1 + 3 * SCREEN_PLANE_WORDS] | (uint16_t)pat3);
            dst += SCREEN_ROW_WORDS;
            length--;
        }
    }
}

/* ---- the gates and the guns of tiles ---- */

/* level5 $056B50: three tiles that open and close (the record's position is the cell) */
void l5_spawn_gates(const struct SpawnRec *record)
{
    int16_t x = (int16_t)(record->x - 8), y = (int16_t)(record->y - 8);
    Cell *cells = (Cell *)(xenon2.level_init_src + (uint16_t)(((uint16_t)x >> 3) & 0xfe) + (int16_t)(uint16_t)((uint32_t)MAP_ROW((uint16_t)y >> 4)));
    const struct Level5Gate *rec = xenon2.level5_data.gates;
    int count;

    cells[0] = 0x8ad3;
    cells[1] = cells[2] = 0;
    cells[3] = 0x8add;
    for (count = 2; count >= 0; count--, rec++) {
        Object *obj = insert_head(LIST_C);
        uint16_t step;

        obj->type = rec->type;
        obj->fn_update = (obj_fn)rec->update;
        obj->fn_draw = (obj_fn)obj_nop_thunk;
        obj->fn_hit = (void *)rec->hit;
        obj->x = x;
        obj->y = y;
        obj->l5.cell = 0;
        obj->energy = (int16_t)xenon2.level5_data.gates_energy;
        obj->hit[0] = 0x3e8;
        obj->hit[2] = 0x3e8;
        obj->map = cells;
        step = rec->step;
        cells += (int16_t)step / 2;
        x = (int16_t)(x + (int16_t)(step << 3));
    }
}

/* level5 $056BEE: a gate: its cells go round */
void l5_gate_update(Object *obj)
{
    int16_t cell = (int16_t)(obj->l5.cell + 4);

    if (cell >= 0x10)
        cell = 0;
    obj->l5.cell = cell;
    obj->map[0] = xenon2.level5_data.gate_update_data[cell / 2];
    obj->map[1] = xenon2.level5_data.gate_update_data[cell / 2 + 1];
    obj->hit[0] = obj->x;
    obj->hit[2] = (int16_t)(obj->x + 0x20);
    obj->hit[1] = (int16_t)(obj->y - xenon2.scroll_pos + 2);
    obj->hit[3] = (int16_t)(obj->hit[1] + 0xc);
}

/* level5 $056C40: a gun in the wall: its cell by four */
void l5_wall_update(Object *obj)
{
    const Cell *table = obj->type == 0x128 ? xenon2.level5_data.wall_cells_128 : xenon2.level5_data.wall_cells_other;

    obj->l5.cell = (int16_t)((obj->l5.cell + 1) & 3);
    obj->map[0] = table[obj->l5.cell];
    obj->hit[0] = (int16_t)(obj->x + 3);
    obj->hit[2] = (int16_t)(obj->hit[0] + 0xa);
    obj->hit[1] = (int16_t)(obj->y - xenon2.scroll_pos + 3);
    obj->hit[3] = (int16_t)(obj->hit[1] + 0xa);
}

/* level5 $056CA4: the hit of the gun on the left: at the end it and the two before it go */
void l5_wall_hit_a(Object *shot, Object *obj, uint16_t damage)
{
    Object *second, *third;
    Cell *c;

    (void)shot;
    obj->fn_draw = (obj_fn)l5_wall_flash_a;
    if (!dies(obj, damage))
        return;
    spawn_effect((int16_t)(obj->x + 8), (int16_t)(obj->y - xenon2.scroll_pos + 8), xenon2.anim_explosion, draw_bob_masked);
    c = obj->map;
    c[0] = 0x87b5;
    c[1] = c[2] = 0;
    c[3] = 0x87ab;
    second = obj->prev;
    kill_thunk(obj);
    third = second->prev;
    kill_thunk(second);
    kill_thunk(third);
    xenon2.player_score += 0xc8;
}

/* level5 $056D5C: the hit of the gun on the right: it and the two after it go */
void l5_wall_hit_b(Object *shot, Object *obj, uint16_t damage)
{
    Object *second, *third;
    Cell *c;

    (void)shot;
    obj->fn_draw = (obj_fn)l5_wall_flash_b;
    if (!dies(obj, damage))
        return;
    spawn_effect((int16_t)(obj->x + 8), (int16_t)(obj->y - xenon2.scroll_pos + 8), xenon2.anim_explosion, draw_bob_masked);
    c = obj->map;
    c[0] = 0x87ab;
    c[-2] = c[-1] = 0;
    c[-3] = 0x87b5;
    second = obj->next;
    kill_thunk(obj);
    third = second->next;
    kill_thunk(second);
    kill_thunk(third);
    xenon2.player_score += 0xc8;
}

/* level5 $056E14: the flash of the gun on the left, one frame */
void l5_wall_flash_a(Object *obj)
{
    obj->fn_draw = (obj_fn)obj_nop_thunk;
    draw_tile_grid_flash(&xenon2.level5_data.wall_flash_a_grid, 0, 0, (int16_t)(obj->y - xenon2.scroll_pos), (uint16_t)((uint16_t)obj->x >> 1));
}

/* level5 $056E1A: the flash of the gun on the right, one frame */
void l5_wall_flash_b(Object *obj)
{
    obj->fn_draw = (obj_fn)obj_nop_thunk;
    draw_tile_grid_flash(&xenon2.level5_data.wall_flash_b_grid, 0, 0, (int16_t)(obj->y - xenon2.scroll_pos), (uint16_t)((uint16_t)obj->x >> 1));
}

/* ---- the bridge pieces ($56E40) ---- */

/* level5 $056E40: a piece of the bridge */
void l5_spawn_bridge(const struct SpawnRec *record)
{
    Object *obj = spawn_marker_enemy(0xf8, record, (void (*)(void))(obj_fn)l5_bridge_update);
    Cell *c;

    obj->fn_hit = (obj_fn)l5_bridge_hit;
    obj->energy = (int16_t)xenon2.level5_data.bridge_energy;
    obj->hit[0] = 0x3e8;
    obj->hit[2] = 0x3e8;
    c = obj->map;
    if (c[-1] == 0x8669) {
        c[-1] = 0xc7;
        c[MAP_COLS - 1] = 0x177;
    }
    if (c[2] == 0x8673) {
        c[2] = 0xc7;
        c[MAP_COLS + 2] = 0x177;
    }
    c[0] = xenon2.level5_data.bridge_cells[0];
    c[1] = xenon2.level5_data.bridge_cells[1];
    c[MAP_COLS] = xenon2.level5_data.bridge_cells[2];
    c[MAP_COLS + 1] = xenon2.level5_data.bridge_cells[3];
}


/* level5 $056EE2: the piece: its cells go round by v28 and now and then it fires all round */
void l5_bridge_update(Object *obj)
{
    Cell *c;
    const Cell *cells;

    if ((int16_t)(xenon2.scroll_max + 0xd0) < obj->y) {
        kill_thunk(obj);
        return;
    }
    obj->hit[0] = (int16_t)(obj->x + 4);
    obj->hit[2] = (int16_t)(obj->hit[0] + 0x18);
    obj->hit[1] = (int16_t)(obj->y - xenon2.scroll_pos + 4);
    obj->hit[3] = (int16_t)(obj->hit[1] + 0x18);
    obj->bridge.cell = (int16_t)((obj->bridge.cell + 1) & 0xf);
    if (add_carry(&obj->shot_count, xenon2.level5_data.bridge_fire_rate)) {
        int dir;

        obj->shot_count = (uint8_t)(random_next() & 0x3f);
        for (dir = 7; dir >= 0; dir--) {
            Object *shot = spawn_enemy_shot((uint16_t)(obj->x + 0x10), (uint16_t)(obj->y - xenon2.scroll_pos + 0x10), (uint16_t)dir, xenon2.level5_data.bridge_shot_steps, 0);

            set_anim(shot, xenon2.level5_data.anim_bridge_update);
        }
    }
    c = obj->map;
    cells = xenon2.level5_data.bridge_cells + (uint32_t)((uint16_t)obj->bridge.cell & 0xe) * 2;
    c[0] = cells[0];
    c[1] = cells[1];
    c[MAP_COLS] = cells[2];
    c[MAP_COLS + 1] = cells[3];
}

/* level5 $056FDC: the hit of the piece: the burst, the cells go back, the score */
void l5_bridge_hit(Object *shot, Object *obj, uint16_t damage)
{
    Cell *c;

    (void)shot;
    obj->fn_draw = (obj_fn)l5_bridge_flash;
    if (!dies(obj, damage))
        return;
    spawn_effect((int16_t)(obj->x + 0x10), (int16_t)(obj->y - xenon2.scroll_pos + 0x10), xenon2.big_explosion_anim, draw_bob_masked);
    c = obj->map;
    if (c[-1] == 0xc7) {
        c[-1] = 0x8669;
        c[MAP_COLS - 1] = 0x8707;
    }
    if (c[2] == 0xc7) {
        c[2] = 0x8673;
        c[MAP_COLS + 2] = 0x8711;
    }
    c[0] = c[1] = 0;
    c[MAP_COLS] = c[MAP_COLS + 1] = 0;
    xenon2.player_score += 0x1f4;
    kill_thunk(obj);
}

/* level5 $05706C: the flash of the piece, one frame */
void l5_bridge_flash(Object *obj)
{
    obj->fn_draw = (obj_fn)obj_nop_thunk;
    draw_tile_grid_flash((const uint16_t *)xenon2.level5_data.bridge_cells, 1, 1, (int16_t)(obj->y - xenon2.scroll_pos), (uint16_t)((uint16_t)obj->x >> 1));
}

/* ---- the units of eight directions ($570AA) ---- */

/* level5 $057090: the unit of the record's group, once (a flag by the group) */
void l5_spawn_group(const struct SpawnRec *record)
{
    uint16_t group = record->index;

    if (xenon2.l5_group_done[group] == 0)
        (l5_spawn_unit(record))->unit.group = (int16_t)group;
}

/* level5 $0570AA: a unit of eight directions made of cells */
Object *l5_spawn_unit(const struct SpawnRec *record)
{
    Object *obj = spawn_marker_enemy(0xf4, record, (void (*)(void))(obj_fn)l5_unit_update);
    Cell *c;
    const Cell *cells;

    obj->unit.dir = (obj->unit.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)record->steps) << 16;
    obj->fn_hit = (obj_fn)l5_unit_hit;
    obj->energy = (int16_t)xenon2.level5_data.group_energy;
    obj->hit[0] = 0x3e8;
    obj->hit[2] = 0x3e8;
    c = obj->map;
    cells = xenon2.level5_data.group_cells + ((uint32_t)(uint16_t)(int16_t)(obj->unit.dir >> 16) << 2);
    obj->unit.group = -1;
    if (c[-1] == 0x867d) {
        c[-1] = 0x80d7;
        c[MAP_COLS - 1] = 0x8187;
    }
    if (c[2] == 0x8687) {
        c[2] = 0x8287;
        c[MAP_COLS + 2] = 0x8339;
    }
    c[0] = cells[0];
    c[1] = cells[1];
    c[MAP_COLS] = cells[2];
    c[MAP_COLS + 1] = cells[3];
    return obj;
}

/* level5 $05715E: the unit: it turns to the player by one step at a time and fires in its direction */
void l5_unit_update(Object *obj)
{
    Cell *c;
    const Cell *cells;

    if ((int16_t)(xenon2.scroll_max + 0xd0) < obj->y) {
        kill_thunk(obj);
        return;
    }
    obj->hit[0] = (int16_t)(obj->x + 4);
    obj->hit[2] = (int16_t)(obj->hit[0] + 0x18);
    obj->hit[1] = (int16_t)(obj->y - xenon2.scroll_pos + 4);
    obj->hit[3] = (int16_t)(obj->hit[1] + 0x18);
    if (obj->unit.tick == 0 && (int16_t)(obj->y - xenon2.scroll_pos) < -16)
        goto cells;
    if (obj->unit.tick != 8) {
        obj->unit.tick = (int16_t)(obj->unit.tick + 1);
        obj->unit.dir = (obj->unit.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)(((int16_t)(obj->unit.dir >> 16) + 1) & 7)) << 16;
        goto cells;
    }
    {
        uint16_t dir = direction8((int16_t)(PLAYER->x - obj->x - 0x10), (int16_t)(PLAYER->y - obj->y + xenon2.scroll_pos - 0x10));

        dir = (uint16_t)(dir - (uint16_t)(int16_t)(obj->unit.dir >> 16));
        if (dir != 0) {
            dir &= 7;
            if (dir < 4)
                obj->unit.dir = (obj->unit.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)((int16_t)(obj->unit.dir >> 16) + 1)) << 16;
            else
                obj->unit.dir = (obj->unit.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)((int16_t)(obj->unit.dir >> 16) - 1)) << 16;
        }
        obj->unit.dir = (obj->unit.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)((int16_t)(obj->unit.dir >> 16) & 7)) << 16;
    }
    if (add_carry(&obj->shot_count, xenon2.level5_data.unit_fire_rate)) {
        int16_t x = (int16_t)(obj->x + 0x10), y = (int16_t)(obj->y - xenon2.scroll_pos + 0x10);
        uint16_t dir_index = (uint16_t)((int16_t)(obj->unit.dir >> 16) & 7);

        obj->shot_count = (uint8_t)(random_next() & 0x3f);
        x = (int16_t)(x + xenon2.level5_data.unit_dirs[dir_index].dx);
        y = (int16_t)(y + xenon2.level5_data.unit_dirs[dir_index].dy);
        dir_index &= 3;
        if (!(xenon2.frame_tick & 1))
            dir_index += 4;
        spawn_enemy_shot((uint16_t)x, (uint16_t)y, (uint16_t)(int16_t)(obj->unit.dir >> 16), xenon2.level5_data.unit_shot_steps, (void *)xenon2.level5_data.unit_shot_pictures[dir_index]);
    }
cells:
    c = obj->map;
    cells = xenon2.level5_data.group_cells + ((uint32_t)(uint16_t)(int16_t)(obj->unit.dir >> 16) << 2);
    c[0] = cells[0];
    c[1] = cells[1];
    c[MAP_COLS] = cells[2];
    c[MAP_COLS + 1] = cells[3];
}

/* level5 $0572EA: the hit of the unit: at the end the burst, its flag, the cells go back */
void l5_unit_hit(Object *shot, Object *obj, uint16_t damage)
{
    Cell *c;

    (void)shot;
    obj->fn_draw = (obj_fn)l5_unit_flash;
    if (!dies(obj, damage))
        return;
    if (obj->unit.group >= 0)
        xenon2.l5_group_done[(uint32_t)(uint16_t)obj->unit.group] = 0xff;
    spawn_effect((int16_t)(obj->x + 0x10), (int16_t)(obj->y - xenon2.scroll_pos + 0x10), xenon2.big_explosion_anim, draw_bob_masked);
    c = obj->map;
    if (c[-1] == 0x80d7) {
        c[-1] = 0x867d;
        c[MAP_COLS - 1] = 0x871b;
    }
    if (c[2] == 0x8287) {
        c[2] = 0x8687;
        c[MAP_COLS + 2] = 0x8725;
    }
    c[0] = c[1] = 0;
    c[MAP_COLS] = c[MAP_COLS + 1] = 0;
    xenon2.player_score += 0x190;
    kill_thunk(obj);
}

/* level5 $057388: the flash of the unit, one frame */
void l5_unit_flash(Object *obj)
{
    obj->fn_draw = (obj_fn)obj_nop_thunk;
    draw_tile_grid_flash((const uint16_t *)xenon2.level5_data.group_cells, 1, 1, (int16_t)(obj->y - xenon2.scroll_pos), (uint16_t)((uint16_t)obj->x >> 1));
}

/* ---- the guns of cells ($573AC) ---- */

/* level5 $0573AC: a gun in the wall that fires out */
void l5_spawn_gun(const struct SpawnRec *record)
{
    int right = record->steps != 0;
    Object *obj = spawn_marker_enemy(right ? 0x10c : 0x108, record, (void (*)(void))(obj_fn)l5_gun_update);
    const Cell *cells = right ? xenon2.level5_data.gun_cells_a : xenon2.level5_data.gun_cells_b;
    const uint16_t *anim = right ? &xenon2.level5_data.gun_anim_a : &xenon2.level5_data.gun_anim_b;

    obj->fn_hit = (void *)hit_tile;
    obj->energy = (int16_t)xenon2.level5_data.gun_energy;
    obj->hit[0] = 0x3e8;
    obj->hit[2] = 0x3e8;
    obj->gfx = (void *)cells;
    obj->anim = anim;
    obj->map[0] = cells[0];
    obj->score = 0x64;
    obj->part_b.dx = 0;
    obj->part_b.dy = 0;
}

/* level5 $05741C: the gun: its cell goes round and it fires every third step */
void l5_gun_update(Object *obj)
{
    const Cell *cells;

    if ((int16_t)(xenon2.scroll_max + 0xd0) < obj->y) {
        kill_thunk(obj);
        return;
    }
    obj->hit[0] = (int16_t)(obj->x + 2);
    obj->hit[2] = (int16_t)(obj->hit[0] + 0xc);
    obj->hit[1] = (int16_t)(obj->y - xenon2.scroll_pos + 2);
    obj->hit[3] = (int16_t)(obj->hit[1] + 0xc);
    if (obj->l5.cell == 0) {
        if (!add_carry(&obj->shot_count, xenon2.level5_data.gun_fire_rate))
            return;
        obj->shot_count = (uint8_t)(random_next() & 0x3f);
    }
    obj->l5.cell = (int16_t)(obj->l5.cell + 1);
    cells = obj->type == 0x108 ? xenon2.level5_data.gun_cells_b : xenon2.level5_data.gun_cells_a;
    obj->map[0] = cells[(uint16_t)obj->l5.cell];
    if (obj->l5.cell == 3) {
        int16_t x = (int16_t)(obj->x + 0x18), y = (int16_t)(obj->y - xenon2.scroll_pos + 8);
        uint16_t dir = 2;
        Object *shot;

        if (obj->type != 0x108) {
            dir = 6;
            x = (int16_t)(x - 0x18);
        }
        shot = spawn_enemy_shot((uint16_t)x, (uint16_t)y, dir, xenon2.level5_data.gun_shot_steps, 0);
        set_anim(shot, obj->type == 0x108 ? xenon2.level5_data.gun_anim_108 : xenon2.level5_data.gun_anim_other);
        return;
    }
    if (obj->l5.cell == 7)
        obj->l5.cell = 0;
}
