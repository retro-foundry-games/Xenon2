/* Level 1: the code of its blob ($54E00): entries, turrets, units, the boss. */
#include "level1.h"
#include "weapons.h"
#include "shop.h"
#include "game_all.h"
#include "weapon_util.h"
#include "amiga/hw.h"

/* the scratch fields of Object, by the kind that uses them */


/* the scratch fields of Object, by the kind that uses them */


/* the scratch fields of Object, by the kind that uses them */


/* the scratch fields of Object, by the kind that uses them */


#define FN(f) ((obj_fn)(f))

/* ---- level 1's blob ($54E00): the jump table at $54E0C and the routines the engine calls ---- */

/* orig $054E0C (-> $5566A4): the level's extra collision test finds nothing (Z) */
int level1_collision_hook(void)
{
    return 0;
}

/* orig $054E10 (-> $055B94): level events by id (rec = the record that fired): a table of the level engine ($E60) for each */
void level1_event(uint16_t id, const uint8_t *rec)
{
    static const uint8_t *const tabs[10] = { xenon2.level1_data.formation_1, xenon2.level1_data.formation_2, xenon2.level1_data.formation_3, xenon2.level1_data.formation_4, xenon2.level1_data.formation_5,
                                             xenon2.level1_data.formation_6, xenon2.level1_data.formation_7, xenon2.level1_data.formation_8, xenon2.level1_data.formation_9, xenon2.level1_data.formation_10 };

    if (id == 0) {
        spawn_wave_326c(0, rec);   /* $E5A */
        return;
    }
    if (id <= 10)
        spawn_formation(0, rec, tabs[id - 1]);   /* $E60 */
}

/* ---- level 1's turrets: markers that draw themselves into the map ---- */

/* the picture of an enemy shot: the table at $68F40 has addresses of longs, the long is the picture */
static const void *level1_shot_gfx(uint16_t idx)
{
    return GPTR(rd32(xenon2.level1_data.shot_gfx_table[idx]));
}

static void copy4(uint8_t *dst, const uint8_t *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}

/* the turret has left the screen: it dies */
static int level1_turret_gone(Object *turret)
{
    if ((int16_t)(xenon2.scroll_max + 0xd0) < turret->y) {
        kill_object_thunk(turret);
        return 1;
    }
    return 0;
}

/* orig $0552CE: update of the turret $E0 / $E4: its box, the map cells it shows (four steps), and now and then a shot */
void level1_turret_a_update(Object *turret)
{
    const uint8_t *table;
    uint8_t *script;
    uint16_t offset, sum;
    int16_t x, y, dir;

    if (level1_turret_gone(turret))
        return;
    turret->hit[0] = (int16_t)(turret->x + 4);
    turret->hit[2] = (int16_t)(turret->x + 4 + 0x18);
    y = (int16_t)(turret->y - xenon2.scroll_pos + 4);
    turret->hit[1] = y;
    turret->hit[3] = (int16_t)(y + 0x18);
    offset = (uint16_t)(turret->l1_turret.step << 3);
    table = turret->type == 0xe0 ? xenon2.level1_data.turret_a_cells_e0 : xenon2.level1_data.turret_a_cells_e4;
    script = turret->script;
    copy4(script, table + offset);
    copy4(script + 0x28, table + offset + 4);
    turret->l1_turret.step = (int16_t)((turret->l1_turret.step + 1) & 3);
    sum = (uint16_t)(((ObjMove *)turret)->shot_count + xenon2.level1_data.shot_params[3]);
    ((ObjMove *)turret)->shot_count = (uint8_t)sum;
    if (!(sum & 0x100))
        return;
    ((ObjMove *)turret)->shot_count = (uint8_t)(random_next() & 0x3f);
    x = (int16_t)(turret->x + 0x10);
    y = (int16_t)(turret->y - xenon2.scroll_pos + 0xe);
    dir = 2;
    if (turret->type != 0xe4) {
        dir = 6;
        x = (int16_t)(x - 0x10);
    }
    spawn_enemy_shot((uint16_t)x, (uint16_t)y, (uint16_t)dir, rd16(xenon2.level1_data.shot_params + 6), level1_shot_gfx(rd16(xenon2.level1_data.shot_params + 4)));
}

/* orig $0555A2: update of the turret $E8 / $EC (sixteen steps of cells) */
void level1_turret_b_update(Object *turret)
{
    const uint8_t *table;
    uint8_t *script;
    uint16_t offset, sum;
    int16_t x, y, dir;

    if (level1_turret_gone(turret))
        return;
    turret->hit[0] = (int16_t)(turret->x + 0xa);
    turret->hit[2] = (int16_t)(turret->x + 0xa + 0x12);
    y = (int16_t)(turret->y - xenon2.scroll_pos + 4);
    turret->hit[1] = y;
    turret->hit[3] = (int16_t)(y + 0x18);
    offset = (uint16_t)(((uint16_t)turret->l1_turret.step & 0xfffc) << 1);
    table = turret->type == 0xe8 ? xenon2.level1_data.turret_b_cells_e8 : xenon2.level1_data.turret_b_cells_ec;
    script = turret->script;
    copy4(script, table + offset);
    copy4(script + 0x28, table + offset + 4);
    turret->l1_turret.step = (int16_t)((turret->l1_turret.step + 1) & 0xf);
    sum = (uint16_t)(((ObjMove *)turret)->shot_count + xenon2.level1_data.shot_params[0xf]);
    ((ObjMove *)turret)->shot_count = (uint8_t)sum;
    if (!(sum & 0x100))
        return;
    ((ObjMove *)turret)->shot_count = (uint8_t)(random_next() & 0x3f);
    x = (int16_t)(turret->x + 0x20);
    y = (int16_t)(turret->y - xenon2.scroll_pos + 0xe);
    dir = 2;
    if (turret->type != 0xec) {
        dir = 6;
        x = (int16_t)(x - 0x20);
    }
    spawn_enemy_shot((uint16_t)x, (uint16_t)y, (uint16_t)dir, rd16(xenon2.level1_data.shot_params + 0x12), level1_shot_gfx(rd16(xenon2.level1_data.shot_params + 0x10)));
}

/* orig $055402: update of the turret $D8 / $DC (twenty steps of cells, a shot every 40 steps) */
void level1_turret_c_update(Object *turret)
{
    const uint8_t *table;
    uint16_t offset, sum;
    int16_t x, y, dir;

    if (level1_turret_gone(turret))
        return;
    turret->hit[0] = (int16_t)(turret->x + 8);
    turret->hit[2] = (int16_t)(turret->x + 8 + 0x10);
    y = (int16_t)(turret->y - xenon2.scroll_pos + 8);
    turret->hit[1] = y;
    turret->hit[3] = (int16_t)(y + 0x10);
    offset = (uint16_t)(((uint16_t)turret->l1_turret.step & 0xfffe) << 1);
    table = turret->type == 0xd8 ? xenon2.level1_data.turret_c_steps_d8 : xenon2.level1_data.turret_c_steps_dc;
    copy4(turret->script, table + offset);
    if (turret->l1_turret.step == 0) {
        sum = (uint16_t)(((ObjMove *)turret)->shot_count + xenon2.level1_data.shot_params[9]);
        ((ObjMove *)turret)->shot_count = (uint8_t)sum;
        if (!(sum & 0x100))
            return;
        ((ObjMove *)turret)->shot_count = (uint8_t)(random_next() & 0x3f);
    }
    turret->l1_turret.step++;
    if (turret->l1_turret.step == 0x28) {
        turret->l1_turret.step = 0;
        return;
    }
    if (turret->l1_turret.step != 0x18)
        return;
    x = (int16_t)(turret->x + 0x1a);
    y = (int16_t)(turret->y - xenon2.scroll_pos + 2);
    dir = 2;
    if (turret->type != 0xdc) {
        dir = 6;
        x = (int16_t)(x - 0x1a);
    }
    spawn_enemy_shot((uint16_t)x, (uint16_t)y, (uint16_t)dir, rd16(xenon2.level1_data.shot_params + 0xc), level1_shot_gfx(rd16(xenon2.level1_data.shot_params + 0xa)));
}

/* the launcher's use of the scratch fields of Object */

/* orig $0556DC: update of the launcher $D0 / $D4: it moves up and down (v28 is its step, v30 its place between 0 and v2a); when the
 * player is level with it (now and then) it goes into list B and fires three shots */
void level1_launcher_update(Object *launcher)
{
    int16_t dy, step;
    int i;

    if ((int16_t)(xenon2.scroll_max + 0xd0 - xenon2.scroll_pos) < launcher->y) {
        kill_object_thunk(launcher);
        return;
    }
    anim_step(launcher);
    launcher->y = (int16_t)(launcher->y + xenon2.scroll_speed);
    if (launcher->launcher.step == 0)
        return;
    dy = (int16_t)(launcher->y - PLAYER->y);
    if (dy <= 0xa && dy >= -0x1e && (uint8_t)random_next() < 0x32) {
        Object *tail;
        const uint8_t *anim;

        launcher->launcher.step = 0;
        launcher->next->prev = launcher->prev;
        launcher->prev->next = launcher->next;
        tail = LIST_B->prev;
        LIST_B->prev = launcher;
        launcher->prev = tail;
        launcher->next = LIST_B;
        tail->next = launcher;
        anim = launcher->type == 0xd4 ? xenon2.level1_data.anim_launcher_d4_fire : xenon2.level1_data.anim_launcher_d0_fire;
        set_anim(launcher, anim);
        for (i = 2; i >= 0; i--) {
            int16_t angle = launcher->type == 0xd4 ? 5 : 1;

            spawn_enemy_shot((uint16_t)launcher->x, (uint16_t)(launcher->y + 7), (uint16_t)(angle + i), rd16(xenon2.level1_data.shot_params + 0x16),
                             level1_shot_gfx(rd16(xenon2.level1_data.shot_params + 0x14)));
        }
        return;
    }
    step = launcher->launcher.step;
    launcher->y = (int16_t)(launcher->y + step);
    launcher->launcher.pos = (int16_t)(launcher->launcher.pos + step);
    if (launcher->launcher.pos == 0 || launcher->launcher.pos == (int16_t)(launcher->launcher.range >> 16))
        launcher->launcher.step = (int16_t)-launcher->launcher.step;
    bbox_from_frame(launcher);
}

/* a turret marker (spawn_marker_enemy with the update routine), hit by hit_tile, energy, no box yet */
static Object *level1_turret(uint16_t type, const uint8_t *rec, void (*update)(void), uint16_t energy)
{
    Object *turret = spawn_marker_enemy(type, rec, update);

    turret->fn_hit = (void *)hit_tile;
    turret->energy = (int16_t)energy;
    turret->hit[0] = 0x3e8;
    turret->hit[2] = 0x3e8;
    return turret;
}

/* the end of the turrets' set-up: the first cells are written into the map ($46), the object hides them again when it is shot */
static void level1_turret_cells(Object *turret, const uint8_t *cells, const uint8_t *anim)
{
    uint8_t *script = turret->script;

    turret->gfx = cells;
    turret->anim = anim;
    copy4(script, cells);
    copy4(script + 0x28, cells + 4);
    turret->score = 0x64;
    turret->tile.cols = 1;
    turret->tile.rows = 1;
}

/* orig $054E18 (-> $055942): level hooks by id (rec = the record that fired, scroll its scroll value): 0 remembers the record, 1 to 3 put
 * a turret (type $E8/$EC, $E0/$E4, $D8/$DC; w0c of the record says which side) into the map, 4 a launcher ($D0/$D4) */
void level1_hook(uint16_t id, const uint8_t *rec, uint16_t scroll)
{
    int flag = rd16(rec + 0xc) != 0;
    Object *obj;

    switch (id) {
    case 0:
        note_spawn_record(rec, scroll);   /* $EA2 */
        return;
    case 1:
        obj = level1_turret(flag ? 0xe8 : 0xec, rec, (void (*)(void))level1_turret_b_update, xenon2.level1_data.turret_energy[0]);
        level1_turret_cells(obj, flag ? xenon2.level1_data.turret_b_cells_e8 : xenon2.level1_data.turret_b_cells_ec, flag ? xenon2.level1_data.turret_b_anim_e8 : xenon2.level1_data.turret_b_anim_ec);
        return;
    case 2:
        obj = level1_turret(flag ? 0xe0 : 0xe4, rec, (void (*)(void))level1_turret_a_update, xenon2.level1_data.boss_energy[1]);
        if (flag) {
            obj->script += 2;
            obj->x = (int16_t)(obj->x + 0x10);
        }
        obj->script -= 2;
        obj->x = (int16_t)(obj->x - 0x10);
        level1_turret_cells(obj, flag ? xenon2.level1_data.turret_a_cells_e0 : xenon2.level1_data.turret_a_cells_e4, flag ? xenon2.level1_data.turret_a_anim_e0 : xenon2.level1_data.turret_a_anim_e4);
        return;
    case 3:
        obj = level1_turret(flag ? 0xd8 : 0xdc, rec, (void (*)(void))level1_turret_c_update, xenon2.level1_data.turret_energy[1]);
        level1_turret_cells(obj, flag ? xenon2.level1_data.turret_c_cells_d8 : xenon2.level1_data.turret_c_cells_dc, xenon2.level1_data.turret_c_anim);
        return;
    case 4:
        obj = alloc_c();
        obj->type = flag ? 0xd4 : 0xd0;
        set_anim(obj, flag ? xenon2.level1_data.anim_launcher_d4 : xenon2.level1_data.anim_launcher_d0);
        obj->fn_update = FN(level1_launcher_update);
        obj->fn_draw = draw_bob_masked;
        obj->fn_hit = (void *)hit_enemy;
        obj->energy = (int16_t)xenon2.level1_data.turret_energy[2];
        obj->big = -1;
        obj->score = 0xfa;
        SET_V5E(obj, 0);
        obj->x = (int16_t)(rd16(rec + 8) - 8 + (flag ? 0x10 : 0));
        obj->y = (int16_t)(rd16(rec + 10) - 8 - xenon2.scroll_pos);
        obj->launcher.step = -1;
        obj->launcher.range = (obj->launcher.range & 0xffffu) | (uint32_t)(uint16_t)((int16_t)(rd16(rec + 6) << 4)) << 16;
        obj->launcher.pos = (int16_t)(obj->launcher.range >> 16);
        obj->kind = 0;
        bbox_from_frame(obj);
        return;
    }
}

/* orig $054E24 (-> $055806): the level's scroll events */
void level1_scroll(void)
{
    const uint16_t *patches = xenon2.level1_data.level_patches;
    uint16_t *vars = xenon2.level_vars;
    int count, i;

    if (xenon2.l1_boss_zone_entered == 0 && (int16_t)(xenon2.scroll_pos + PLAYER->y) < 0xa80) {
        xenon2.level_over = 0xff;
        xenon2.l1_boss_zone_entered = 0xff;
        xenon2.scroll_pos = 0x9c0;
        xenon2.scroll_max = 0x9c0;
        xenon2.map_row_offset = 0x1860;
        xenon2.scroll_fine = 0;
    }
    if ((int16_t)xenon2.scroll_pos >= 0xa40 && (int16_t)xenon2.scroll_pos <= 0xd10) {
        if ((int16_t)xenon2.scroll_max < 0xd10)
            xenon2.scroll_max = 0xd10;
        level1_spawn_units();
        for (count = 15; count >= 0; count--) {
            if (*vars != 0) {
                uint8_t *dst = xenon2.level_init_src + (int16_t)patches[0];

                (*vars)--;
                wr16(dst, patches[1 + (*vars & 3)]);
            }
            vars++;
            patches += 5;
        }
    }
    for (i = 0; i < 5; i++)
        xenon2.unit_live[i] = 0;
}

/* orig $0563AA: update of the level 1 boss (type $50) */
void level1_boss_update(Object *boss)
{
    Object *part, *prev_part, *link;
    int i;
    int16_t wave, new_y, lost, box_y;
    int32_t turn;
    const uint8_t *src;
    static const uint8_t *const sources[4] = { xenon2.level1_data.boss_draw_sources, xenon2.level1_data.boss_draw_sources + 16, xenon2.level1_data.boss_draw_sources + 32, xenon2.level1_data.boss_draw_sources + 16 };

    if ((int16_t)xenon2.scroll_pos > 0x1c0)
        goto hide;
    if (xenon2.l1_boss_spawned == 0) {
        xenon2.l1_boss_spawned = 0xff;
        prev_part = boss;
        for (i = 7; i >= 0; i--) {
            part = obj_free_pop();
            if (i == 7) {
                link = LIST_C->prev;
                LIST_C->prev = part;
                part->prev = link;
                part->next = LIST_C;
                link->next = part;
            } else {
                link = prev_part->prev;
                prev_part->prev = part;
                part->prev = link;
                part->next = prev_part;
                link->next = part;
            }
            part->type = 0xfc;
            if (i == 0)
                part->type = 0x100;
            part->fn_update = level1_boss_part_update;
            part->fn_draw = draw_bob_masked;
            part->fn_hit = (void *)hit_kill_shooter;
            part->gfx = xenon2.level1_data.boss_part_picture;
            part->anim_delay = 0;
            SET_V5E(part, 0);
            part->anim = (const uint8_t *)prev_part;
            prev_part = part;
            part->big = 0;
            part->persists = -1;
            part->y = -100;
            part->hit[0] = 0x3e8;
            part->hit[2] = 0x3e8;
        }
        boss->l1.angle = 0;
        boss->turn = 0;
        boss->turn_step = 0;
    }
    if ((int16_t)xenon2.scroll_max <= 0x1c0)
        xenon2.scroll_max = 0x1c0;
    boss->hit[0] = 0x80;
    boss->hit[2] = 0xc8;
    box_y = (int16_t)(boss->y - xenon2.scroll_pos + 4);
    boss->hit[1] = box_y;
    boss->hit[3] = (int16_t)(box_y + 0x50);
    if (boss->l1_boss.phase == 0) {
        if ((int16_t)xenon2.scroll_pos > 0x30)
            goto draw_table;
        boss->l1_boss.phase = 1;
        boss->l1_boss.move = 0;
        boss->l1_boss.wait = 0xaa;
    }
    if (boss->l1_boss.phase < 0) {
        if (boss->steps != 0)
            boss->steps--;
    } else if (boss->steps < 0xb) {
        if (boss->steps == 0)
            xenon2.sfx_cmd2 = 0xf;
        boss->steps++;
    }
    boss->l1_boss.phase++;
    if (boss->l1_boss.phase == 0)
        boss->l1_boss.phase++;
    if (boss->l1_boss.phase >= 0x64)
        boss->l1_boss.phase = -40;
    wave = xenon2.frame_tick & 0x7f;
    if (wave & 0x40)
        wave = (int16_t)-(wave - 0x7f);
    wave = (int8_t)wave;
    boss->l1.angle = (boss->l1.angle & 0xffff0000u) | (uint16_t)((int16_t)(wave + 0x20));
    wave = (int16_t)((xenon2.frame_tick + 0x50) & 0xff);
    if (wave & 0x80)
        wave = (~wave) & 0xff;
    turn = (int32_t)(int16_t)(wave - 0x40) << 10;
    boss->turn = (uint32_t)turn;
    wave = (int16_t)((xenon2.frame_tick + 0xa0) & 0x1ff);
    if (wave & 0x100)
        wave ^= 0x1ff;
    wave -= 0x80;
    boss->turn_step = (uint16_t)(wave << 5);
    new_y = (int16_t)(boss->l1_boss.move + boss->y);
    if (new_y < 0x10) {
        new_y = 0x10;
        boss->l1_boss.wait = 0;
    }
    if (new_y > 0x1e0) {
        new_y = 0x1e0;
        boss->l1_boss.wait = 0;
    }
    boss->y = new_y;
    boss->l1_boss.wait--;
    if (boss->l1_boss.wait < 0) {
        if (boss->l1_boss.move == -1) {
            boss->l1_boss.move = 0;
            boss->l1_boss.wait = 0xaa;
        } else if (boss->l1_boss.move == 1) {
            boss->l1_boss.move = 0;
            boss->l1_boss.wait = 0x3c;
        } else {
            boss->l1_boss.wait = 0x32;
            boss->l1_boss.move = 1;
            if (boss->y != 0x10)
                boss->l1_boss.move = -1;
        }
    }
    lost = (int16_t)(0x1e - boss->energy);
    xenon2.l1_eyes_phase = (uint16_t)((xenon2.l1_eyes_phase & 0xff00) | ((xenon2.l1_eyes_phase + lost) & 0xff));
draw_table:
    src = sources[(xenon2.frame_tick & 0xc) >> 2];
    copy_long(&xenon2.level1_data.boss_draw_table[0], src);
    copy_long(&xenon2.level1_data.boss_draw_table[4], src + 4);
    copy_long(&xenon2.level1_data.boss_draw_table[12], src + 8);
    copy_long(&xenon2.level1_data.boss_draw_table[16], src + 12);
    if (boss->l1_boss.phase != 0)
        return;
hide:
    boss->hit[0] = 0x3e8;
    boss->hit[2] = 0x3e8;
}

/* orig $05661A: draw routine of the level 1 boss */
void level1_boss_draw(Object *boss)
{
    static const void *const eyes[4] = { xenon2.level1_data.eyes_a, xenon2.level1_data.eyes_b, xenon2.level1_data.eyes_c, xenon2.level1_data.eyes_b };
    uint16_t eye;

    if ((int16_t)xenon2.scroll_pos >= 0x280)
        return;
    draw_tile_grid(xenon2.level1_data.boss_grid, 5, 6, (uint16_t)(boss->y - xenon2.scroll_pos), 0x38);
    eye = (xenon2.l1_eyes_phase >> 4) & 0xc;
    draw_bob_masked_at(0x98, (uint16_t)(boss->y - xenon2.scroll_pos + 0x42), eyes[eye >> 2]);
}

/* orig $054E20 (-> $5566A8): level 1 starts: its data is copied once, the boss is made */
void level1_init(void)
{
    Object *boss, *link;
    int i;

    if (xenon2.level_init_src == xenon2.level1_data.data_src) {
        for (i = 11999; i >= 0; i--)
            xenon2.level_data_dst[i] = xenon2.level1_data.data_src[i];
        xenon2.level_init_src = xenon2.level_data_dst;
    }
    xenon2.scroll_min = 0;
    xenon2.scroll_max = 0x11ff;
    xenon2.l1_eyes_phase = 0;
    boss = obj_free_pop();
    link = LIST_C->prev;
    LIST_C->prev = boss;
    boss->prev = link;
    boss->next = LIST_C;
    link->next = boss;
    boss->type = 0x50;
    boss->fn_update = FN(level1_boss_update);
    boss->fn_draw = FN(level1_boss_draw);
    boss->fn_hit = (void *)level1_boss_hit;
    SET_V5E(boss, 0);
    boss->persists = -1;
    boss->screen_fixed = -1;
    boss->y = 0x10;
    boss->energy = xenon2.level1_data.boss_energy[0];
    boss->l1_boss.phase = 0;
    boss->steps = 0;
    xenon2.l1_boss_spawned = 0;
    for (i = 0; i < 5; i++)
        xenon2.unit_live[i] = 0;
    xenon2.l1_boss_zone_entered = 0;
    for (i = 0; i < 16; i++)
        xenon2.level_vars[i] = 0;
    xenon2.unit_seed[0] = 0x45d272ba;
    xenon2.unit_seed[1] = 0xaa6a4a56;
    xenon2.unit_seed[2] = 0x4aba5d4d;
    xenon2.unit_seed[3] = 0xd22512c5;
    xenon2.unit_seed[4] = 0x28e51aec;
    xenon2.difficulty_x1 = (uint16_t)(xenon2.difficulty * 1);
    xenon2.difficulty_x2 = (uint16_t)(xenon2.difficulty * 2);
}


/* orig $0055E2C: level 1: starts every unit (a chain of enemies on a path) that is not on the screen; unit_live flags the live ones */
void level1_spawn_units(void)
{
    uint16_t unit;

    for (unit = 0; unit < 5; unit++) {
        if (xenon2.unit_live[unit] == 0) {
            xenon2.unit_live[unit] = 0xff;
            level1_spawn_unit(unit);
        }
    }
}

/* orig $055E50: unit starts: a place of the table is picked from the random state of the unit (the five longs at $D46 rotate right by
 * 5), then objects are made: the first one and a marker pair around the unit's pieces (type $BC after the last piece, $B8 first), the
 * head (type $10C, it follows the path) and the pieces (level1_data.segs, each follows the object before it) */
void level1_spawn_unit(uint16_t unit)
{
    uint32_t r = xenon2.unit_seed[unit];
    const struct LevelSpawn *s;
    Object *first, *obj;
    int16_t x, y, wait, counter, n, i;

    r = (r >> 5) | (r << 27);
    xenon2.unit_seed[unit] = r;
    s = &xenon2.level1_data.unit_starts[r & 0xf];
    x = s->x;
    y = (int16_t)(s->y - xenon2.scroll_step - xenon2.scroll_pos);
    if (xenon2.level_vars[(uint16_t)s->index] == 0)
        xenon2.level_vars[(uint16_t)s->index] = 0x14;
    counter = s->flag;
    wait = 0;
    n = (int16_t)xenon2.level1_data.unit_pieces_less_one;
    first = alloc_c();
    obj = alloc_c();
    obj->type = 0x10c;
    obj->fn_update = FN(level1_head_update);
    obj->fn_draw = obj_nop_thunk;
    obj->fn_hit = (void *)level1_hit_free;
    obj->unit.counter = counter;
    SET_V5E(obj, 0);
    obj->script = (uint8_t *)s->path;
    ((ObjMove *)obj)->cursor = s->path;
    obj->x = x;
    obj->y = y;
    obj->unit.wait = wait;
    obj->steps = (int16_t)xenon2.level1_data.boss_steps;
    obj->unit.id = (int16_t)unit;
    obj->hit[0] = 0x3e8;
    obj->hit[2] = 0x3e8;
    obj->score = 0;
    for (i = 0; i <= n; i++) {
        const struct LevelSeg *seg = &xenon2.level1_data.segs[i];

        obj = alloc_c();
        obj->type = seg->type;
        obj->fn_update = FN(level1_segment_update);
        obj->fn_draw = obj_nop_thunk;
        obj->fn_hit = (void *)seg->hit;
        obj->gfx = seg->gfx;
        obj->score = (int16_t)seg->score;
        obj->unit.counter = counter;
        obj->energy = 1;
        obj->big = 0;
        SET_V5E(obj, 0);
        obj->anim_delay = 0;
        obj->script = (uint8_t *)s->path;
        ((ObjMove *)obj)->cursor = s->path;
        obj->x = x;
        obj->y = y;
        obj->kind = 0;
        obj->unit.wait = wait;
        obj->steps = 4;
        obj->unit.id = (int16_t)unit;
        wait = (int16_t)(wait - 7);
        bbox_from_frame(obj);
    }
    obj = alloc_c();
    obj->type = 0xb8;
    obj->fn_update = obj_nop_thunk;
    obj->fn_draw = obj_nop_thunk;
    obj->fn_hit = (void *)obj_nop_thunk;
    obj->hit[0] = 0x3e8;
    obj->hit[2] = 0x3e8;
    obj->chain = first;
    first->type = 0xbc;
    first->fn_update = FN(group_box_update);
    first->fn_draw = obj_nop_thunk;
    first->fn_hit = (void *)obj_nop_thunk;
    first->hit[0] = 0x3e8;
    first->hit[2] = 0x3e8;
    first->target = obj;
}

/* orig $0561B4: update of the head of a unit (type $10C): it follows its path (scrolled by scroll_speed), keeps the unit marked live */
void level1_head_update(Object *head)
{
    head->y = (int16_t)(head->y + xenon2.scroll_speed);
    xenon2.unit_live[(uint16_t)head->unit.id] = 0xff;
    obj_script_step(head);
    if (head->type == 4)
        head->fn_update(head);
}

/* orig $056034: hit routine of the head: the object hit is freed */
void level1_hit_free(Object *shooter, Object *target, uint16_t damage)
{
    kill_object_thunk(target);
}

/* orig $0560C2: update of a piece of a unit (types $110, $114): it takes the place and the direction of the object before it in the list;
 * the last piece (the next is the marker $BC) keeps the unit's counter alive and goes */
void level1_segment_update(Object *seg)
{
    Object *ahead;
    uint16_t offset;
    const uint8_t *const *tab;

    xenon2.unit_live[(uint16_t)seg->unit.id] = 0xff;
    ahead = OBJ_FWD(seg);
    if (ahead->type == 0xbc) {
        if (xenon2.level_vars[(uint16_t)seg->unit.counter] == 0)
            xenon2.level_vars[(uint16_t)seg->unit.counter] = 0x14;
        kill_object_thunk(seg);
        seg->fn_update(seg);
        return;
    }
    seg->x = ahead->x;
    seg->y = (int16_t)(ahead->y + xenon2.scroll_speed);
    seg->l1.angle = ahead->l1.angle;
    seg->unit.wait = ahead->unit.wait;
    if (seg->fn_draw == obj_nop_thunk && seg->unit.wait >= 0)
        seg->fn_draw = draw_bob_masked;
    offset = (uint16_t)(((uint16_t)((int16_t)seg->l1.angle + 0x10) >> 3) & 0x1c);
    tab = &xenon2.level1_data.piece_pictures[seg->type == 0x110 ? 8 : 0];
    seg->gfx = tab[offset / 4];
    bbox_from_frame(seg);
}

/* the end of $055C2E and $055C46: the piece bursts into a piece that flies away (animation anim), its score is added, the object goes */
static void level1_piece_burst(Object *piece, const uint8_t *anim)
{
    Object *n = alloc_b(0);

    n->type = piece->type;
    n->fn_update = FN(level1_debris_update);
    n->fn_draw = draw_bob_masked;
    n->x = piece->x;
    n->y = piece->y;
    set_anim(n, anim);
    n->l1.angle = (uint16_t)((int16_t)(random_next() & 0xff));
    xenon2.player_score += (uint16_t)piece->score;
    kill_object_thunk(piece);
}

/* orig $055C2E: hit routine of a piece: it flashes; with no energy left it bursts (anim $55250) */
void level1_hit_piece_a(Object *shooter, Object *target, uint16_t damage)
{
    uint16_t old = (uint16_t)target->energy;

    target->fn_draw = draw_bob_or_first;
    target->energy = (int16_t)(old - damage);
    if (old > damage)
        return;
    level1_piece_burst(target, xenon2.level1_data.anim_piece_a);
}

/* orig $055C46: the same with the animation $55274 */
void level1_hit_piece_b(Object *shooter, Object *target, uint16_t damage)
{
    uint16_t old = (uint16_t)target->energy;

    target->fn_draw = draw_bob_or_first;
    target->energy = (int16_t)(old - damage);
    if (old > damage)
        return;
    level1_piece_burst(target, xenon2.level1_data.anim_piece_b);
}

/* orig $05605A: update of a burst piece: it moves by a fixed step from the table at piece_directions picked by its random byte ($2D) and goes when it
 * leaves the screen */
void level1_debris_update(Object *debris)
{
    uint16_t angle;
    int16_t x, y;

    anim_step(debris);
    angle = (uint16_t)(int16_t)debris->l1.angle;
    y = (int16_t)(xenon2.piece_directions[angle] >> 2);
    x = (int16_t)(xenon2.piece_directions[(uint16_t)(angle + 0x40)] >> 2);
    x = (int16_t)(x + debris->x);
    y = (int16_t)(y + debris->y);
    if ((uint16_t)x >= 0x140 || (uint16_t)y >= 0xc0) {
        kill_object_thunk(debris);
        return;
    }
    debris->x = x;
    debris->y = y;
    debris->fn_draw = draw_bob_masked;
}

/* orig $056676: the draw of the boss after a hit: back to the normal draw ($5661A) and the picture drawn as a flash */
void level1_boss_flash(Object *boss)
{
    boss->fn_draw = FN(level1_boss_draw);
    if ((int16_t)xenon2.scroll_pos >= 0x280)
        return;
    draw_tile_grid_flash(xenon2.level1_data.boss_grid, 5, 6, (int16_t)(boss->y - xenon2.scroll_pos), 0x38);
}

/* orig $0561DA: hit routine of the level 1 boss: when the shooter's box meets the part of the boss that can be hurt the boss flashes
 * and loses energy; at no energy it goes: bonuses fall, the screen is full of explosions, the enemies are freed and the level is done */
void level1_boss_hit(Object *shooter, Object *boss, uint16_t damage)
{
    uint16_t old;
    int16_t y;

    if (xenon2.l1_boss_spawned == 0)
        return;
    y = (int16_t)(boss->y - xenon2.scroll_pos + 0x44);
    if (!box_overlap((int16_t)xenon2.hit_box[0], (int16_t)xenon2.hit_box[1], (int16_t)xenon2.hit_box[2], (int16_t)xenon2.hit_box[3], 0x98, y, 0xa7, (int16_t)(y + 0xe)))
        return;
    boss->fn_draw = FN(level1_boss_flash);
    old = (uint16_t)boss->energy;
    boss->energy = (int16_t)(old - damage);
    if (old > damage)
        return;
    xenon2.l1_boss_spawned = 0;
    kill_object_thunk(boss);
    spawn_bonuses(8);
    debris_burst(shooter, 0x13, 0, 0, 0x140, 0xc0, xenon2.big_explosion_anim);
    free_object_chain(LIST_HEAD(LIST_ENEMY));
    xenon2.boss_dead = 0xff;
}

/* orig $056278: update of a part of the level 1 boss ($FC, the last $100): it takes the place and the numbers of the part before it
 * ($1C), the first one the boss's; the last part turns towards the player and shoots */
void level1_boss_part_update(Object *part)
{
    ObjMove *part_move = (ObjMove *)part;
    Object *prev;

    if (xenon2.l1_boss_spawned == 0) {
        kill_object_thunk(part);
        return;
    }
    prev = (Object *)part->anim;
    part->x = prev->x;
    part->x_frac = prev->x_frac;
    part->y = prev->y;
    part->y_frac = prev->y_frac;
    part->l1.angle = prev->l1.angle;
    part->turn = prev->turn;
    part->turn_step = prev->turn_step;
    part->steps = prev->steps;
    if (prev->type == 0x50) {
        part->x = 0xa0;
        part->x_frac = 0;
        part->y = (int16_t)(part->y + 0x60);
        part->y_frac = 0;
        part->y = (int16_t)(part->y - xenon2.scroll_pos);
    }
    part->l1.wait = 0x64;
    enemy_update_no_anim(part);
    if (part->steps == 0) {
        part->hit[0] = 0x3e8;
        part->hit[2] = 0x3e8;
        return;
    }
    if (part->type == 0x100) {
        uint16_t dir = direction8((int16_t)(PLAYER->x - part->x), (int16_t)(PLAYER->y - part->y));

        part->l1.angle = (part->l1.angle & 0xffffu) | (uint32_t)(uint16_t)((int16_t)dir) << 16;
        part->gfx = xenon2.level1_data.boss_part_pictures[dir];
        if (part->steps >= 0xb) {
            uint16_t sum = (uint16_t)(part_move->shot_count + 0xf);

            part_move->shot_count = (uint8_t)sum;
            if (sum & 0x100) {
                Object *shot;

                part_move->shot_count = (uint8_t)(random_next() & 0x3f);
                shot = spawn_enemy_shot((uint16_t)part->x, (uint16_t)part->y, (uint16_t)(int16_t)(part->l1.angle >> 16), 4, 0);
                set_anim(shot, xenon2.level1_data.anim_shot);
            }
        }
    }
    bbox_from_frame(part);
}

/* orig $054E2C: the level's hook for a change of players: none in level 1 */
void level1_swap_hook(void)
{
}
