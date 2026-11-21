/* Level 1: the code of its blob ($54E00): entries, turrets, units, the boss. */
#include "level1.h"
#include "weapons.h"
#include "shop.h"
#include "game_all.h"
#include "weapon_util.h"
#include "amiga/hw.h"

/* the scratch fields of Object, by the kind that uses them */
#define L1_ANGLE_HI(o) ((o)->v2a)
#define L1_ANGLE_LO(o) ((o)->v2c)
#define UNIT_COUNTER(o) ((o)->v2e)
#define UNIT_ID(o) ((o)->v30)
#define UNIT_WAIT(o) ((o)->v28)


/* the scratch fields of Object, by the kind that uses them */


/* the scratch fields of Object, by the kind that uses them */
#define L1_BOSS_MOVE(o) ((o)->v2e)
#define L1_BOSS_PHASE(o) ((o)->v28)
#define L1_BOSS_WAIT(o) ((o)->v30)


/* the scratch fields of Object, by the kind that uses them */
#define L1_TURRET_STEP(o) ((o)->v28)


#define FN(f) ((obj_fn)(f))

/* ---- level 1's blob ($54E00): the jump table at $54E0C and the routines the engine calls ---- */

/* orig $054E0C (-> $5566A4): the level's extra collision test finds nothing (d4 = 0, Z) */
int level1_collision_hook(void)
{
    return 0;
}

/* orig $054E10 (-> $055B94): level events by d0 (a1 = the record that fired): a table of the level engine ($E60) for each */
void level1_event(uint16_t d0, const uint8_t *a1)
{
    static const uint8_t *const tabs[10] = { level_54e66, level_54e80, level_54e9a, level_54eb4, level_54ece,
                                             level_54ee8, level_54f02, level_54f34, level_54f4e, level_54f68 };

    if (d0 == 0) {
        spawn_wave_326c(0, a1);   /* $E5A */
        return;
    }
    if (d0 <= 10)
        spawn_formation(0, a1, tabs[d0 - 1]);   /* $E60 */
}

/* ---- level 1's turrets: markers that draw themselves into the map ---- */

/* the picture of an enemy shot: the table at $68F40 has addresses of longs, the long is the picture */
static const void *level1_shot_gfx(uint16_t idx)
{
    return GPTR(rd32(level_68f40[idx]));
}

static void copy4(uint8_t *dst, const uint8_t *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}

/* the turret has left the screen: it dies */
static int level1_turret_gone(Object *a0)
{
    if ((int16_t)(scroll_max + 0xd0) < a0->y) {
        kill_object_thunk(a0);
        return 1;
    }
    return 0;
}

/* orig $0552CE: update of the turret $E0 / $E4: its box, the map cells it shows (four steps), and now and then a shot */
void level1_turret_a_update(Object *a0)
{
    const uint8_t *a2;
    uint8_t *a1;
    uint16_t d0, sum;
    int16_t x, y, d2;

    if (level1_turret_gone(a0))
        return;
    a0->hit[0] = (int16_t)(a0->x + 4);
    a0->hit[2] = (int16_t)(a0->x + 4 + 0x18);
    y = (int16_t)(a0->y - scroll_pos + 4);
    a0->hit[1] = y;
    a0->hit[3] = (int16_t)(y + 0x18);
    d0 = (uint16_t)(L1_TURRET_STEP(a0) << 3);
    a2 = a0->type == 0xe0 ? level_553b2 : level_553d2;
    a1 = a0->script;
    copy4(a1, a2 + d0);
    copy4(a1 + 0x28, a2 + d0 + 4);
    L1_TURRET_STEP(a0) = (int16_t)((L1_TURRET_STEP(a0) + 1) & 3);
    sum = (uint16_t)(((ObjMove *)a0)->shot_count + level_54e40[3]);
    ((ObjMove *)a0)->shot_count = (uint8_t)sum;
    if (!(sum & 0x100))
        return;
    ((ObjMove *)a0)->shot_count = (uint8_t)(random_next() & 0x3f);
    x = (int16_t)(a0->x + 0x10);
    y = (int16_t)(a0->y - scroll_pos + 0xe);
    d2 = 2;
    if (a0->type != 0xe4) {
        d2 = 6;
        x = (int16_t)(x - 0x10);
    }
    spawn_enemy_shot((uint16_t)x, (uint16_t)y, (uint16_t)d2, rd16(level_54e40 + 6), level1_shot_gfx(rd16(level_54e40 + 4)));
}

/* orig $0555A2: update of the turret $E8 / $EC (sixteen steps of cells) */
void level1_turret_b_update(Object *a0)
{
    const uint8_t *a2;
    uint8_t *a1;
    uint16_t d0, sum;
    int16_t x, y, d2;

    if (level1_turret_gone(a0))
        return;
    a0->hit[0] = (int16_t)(a0->x + 0xa);
    a0->hit[2] = (int16_t)(a0->x + 0xa + 0x12);
    y = (int16_t)(a0->y - scroll_pos + 4);
    a0->hit[1] = y;
    a0->hit[3] = (int16_t)(y + 0x18);
    d0 = (uint16_t)(((uint16_t)L1_TURRET_STEP(a0) & 0xfffc) << 1);
    a2 = a0->type == 0xe8 ? level_556ac : level_5568c;
    a1 = a0->script;
    copy4(a1, a2 + d0);
    copy4(a1 + 0x28, a2 + d0 + 4);
    L1_TURRET_STEP(a0) = (int16_t)((L1_TURRET_STEP(a0) + 1) & 0xf);
    sum = (uint16_t)(((ObjMove *)a0)->shot_count + level_54e40[0xf]);
    ((ObjMove *)a0)->shot_count = (uint8_t)sum;
    if (!(sum & 0x100))
        return;
    ((ObjMove *)a0)->shot_count = (uint8_t)(random_next() & 0x3f);
    x = (int16_t)(a0->x + 0x20);
    y = (int16_t)(a0->y - scroll_pos + 0xe);
    d2 = 2;
    if (a0->type != 0xec) {
        d2 = 6;
        x = (int16_t)(x - 0x20);
    }
    spawn_enemy_shot((uint16_t)x, (uint16_t)y, (uint16_t)d2, rd16(level_54e40 + 0x12), level1_shot_gfx(rd16(level_54e40 + 0x10)));
}

/* orig $055402: update of the turret $D8 / $DC (twenty steps of cells, a shot every 40 steps) */
void level1_turret_c_update(Object *a0)
{
    const uint8_t *a2;
    uint16_t d0, sum;
    int16_t x, y, d2;

    if (level1_turret_gone(a0))
        return;
    a0->hit[0] = (int16_t)(a0->x + 8);
    a0->hit[2] = (int16_t)(a0->x + 8 + 0x10);
    y = (int16_t)(a0->y - scroll_pos + 8);
    a0->hit[1] = y;
    a0->hit[3] = (int16_t)(y + 0x10);
    d0 = (uint16_t)(((uint16_t)L1_TURRET_STEP(a0) & 0xfffe) << 1);
    a2 = a0->type == 0xd8 ? level_5554a : level_554fa;
    copy4(a0->script, a2 + d0);
    if (L1_TURRET_STEP(a0) == 0) {
        sum = (uint16_t)(((ObjMove *)a0)->shot_count + level_54e40[9]);
        ((ObjMove *)a0)->shot_count = (uint8_t)sum;
        if (!(sum & 0x100))
            return;
        ((ObjMove *)a0)->shot_count = (uint8_t)(random_next() & 0x3f);
    }
    L1_TURRET_STEP(a0)++;
    if (L1_TURRET_STEP(a0) == 0x28) {
        L1_TURRET_STEP(a0) = 0;
        return;
    }
    if (L1_TURRET_STEP(a0) != 0x18)
        return;
    x = (int16_t)(a0->x + 0x1a);
    y = (int16_t)(a0->y - scroll_pos + 2);
    d2 = 2;
    if (a0->type != 0xdc) {
        d2 = 6;
        x = (int16_t)(x - 0x1a);
    }
    spawn_enemy_shot((uint16_t)x, (uint16_t)y, (uint16_t)d2, rd16(level_54e40 + 0xc), level1_shot_gfx(rd16(level_54e40 + 0xa)));
}

/* the launcher's use of the scratch fields of Object */
#define LAUNCHER_STEP(o)  ((o)->v28)
#define LAUNCHER_POS(o)   ((o)->v30)
#define LAUNCHER_RANGE(o) ((o)->v2a)

/* orig $0556DC: update of the launcher $D0 / $D4: it moves up and down (v28 is its step, v30 its place between 0 and v2a); when the
 * player is level with it (now and then) it goes into list B and fires three shots */
void level1_launcher_update(Object *a0)
{
    int16_t d0;
    int d7;

    if ((int16_t)(scroll_max + 0xd0 - scroll_pos) < a0->y) {
        kill_object_thunk(a0);
        return;
    }
    anim_step(a0);
    a0->y = (int16_t)(a0->y + scroll_speed);
    if (LAUNCHER_STEP(a0) == 0)
        return;
    d0 = (int16_t)(a0->y - PLAYER->y);
    if (d0 <= 0xa && d0 >= -0x1e && (uint8_t)random_next() < 0x32) {
        Object *tail;
        const uint8_t *a2;

        LAUNCHER_STEP(a0) = 0;
        a0->next->prev = a0->prev;
        a0->prev->next = a0->next;
        tail = LIST_B->prev;
        LIST_B->prev = a0;
        a0->prev = tail;
        a0->next = LIST_B;
        tail->next = a0;
        a2 = a0->type == 0xd4 ? anim_55236 : anim_551f8;
        set_anim(a0, a2);
        for (d7 = 2; d7 >= 0; d7--) {
            int16_t d2 = a0->type == 0xd4 ? 5 : 1;

            spawn_enemy_shot((uint16_t)a0->x, (uint16_t)(a0->y + 7), (uint16_t)(d2 + d7), rd16(level_54e40 + 0x16),
                             level1_shot_gfx(rd16(level_54e40 + 0x14)));
        }
        return;
    }
    d0 = LAUNCHER_STEP(a0);
    a0->y = (int16_t)(a0->y + d0);
    LAUNCHER_POS(a0) = (int16_t)(LAUNCHER_POS(a0) + d0);
    if (LAUNCHER_POS(a0) == 0 || LAUNCHER_POS(a0) == LAUNCHER_RANGE(a0))
        LAUNCHER_STEP(a0) = (int16_t)-LAUNCHER_STEP(a0);
    bbox_from_frame(a0);
}

/* a turret marker (spawn_marker_enemy with the update routine), hit by hit_tile, energy, no box yet */
static Object *level1_turret(uint16_t type, const uint8_t *a1, void (*update)(void), uint16_t energy)
{
    Object *a0 = spawn_marker_enemy(type, a1, update);

    a0->fn_hit = (void *)hit_tile;
    a0->energy = (int16_t)energy;
    a0->hit[0] = 0x3e8;
    a0->hit[2] = 0x3e8;
    return a0;
}

/* the end of the turrets' set-up: the first cells are written into the map ($46), the object hides them again when it is shot */
static void level1_turret_cells(Object *a0, const uint8_t *a2, const uint8_t *a4)
{
    uint8_t *a3 = a0->script;

    a0->gfx = a2;
    a0->anim = a4;
    copy4(a3, a2);
    copy4(a3 + 0x28, a2 + 4);
    a0->score = 0x64;
    a0->v3e = 1;
    a0->v40 = 1;
}

/* orig $054E18 (-> $055942): level hooks by d0 (a1 = the record that fired, d2 its scroll value): 0 remembers the record, 1 to 3 put
 * a turret (type $E8/$EC, $E0/$E4, $D8/$DC; w0c of the record says which side) into the map, 4 a launcher ($D0/$D4) */
void level1_hook(uint16_t d0, const uint8_t *a1, uint16_t d2)
{
    int flag = rd16(a1 + 0xc) != 0;
    Object *a0;

    switch (d0) {
    case 0:
        note_spawn_record(a1, d2);   /* $EA2 */
        return;
    case 1:
        a0 = level1_turret(flag ? 0xe8 : 0xec, a1, (void (*)(void))level1_turret_b_update, level_54e5c[0]);
        level1_turret_cells(a0, flag ? level_556ac : level_5568c, flag ? level_556cc : level_556d4);
        return;
    case 2:
        a0 = level1_turret(flag ? 0xe0 : 0xe4, a1, (void (*)(void))level1_turret_a_update, level_54e58[1]);
        if (flag) {
            a0->script += 2;
            a0->x = (int16_t)(a0->x + 0x10);
        }
        a0->script -= 2;
        a0->x = (int16_t)(a0->x - 0x10);
        level1_turret_cells(a0, flag ? level_553b2 : level_553d2, flag ? level_553f2 : level_553fa);
        return;
    case 3:
        a0 = level1_turret(flag ? 0xd8 : 0xdc, a1, (void (*)(void))level1_turret_c_update, level_54e5c[1]);
        level1_turret_cells(a0, flag ? level_55adc : level_55ad4, level_5559a);
        return;
    case 4:
        a0 = alloc_c();
        a0->type = flag ? 0xd4 : 0xd0;
        set_anim(a0, flag ? anim_55212 : anim_551d4);
        a0->fn_update = FN(level1_launcher_update);
        a0->fn_draw = draw_bob_masked;
        a0->fn_hit = (void *)hit_enemy;
        a0->energy = (int16_t)level_54e5c[2];
        a0->big = -1;
        a0->score = 0xfa;
        SET_V5E(a0, 0);
        a0->x = (int16_t)(rd16(a1 + 8) - 8 + (flag ? 0x10 : 0));
        a0->y = (int16_t)(rd16(a1 + 10) - 8 - scroll_pos);
        LAUNCHER_STEP(a0) = -1;
        LAUNCHER_RANGE(a0) = (int16_t)(rd16(a1 + 6) << 4);
        LAUNCHER_POS(a0) = a0->v2a;
        a0->kind = 0;
        bbox_from_frame(a0);
        return;
    }
}

/* orig $054E24 (-> $055806): the level's scroll events */
void level1_scroll(void)
{
    const uint16_t *a1 = level_558a2;
    uint16_t *a4 = level_vars;
    int d7, i;

    if (l1_boss_zone_entered == 0 && (int16_t)(scroll_pos + PLAYER->y) < 0xa80) {
        level_over = 0xff;
        l1_boss_zone_entered = 0xff;
        scroll_pos = 0x9c0;
        scroll_max = 0x9c0;
        map_row_offset = 0x1860;
        scroll_fine = 0;
    }
    if ((int16_t)scroll_pos >= 0xa40 && (int16_t)scroll_pos <= 0xd10) {
        if ((int16_t)scroll_max < 0xd10)
            scroll_max = 0xd10;
        level1_spawn_units();
        for (d7 = 15; d7 >= 0; d7--) {
            if (*a4 != 0) {
                uint8_t *a2 = level_init_src + (int16_t)a1[0];

                (*a4)--;
                wr16(a2, a1[1 + (*a4 & 3)]);
            }
            a4++;
            a1 += 5;
        }
    }
    for (i = 0; i < 5; i++)
        unit_live[i] = 0;
}

/* orig $0563AA: update of the level 1 boss (type $50) */
void level1_boss_update(Object *a0)
{
    Object *a1, *a2, *a5;
    int d7;
    int16_t d0, d5;
    int32_t l;
    const uint8_t *src;
    static const uint8_t *const sources[4] = { level_56810, level_56810 + 16, level_56810 + 32, level_56810 + 16 };

    if ((int16_t)scroll_pos > 0x1c0)
        goto hide;
    if (l1_boss_spawned == 0) {
        l1_boss_spawned = 0xff;
        a2 = a0;
        for (d7 = 7; d7 >= 0; d7--) {
            a1 = obj_free_pop();
            if (d7 == 7) {
                a5 = LIST_C->prev;
                LIST_C->prev = a1;
                a1->prev = a5;
                a1->next = LIST_C;
                a5->next = a1;
            } else {
                a5 = a2->prev;
                a2->prev = a1;
                a1->prev = a5;
                a1->next = a2;
                a5->next = a1;
            }
            a1->type = 0xfc;
            if (d7 == 0)
                a1->type = 0x100;
            a1->fn_update = level1_boss_part_update;
            a1->fn_draw = draw_bob_masked;
            a1->fn_hit = (void *)hit_kill_shooter;
            a1->gfx = level_59e28;
            a1->anim_delay = 0;
            SET_V5E(a1, 0);
            a1->anim = (const uint8_t *)a2;
            a2 = a1;
            a1->big = 0;
            a1->persists = -1;
            a1->y = -100;
            a1->hit[0] = 0x3e8;
            a1->hit[2] = 0x3e8;
        }
        L1_ANGLE_HI(a0) = 0;
        L1_ANGLE_LO(a0) = 0;
        a0->turn[0] = 0;
        a0->turn[1] = 0;
        a0->turn[2] = 0;
    }
    if ((int16_t)scroll_max <= 0x1c0)
        scroll_max = 0x1c0;
    a0->hit[0] = 0x80;
    a0->hit[2] = 0xc8;
    d5 = (int16_t)(a0->y - scroll_pos + 4);
    a0->hit[1] = d5;
    a0->hit[3] = (int16_t)(d5 + 0x50);
    if (L1_BOSS_PHASE(a0) == 0) {
        if ((int16_t)scroll_pos > 0x30)
            goto draw_table;
        L1_BOSS_PHASE(a0) = 1;
        L1_BOSS_MOVE(a0) = 0;
        L1_BOSS_WAIT(a0) = 0xaa;
    }
    if (L1_BOSS_PHASE(a0) < 0) {
        if (a0->steps != 0)
            a0->steps--;
    } else if (a0->steps < 0xb) {
        if (a0->steps == 0)
            sfx_cmd2 = 0xf;
        a0->steps++;
    }
    L1_BOSS_PHASE(a0)++;
    if (L1_BOSS_PHASE(a0) == 0)
        L1_BOSS_PHASE(a0)++;
    if (L1_BOSS_PHASE(a0) >= 0x64)
        L1_BOSS_PHASE(a0) = -40;
    d0 = frame_tick & 0x7f;
    if (d0 & 0x40)
        d0 = (int16_t)-(d0 - 0x7f);
    d0 = (int8_t)d0;
    L1_ANGLE_LO(a0) = (int16_t)(d0 + 0x20);
    d0 = (int16_t)((frame_tick + 0x50) & 0xff);
    if (d0 & 0x80)
        d0 = (~d0) & 0xff;
    l = (int32_t)(int16_t)(d0 - 0x40) << 10;
    a0->turn[0] = (uint16_t)(l >> 16);
    a0->turn[1] = (uint16_t)l;
    d0 = (int16_t)((frame_tick + 0xa0) & 0x1ff);
    if (d0 & 0x100)
        d0 ^= 0x1ff;
    d0 -= 0x80;
    a0->turn[2] = (uint16_t)(d0 << 5);
    d0 = (int16_t)(L1_BOSS_MOVE(a0) + a0->y);
    if (d0 < 0x10) {
        d0 = 0x10;
        L1_BOSS_WAIT(a0) = 0;
    }
    if (d0 > 0x1e0) {
        d0 = 0x1e0;
        L1_BOSS_WAIT(a0) = 0;
    }
    a0->y = d0;
    L1_BOSS_WAIT(a0)--;
    if (L1_BOSS_WAIT(a0) < 0) {
        if (L1_BOSS_MOVE(a0) == -1) {
            L1_BOSS_MOVE(a0) = 0;
            L1_BOSS_WAIT(a0) = 0xaa;
        } else if (L1_BOSS_MOVE(a0) == 1) {
            L1_BOSS_MOVE(a0) = 0;
            L1_BOSS_WAIT(a0) = 0x3c;
        } else {
            L1_BOSS_WAIT(a0) = 0x32;
            L1_BOSS_MOVE(a0) = 1;
            if (a0->y != 0x10)
                L1_BOSS_MOVE(a0) = -1;
        }
    }
    d0 = (int16_t)(0x1e - a0->energy);
    l1_eyes_phase = (uint16_t)((l1_eyes_phase & 0xff00) | ((l1_eyes_phase + d0) & 0xff));
draw_table:
    src = sources[(frame_tick & 0xc) >> 2];
    copy_long(&level_567fa[0], src);
    copy_long(&level_567fa[4], src + 4);
    copy_long(&level_567fa[12], src + 8);
    copy_long(&level_567fa[16], src + 12);
    if (L1_BOSS_PHASE(a0) != 0)
        return;
hide:
    a0->hit[0] = 0x3e8;
    a0->hit[2] = 0x3e8;
}

/* orig $05661A: draw routine of the level 1 boss */
void level1_boss_draw(Object *a0)
{
    static const void *const eyes[4] = { level_5c0e4, level_5c194, level_5c244, level_5c194 };
    uint16_t d7;

    if ((int16_t)scroll_pos >= 0x280)
        return;
    draw_tile_grid(level_567bc, 5, 6, (uint16_t)(a0->y - scroll_pos), 0x38);
    d7 = (l1_eyes_phase >> 4) & 0xc;
    draw_bob_masked_at(0x98, (uint16_t)(a0->y - scroll_pos + 0x42), eyes[d7 >> 2]);
}

/* orig $054E20 (-> $5566A8): level 1 starts: its data is copied once, the boss is made */
void level1_init(void)
{
    Object *a0, *a5;
    int i;

    if (level_init_src == level_data_src) {
        for (i = 11999; i >= 0; i--)
            level_data_dst[i] = level_data_src[i];
        level_init_src = level_data_dst;
    }
    scroll_min = 0;
    scroll_max = 0x11ff;
    l1_eyes_phase = 0;
    a0 = obj_free_pop();
    a5 = LIST_C->prev;
    LIST_C->prev = a0;
    a0->prev = a5;
    a0->next = LIST_C;
    a5->next = a0;
    a0->type = 0x50;
    a0->fn_update = FN(level1_boss_update);
    a0->fn_draw = FN(level1_boss_draw);
    a0->fn_hit = (void *)level1_boss_hit;
    SET_V5E(a0, 0);
    a0->persists = -1;
    a0->screen_fixed = -1;
    a0->y = 0x10;
    a0->energy = level_54e58[0];
    L1_BOSS_PHASE(a0) = 0;
    a0->steps = 0;
    l1_boss_spawned = 0;
    for (i = 0; i < 5; i++)
        unit_live[i] = 0;
    l1_boss_zone_entered = 0;
    for (i = 0; i < 16; i++)
        level_vars[i] = 0;
    level_vars[16] = 0x45d2; level_vars[17] = 0x72ba;
    level_vars[18] = 0xaa6a; level_vars[19] = 0x4a56;
    level_vars[20] = 0x4aba; level_vars[21] = 0x5d4d;
    level_vars[22] = 0xd225; level_vars[23] = 0x12c5;
    level_vars[24] = 0x28e5; level_vars[25] = 0x1aec;
    difficulty_x1 = (uint16_t)(difficulty * 1);
    difficulty_x2 = (uint16_t)(difficulty * 2);
}


/* orig $0055E2C: level 1: starts every unit (a chain of enemies on a path) that is not on the screen; unit_live flags the live ones */
void level1_spawn_units(void)
{
    uint16_t d6;

    for (d6 = 0; d6 < 5; d6++) {
        if (unit_live[d6] == 0) {
            unit_live[d6] = 0xff;
            level1_spawn_unit(d6);
        }
    }
}

/* orig $055E50: unit d6 starts: a place of the table is picked from the random state of the unit (the five longs at $D46 rotate right by
 * 5), then objects are made: the first one and a marker pair around the unit's pieces (type $BC after the last piece, $B8 first), the
 * head (type $10C, it follows the path) and the pieces (level_segs, each follows the object before it) */
void level1_spawn_unit(uint16_t d6)
{
    uint16_t *state = &level_vars[16 + 2 * d6];
    uint32_t r = ((uint32_t)state[0] << 16) | state[1];
    const struct LevelSpawn *s;
    Object *first, *a0;
    int16_t x, y, d2, d3, n, i;

    r = (r >> 5) | (r << 27);
    state[0] = (uint16_t)(r >> 16);
    state[1] = (uint16_t)r;
    s = &level_spawns[r & 0xf];
    x = s->x;
    y = (int16_t)(s->y - scroll_step - scroll_pos);
    if (level_vars[(uint16_t)s->index] == 0)
        level_vars[(uint16_t)s->index] = 0x14;
    d3 = s->flag;
    d2 = 0;
    n = (int16_t)level_55ce6[0];
    first = alloc_c();
    a0 = alloc_c();
    a0->type = 0x10c;
    a0->fn_update = FN(level1_head_update);
    a0->fn_draw = obj_nop_thunk;
    a0->fn_hit = (void *)level1_hit_free;
    UNIT_COUNTER(a0) = d3;
    SET_V5E(a0, 0);
    a0->script = s->path;
    ((ObjMove *)a0)->cursor = s->path;
    a0->x = x;
    a0->y = y;
    UNIT_WAIT(a0) = d2;
    a0->steps = (int16_t)level_54e62[0];
    UNIT_ID(a0) = (int16_t)d6;
    a0->hit[0] = 0x3e8;
    a0->hit[2] = 0x3e8;
    a0->score = 0;
    for (i = 0; i <= n; i++) {
        const struct LevelSeg *seg = &level_segs[i];

        a0 = alloc_c();
        a0->type = seg->type;
        a0->fn_update = FN(level1_segment_update);
        a0->fn_draw = obj_nop_thunk;
        a0->fn_hit = (void *)seg->hit;
        a0->gfx = seg->gfx;
        a0->score = (int16_t)seg->score;
        UNIT_COUNTER(a0) = d3;
        a0->energy = 1;
        a0->big = 0;
        SET_V5E(a0, 0);
        a0->anim_delay = 0;
        a0->script = s->path;
        ((ObjMove *)a0)->cursor = s->path;
        a0->x = x;
        a0->y = y;
        a0->kind = 0;
        UNIT_WAIT(a0) = d2;
        a0->steps = 4;
        UNIT_ID(a0) = (int16_t)d6;
        d2 = (int16_t)(d2 - 7);
        bbox_from_frame(a0);
    }
    a0 = alloc_c();
    a0->type = 0xb8;
    a0->fn_update = obj_nop_thunk;
    a0->fn_draw = obj_nop_thunk;
    a0->fn_hit = (void *)obj_nop_thunk;
    a0->hit[0] = 0x3e8;
    a0->hit[2] = 0x3e8;
    a0->chain = first;
    first->type = 0xbc;
    first->fn_update = FN(group_box_update);
    first->fn_draw = obj_nop_thunk;
    first->fn_hit = (void *)obj_nop_thunk;
    first->hit[0] = 0x3e8;
    first->hit[2] = 0x3e8;
    first->target = a0;
}

/* orig $0561B4: update of the head of a unit (type $10C): it follows its path (scrolled by scroll_speed), keeps the unit marked live */
void level1_head_update(Object *a0)
{
    a0->y = (int16_t)(a0->y + scroll_speed);
    unit_live[(uint16_t)UNIT_ID(a0)] = 0xff;
    obj_script_step(a0);
    if (a0->type == 4)
        a0->fn_update(a0);
}

/* orig $056034: hit routine of the head: the object hit (a1) is freed */
void level1_hit_free(Object *a0, Object *a1, uint16_t d0)
{
    kill_object_thunk(a1);
}

/* orig $0560C2: update of a piece of a unit (types $110, $114): it takes the place and the direction of the object before it in the list;
 * the last piece (the next is the marker $BC) keeps the unit's counter alive and goes */
void level1_segment_update(Object *a0)
{
    Object *a1;
    uint16_t d2;
    uint8_t **tab;

    unit_live[(uint16_t)UNIT_ID(a0)] = 0xff;
    a1 = OBJ_FWD(a0);
    if (a1->type == 0xbc) {
        if (level_vars[(uint16_t)UNIT_COUNTER(a0)] == 0)
            level_vars[(uint16_t)UNIT_COUNTER(a0)] = 0x14;
        kill_object_thunk(a0);
        a0->fn_update(a0);
        return;
    }
    a0->x = a1->x;
    a0->y = (int16_t)(a1->y + scroll_speed);
    L1_ANGLE_HI(a0) = L1_ANGLE_HI(a1);
    L1_ANGLE_LO(a0) = L1_ANGLE_LO(a1);
    UNIT_WAIT(a0) = UNIT_WAIT(a1);
    if (a0->fn_draw == obj_nop_thunk && UNIT_WAIT(a0) >= 0)
        a0->fn_draw = draw_bob_masked;
    d2 = (uint16_t)(((uint16_t)(L1_ANGLE_LO(a0) + 0x10) >> 3) & 0x1c);
    tab = &level_56174[a0->type == 0x110 ? 8 : 0];
    a0->gfx = tab[d2 / 4];
    bbox_from_frame(a0);
}

/* the end of $055C2E and $055C46: the piece a1 bursts into a piece that flies away (animation a2), its score is added, the object goes */
static void level1_piece_burst(Object *a1, const uint8_t *a2)
{
    Object *n = alloc_b(0);

    n->type = a1->type;
    n->fn_update = FN(level1_debris_update);
    n->fn_draw = draw_bob_masked;
    n->x = a1->x;
    n->y = a1->y;
    set_anim(n, a2);
    L1_ANGLE_HI(n) = 0;
    L1_ANGLE_LO(n) = (int16_t)(random_next() & 0xff);
    player_score += (uint16_t)a1->score;
    kill_object_thunk(a1);
}

/* orig $055C2E: hit routine of a piece: it flashes; with no energy left it bursts (anim $55250) */
void level1_hit_piece_a(Object *a0, Object *a1, uint16_t d0)
{
    uint16_t old = (uint16_t)a1->energy;

    a1->fn_draw = draw_bob_or_first;
    a1->energy = (int16_t)(old - d0);
    if (old > d0)
        return;
    level1_piece_burst(a1, anim_55250);
}

/* orig $055C46: the same with the animation $55274 */
void level1_hit_piece_b(Object *a0, Object *a1, uint16_t d0)
{
    uint16_t old = (uint16_t)a1->energy;

    a1->fn_draw = draw_bob_or_first;
    a1->energy = (int16_t)(old - d0);
    if (old > d0)
        return;
    level1_piece_burst(a1, anim_55274);
}

/* orig $05605A: update of a burst piece: it moves by a fixed step from the table at piece_directions picked by its random byte ($2D) and goes when it
 * leaves the screen */
void level1_debris_update(Object *a0)
{
    uint16_t d2;
    int16_t d0, d1;

    anim_step(a0);
    d2 = (uint16_t)L1_ANGLE_LO(a0);
    d1 = (int16_t)(piece_directions[d2] >> 2);
    d0 = (int16_t)(piece_directions[(uint16_t)(d2 + 0x40)] >> 2);
    d0 = (int16_t)(d0 + a0->x);
    d1 = (int16_t)(d1 + a0->y);
    if ((uint16_t)d0 >= 0x140 || (uint16_t)d1 >= 0xc0) {
        kill_object_thunk(a0);
        return;
    }
    a0->x = d0;
    a0->y = d1;
    a0->fn_draw = draw_bob_masked;
}

/* orig $056676: the draw of the boss after a hit: back to the normal draw ($5661A) and the picture drawn as a flash */
void level1_boss_flash(Object *a0)
{
    a0->fn_draw = FN(level1_boss_draw);
    if ((int16_t)scroll_pos >= 0x280)
        return;
    draw_tile_grid_flash(level_567bc, 5, 6, (int16_t)(a0->y - scroll_pos), 0x38);
}

/* orig $0561DA: hit routine of the level 1 boss: when the shooter's box meets the part of the boss that can be hurt the boss flashes
 * and loses energy; at no energy it goes: bonuses fall, the screen is full of explosions, the enemies are freed and the level is done */
void level1_boss_hit(Object *a0, Object *a1, uint16_t d0)
{
    uint16_t old;
    int16_t y;

    if (l1_boss_spawned == 0)
        return;
    y = (int16_t)(a1->y - scroll_pos + 0x44);
    if (!box_overlap((int16_t)hit_box[0], (int16_t)hit_box[1], (int16_t)hit_box[2], (int16_t)hit_box[3], 0x98, y, 0xa7, (int16_t)(y + 0xe)))
        return;
    a1->fn_draw = FN(level1_boss_flash);
    old = (uint16_t)a1->energy;
    a1->energy = (int16_t)(old - d0);
    if (old > d0)
        return;
    l1_boss_spawned = 0;
    kill_object_thunk(a1);
    spawn_bonuses(8);
    debris_burst(a0, 0x13, 0, 0, 0x140, 0xc0, big_explosion_anim);
    free_object_chain(LIST_HEAD(LIST_ENEMY));
    boss_dead = 0xff;
}

/* orig $056278: update of a part of the level 1 boss ($FC, the last $100): it takes the place and the numbers of the part before it
 * ($1C), the first one the boss's; the last part turns towards the player and shoots */
void level1_boss_part_update(Object *a0)
{
    ObjMove *m = (ObjMove *)a0;
    Object *a1;

    if (l1_boss_spawned == 0) {
        kill_object_thunk(a0);
        return;
    }
    a1 = (Object *)a0->anim;
    a0->x = a1->x;
    a0->x_frac = a1->x_frac;
    a0->y = a1->y;
    a0->y_frac = a1->y_frac;
    L1_ANGLE_HI(a0) = L1_ANGLE_HI(a1);
    L1_ANGLE_LO(a0) = L1_ANGLE_LO(a1);
    a0->turn[0] = a1->turn[0];
    a0->turn[1] = a1->turn[1];
    a0->turn[2] = a1->turn[2];
    a0->steps = a1->steps;
    if (a1->type == 0x50) {
        a0->x = 0xa0;
        a0->x_frac = 0;
        a0->y = (int16_t)(a0->y + 0x60);
        a0->y_frac = 0;
        a0->y = (int16_t)(a0->y - scroll_pos);
    }
    a0->v28 = 0x64;
    enemy_update_no_anim(a0);
    if (a0->steps == 0) {
        a0->hit[0] = 0x3e8;
        a0->hit[2] = 0x3e8;
        return;
    }
    if (a0->type == 0x100) {
        uint16_t dir = direction8((int16_t)(PLAYER->x - a0->x), (int16_t)(PLAYER->y - a0->y));

        L1_ANGLE_HI(a0) = (int16_t)dir;
        a0->gfx = level_5638a[dir];
        if (a0->steps >= 0xb) {
            uint16_t sum = (uint16_t)(m->shot_count + 0xf);

            m->shot_count = (uint8_t)sum;
            if (sum & 0x100) {
                Object *shot;

                m->shot_count = (uint8_t)(random_next() & 0x3f);
                shot = spawn_enemy_shot((uint16_t)a0->x, (uint16_t)a0->y, (uint16_t)L1_ANGLE_HI(a0), 4, 0);
                set_anim(shot, anim_552b0);
            }
        }
    }
    bbox_from_frame(a0);
}

/* orig $054E2C: the level's hook for a change of players: none in level 1 */
void level1_swap_hook(void)
{
}
