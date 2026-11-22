/* level4: level 4's own code (the blob at $54E40 while level 4 is loaded), written like level3.c; a function has `level4 $ADDR` for its address. */
#include "weapons.h"
#include "shop.h"
#include "game_all.h"
#include <string.h>


/* level 4: its parameter block at E42 (energies, fire rates, shot steps), by the object that reads them */

/* the scratch fields of Object, by the kind that uses them */


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

static void kill_thunk(Object *o)
{
    o->type = 4;
    o->fn_update = (obj_fn)free_object;
    o->fn_draw = (obj_fn)obj_nop_thunk;
    o->fn_hit = (void *)obj_nop_thunk;
    o->is_part = 0;
    o->persists = 0;
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

/* the player's direction from the object (the eight of direction8), the offsets are the original's */
static uint16_t aim_at_player(const Object *obj, int16_t dx, int16_t dy)
{
    return direction8((int16_t)(PLAYER->x - obj->x - dx), (int16_t)(PLAYER->y - obj->y - dy));
}

/* the picture of the table at $77B70 (by the index of the level) */
static const uint16_t *shot_picture(uint16_t index)
{
    return xenon2.level4_data.shot_gfx_table[index]->pic;
}

/* the four cells of a table entry go to two cells at the cell and two a row below it */
static void put_cells(Cell *cell, const Cell *table)
{
    cell[0] = table[0];
    cell[1] = table[1];
    cell[MAP_COLS] = table[2];
    cell[MAP_COLS + 1] = table[3];
}

void l4_tower_update(Object *obj);
void l4_tower_hit(Object *shot, Object *enemy, uint16_t damage);
void l4_tower_draw(Object *obj);
void l4_spawn_boss2(const struct SpawnRec *record, uint16_t slots);
void l4_spawn_walker(const struct SpawnRec *record);
void l4_spawn_boss1(void);
void l4_spawn_bar(const struct SpawnRec *record);

/* level4 $054E10, $057272: level events by which (record = the record that fired): 0 the wave of $326C, 1 to 6 formations */
void l4_event(uint16_t which, const struct SpawnRec *record)
{
    const struct SpawnDef *tables[7] = {
        0, xenon2.level4_data.formation_1.defs, &xenon2.level4_data.formation_2.defs, &xenon2.level4_data.formation_3.defs,
        &xenon2.level4_data.formation_4.defs, &xenon2.level4_data.formation_5.defs, &xenon2.level4_data.formation_6.defs };

    if (which == 0)
        spawn_wave_326c(0, record);   /* $E5A */
    else if (which < 7)
        spawn_formation(0, record, tables[which]);
}

/* level4 $054E18, $05585E: the hooks by which */
void l4_hook(uint16_t which, const struct SpawnRec *record, uint16_t param)
{
    switch (which) {
    case 0:
        note_spawn_record(record, param);   /* $EA2 */
        break;
    case 1:
        l4_spawn_tower(record);
        break;
    case 2:
        l4_spawn_boss2(record, param);
        break;
    case 3:
        l4_spawn_walker(record);
        break;
    case 4:
        l4_spawn_boss1();
        break;
    case 5:
        l4_spawn_bar(record);
        break;
    }
}

/* level4 $054E20, $0572D8: the data is copied once (backwards: the copy lies above its source) */
void l4_init(void)
{
    int i;

    if (xenon2.level_init_src != (uint8_t *)xenon2.level_data_dst) {
        for (i = 5999; i >= 0; i--)
            xenon2.level_data_dst[i] = i >= 80 ? xenon2.level_data_dst[i - 80] : xenon2.level4_data.data_src[i];
        xenon2.level_init_src = (uint8_t *)xenon2.level_data_dst;
    }
    xenon2.scroll_min = 0x880;
    xenon2.scroll_max = 0x11ff;
    xenon2.l4_boss2_spawned = 0;
    xenon2.l4_boss1_spawned = 0;
    xenon2.level4_data.boss1_pieces[1].energy = xenon2.level4_data.init_patch;
    xenon2.level4_data.boss1_pieces[2].energy = xenon2.level4_data.init_patch;
    xenon2.level4_data.boss1_pieces[0].energy = xenon2.level4_data.init_patch_2;
    xenon2.level4_data.boss2_pieces[16].energy = xenon2.level4_data.init_patch_3;
    xenon2.level4_data.boss2_pieces[17].energy = xenon2.level4_data.init_patch_3;
    xenon2.level4_data.boss2_pieces[18].energy = xenon2.level4_data.init_patch_3;
    xenon2.level4_data.boss2_pieces[19].energy = xenon2.level4_data.init_patch_3;
    xenon2.level4_data.boss2_pieces[15].energy = xenon2.level4_data.init_patch_4;
    xenon2.level4_data.boss2_pieces[4].energy = xenon2.level4_data.init_patch_5;
    xenon2.difficulty_x1 = (uint16_t)(xenon2.difficulty * 6);
    xenon2.difficulty_x2 = (uint16_t)(xenon2.difficulty * 8);
}

/* level4 $057064: the tower of tiles: it fires four ways by its phase v28 */
void l4_tower_update(Object *obj)
{
    const Cell *cells;
    uint16_t top;

    if ((int16_t)(xenon2.scroll_max + 0xd0) < obj->y) {
        kill_thunk(obj);
        return;
    }
    obj->hit[0] = (int16_t)(obj->x + 2);
    obj->hit[2] = (int16_t)(obj->x + 0x1e);
    top = (uint16_t)(obj->y - xenon2.scroll_pos + 2);
    obj->hit[1] = (int16_t)top;
    obj->hit[3] = (int16_t)(top + 0x1c);
    if (obj->tower.phase == 0) {
        uint16_t sum = (uint16_t)(obj->shot_count + xenon2.level4_data.tower_fire_rate);

        obj->shot_count = (uint8_t)sum;
        if (sum > 0xff) {
            obj->shot_count = (uint8_t)(random_next() & 0x3f);
            obj->tower.phase = 1;
        }
        return;
    }
    cells = (obj->type == 0xd0 ? xenon2.level4_data.tower_cells_d0 : xenon2.level4_data.tower_cells_d4) + ((uint16_t)obj->tower.phase << 2);
    put_cells(obj->map, cells);
    obj->tower.phase++;
    if (obj->tower.phase == 4) {
        int16_t x = (int16_t)(obj->x + 0x12), y = (int16_t)(obj->y - xenon2.scroll_pos + 0xe);
        uint16_t angle = 2;

        if (obj->type != 0xd4) {
            angle = 6;
            x = (int16_t)(x - 8);
        }
        spawn_enemy_shot((uint16_t)x, (uint16_t)y, angle, xenon2.level4_data.tower_shot_steps, xenon2.level4_data.shot_gfx_table[xenon2.level4_data.tower_param]->pic);
        return;
    }
    if (obj->tower.phase == 6)
        obj->tower.phase = 0;
}

/* level4 $0571C0: the hit of a tower: it flashes; when it is shot to pieces it bursts and the cells are the broken ones */
void l4_tower_hit(Object *shot, Object *enemy, uint16_t damage)
{
    uint16_t old = (uint16_t)enemy->energy;

    enemy->energy = (int16_t)(old - damage);
    if (old > damage) {
        enemy->fn_draw = (obj_fn)l4_tower_draw;
        return;
    }
    spawn_effect((int16_t)(enemy->x + 0x10), (int16_t)(enemy->y - xenon2.scroll_pos + 0x10), xenon2.big_explosion_anim, draw_bob_masked);
    put_cells(enemy->map, enemy->type == 0xd0 ? xenon2.level4_data.tower_hit_cells_d0 : xenon2.level4_data.tower_hit_cells_d4);
    xenon2.player_score += 0x190;
    kill_thunk(enemy);
}

/* level4 $057246: the flash of a tower, one frame */
void l4_tower_draw(Object *obj)
{
    obj->fn_draw = (obj_fn)obj_nop_thunk;
    draw_tile_grid_flash((const uint16_t *)(obj->type == 0xd0 ? xenon2.level4_data.tower_cells_d0 : xenon2.level4_data.tower_cells_d4), 1, 1, (int16_t)(obj->y - xenon2.scroll_pos), (uint16_t)((uint16_t)obj->x >> 1));
}

/* level4 $05700A: a marker makes a tower; the record's word at +$C says which of the two */
void l4_spawn_tower(const struct SpawnRec *record)
{
    int flag = record->steps != 0;
    Object *obj = spawn_marker_enemy(flag ? 0xd0 : 0xd4, record, (void (*)(void))l4_tower_update);

    obj->fn_draw = (obj_fn)obj_nop_thunk;
    obj->fn_hit = (void *)l4_tower_hit;
    obj->energy = (int16_t)xenon2.level4_data.tower_energy;
    obj->hit[0] = 0x3e8;
    obj->hit[2] = 0x3e8;
    put_cells(obj->map, flag ? xenon2.level4_data.tower_cells_d0 : xenon2.level4_data.tower_cells_d4);
}

/* level4 $055090: the scroll: the floor of the scroll ends at $DF0 and between $AE0 and $E50 an enemy comes every 16 pixels */
void l4_scroll(void)
{
    if ((int16_t)xenon2.scroll_max >= 0xdf0 && (int16_t)xenon2.scroll_pos <= 0xdf0)
        xenon2.scroll_max = 0xdf0;
    if ((xenon2.frame_tick & 0xf) != 0 || (int16_t)xenon2.scroll_pos > 0xe50 || (int16_t)xenon2.scroll_pos < 0xae0)
        return;
    {
        Object *obj = alloc_in(LIST_C);
        uint16_t offset = (random_next() & 1) ? 0xc : 0;
        const struct Level4ScrollEnemy *entry;

        if (xenon2.frame_tick & 0x20)
            offset += 0x18;
        entry = &xenon2.level4_data.scroll_enemies[offset / 12];
        obj->type = (int16_t)entry->type;
        obj->x = (int16_t)entry->x;
        obj->fn_update = (obj_fn)entry->update;
        obj->fn_hit = (void *)hit_enemy;
        obj->fn_draw = draw_bob_masked;
        anim_header_set(obj, entry->anim);
        obj->shot_count = (uint8_t)random_next();
        obj->y = -0x20;
        obj->l4.size = 0;
        obj->energy = (int16_t)xenon2.level4_data.scroll_energy;
        obj->big = 0xff;
        obj->kind = 0;
        obj->score = 0x12c;
        bbox_from_frame(obj);
    }
}

/* ---- the enemies of the scroll ---- */

static void l4_kill(Object *obj)
{
    kill_thunk(obj);
}

/* level4 $05524A: a gun that comes down the screen: now and then a part flies off it and it fires */
void l4_enemy_a(Object *obj)
{
    anim_step(obj);
    obj->y = (int16_t)(obj->y + xenon2.scroll_speed + xenon2.level4_data.enemy_a_fall);
    if (obj->y >= 0xc8) {
        l4_kill(obj);
        return;
    }
    obj->l4_gun.tick++;
    if (obj->l4_gun.tick == 8) {
        if (add_carry(&obj->shot_count, xenon2.level4_data.enemy_a_fire_rate)) {
            Object *part;
            const struct Level4EnemyGun *gun = &xenon2.level4_data.enemy_a_guns[obj->type == 0xc8 ? 0 : 1];

            obj->shot_count = (uint8_t)(random_next() & 0x3f);
            part = alloc_in(LIST_B);
            part->type = gun->type;
            part->fn_update = (obj_fn)l4_shot_a;
            part->fn_draw = draw_bob_masked;
            part->x = (int16_t)gun->x;
            part->y = (int16_t)(obj->y - 0x1e);
            part->l4_gun.tick = 0;
            set_anim(part, gun->anim);
        }
    } else if (obj->l4_gun.tick == 0xc) {
        obj->l4_gun.tick = 0;
    }
    if (add_carry(&obj->shot_rate, (uint8_t)xenon2.level4_data.tower_energy)) {
        int16_t x = (int16_t)(obj->x + 0x14);

        obj->shot_rate = (uint8_t)(random_next() & 0x3f);
        if (obj->type != 0xc8)
            x = (int16_t)(x - 0x24);
        spawn_enemy_shot((uint16_t)x, (uint16_t)obj->y, 4, xenon2.level4_data.enemy_a_shot_steps, shot_picture(xenon2.level4_data.enemy_a_shot_picture));
    }
    bbox_from_frame(obj);
}

/* level4 $05541E: the part of the gun: when its animation ends it becomes an explosion; two of its pictures drop an enemy that flies */
void l4_shot_a(Object *obj)
{
    int16_t angle, dx;
    const uint16_t *anim, *cells;
    uint16_t kind;
    Object *drop;
    uint16_t *group;
    int i;

    obj->y = (int16_t)(obj->y + xenon2.scroll_speed);
    anim_step(obj);
    if (obj->anim_delay == 0) {
        const struct Pic *frame = obj->gfx;
        int16_t x, y;

        x = (int16_t)(obj->x - (int16_t)frame->hot_x);
        y = (int16_t)(obj->y - (int16_t)frame->hot_y);
        x = (int16_t)(x * 2 + (int16_t)frame->width);
        y = (int16_t)(y * 2 + (int16_t)frame->height);
        obj->type = 0xc;
        obj->fn_update = (obj_fn)animate_only;
        obj->x = (int16_t)(x >> 1);
        obj->y = (int16_t)(y >> 1);
        anim_header_set(obj, xenon2.anim_explosion);
        return;
    }
    if (obj->gfx == (void *)xenon2.level4_data.shot_a_picture) {
        angle = 0x80;
        dx = -10;
        cells = xenon2.level4_data.shot_a_cells_2;
    } else if (obj->gfx == (void *)xenon2.level4_data.shot_a_picture_3) {
        angle = 0;
        dx = 10;
        cells = xenon2.level4_data.shot_a_cells_4;
    } else {
        return;
    }
    drop = alloc_in(LIST_C);
    drop->type = 0xf8;
    drop->fn_update = (obj_fn)l4_enemy_f8;
    drop->fn_draw = draw_bob_masked;
    drop->fn_hit = (void *)hit_enemy;
    drop->l4.angle = (uint16_t)(angle);
    drop->turn = 0;
    drop->turn_step = 0;
    drop->x = (int16_t)(obj->x + dx);
    drop->y = obj->y;
    drop->path.wait = 0;
    drop->score = 0x64;
    drop->energy = (int16_t)xenon2.level4_data.shot_a_energy;
    drop->big = 0;
    drop->kind = (int16_t)xenon2.pickup_cycle;
    xenon2.pickup_cycle++;
    group = xenon2.pickup_groups;
    for (i = 0; i < 8; i++, group += 2)
        if (group[0] == 0) {
            group[0] = (uint16_t)drop->kind;
            group[1] = 1;
            break;
        }
    drop->hit[0] = 0x3e8;
    drop->hit[2] = 0x3e8;
    kind = (uint16_t)((uint16_t)angle >> 3);
    anim = xenon2.level4_data.anims_by_kind[kind / 4];
    set_anim(drop, anim);
    drop->script = (uint8_t *)cells;
    drop->cursor = (const struct PathCmd *)cells;
    drop->steps = (int16_t)xenon2.level4_data.shot_a_steps;
    drop->shot_count = drop->shot_rate = 0;
}

/* level4 $055724: the flying enemy: it follows its script and shows the picture of its direction */
void l4_enemy_f8(Object *obj)
{
    uint16_t old_angle = (uint16_t)(int16_t)obj->l4.angle, index, old_index;

    enemy_update(obj);
    if (obj->type != 0xf8) {
        const uint8_t *table;

        obj->type = 0xf8;
        obj->fn_update = (obj_fn)l4_enemy_f8;
        obj->fn_hit = (void *)hit_enemy;
        obj->fn_draw = draw_bob_masked;
        index = (uint16_t)(random_next() & 8);
        if (obj->x >= 0xa0)
            index = (uint16_t)(index + 4);
        table = xenon2.level4_data.enemy_f8_paths[index / 4];
        obj->script = (uint8_t *)table;
        obj->cursor = (const struct PathCmd *)table;
        obj->path.wait = 0;
    }
    index = (uint16_t)((((uint16_t)(int16_t)obj->l4.angle + 0x10) >> 3) & 0x1c);
    old_index = (uint16_t)(((old_angle + 0x10) >> 3) & 0x1c);
    if (index == old_index)
        return;
    set_anim(obj, xenon2.level4_data.anims_by_kind[index / 4]);
    bbox_from_frame(obj);
}

/* level4 $0557CC: a gun that fires rings of shots */
void l4_enemy_b(Object *obj)
{
    anim_step(obj);
    obj->y = (int16_t)(obj->y + xenon2.scroll_speed + xenon2.level4_data.enemy_b_fall);
    if (obj->y >= 0xc8) {
        l4_kill(obj);
        return;
    }
    if (add_carry(&obj->shot_count, xenon2.level4_data.enemy_b_fire_rate)) {
        int16_t x = (int16_t)(obj->x + 0x1a), y = (int16_t)(obj->y + 0x1a);
        uint16_t angle = 0xa;
        const uint16_t *gfx = xenon2.level4_data.enemy_b_picture;

        obj->shot_count = (uint8_t)(random_next() & 0x3f);
        if (obj->type != 0xd8) {
            angle = 0xd;
            x = (int16_t)(x - 0x34);
            gfx = xenon2.level4_data.enemy_b_picture_2;
        }
        spawn_enemy_shot((uint16_t)x, (uint16_t)y, angle, xenon2.level4_data.enemy_b_shot_steps, gfx);
    }
    bbox_from_frame(obj);
}

/* ---- the first boss ($55F80): nineteen objects, the first the core ---- */

/* level4 $055F80: the core, two guns and a chain of sixteen pieces by the table at $55880 */
void l4_spawn_boss1(void)
{
    const struct Level4Boss1Piece *piece = xenon2.level4_data.boss1_pieces;
    Object *first = 0;
    int n;

    if (xenon2.l4_boss1_spawned != 0)
        return;
    xenon2.l4_boss1_spawned = 0xff;
    for (n = 0; n < 19; n++) {
        Object *obj = insert_head(LIST_C);

        if (!first)
            first = obj;
        obj->type = 0x50;
        obj->fn_update = (obj_fn)piece->update;
        obj->fn_hit = (void *)piece->hit;
        obj->fn_draw = (obj_fn)piece->draw;
        obj->x = (int16_t)piece->x;
        obj->persists = -1;
        obj->y = (int16_t)(piece->y - xenon2.scroll_pos);
        obj->l4.size = 0;
        obj->l4.angle = 0;
        obj->turn = 0;
        obj->turn_step = 0;
        obj->steps = 3;
        obj->energy = (int16_t)piece->energy;
        obj->big = -1;
        obj->l4.extra = 0;
        obj->hit[0] = 0x3e8;
        obj->hit[2] = 0x3e8;
        obj->target = first;
        piece++;
    }
    xenon2.l4_boss1_guns_left = 2;
}

/* the box of a gun or its remains: six pixels in, twenty wide */
static void gun_box(Object *obj)
{
    obj->hit[0] = (int16_t)(obj->x + 6);
    obj->hit[2] = (int16_t)(obj->hit[0] + 0x14);
    obj->hit[1] = (int16_t)(obj->y + 6);
    obj->hit[3] = (int16_t)(obj->hit[1] + 0x14);
}

/* level4 $055C74: the core: it keeps the screen from scrolling past it and now and then it fires (sfx_cmd2 = the shake) */
void l4_b1_core(Object *obj)
{
    uint16_t sway;

    if ((int16_t)xenon2.scroll_max < 0x10)
        xenon2.scroll_max = 0x10;
    sway = (uint16_t)(xenon2.scroll_speed + xenon2.level4_data.b1_core_sway[(xenon2.frame_tick & 0xf)]);
    xenon2.l4_core_sway = sway;
    obj->y = (int16_t)(obj->y + (int16_t)sway);
    if (xenon2.l4_boss1_guns_left != 0) {
        obj->hit[0] = 0x3e8;
        obj->hit[2] = 0x3e8;
    } else {
        obj->hit[0] = 0x94;
        obj->hit[2] = 0xac;
        obj->hit[1] = (int16_t)(obj->y + 0x20);
        obj->hit[3] = (int16_t)(obj->hit[1] + 8);
    }
    if (obj->core.tick == 0) {
        if (!add_carry(&obj->shot_count, xenon2.level4_data.b1_core_fire_rate))
            return;
        obj->shot_count = (uint8_t)(random_next() & 0x3f);
        xenon2.sfx_cmd2 = 0xf;
    }
    {
        uint8_t low = (uint8_t)((obj->core.tick & 0xff) + 1);

        obj->core.tick = (int16_t)((obj->core.tick & 0xff00) | low);
        if (low >= 0x80) {
            obj->core.tick = 0;
            return;
        }
        if (low == 0x78)
            xenon2.sfx_cmd2 = 0xf;
    }
}

/* level4 $0559D6: the hit of the core: when its energy is gone the level's objects go and the bonuses fall */
void l4_b1_core_hit(Object *shot, Object *enemy, uint16_t damage)
{
    uint16_t old = (uint16_t)enemy->energy;

    enemy->fn_draw = (obj_fn)l4_b1_core_flash;
    enemy->energy = (int16_t)(old - damage);
    if (old > damage)
        return;
    free_object_chain(LIST_C->prev);
    spawn_bonuses(9);
    debris_burst(shot, 0x13, 0x70, enemy->y, 0x60, 0x50, xenon2.big_explosion_anim);
    xenon2.boss_dead = 0xff;
}

/* level4 $055A1C: the hit of a gun */
void l4_b1_gun_hit(Object *shot, Object *enemy, uint16_t damage)
{
    uint16_t old = (uint16_t)enemy->energy;

    enemy->fn_draw = (obj_fn)l4_b1_gun_flash;
    enemy->energy = (int16_t)(old - damage);
    if (old > damage)
        return;
    xenon2.l4_boss1_guns_left = (uint16_t)(xenon2.l4_boss1_guns_left - 1);
    spawn_effect((int16_t)(enemy->x + 0x10), (int16_t)(enemy->y - xenon2.scroll_pos + 0x20), xenon2.big_explosion_anim, draw_bob_masked);
    enemy->b1_gun.dir = (enemy->b1_gun.dir & 0xffffu) | (uint32_t)(uint16_t)(-1) << 16;
    enemy->fn_update = (obj_fn)l4_gun_dead;
    enemy->fn_hit = (void *)obj_nop_thunk;
}

/* level4 $055D0E: the remains of a gun */
void l4_gun_dead(Object *obj)
{
    obj->y = (int16_t)(obj->y + xenon2.l4_core_sway);
    gun_box(obj);
}

/* level4 $055D3C: a gun: it turns to the player and fires in his direction */
void l4_b1_gun(Object *obj)
{
    uint16_t aim;
    const uint8_t *table = obj->x < 0xa0 ? xenon2.level4_data.b1_gun_dirs_left : xenon2.level4_data.b1_gun_dirs_right;

    obj->y = (int16_t)(obj->y + xenon2.l4_core_sway);
    gun_box(obj);
    aim = aim_at_player(obj, 0x10, 0x10);
    obj->b1_gun.dir = (obj->b1_gun.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)table[aim]) << 16;
    if (add_carry(&obj->shot_count, xenon2.level4_data.b1_gun_fire_rate)) {
        Object *s;
        uint16_t angle = (uint16_t)(int16_t)(obj->b1_gun.dir >> 16);
        const uint16_t *anim = xenon2.level4_data.b1_gun_anims_by_angle[angle];

        obj->shot_count = (uint8_t)(random_next() & 0x3f);
        s = spawn_enemy_shot((uint16_t)(obj->x + 0x10), (uint16_t)(obj->y + 0x10), angle, xenon2.level4_data.b1_gun_shot_steps, anim);
        set_anim(s, anim);
        s->x = (int16_t)(s->x + (int16_t)xenon2.level4_data.b1_gun_dx_by_dir[(uint16_t)(int16_t)(s->b1_gun.dir >> 16)]);
        s->y = (int16_t)(s->y + (int16_t)xenon2.level4_data.b1_gun_dy_by_dir[(uint16_t)(int16_t)(s->b1_gun.dir >> 16)]);
    }
}

/* the picture of a gun or the core by the cells of a table */
static void gun_cells(Object *obj, int flash)
{
    const uint16_t *const *grid = obj->x < 0xa0 ? xenon2.level4_data.gun_cells_left : xenon2.level4_data.gun_cells_right;
    const uint16_t *cells = grid[(int16_t)(int16_t)(obj->b1_gun.dir >> 16)];

    if (flash)
        draw_tile_grid_flash(cells, 1, 1, obj->y, (uint16_t)((uint16_t)obj->x >> 1));
    else
        draw_tile_grid(cells, 1, 1, obj->y, (uint16_t)((uint16_t)obj->x >> 1));
}

/* level4 $055B30: a gun's flash */
void l4_b1_gun_flash(Object *obj)
{
    obj->fn_draw = (obj_fn)l4_b1_gun_draw;
    gun_cells(obj, 1);
}

/* level4 $055B6A: a gun's picture */
void l4_b1_gun_draw(Object *obj)
{
    gun_cells(obj, 0);
}

static void core_cells(Object *obj, int flash)
{
    uint16_t rows = 4;
    const uint16_t *cells = xenon2.level4_data.core_cells;

    if (obj->core.tick != 0) {
        rows = 5;
        cells = xenon2.level4_data.core_cells_tall;
    }
    if (flash)
        draw_tile_grid_flash(cells, 5, rows, obj->y, 0x38);
    else
        draw_tile_grid(cells, 5, rows, obj->y, 0x38);
}

/* level4 $055A68: the core's picture */
void l4_b1_core_draw(Object *obj)
{
    core_cells(obj, 0);
}

/* level4 $055A86: the core's flash */
void l4_b1_core_flash(Object *obj)
{
    obj->fn_draw = (obj_fn)l4_b1_core_draw;
    core_cells(obj, 1);
}

/* level4 $055C34: the picture of a piece of the chain while it has its size */
void l4_b1_piece_draw(Object *obj)
{
    if (obj->steps != 0)
        draw_bob_masked(obj);
}

/* the chain of the pieces: the head of the wave: its direction and speed follow the scroll position (the $567EE of the second boss) */
static void l4_pose(Object *obj)
{
    uint16_t phase = (uint16_t)(xenon2.frame_tick * 4) & 0x7f;
    int32_t offset;

    if (phase & 0x40)
        phase = (uint16_t)(0x7f - phase);
    obj->l4.angle = (obj->l4.angle & 0xffff0000u) | (uint16_t)((int16_t)(phase + 0xa0));
    phase = (uint16_t)((xenon2.frame_tick * 2 + 0x50) & 0xff);
    if (phase & 0x80)
        phase = (uint16_t)((phase & 0xff00) | (~phase & 0xff));
    offset = (int16_t)(phase - 0x40);
    obj->turn = (uint32_t)offset << 8;
    phase = (uint16_t)((xenon2.frame_tick + 0xa0) & 0x1ff);
    if (phase & 0x100)
        phase ^= 0x1ff;
    obj->turn_step = (uint16_t)((phase - 0x80) << 2);
    bbox_from_frame(obj);
}

/* level4 $055ED8: the head of the chain: it wiggles by the scroll, and the pieces' size follows the core's */
void l4_b1_head(Object *obj)
{
    int16_t size;

    obj->gfx = (void *)xenon2.level4_data.b1_head_picture;
    obj->anim_delay = 0;
    obj->y = (int16_t)(obj->y + xenon2.l4_core_sway);
    l4_pose(obj);
    obj->l4.angle = (obj->l4.angle & 0xffff0000u) | (uint16_t)((int16_t)((int16_t)obj->l4.angle ^ 0x80));
    size = obj->target->l4.size;
    if (size > 8) {
        size = (int16_t)(0x80 - size);
        if (size > 8)
            size = 8;
    }
    obj->steps = size;
    if (size == 0) {
        obj->hit[0] = 0x3e8;
        obj->hit[2] = 0x3e8;
    }
}

static void follow_chain(Object *obj);

static void l4_b1_piece(Object *obj, const uint16_t *gfx)
{
    obj->gfx = (void *)gfx;
    obj->anim_delay = 0;
    obj->y = (int16_t)(obj->y + xenon2.l4_core_sway);
    follow_chain(obj);
    if (obj->steps == 0) {
        obj->hit[0] = 0x3e8;
        obj->hit[2] = 0x3e8;
    }
}

/* level4 $055F26: a piece of the chain */
void l4_b1_body(Object *obj)
{
    l4_b1_piece(obj, xenon2.level4_data.b1_head_picture);
}

/* level4 $055F52: the last piece */
void l4_b1_tail(Object *obj)
{
    l4_b1_piece(obj, xenon2.level4_data.b1_tail_picture);
}

/* ---- the second boss ($56976): twenty objects, a worm with a head, a gun and a ring of eyes ---- */

/* level4 $056976: the pieces by the table at $5613A (the latch of the weapons first) */
void l4_spawn_boss2(const struct SpawnRec *record, uint16_t slots)
{
    const struct Level4Boss2Piece *piece = xenon2.level4_data.boss2_pieces;
    int n;

    (void)record;
    if (xenon2.l4_boss2_spawned != 0)
        return;
    xenon2.l4_boss2_spawned = 0xff;
    xenon2.scroll_y = 0x9b0;
    xenon2.player_x_saved = 0xa0;
    xenon2.player_y_saved = 0xb0;
    latch_slots(slots);
    for (n = 0; n < 20; n++) {
        Object *obj = insert_head(LIST_C);

        obj->type = 0x54;
        obj->fn_update = (obj_fn)piece->update;
        obj->fn_draw = draw_bob_masked;
        obj->fn_hit = (void *)piece->hit;
        set_anim(obj, piece->anim);
        obj->persists = -1;
        obj->x = (int16_t)piece->x;
        obj->y = (int16_t)(piece->y - xenon2.scroll_pos);
        obj->l4.size = 0;
        obj->l4.angle = (uint16_t)((int16_t)piece->angle);
        obj->turn = 0;
        obj->turn_step = 0;
        obj->steps = 3;
        obj->energy = piece->energy;
        obj->big = (int8_t)piece->big;
        obj->l4.extra = 0;
        bbox_from_frame(obj);
        piece++;
    }
    xenon2.l4_boss2_pieces_left = 5;
}

/* level4 $0564FE: the piece takes the place, angle, speed and size of the one before it and moves by the script */
static void follow_chain(Object *obj)
{
    Object *lead = obj->next;

    obj->x = lead->x;
    obj->x_frac = lead->x_frac;
    obj->y = lead->y;
    obj->y_frac = lead->y_frac;
    obj->l4.angle = lead->l4.angle;
    obj->turn = lead->turn;
    obj->turn_step = lead->turn_step;
    obj->steps = lead->steps;
    obj->l4.size = 0x64;
    obj_script_run(obj);
}

/* the head's picture by its angle (four), and its box */
static void head_picture(Object *obj)
{
    obj->gfx = (void *)xenon2.level4_data.head_pictures[(((uint16_t)(int16_t)obj->head.angle + 0x10) >> 5) & 3];
    bbox_from_frame(obj);
}

/* the head's use of the scratch fields of Object */

/* level4 $056410: the head: it turns to the player, now and then it darts at him; v28 is the length of its bend */
void l4_b2_head(Object *obj)
{
    uint16_t angle;

    obj->y = (int16_t)(obj->y + xenon2.scroll_speed);
    if (obj->head.bend == 0) {
        angle = aim_at_player(obj, 0, 0);
        if (angle < 2)
            angle = 2;
        if (angle > 6)
            angle = 6;
        angle = (uint16_t)(angle << 5);
        angle = (uint16_t)((angle & 0xff00) | ((angle - 0x40) & 0xff));
        obj->l4.angle = (obj->l4.angle & 0xffffu) | (uint32_t)(uint16_t)(0) << 16;
        obj->head.angle = (obj->head.angle & 0xffff0000u) | (uint16_t)((int16_t)angle);
        obj->turn = 0;
        obj->turn_step = 0;
        if ((uint8_t)random_next() >= xenon2.level4_data.b2_head_chance) {
            head_picture(obj);
            return;
        }
        obj->head.dart = 2;
        angle = aim_at_player(obj, 0, 0);
        if (angle < 2)
            angle = 2;
        if (angle > 6)
            angle = 6;
        angle = (uint16_t)(angle << 5);
        angle = (uint16_t)((angle & 0xff00) | ((angle - 0x60) & 0xff));
        obj->l4.angle = (obj->l4.angle & 0xffffu) | (uint32_t)(uint16_t)(0) << 16;
        obj->head.angle = (obj->head.angle & 0xffff0000u) | (uint16_t)((int16_t)angle);
        {
            uint32_t turn = ((random_next() & 0x1f) + 0x20) << 10;

            obj->turn = turn;
            obj->turn_step = 0;
        }
        if ((int16_t)obj->head.angle < 0x20 || !((int16_t)obj->head.angle < 0xa0)) {
            obj->head.angle = (obj->head.angle & 0xffff0000u) | (uint16_t)((int16_t)(((int16_t)obj->head.angle & 0xff00) | (((int16_t)obj->head.angle + 0x40) & 0xff)));
            obj->turn = (uint32_t)(0 - obj->turn);
        }
    }
    obj->head.bend = (int16_t)(obj->head.bend + obj->head.dart);
    if (obj->head.bend >= 0x18)
        obj->head.dart = (int16_t)-obj->head.dart;
    obj->steps = (int16_t)(obj->head.bend + 4);
    head_picture(obj);
}

/* level4 $056530: the pieces behind the head */
void l4_b2_follow(Object *obj)
{
    obj->y = (int16_t)(obj->y + xenon2.scroll_speed);
    follow_chain(obj);
    head_picture(obj);
}

/* level4 $056586: the gun: the screen stops at it; it turns to the player */
void l4_b2_gun(Object *obj)
{
    uint16_t dir;

    if ((int16_t)xenon2.scroll_max < 0x9b0)
        xenon2.scroll_max = 0x9b0;
    obj->y = (int16_t)(obj->y + xenon2.scroll_speed);
    follow_chain(obj);
    dir = aim_at_player(obj, 0, 0);
    if (dir < 2)
        dir = 2;
    if (dir > 6)
        dir = 6;
    obj->aim.dir = (obj->aim.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)dir) << 16;
    obj->gfx = (void *)xenon2.level4_data.b2_gun_pictures[dir];
    bbox_from_frame(obj);
}

/* level4 $0565FA: the gun's barrel: it sits on the gun, shows its direction and fires */
void l4_b2_barrel(Object *obj)
{
    Object *gun;

    anim_step(obj);
    gun = obj->next;
    obj->x = gun->x;
    obj->y = gun->y;
    if ((int16_t)(gun->aim.dir >> 16) != (int16_t)(obj->aim.dir >> 16)) {
        obj->aim.dir = (obj->aim.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)(gun->aim.dir >> 16)) << 16;
        set_anim(obj, xenon2.level4_data.b2_barrel_anims_by_dir[(uint16_t)(int16_t)(obj->aim.dir >> 16)]);
    }
    if (add_carry(&obj->shot_count, xenon2.level4_data.b2_barrel_fire_rate)) {
        uint16_t dir = (uint16_t)(int16_t)(obj->aim.dir >> 16);
        Object *s;

        obj->shot_count = (uint8_t)(random_next() & 0x3f);
        s = spawn_enemy_shot((uint16_t)(obj->x + (int16_t)xenon2.level4_data.b2_muzzle_dx[dir]), (uint16_t)(obj->y + (int16_t)xenon2.level4_data.b2_muzzle_dy[dir]), dir, xenon2.level4_data.b2_barrel_shot_steps,
                             (void *)xenon2.level4_data.b2_shot_anim);
        set_anim(s, xenon2.level4_data.b2_shot_anim);
    }
    bbox_from_frame(obj);
}

/* level4 $0567E0: the first of the tail: its angle and sway follow the scroll */
void l4_b2_wave(Object *obj)
{
    obj->y = (int16_t)(obj->y + xenon2.scroll_speed);
    obj->steps = 0xc;
    l4_pose(obj);
}

/* level4 $056854: a piece of the tail */
void l4_b2_link(Object *obj)
{
    obj->y = (int16_t)(obj->y + xenon2.scroll_speed);
    follow_chain(obj);
    bbox_from_frame(obj);
}

/* level4 $056864: the last piece of the tail: it fires rings of eight */
void l4_b2_ring(Object *obj)
{
    obj->y = (int16_t)(obj->y + xenon2.scroll_speed);
    anim_step(obj);
    follow_chain(obj);
    if (add_carry(&obj->shot_count, xenon2.level4_data.b2_ring_fire_rate)) {
        int dir;

        obj->shot_count = (uint8_t)(random_next() & 0x3f);
        for (dir = 7; dir >= 0; dir--) {
            Object *s = spawn_enemy_shot((uint16_t)obj->x, (uint16_t)obj->y, (uint16_t)dir, xenon2.level4_data.b2_ring_shot_steps, shot_picture(xenon2.level4_data.b2_ring_shot_picture));

            set_anim(s, xenon2.level4_data.b2_shot_anim);
        }
    }
    bbox_from_frame(obj);
}

/* level4 $0568F2: an eye: it looks at the player and fires */
void l4_b2_eye(Object *obj)
{
    uint16_t dir;

    obj->y = (int16_t)(obj->y + xenon2.scroll_speed);
    anim_step(obj);
    dir = aim_at_player(obj, 0, 0);
    if (dir != (uint16_t)(int16_t)(obj->aim.dir >> 16)) {
        obj->aim.dir = (obj->aim.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)dir) << 16;
        set_anim(obj, xenon2.level4_data.b2_eye_anims[dir]);
    }
    if (add_carry(&obj->shot_count, xenon2.level4_data.b2_eye_fire_rate)) {
        obj->shot_count = (uint8_t)(random_next() & 0x3f);
        spawn_enemy_shot((uint16_t)obj->x, (uint16_t)obj->y, (uint16_t)(int16_t)(obj->aim.dir >> 16), xenon2.level4_data.b2_eye_shot_steps, (void *)xenon2.level4_data.b2_eye_shot_picture);
    }
    bbox_from_frame(obj);
    obj->hit[1] = (int16_t)(obj->hit[1] - 8);
    obj->hit[3] = (int16_t)(obj->hit[3] + 8);
}

/* level4 $056354: the hit of the gun (the barrel's, $056350, goes to the gun under it): when its energy is gone everything of the level goes,
 * the bonuses fall and the screen goes back to the first stretch */
void l4_b2_gun_hit(Object *shot, Object *enemy, uint16_t damage)
{
    Object *o, *prev;
    uint16_t old;

    if (xenon2.l4_boss2_pieces_left != 0)
        return;
    o = enemy->prev;
    enemy->fn_draw = draw_bob_or_first;
    o->fn_draw = draw_bob_or_first;
    old = (uint16_t)enemy->energy;
    enemy->energy = (int16_t)(old - damage);
    if (old > damage)
        return;
    for (o = LIST_C->prev; o->type != 0; o = prev) {
        prev = o->prev;
        kill_thunk(o);
    }
    debris_burst(shot, 0x27, 0, 0, 0x140, 0xc0, xenon2.big_explosion_anim);
    spawn_bonuses(4);
    memset(xenon2.level_init_src + 0x1658, 0, 0xa0 * 4);
    xenon2.scroll_min = 0;
    xenon2.player_score += 0x7d0;
    xenon2.scroll_step_target = 1;
    xenon2.scroll_pos = 0x8a0;
    xenon2.scroll_max = 0x8a0;
    xenon2.map_row_offset = 0x1590;
    xenon2.scroll_fine = 0;
}

/* level4 $056350 */
void l4_b2_barrel_hit(Object *shot, Object *enemy, uint16_t damage)
{
    l4_b2_gun_hit(shot, enemy->next, damage);
}

/* level4 $0562CA: the hit of the last piece of the tail: when its energy is gone ten pieces burst */
void l4_b2_ring_hit(Object *shot, Object *enemy, uint16_t damage)
{
    uint16_t old = (uint16_t)enemy->energy;
    int n;

    enemy->fn_draw = draw_bob_or_first;
    enemy->energy = (int16_t)(old - damage);
    if (old > damage)
        return;
    xenon2.l4_boss2_pieces_left = (uint16_t)(xenon2.l4_boss2_pieces_left - 1);
    xenon2.scroll_step_target = 0xffff;
    for (n = 0; n < 10; n++) {
        Object *next = enemy->next;

        enemy->score = 0x64;
        enemy_killed(shot, enemy);
        enemy = next;
    }
}

/* level4 $0562FE: the hit of an eye: shots at its top or bottom edge are shielded off */
void l4_b2_eye_hit(Object *shot, Object *enemy, uint16_t damage)
{
    uint16_t old;

    if (box_overlap((int16_t)xenon2.hit_box[0], (int16_t)xenon2.hit_box[1], (int16_t)xenon2.hit_box[2], (int16_t)xenon2.hit_box[3], enemy->hit[0], enemy->hit[3], enemy->hit[2], enemy->hit[3]) ||
        box_overlap((int16_t)xenon2.hit_box[0], (int16_t)xenon2.hit_box[1], (int16_t)xenon2.hit_box[2], (int16_t)xenon2.hit_box[3], enemy->hit[0], enemy->hit[1], enemy->hit[2], enemy->hit[1])) {
        hit_kill_shooter(shot, enemy, damage);
        return;
    }
    enemy->fn_draw = draw_bob_or_first;
    old = (uint16_t)enemy->energy;
    enemy->energy = (int16_t)(old - damage);
    if (old > damage)
        return;
    xenon2.l4_boss2_pieces_left = (uint16_t)(xenon2.l4_boss2_pieces_left - 1);
    enemy->score = 0x12c;
    enemy_killed(shot, enemy);
}

/* ---- the bars ($56A2C) ---- */

/* level4 $056A2C: a bar that swings out of the wall (the record's word at +$C says which side) */
void l4_spawn_bar(const struct SpawnRec *record)
{
    int right = record->steps != 0;
    Object *obj = spawn_marker_front(right ? 0x110 : 0x10c, record, (void (*)(void))(obj_fn)l4_bar_update);

    obj->y = (int16_t)(obj->y - xenon2.scroll_pos);
    obj->fn_draw = (obj_fn)l4_bar_draw;
    set_anim(obj, obj->type == 0x10c ? xenon2.level4_data.bar_anim_10c : xenon2.level4_data.bar_anim_other);
}

/* level4 $056ABC: the bar: it swings out and back; it hurts the player */
void l4_bar_update(Object *obj)
{
    int16_t left, top, right;

    obj->y = (int16_t)(obj->y + xenon2.scroll_speed);
    if (obj->y > 0x188) {
        kill_thunk(obj);
        return;
    }
    anim_step(obj);
    if (obj->bar.phase == 0) {
        obj->bar.step = (obj->bar.step & 0xffffu) | (uint32_t)(uint16_t)(1) << 16;
        if (!add_carry(&obj->shot_count, xenon2.level4_data.bar_fire_rate))
            goto box;
        obj->shot_count = (uint8_t)(random_next() & 0x3f);
    }
    obj->bar.phase = (int16_t)(obj->bar.phase + (int16_t)(obj->bar.step >> 16));
    if (obj->bar.phase == 6)
        obj->bar.step = (obj->bar.step & 0xffffu) | (uint32_t)(uint16_t)((int16_t)-(int16_t)(obj->bar.step >> 16)) << 16;
box:
    if (obj->type == 0x10c) {
        left = obj->x;
        right = (int16_t)(((uint16_t)obj->bar.phase << 4) + left + 0x10);
    } else {
        right = (int16_t)(obj->x + 0x10);
        left = (int16_t)(right - ((uint16_t)obj->bar.phase << 4) - 0x10);
    }
    top = (int16_t)(obj->y + 8);
    obj->hit[0] = left;
    obj->hit[1] = top;
    obj->hit[2] = right;
    obj->hit[3] = (int16_t)(top + 0x10);
    if (!player_hits(obj))
        return;
    if (xenon2.shield_timer != 0)
        return;
    player_damage(obj, 6);
    PLAYER->fn_draw = (obj_fn)draw_bob_or_first;
}

/* level4 $056B8E: the bar: the tiles of its shaft and the picture of its end */
void l4_bar_draw(Object *obj)
{
    int16_t x, y;
    uint16_t span = (uint16_t)obj->bar.phase;

    if (obj->type == 0x10c) {
        if (span != 0)
            draw_tile_grid(xenon2.level4_data.bar_cells + sizeof xenon2.level4_data.bar_cells / sizeof xenon2.level4_data.bar_cells[0] - span, (uint16_t)(span - 1), 1, obj->y, (uint16_t)((uint16_t)obj->x >> 1));
        x = (int16_t)((span << 4) + obj->x);
        y = (int16_t)(obj->y + 8);
    } else {
        if (span != 0)
            draw_tile_grid(xenon2.level4_data.bar_cells + sizeof xenon2.level4_data.bar_cells / sizeof xenon2.level4_data.bar_cells[0] - span, (uint16_t)(span - 1), 1, obj->y,
                           (uint16_t)((uint16_t)((obj->x - (int16_t)(span << 4)) + 0x10) >> 1));
        x = (int16_t)(obj->x - (int16_t)(span << 4) + 0xf);
        y = (int16_t)(obj->y + 8);
    }
    draw_bob_masked_at((uint16_t)x, (uint16_t)y, (const uint16_t *)obj->gfx);
}

/* ---- the walker ($56C3E): it stands, then walks to the other side along the screen ---- */

/* level4 $056C3E */
void l4_spawn_walker(const struct SpawnRec *record)
{
    int right = record->steps != 0;
    Object *obj = spawn_marker_enemy(right ? 0xe4 : 0xe0, record, (void (*)(void))(obj_fn)l4_walker_update);

    obj->y = (int16_t)(obj->y + 0x1c);
    if (obj->type == 0xe0)
        obj->x = (int16_t)(obj->x + 0x10);
    obj->fn_draw = (obj_fn)l4_walker_draw;
    obj->fn_hit = (obj_fn)l4_walker_hit;
    obj->energy = (int16_t)xenon2.level4_data.walker_energy;
    obj->big = -1;
    obj->hit[0] = 0x3e8;
    obj->hit[2] = 0x3e8;
    obj->score = 0xc8;
    obj->walker.side = (int16_t)record->index;
    set_anim(obj, obj->type == 0xe0 ? xenon2.level4_data.walker_anim_e0 : xenon2.level4_data.walker_anim_other);
    obj->kind = 0;
    obj->shot_count = (uint8_t)(xenon2.level4_data.walker_v5e >> 8);
    obj->shot_rate = (uint8_t)xenon2.level4_data.walker_v5e;
    obj->walker_b.home_x = obj->x;
    obj->walker_b.home_y = obj->y;
}

/* the walker goes back to where it came from */
static void walker_reset(Object *obj)
{
    obj->x = obj->walker_b.home_x;
    obj->y = obj->walker_b.home_y;
    obj->walker.state = 0;
    obj->hit[0] = 0x3e8;
    obj->hit[2] = 0x3e8;
    set_anim(obj, obj->type == 0xe0 ? xenon2.level4_data.walker_anim_e0 : xenon2.level4_data.walker_anim_other);
    obj->energy = (int16_t)xenon2.level4_data.walker_energy;
}

/* level4 $056D1C */
void l4_walker_update(Object *obj)
{
    if ((int16_t)(xenon2.scroll_max + 0xd0) < obj->y) {
        kill_thunk(obj);
        return;
    }
    anim_step(obj);
    if (obj->walker.state == 0) {
        if ((uint8_t)random_next() < xenon2.level4_data.walker_chance) {
            obj->walker.state = 1;
            obj->anim_delay = 1;
        }
        return;
    }
    if (obj->type == 0xe0)
        obj->x = (int16_t)(obj->x + xenon2.level4_data.walker_step_x);
    else
        obj->x = (int16_t)(obj->x - xenon2.level4_data.walker_step_x);
    if (obj->walker.state == 1) {
        if (obj->anim_delay == 0) {
            obj->walker.state = 2;
            obj->walker.dy = (int16_t)(PLAYER->y - obj->y + xenon2.scroll_pos);
        }
        goto box;
    }
    if (obj->x >= 0x28 && obj->x <= 0x118) {
        if (obj->walker.dy >= 0)
            obj->y = (int16_t)(obj->y + xenon2.level4_data.walker_step_x);
        else
            obj->y = (int16_t)(obj->y - xenon2.level4_data.walker_step_x);
    }
    if (obj->x >= -0x20 && obj->x < 0x160)
        goto box;
    walker_reset(obj);
    return;
box:
    bbox_from_frame(obj);
    obj->hit[1] = (int16_t)(obj->hit[1] - xenon2.scroll_pos);
    obj->hit[3] = (int16_t)(obj->hit[3] - xenon2.scroll_pos);
}

/* level4 $056E84: the hit of the walker: when it is shot to pieces it bursts, drops a pickup and goes back */
void l4_walker_hit(Object *shot, Object *enemy, uint16_t damage)
{
    uint16_t old = (uint16_t)enemy->energy;
    const struct Pic *frame;
    int16_t x, y;
    Object *p;

    enemy->energy = (int16_t)(old - damage);
    if (old > damage) {
        enemy->fn_draw = (obj_fn)l4_walker_flash;
        return;
    }
    frame = enemy->gfx;
    x = (int16_t)(((int16_t)(enemy->x - (int16_t)frame->hot_x) * 2 + (int16_t)frame->width) >> 1);
    y = (int16_t)(((int16_t)(enemy->y - (int16_t)frame->hot_y) * 2 + (int16_t)frame->height) >> 1);
    y = (int16_t)(y - xenon2.scroll_pos);
    spawn_effect(x, y, xenon2.big_explosion_anim, draw_bob_masked);
    xenon2.player_score += (uint16_t)enemy->score;
    p = alloc_in(LIST_B);
    p->type = 0x18;
    p->pickup.mode = 7;
    p->fn_update = (obj_fn)pickup_update;
    p->fn_draw = draw_bob_masked;
    anim_header_set(p, xenon2.anim_bonus_big);
    p->x = enemy->x;
    p->y = (int16_t)(enemy->y - xenon2.scroll_pos);
    p->pickup.dir = (p->pickup.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)random_next()) << 16;
    walker_reset(enemy);
}

/* the tiles of a walker's legs, by its side v30 */
static void walker_tiles(Object *obj)
{
    uint16_t offset = (uint16_t)(obj->walker.side * 2);

    if (obj->walker.state >= 2)
        return;
    draw_tile_grid(obj->type == 0xe0 ? xenon2.level4_data.walker_grid_e0 : xenon2.level4_data.walker_grid_e4, 0, 2, (int16_t)(xenon2.level4_data.walker_grid_y[offset / 2] - xenon2.scroll_pos), xenon2.level4_data.walker_grid_x[offset / 2]);
}

/* level4 $056F3E: the walker's flash */
void l4_walker_flash(Object *obj)
{
    int16_t y = (int16_t)(obj->y - xenon2.scroll_pos);

    obj->fn_draw = (obj_fn)l4_walker_draw;
    if (y > 0xdc)
        return;
    draw_bob_or_at((uint16_t)obj->x, (uint16_t)y, (const uint16_t *)obj->gfx);
    walker_tiles(obj);
}

/* level4 $056F6A: the walker's picture */
void l4_walker_draw(Object *obj)
{
    int16_t y = (int16_t)(obj->y - xenon2.scroll_pos);

    if (y > 0xdc)
        return;
    draw_bob_masked_at((uint16_t)obj->x, (uint16_t)y, (const uint16_t *)obj->gfx);
    walker_tiles(obj);
}
