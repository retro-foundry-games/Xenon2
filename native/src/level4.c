/* level4: level 4's own code (the blob at $54E00 while level 4 is loaded), written like level3.c; a function has `level4 $ADDR` for its address. */
#include "weapons.h"
#include "shop.h"
#include "game_all.h"
#include "blobs.h"
#include <string.h>


/* level 4: its parameter block at E42 (energies, fire rates, shot steps), by the object that reads them */
#define PICKUP_MODE(o) ((o)->v28)
#define PICKUP_DIR(o)  ((o)->v2a)
#define PATH_WAIT(o) ((o)->v28)

/* the scratch fields of Object, by the kind that uses them */
#define L4_SIZE(o) ((o)->v28)
#define WALKER_DY(o) ((o)->v2e)

#define BAR_STEP(o) ((o)->v2a)

/* the scratch fields of Object, by the kind that uses them */
#define L4_ANGLE_HI(o) ((o)->v2a)
#define L4_ANGLE_LO(o) ((o)->v2c)


/* the scratch fields of Object, by the kind that uses them */
#define TOWER_PHASE(o) ((o)->v28)


/* the scratch fields of Object, by the kind that uses them */
#define AIM_DIR(o) ((o)->v2a)
#define B1_GUN_DIR(o) ((o)->v2a)
#define BAR_PHASE(o) ((o)->v28)
#define CORE_TICK(o) ((o)->v28)
#define HEAD_ANGLE(o) ((o)->v2c)
#define HEAD_DART(o) ((o)->v2e)
#define L4_GUN_TICK(o) ((o)->v28)
#define WALKER_HOME_X(o) ((o)->v3e)
#define WALKER_HOME_Y(o) ((o)->v40)
#define WALKER_SIDE(o) ((o)->v30)
#define WALKER_STATE(o) ((o)->v28)


#define FN(f) ((obj_fn)(f))
#define W(a) sh_r16(a)
#define B(a) sh_r8(a)

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

static void set_anim(Object *o, const uint8_t *anim)
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

static uint32_t get4e(const Object *o)
{
    return ((uint32_t)o->turn[0] << 16) | o->turn[1];
}

static void put4e(Object *o, uint32_t v)
{
    o->turn[0] = (uint16_t)(v >> 16);
    o->turn[1] = (uint16_t)v;
}

/* the player's direction from the object (the eight of direction8), the offsets are the original's */
static uint16_t aim_at_player(const Object *a0, int16_t dx, int16_t dy)
{
    return direction8((int16_t)(PLAYER->x - a0->x - dx), (int16_t)(PLAYER->y - a0->y - dy));
}

/* the picture of the table at $77B70 (by the index of the level) */
static uint32_t shot_picture(uint16_t index)
{
    return sh_r32(sh_r32(0x77b70 + 4 * (uint32_t)index));
}

static void copy4(uint8_t *dst, const uint8_t *src)
{
    int i;

    for (i = 0; i < 4; i++)
        dst[i] = src[i];
}

/* the two longs of a table entry go to the long at the cell and the one a row below it */
static void put_cells(uint8_t *cell, const uint8_t *table)
{
    copy4(cell, table);
    copy4(cell + 0x28, table + 4);
}

void l4_tower_update(Object *a0);
void l4_tower_hit(Object *a0, Object *a1, uint16_t d0);
void l4_tower_draw(Object *a0);
void l4_spawn_boss2(const uint8_t *a1, uint16_t d2);
void l4_spawn_walker(const uint8_t *a1);
void l4_spawn_boss1(void);
void l4_spawn_bar(const uint8_t *a1);

/* level4 $054E10, $057272: level events by d0 (a1 = the record that fired): 0 the wave of $326C, 1 to 6 formations */
void l4_event(uint16_t d0, const uint8_t *a1)
{
    const uint8_t *tables[7] = { 0, level4_blob.formation_1, level4_blob.formation_2, level4_blob.formation_3, level4_blob.formation_4,
                                 level4_blob.formation_5, level4_blob.formation_6 };

    if (d0 == 0)
        spawn_wave_326c(0, a1);   /* $E5A */
    else if (d0 < 7)
        spawn_formation(0, a1, tables[d0]);
}

/* level4 $054E18, $05585E: the hooks by d0 */
void l4_hook(uint16_t d0, const uint8_t *a1, uint16_t d2)
{
    switch (d0) {
    case 0:
        note_spawn_record(a1, d2);   /* $EA2 */
        break;
    case 1:
        l4_spawn_tower(a1);
        break;
    case 2:
        l4_spawn_boss2(a1, d2);
        break;
    case 3:
        l4_spawn_walker(a1);
        break;
    case 4:
        l4_spawn_boss1();
        break;
    case 5:
        l4_spawn_bar(a1);
        break;
    }
}

/* level4 $054E20, $0572D8: the data is copied once (backwards: the copy lies above its source) */
void l4_init(void)
{
    int i;

    if (level_init_src == (uint8_t *)GPTR(0x7d080)) {
        for (i = 11999; i >= 0; i--)
            ((uint8_t *)GPTR(0x7d120))[i] = ((const uint8_t *)GPTR(0x7d080))[i];
        level_init_src = (uint8_t *)GPTR(0x7d120);
    }
    scroll_min = 0x880;
    scroll_max = 0x11ff;
    sh_w8(0xd97, 0);
    sh_w8(0xd98, 0);
    level4_blob.boss1_pieces[1].energy = level4_blob.init_patch;
    level4_blob.boss1_pieces[2].energy = level4_blob.init_patch;
    level4_blob.boss1_pieces[0].energy = level4_blob.init_patch_2;
    level4_blob.boss2_pieces[16].energy = level4_blob.init_patch_3;
    level4_blob.boss2_pieces[17].energy = level4_blob.init_patch_3;
    level4_blob.boss2_pieces[18].energy = level4_blob.init_patch_3;
    level4_blob.boss2_pieces[19].energy = level4_blob.init_patch_3;
    level4_blob.boss2_pieces[15].energy = level4_blob.init_patch_4;
    level4_blob.boss2_pieces[4].energy = level4_blob.init_patch_5;
    difficulty_x1 = (uint16_t)(difficulty * 6);
    difficulty_x2 = (uint16_t)(difficulty * 8);
}

/* level4 $057064: the tower of tiles: it fires four ways by its phase v28 */
void l4_tower_update(Object *a0)
{
    const uint8_t *cells;
    uint16_t d0;

    if ((int16_t)(scroll_max + 0xd0) < a0->y) {
        kill_thunk(a0);
        return;
    }
    a0->hit[0] = (int16_t)(a0->x + 2);
    a0->hit[2] = (int16_t)(a0->x + 0x1e);
    d0 = (uint16_t)(a0->y - scroll_pos + 2);
    a0->hit[1] = (int16_t)d0;
    a0->hit[3] = (int16_t)(d0 + 0x1c);
    if (TOWER_PHASE(a0) == 0) {
        uint16_t sum = (uint16_t)(a0->v5e_b[0] + level4_blob.tower_fire_rate);

        a0->v5e_b[0] = (uint8_t)sum;
        if (sum > 0xff) {
            a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
            TOWER_PHASE(a0) = 1;
        }
        return;
    }
    cells = (a0->type == 0xd0 ? level4_blob.tower_cells_d0 : level4_blob.tower_cells_d4) + ((uint16_t)TOWER_PHASE(a0) << 3);
    put_cells(a0->script, cells);
    TOWER_PHASE(a0)++;
    if (TOWER_PHASE(a0) == 4) {
        int16_t x = (int16_t)(a0->x + 0x12), y = (int16_t)(a0->y - scroll_pos + 0xe);
        uint16_t angle = 2;
        uint32_t table = sh_r32(0x77b70 + 4 * (uint32_t)level4_blob.tower_param);

        if (a0->type != 0xd4) {
            angle = 6;
            x = (int16_t)(x - 8);
        }
        spawn_enemy_shot((uint16_t)x, (uint16_t)y, angle, level4_blob.tower_shot_steps, GPTR(sh_r32(table)));
        return;
    }
    if (TOWER_PHASE(a0) == 6)
        TOWER_PHASE(a0) = 0;
}

/* level4 $0571C0: the hit of a tower: it flashes; when it is shot to pieces it bursts and the cells are the broken ones */
void l4_tower_hit(Object *a0, Object *a1, uint16_t d0)
{
    uint16_t old = (uint16_t)a1->energy;

    a1->energy = (int16_t)(old - d0);
    if (old > d0) {
        a1->fn_draw = FN(l4_tower_draw);
        return;
    }
    spawn_effect((int16_t)(a1->x + 0x10), (int16_t)(a1->y - scroll_pos + 0x10), big_explosion_anim, draw_bob_masked);
    put_cells(a1->script, a1->type == 0xd0 ? level4_blob.tower_hit_cells_d0 : level4_blob.tower_hit_cells_d4);
    player_score += 0x190;
    kill_thunk(a1);
}

/* level4 $057246: the flash of a tower, one frame */
void l4_tower_draw(Object *a0)
{
    a0->fn_draw = (obj_fn)obj_nop_thunk;
    draw_tile_grid_flash((a0->type == 0xd0 ? level4_blob.tower_cells_d0 : level4_blob.tower_cells_d4), 1, 1, (int16_t)(a0->y - scroll_pos), (uint16_t)((uint16_t)a0->x >> 1));
}

/* level4 $05700A: a marker makes a tower; the record's word at +$C says which of the two */
void l4_spawn_tower(const uint8_t *a1)
{
    int flag = rd16(a1 + 0xc) != 0;
    Object *a0 = spawn_marker_enemy(flag ? 0xd0 : 0xd4, a1, (void (*)(void))l4_tower_update);

    a0->fn_draw = (obj_fn)obj_nop_thunk;
    a0->fn_hit = (void *)l4_tower_hit;
    a0->energy = (int16_t)level4_blob.tower_energy;
    a0->hit[0] = 0x3e8;
    a0->hit[2] = 0x3e8;
    put_cells(a0->script, flag ? level4_blob.tower_cells_d0 : level4_blob.tower_cells_d4);
}

/* level4 $055090: the scroll: the floor of the scroll ends at $DF0 and between $AE0 and $E50 an enemy comes every 16 pixels */
void l4_scroll(void)
{
    if ((int16_t)scroll_max >= 0xdf0 && (int16_t)scroll_pos <= 0xdf0)
        scroll_max = 0xdf0;
    if ((frame_tick & 0xf) != 0 || (int16_t)scroll_pos > 0xe50 || (int16_t)scroll_pos < 0xae0)
        return;
    {
        Object *a0 = alloc_in(LIST_C);
        uint16_t d0 = (random_next() & 1) ? 0xc : 0;
        const struct Level4ScrollEnemy *a2;

        if (frame_tick & 0x20)
            d0 += 0x18;
        a2 = &level4_blob.scroll_enemies[d0 / 12];
        a0->type = (int16_t)a2->type;
        a0->x = (int16_t)a2->x;
        a0->fn_update = (obj_fn)a2->update;
        a0->fn_hit = (void *)hit_enemy;
        a0->fn_draw = draw_bob_masked;
        anim_header_set(a0, a2->anim);
        a0->v5e_b[0] = (uint8_t)random_next();
        a0->y = -0x20;
        a0->v28 = 0;
        a0->energy = (int16_t)level4_blob.scroll_energy;
        a0->big = 0xff;
        a0->kind = 0;
        a0->score = 0x12c;
        bbox_from_frame(a0);
    }
}

/* ---- the enemies of the scroll ---- */

static void l4_kill(Object *a0)
{
    kill_thunk(a0);
}

/* level4 $05524A: a gun that comes down the screen: now and then a part flies off it and it fires */
void l4_enemy_a(Object *a0)
{
    anim_step(a0);
    a0->y = (int16_t)(a0->y + scroll_speed + level4_blob.enemy_a_fall);
    if (a0->y >= 0xc8) {
        l4_kill(a0);
        return;
    }
    L4_GUN_TICK(a0)++;
    if (L4_GUN_TICK(a0) == 8) {
        if (add_carry(&a0->v5e_b[0], level4_blob.enemy_a_fire_rate)) {
            Object *a1;
            const struct Level4EnemyGun *a2 = &level4_blob.enemy_a_guns[a0->type == 0xc8 ? 0 : 1];

            a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
            a1 = alloc_in(LIST_B);
            a1->type = a2->type;
            a1->fn_update = FN(l4_shot_a);
            a1->fn_draw = draw_bob_masked;
            a1->x = (int16_t)a2->x;
            a1->y = (int16_t)(a0->y - 0x1e);
            L4_GUN_TICK(a1) = 0;
            set_anim(a1, a2->anim);
        }
    } else if (L4_GUN_TICK(a0) == 0xc) {
        L4_GUN_TICK(a0) = 0;
    }
    if (add_carry(&a0->v5e_b[1], (uint8_t)level4_blob.tower_energy)) {
        int16_t x = (int16_t)(a0->x + 0x14);

        a0->v5e_b[1] = (uint8_t)(random_next() & 0x3f);
        if (a0->type != 0xc8)
            x = (int16_t)(x - 0x24);
        spawn_enemy_shot((uint16_t)x, (uint16_t)a0->y, 4, level4_blob.enemy_a_shot_steps, GPTR(shot_picture(level4_blob.enemy_a_shot_picture)));
    }
    bbox_from_frame(a0);
}

/* level4 $05541E: the part of the gun: when its animation ends it becomes an explosion; two of its pictures drop an enemy that flies */
void l4_shot_a(Object *a0)
{
    int16_t d0, d1;
    const uint8_t *anim, *cells;
    uint16_t kind;
    Object *a1;
    uint16_t *e;
    int i;

    a0->y = (int16_t)(a0->y + scroll_speed);
    anim_step(a0);
    if (a0->anim_delay == 0) {
        const uint8_t *a5 = (const uint8_t *)a0->gfx;

        d0 = (int16_t)(a0->x - (int16_t)rd16(a5));
        d1 = (int16_t)(a0->y - (int16_t)rd16(a5 + 2));
        d0 = (int16_t)(d0 * 2 + (int16_t)rd16(a5 + 4));
        d1 = (int16_t)(d1 * 2 + (int16_t)rd16(a5 + 6));
        a0->type = 0xc;
        a0->fn_update = (obj_fn)animate_only;
        a0->x = (int16_t)(d0 >> 1);
        a0->y = (int16_t)(d1 >> 1);
        anim_header_set(a0, anim_explosion);
        return;
    }
    if (a0->gfx == (void *)level4_blob.shot_a_picture) {
        d0 = 0x80;
        d1 = -10;
        cells = level4_blob.shot_a_cells_2;
    } else if (a0->gfx == (void *)level4_blob.shot_a_picture_3) {
        d0 = 0;
        d1 = 10;
        cells = level4_blob.shot_a_cells_4;
    } else {
        return;
    }
    a1 = alloc_in(LIST_C);
    a1->type = 0xf8;
    a1->fn_update = FN(l4_enemy_f8);
    a1->fn_draw = draw_bob_masked;
    a1->fn_hit = (void *)hit_enemy;
    L4_ANGLE_HI(a1) = 0;
    L4_ANGLE_LO(a1) = d0;
    a1->turn[0] = a1->turn[1] = a1->turn[2] = 0;
    a1->x = (int16_t)(a0->x + d1);
    a1->y = a0->y;
    PATH_WAIT(a1) = 0;
    a1->score = 0x64;
    a1->energy = (int16_t)level4_blob.shot_a_energy;
    a1->big = 0;
    a1->kind = (int16_t)pickup_cycle;
    pickup_cycle++;
    e = pickup_groups;
    for (i = 0; i < 8; i++, e += 2)
        if (e[0] == 0) {
            e[0] = (uint16_t)a1->kind;
            e[1] = 1;
            break;
        }
    a1->hit[0] = 0x3e8;
    a1->hit[2] = 0x3e8;
    kind = (uint16_t)((uint16_t)d0 >> 3);
    anim = level4_blob.anims_by_kind[kind / 4];
    set_anim(a1, anim);
    a1->script = (uint8_t *)cells;
    a1->cursor = cells;
    a1->steps = (int16_t)level4_blob.shot_a_steps;
    SET_V5E(a1, 0);
}

/* level4 $055724: the flying enemy: it follows its script and shows the picture of its direction */
void l4_enemy_f8(Object *a0)
{
    uint16_t old = (uint16_t)L4_ANGLE_LO(a0), d0, d1;

    enemy_update(a0);
    if (a0->type != 0xf8) {
        const uint8_t *table;

        a0->type = 0xf8;
        a0->fn_update = FN(l4_enemy_f8);
        a0->fn_hit = (void *)hit_enemy;
        a0->fn_draw = draw_bob_masked;
        d0 = (uint16_t)(random_next() & 8);
        if (a0->x >= 0xa0)
            d0 = (uint16_t)(d0 + 4);
        table = level4_blob.enemy_f8_paths[d0 / 4];
        a0->script = (uint8_t *)table;
        a0->cursor = table;
        PATH_WAIT(a0) = 0;
    }
    d0 = (uint16_t)((((uint16_t)L4_ANGLE_LO(a0) + 0x10) >> 3) & 0x1c);
    d1 = (uint16_t)(((old + 0x10) >> 3) & 0x1c);
    if (d0 == d1)
        return;
    set_anim(a0, level4_blob.anims_by_kind[d0 / 4]);
    bbox_from_frame(a0);
}

/* level4 $0557CC: a gun that fires rings of shots */
void l4_enemy_b(Object *a0)
{
    anim_step(a0);
    a0->y = (int16_t)(a0->y + scroll_speed + level4_blob.enemy_b_fall);
    if (a0->y >= 0xc8) {
        l4_kill(a0);
        return;
    }
    if (add_carry(&a0->v5e_b[0], level4_blob.enemy_b_fire_rate)) {
        int16_t x = (int16_t)(a0->x + 0x1a), y = (int16_t)(a0->y + 0x1a);
        uint16_t angle = 0xa;
        const uint8_t *gfx = level4_blob.enemy_b_picture;

        a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
        if (a0->type != 0xd8) {
            angle = 0xd;
            x = (int16_t)(x - 0x34);
            gfx = level4_blob.enemy_b_picture_2;
        }
        spawn_enemy_shot((uint16_t)x, (uint16_t)y, angle, level4_blob.enemy_b_shot_steps, gfx);
    }
    bbox_from_frame(a0);
}

/* ---- the first boss ($55F80): nineteen objects, the first the core ---- */

/* level4 $055F80: the core, two guns and a chain of sixteen pieces by the table at $55880 */
void l4_spawn_boss1(void)
{
    const struct Level4Boss1Piece *a1 = level4_blob.boss1_pieces;
    Object *first = 0;
    int n;

    if (B(0xd98) != 0)
        return;
    sh_w8(0xd98, 0xff);
    for (n = 0; n < 19; n++) {
        Object *a0 = insert_head(LIST_C);

        if (!first)
            first = a0;
        a0->type = 0x50;
        a0->fn_update = (obj_fn)a1->update;
        a0->fn_hit = (void *)a1->hit;
        a0->fn_draw = (obj_fn)a1->draw;
        a0->x = (int16_t)a1->x;
        a0->persists = -1;
        a0->y = (int16_t)(a1->y - scroll_pos);
        L4_SIZE(a0) = 0;
        L4_ANGLE_HI(a0) = 0;
        L4_ANGLE_LO(a0) = 0;
        a0->turn[0] = a0->turn[1] = a0->turn[2] = 0;
        a0->steps = 3;
        a0->energy = (int16_t)a1->energy;
        a0->big = -1;
        a0->v2e = 0;
        a0->hit[0] = 0x3e8;
        a0->hit[2] = 0x3e8;
        a0->target = first;
        a1++;
    }
    sh_w16(0xd6e, 2);
}

/* the box of a gun or its remains: six pixels in, twenty wide */
static void gun_box(Object *a0)
{
    a0->hit[0] = (int16_t)(a0->x + 6);
    a0->hit[2] = (int16_t)(a0->hit[0] + 0x14);
    a0->hit[1] = (int16_t)(a0->y + 6);
    a0->hit[3] = (int16_t)(a0->hit[1] + 0x14);
}

/* level4 $055C74: the core: it keeps the screen from scrolling past it and now and then it fires (sfx_cmd2 = the shake) */
void l4_b1_core(Object *a0)
{
    uint16_t d0;

    if ((int16_t)scroll_max < 0x10)
        scroll_max = 0x10;
    d0 = (uint16_t)(scroll_speed + level4_blob.b1_core_sway[(frame_tick & 0xf)]);
    sh_w16(0xd70, d0);
    a0->y = (int16_t)(a0->y + (int16_t)d0);
    if (W(0xd6e) != 0) {
        a0->hit[0] = 0x3e8;
        a0->hit[2] = 0x3e8;
    } else {
        a0->hit[0] = 0x94;
        a0->hit[2] = 0xac;
        a0->hit[1] = (int16_t)(a0->y + 0x20);
        a0->hit[3] = (int16_t)(a0->hit[1] + 8);
    }
    if (CORE_TICK(a0) == 0) {
        if (!add_carry(&a0->v5e_b[0], level4_blob.b1_core_fire_rate))
            return;
        a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
        sfx_cmd2 = 0xf;
    }
    {
        uint8_t low = (uint8_t)((CORE_TICK(a0) & 0xff) + 1);

        CORE_TICK(a0) = (int16_t)((CORE_TICK(a0) & 0xff00) | low);
        if (low >= 0x80) {
            CORE_TICK(a0) = 0;
            return;
        }
        if (low == 0x78)
            sfx_cmd2 = 0xf;
    }
}

/* level4 $0559D6: the hit of the core: when its energy is gone the level's objects go and the bonuses fall */
void l4_b1_core_hit(Object *a0, Object *a1, uint16_t d0)
{
    uint16_t old = (uint16_t)a1->energy;

    a1->fn_draw = FN(l4_b1_core_flash);
    a1->energy = (int16_t)(old - d0);
    if (old > d0)
        return;
    free_object_chain(LIST_C->prev);
    spawn_bonuses(9);
    debris_burst(a0, 0x13, 0x70, a1->y, 0x60, 0x50, big_explosion_anim);
    boss_dead = 0xff;
}

/* level4 $055A1C: the hit of a gun */
void l4_b1_gun_hit(Object *a0, Object *a1, uint16_t d0)
{
    uint16_t old = (uint16_t)a1->energy;

    a1->fn_draw = FN(l4_b1_gun_flash);
    a1->energy = (int16_t)(old - d0);
    if (old > d0)
        return;
    sh_w16(0xd6e, (uint16_t)(W(0xd6e) - 1));
    spawn_effect((int16_t)(a1->x + 0x10), (int16_t)(a1->y - scroll_pos + 0x20), big_explosion_anim, draw_bob_masked);
    B1_GUN_DIR(a1) = -1;
    a1->fn_update = FN(l4_gun_dead);
    a1->fn_hit = (void *)obj_nop_thunk;
}

/* level4 $055D0E: the remains of a gun */
void l4_gun_dead(Object *a0)
{
    a0->y = (int16_t)(a0->y + W(0xd70));
    gun_box(a0);
}

/* level4 $055D3C: a gun: it turns to the player and fires in his direction */
void l4_b1_gun(Object *a0)
{
    uint16_t d0;
    const uint8_t *table = a0->x < 0xa0 ? level4_blob.b1_gun_dirs_left : level4_blob.b1_gun_dirs_right;

    a0->y = (int16_t)(a0->y + W(0xd70));
    gun_box(a0);
    d0 = aim_at_player(a0, 0x10, 0x10);
    B1_GUN_DIR(a0) = (int16_t)table[d0];
    if (add_carry(&a0->v5e_b[0], level4_blob.b1_gun_fire_rate)) {
        Object *s;
        uint16_t angle = (uint16_t)B1_GUN_DIR(a0);
        const uint8_t *anim = level4_blob.b1_gun_anims_by_angle[angle];

        a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
        s = spawn_enemy_shot((uint16_t)(a0->x + 0x10), (uint16_t)(a0->y + 0x10), angle, level4_blob.b1_gun_shot_steps, anim);
        set_anim(s, anim);
        s->x = (int16_t)(s->x + (int16_t)level4_blob.b1_gun_dx_by_dir[(uint16_t)B1_GUN_DIR(s)]);
        s->y = (int16_t)(s->y + (int16_t)level4_blob.b1_gun_dy_by_dir[(uint16_t)B1_GUN_DIR(s)]);
    }
}

/* the picture of a gun or the core by the cells of a table */
static void gun_cells(Object *a0, int flash)
{
    const uint8_t *const *t = a0->x < 0xa0 ? level4_blob.gun_cells_left : level4_blob.gun_cells_right;
    const uint8_t *cells = t[(int16_t)B1_GUN_DIR(a0)];

    if (flash)
        draw_tile_grid_flash(cells, 1, 1, a0->y, (uint16_t)((uint16_t)a0->x >> 1));
    else
        draw_tile_grid(cells, 1, 1, a0->y, (uint16_t)((uint16_t)a0->x >> 1));
}

/* level4 $055B30: a gun's flash */
void l4_b1_gun_flash(Object *a0)
{
    a0->fn_draw = FN(l4_b1_gun_draw);
    gun_cells(a0, 1);
}

/* level4 $055B6A: a gun's picture */
void l4_b1_gun_draw(Object *a0)
{
    gun_cells(a0, 0);
}

static void core_cells(Object *a0, int flash)
{
    uint16_t rows = 4;
    const uint8_t *cells = level4_blob.core_cells;

    if (CORE_TICK(a0) != 0) {
        rows = 5;
        cells = level4_blob.core_cells_tall;
    }
    if (flash)
        draw_tile_grid_flash(cells, 5, rows, a0->y, 0x38);
    else
        draw_tile_grid(cells, 5, rows, a0->y, 0x38);
}

/* level4 $055A68: the core's picture */
void l4_b1_core_draw(Object *a0)
{
    core_cells(a0, 0);
}

/* level4 $055A86: the core's flash */
void l4_b1_core_flash(Object *a0)
{
    a0->fn_draw = FN(l4_b1_core_draw);
    core_cells(a0, 1);
}

/* level4 $055C34: the picture of a piece of the chain while it has its size */
void l4_b1_piece_draw(Object *a0)
{
    if (a0->steps != 0)
        draw_bob_masked(a0);
}

/* the chain of the pieces: the head of the wave: its direction and speed follow the scroll position (the $567EE of the second boss) */
static void l4_pose(Object *a0)
{
    uint16_t d0 = (uint16_t)(frame_tick * 4) & 0x7f;
    int32_t l;

    if (d0 & 0x40)
        d0 = (uint16_t)(0x7f - d0);
    L4_ANGLE_LO(a0) = (int16_t)(d0 + 0xa0);
    d0 = (uint16_t)((frame_tick * 2 + 0x50) & 0xff);
    if (d0 & 0x80)
        d0 = (uint16_t)((d0 & 0xff00) | (~d0 & 0xff));
    l = (int16_t)(d0 - 0x40);
    put4e(a0, (uint32_t)l << 8);
    d0 = (uint16_t)((frame_tick + 0xa0) & 0x1ff);
    if (d0 & 0x100)
        d0 ^= 0x1ff;
    a0->turn[2] = (uint16_t)((d0 - 0x80) << 2);
    bbox_from_frame(a0);
}

/* level4 $055ED8: the head of the chain: it wiggles by the scroll, and the pieces' size follows the core's */
void l4_b1_head(Object *a0)
{
    int16_t d0;

    a0->gfx = (void *)level4_blob.b1_head_picture;
    a0->anim_delay = 0;
    a0->y = (int16_t)(a0->y + W(0xd70));
    l4_pose(a0);
    L4_ANGLE_LO(a0) = (int16_t)(L4_ANGLE_LO(a0) ^ 0x80);
    d0 = L4_SIZE(a0->target);
    if (d0 > 8) {
        d0 = (int16_t)(0x80 - d0);
        if (d0 > 8)
            d0 = 8;
    }
    a0->steps = d0;
    if (d0 == 0) {
        a0->hit[0] = 0x3e8;
        a0->hit[2] = 0x3e8;
    }
}

static void follow_chain(Object *a0);

static void l4_b1_piece(Object *a0, const uint8_t *gfx)
{
    a0->gfx = (void *)gfx;
    a0->anim_delay = 0;
    a0->y = (int16_t)(a0->y + W(0xd70));
    follow_chain(a0);
    if (a0->steps == 0) {
        a0->hit[0] = 0x3e8;
        a0->hit[2] = 0x3e8;
    }
}

/* level4 $055F26: a piece of the chain */
void l4_b1_body(Object *a0)
{
    l4_b1_piece(a0, level4_blob.b1_head_picture);
}

/* level4 $055F52: the last piece */
void l4_b1_tail(Object *a0)
{
    l4_b1_piece(a0, level4_blob.b1_tail_picture);
}

/* ---- the second boss ($56976): twenty objects, a worm with a head, a gun and a ring of eyes ---- */

/* level4 $056976: the pieces by the table at $5613A (the latch of the weapons first) */
void l4_spawn_boss2(const uint8_t *a1_record, uint16_t d2)
{
    const struct Level4Boss2Piece *a1 = level4_blob.boss2_pieces;
    int n;

    (void)a1_record;
    if (B(0xd97) != 0)
        return;
    sh_w8(0xd97, 0xff);
    scroll_y = 0x9b0;
    player_x_saved = 0xa0;
    player_y_saved = 0xb0;
    latch_slots(d2);
    for (n = 0; n < 20; n++) {
        Object *a0 = insert_head(LIST_C);

        a0->type = 0x54;
        a0->fn_update = (obj_fn)a1->update;
        a0->fn_draw = draw_bob_masked;
        a0->fn_hit = (void *)a1->hit;
        set_anim(a0, a1->anim);
        a0->persists = -1;
        a0->x = (int16_t)a1->x;
        a0->y = (int16_t)(a1->y - scroll_pos);
        L4_SIZE(a0) = 0;
        L4_ANGLE_HI(a0) = 0;
        L4_ANGLE_LO(a0) = (int16_t)a1->angle;
        a0->turn[0] = a0->turn[1] = a0->turn[2] = 0;
        a0->steps = 3;
        a0->energy = a1->energy;
        a0->big = (int8_t)a1->big;
        a0->v2e = 0;
        bbox_from_frame(a0);
        a1++;
    }
    sh_w16(0xd6c, 5);
}

/* level4 $0564FE: the piece takes the place, angle, speed and size of the one before it and moves by the script */
static void follow_chain(Object *a0)
{
    Object *a1 = a0->next;

    a0->x = a1->x;
    a0->x_frac = a1->x_frac;
    a0->y = a1->y;
    a0->y_frac = a1->y_frac;
    L4_ANGLE_HI(a0) = L4_ANGLE_HI(a1);
    L4_ANGLE_LO(a0) = L4_ANGLE_LO(a1);
    a0->turn[0] = a1->turn[0];
    a0->turn[1] = a1->turn[1];
    a0->turn[2] = a1->turn[2];
    a0->steps = a1->steps;
    L4_SIZE(a0) = 0x64;
    obj_script_run(a0);
}

/* the head's picture by its angle (four), and its box */
static void head_picture(Object *a0)
{
    a0->gfx = (void *)level4_blob.head_pictures[(((uint16_t)HEAD_ANGLE(a0) + 0x10) >> 5) & 3];
    bbox_from_frame(a0);
}

/* the head's use of the scratch fields of Object */
#define HEAD_BEND(o) ((o)->v28)

/* level4 $056410: the head: it turns to the player, now and then it darts at him; v28 is the length of its bend */
void l4_b2_head(Object *a0)
{
    uint16_t d0;

    a0->y = (int16_t)(a0->y + scroll_speed);
    if (HEAD_BEND(a0) == 0) {
        d0 = aim_at_player(a0, 0, 0);
        if (d0 < 2)
            d0 = 2;
        if (d0 > 6)
            d0 = 6;
        d0 = (uint16_t)(d0 << 5);
        d0 = (uint16_t)((d0 & 0xff00) | ((d0 - 0x40) & 0xff));
        L4_ANGLE_HI(a0) = 0;
        HEAD_ANGLE(a0) = (int16_t)d0;
        a0->turn[0] = a0->turn[1] = a0->turn[2] = 0;
        if ((uint8_t)random_next() >= level4_blob.b2_head_chance) {
            head_picture(a0);
            return;
        }
        HEAD_DART(a0) = 2;
        d0 = aim_at_player(a0, 0, 0);
        if (d0 < 2)
            d0 = 2;
        if (d0 > 6)
            d0 = 6;
        d0 = (uint16_t)(d0 << 5);
        d0 = (uint16_t)((d0 & 0xff00) | ((d0 - 0x60) & 0xff));
        L4_ANGLE_HI(a0) = 0;
        HEAD_ANGLE(a0) = (int16_t)d0;
        {
            uint32_t r = ((random_next() & 0x1f) + 0x20) << 10;

            put4e(a0, r);
            a0->turn[2] = 0;
        }
        if (HEAD_ANGLE(a0) < 0x20 || !(HEAD_ANGLE(a0) < 0xa0)) {
            HEAD_ANGLE(a0) = (int16_t)((HEAD_ANGLE(a0) & 0xff00) | ((HEAD_ANGLE(a0) + 0x40) & 0xff));
            put4e(a0, (uint32_t)(0 - get4e(a0)));
        }
    }
    HEAD_BEND(a0) = (int16_t)(HEAD_BEND(a0) + HEAD_DART(a0));
    if (HEAD_BEND(a0) >= 0x18)
        HEAD_DART(a0) = (int16_t)-HEAD_DART(a0);
    a0->steps = (int16_t)(HEAD_BEND(a0) + 4);
    head_picture(a0);
}

/* level4 $056530: the pieces behind the head */
void l4_b2_follow(Object *a0)
{
    a0->y = (int16_t)(a0->y + scroll_speed);
    follow_chain(a0);
    head_picture(a0);
}

/* level4 $056586: the gun: the screen stops at it; it turns to the player */
void l4_b2_gun(Object *a0)
{
    uint16_t d0;

    if ((int16_t)scroll_max < 0x9b0)
        scroll_max = 0x9b0;
    a0->y = (int16_t)(a0->y + scroll_speed);
    follow_chain(a0);
    d0 = aim_at_player(a0, 0, 0);
    if (d0 < 2)
        d0 = 2;
    if (d0 > 6)
        d0 = 6;
    AIM_DIR(a0) = (int16_t)d0;
    a0->gfx = (void *)level4_blob.b2_gun_pictures[d0];
    bbox_from_frame(a0);
}

/* level4 $0565FA: the gun's barrel: it sits on the gun, shows its direction and fires */
void l4_b2_barrel(Object *a0)
{
    Object *a1;

    anim_step(a0);
    a1 = a0->next;
    a0->x = a1->x;
    a0->y = a1->y;
    if (AIM_DIR(a1) != AIM_DIR(a0)) {
        AIM_DIR(a0) = AIM_DIR(a1);
        set_anim(a0, level4_blob.b2_barrel_anims_by_dir[(uint16_t)AIM_DIR(a0)]);
    }
    if (add_carry(&a0->v5e_b[0], level4_blob.b2_barrel_fire_rate)) {
        uint16_t d2 = (uint16_t)AIM_DIR(a0);
        Object *s;

        a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
        s = spawn_enemy_shot((uint16_t)(a0->x + (int16_t)level4_blob.b2_muzzle_dx[d2]), (uint16_t)(a0->y + (int16_t)level4_blob.b2_muzzle_dy[d2]), d2, level4_blob.b2_barrel_shot_steps,
                             (void *)level4_blob.b2_shot_anim);
        set_anim(s, level4_blob.b2_shot_anim);
    }
    bbox_from_frame(a0);
}

/* level4 $0567E0: the first of the tail: its angle and sway follow the scroll */
void l4_b2_wave(Object *a0)
{
    a0->y = (int16_t)(a0->y + scroll_speed);
    a0->steps = 0xc;
    l4_pose(a0);
}

/* level4 $056854: a piece of the tail */
void l4_b2_link(Object *a0)
{
    a0->y = (int16_t)(a0->y + scroll_speed);
    follow_chain(a0);
    bbox_from_frame(a0);
}

/* level4 $056864: the last piece of the tail: it fires rings of eight */
void l4_b2_ring(Object *a0)
{
    a0->y = (int16_t)(a0->y + scroll_speed);
    anim_step(a0);
    follow_chain(a0);
    if (add_carry(&a0->v5e_b[0], level4_blob.b2_ring_fire_rate)) {
        int d7;

        a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
        for (d7 = 7; d7 >= 0; d7--) {
            Object *s = spawn_enemy_shot((uint16_t)a0->x, (uint16_t)a0->y, (uint16_t)d7, level4_blob.b2_ring_shot_steps, GPTR(shot_picture(level4_blob.b2_ring_shot_picture)));

            set_anim(s, level4_blob.b2_shot_anim);
        }
    }
    bbox_from_frame(a0);
}

/* level4 $0568F2: an eye: it looks at the player and fires */
void l4_b2_eye(Object *a0)
{
    uint16_t d0;

    a0->y = (int16_t)(a0->y + scroll_speed);
    anim_step(a0);
    d0 = aim_at_player(a0, 0, 0);
    if (d0 != (uint16_t)AIM_DIR(a0)) {
        AIM_DIR(a0) = (int16_t)d0;
        set_anim(a0, level4_blob.b2_eye_anims[d0]);
    }
    if (add_carry(&a0->v5e_b[0], level4_blob.b2_eye_fire_rate)) {
        a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
        spawn_enemy_shot((uint16_t)a0->x, (uint16_t)a0->y, (uint16_t)AIM_DIR(a0), level4_blob.b2_eye_shot_steps, (void *)level4_blob.b2_eye_shot_picture);
    }
    bbox_from_frame(a0);
    a0->hit[1] = (int16_t)(a0->hit[1] - 8);
    a0->hit[3] = (int16_t)(a0->hit[3] + 8);
}

/* level4 $056354: the hit of the gun (the barrel's, $056350, goes to the gun under it): when its energy is gone everything of the level goes,
 * the bonuses fall and the screen goes back to the first stretch */
void l4_b2_gun_hit(Object *a0, Object *a1, uint16_t d0)
{
    Object *a2, *a3;
    uint16_t old;

    if (W(0xd6c) != 0)
        return;
    a2 = a1->prev;
    a1->fn_draw = draw_bob_or_first;
    a2->fn_draw = draw_bob_or_first;
    old = (uint16_t)a1->energy;
    a1->energy = (int16_t)(old - d0);
    if (old > d0)
        return;
    for (a2 = LIST_C->prev; a2->type != 0; a2 = a3) {
        a3 = a2->prev;
        kill_thunk(a2);
    }
    debris_burst(a0, 0x27, 0, 0, 0x140, 0xc0, big_explosion_anim);
    spawn_bonuses(4);
    memset(level_init_src + 0x1658, 0, 0xa0 * 4);
    scroll_min = 0;
    player_score += 0x7d0;
    scroll_step_target = 1;
    scroll_pos = 0x8a0;
    scroll_max = 0x8a0;
    map_row_offset = 0x1590;
    scroll_fine = 0;
}

/* level4 $056350 */
void l4_b2_barrel_hit(Object *a0, Object *a1, uint16_t d0)
{
    l4_b2_gun_hit(a0, a1->next, d0);
}

/* level4 $0562CA: the hit of the last piece of the tail: when its energy is gone ten pieces burst */
void l4_b2_ring_hit(Object *a0, Object *a1, uint16_t d0)
{
    uint16_t old = (uint16_t)a1->energy;
    int n;

    a1->fn_draw = draw_bob_or_first;
    a1->energy = (int16_t)(old - d0);
    if (old > d0)
        return;
    sh_w16(0xd6c, (uint16_t)(W(0xd6c) - 1));
    scroll_step_target = 0xffff;
    for (n = 0; n < 10; n++) {
        Object *next = a1->next;

        a1->score = 0x64;
        enemy_killed(a0, a1);
        a1 = next;
    }
}

/* level4 $0562FE: the hit of an eye: shots at its top or bottom edge are shielded off */
void l4_b2_eye_hit(Object *a0, Object *a1, uint16_t d0)
{
    uint16_t old;

    if (box_overlap((int16_t)hit_box[0], (int16_t)hit_box[1], (int16_t)hit_box[2], (int16_t)hit_box[3], a1->hit[0], a1->hit[3], a1->hit[2], a1->hit[3]) ||
        box_overlap((int16_t)hit_box[0], (int16_t)hit_box[1], (int16_t)hit_box[2], (int16_t)hit_box[3], a1->hit[0], a1->hit[1], a1->hit[2], a1->hit[1])) {
        hit_kill_shooter(a0, a1, d0);
        return;
    }
    a1->fn_draw = draw_bob_or_first;
    old = (uint16_t)a1->energy;
    a1->energy = (int16_t)(old - d0);
    if (old > d0)
        return;
    sh_w16(0xd6c, (uint16_t)(W(0xd6c) - 1));
    a1->score = 0x12c;
    enemy_killed(a0, a1);
}

/* ---- the bars ($56A2C) ---- */

/* level4 $056A2C: a bar that swings out of the wall (the record's word at +$C says which side) */
void l4_spawn_bar(const uint8_t *a1)
{
    int right = rd16(a1 + 0xc) != 0;
    Object *a0 = spawn_marker_front(right ? 0x110 : 0x10c, a1, (void (*)(void))FN(l4_bar_update));

    a0->y = (int16_t)(a0->y - scroll_pos);
    a0->fn_draw = FN(l4_bar_draw);
    set_anim(a0, a0->type == 0x10c ? level4_blob.bar_anim_10c : level4_blob.bar_anim_other);
}

/* level4 $056ABC: the bar: it swings out and back; it hurts the player */
void l4_bar_update(Object *a0)
{
    int16_t d0, d1, d2;

    a0->y = (int16_t)(a0->y + scroll_speed);
    if (a0->y > 0x188) {
        kill_thunk(a0);
        return;
    }
    anim_step(a0);
    if (BAR_PHASE(a0) == 0) {
        BAR_STEP(a0) = 1;
        if (!add_carry(&a0->v5e_b[0], level4_blob.bar_fire_rate))
            goto box;
        a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
    }
    BAR_PHASE(a0) = (int16_t)(BAR_PHASE(a0) + BAR_STEP(a0));
    if (BAR_PHASE(a0) == 6)
        BAR_STEP(a0) = (int16_t)-BAR_STEP(a0);
box:
    if (a0->type == 0x10c) {
        d0 = a0->x;
        d2 = (int16_t)(((uint16_t)BAR_PHASE(a0) << 4) + d0 + 0x10);
    } else {
        d2 = (int16_t)(a0->x + 0x10);
        d0 = (int16_t)(d2 - ((uint16_t)BAR_PHASE(a0) << 4) - 0x10);
    }
    d1 = (int16_t)(a0->y + 8);
    a0->hit[0] = d0;
    a0->hit[1] = d1;
    a0->hit[2] = d2;
    a0->hit[3] = (int16_t)(d1 + 0x10);
    if (!player_hits(a0))
        return;
    if (shield_timer != 0)
        return;
    player_damage(a0, 6);
    PLAYER->fn_draw = (obj_fn)draw_bob_or_first;
}

/* level4 $056B8E: the bar: the tiles of its shaft and the picture of its end */
void l4_bar_draw(Object *a0)
{
    int16_t x, y;
    uint16_t d0 = (uint16_t)BAR_PHASE(a0);

    if (a0->type == 0x10c) {
        if (d0 != 0)
            draw_tile_grid(level4_blob.bar_cells + sizeof level4_blob.bar_cells - 2 * d0, (uint16_t)(d0 - 1), 1, a0->y, (uint16_t)((uint16_t)a0->x >> 1));
        x = (int16_t)((d0 << 4) + a0->x);
        y = (int16_t)(a0->y + 8);
    } else {
        if (d0 != 0)
            draw_tile_grid(level4_blob.bar_cells + sizeof level4_blob.bar_cells - 2 * d0, (uint16_t)(d0 - 1), 1, a0->y,
                           (uint16_t)((uint16_t)((a0->x - (int16_t)(d0 << 4)) + 0x10) >> 1));
        x = (int16_t)(a0->x - (int16_t)(d0 << 4) + 0xf);
        y = (int16_t)(a0->y + 8);
    }
    draw_bob_masked_at((uint16_t)x, (uint16_t)y, (const uint8_t *)a0->gfx);
}

/* ---- the walker ($56C3E): it stands, then walks to the other side along the screen ---- */

/* level4 $056C3E */
void l4_spawn_walker(const uint8_t *a1)
{
    int right = rd16(a1 + 0xc) != 0;
    Object *a0 = spawn_marker_enemy(right ? 0xe4 : 0xe0, a1, (void (*)(void))FN(l4_walker_update));

    a0->y = (int16_t)(a0->y + 0x1c);
    if (a0->type == 0xe0)
        a0->x = (int16_t)(a0->x + 0x10);
    a0->fn_draw = FN(l4_walker_draw);
    a0->fn_hit = FN(l4_walker_hit);
    a0->energy = (int16_t)level4_blob.walker_energy;
    a0->big = -1;
    a0->hit[0] = 0x3e8;
    a0->hit[2] = 0x3e8;
    a0->score = 0xc8;
    WALKER_SIDE(a0) = (int16_t)rd16(a1 + 6);
    set_anim(a0, a0->type == 0xe0 ? level4_blob.walker_anim_e0 : level4_blob.walker_anim_other);
    a0->kind = 0;
    SET_V5E(a0, level4_blob.walker_v5e);
    WALKER_HOME_X(a0) = a0->x;
    WALKER_HOME_Y(a0) = a0->y;
}

/* the walker goes back to where it came from */
static void walker_reset(Object *a0)
{
    a0->x = WALKER_HOME_X(a0);
    a0->y = WALKER_HOME_Y(a0);
    WALKER_STATE(a0) = 0;
    a0->hit[0] = 0x3e8;
    a0->hit[2] = 0x3e8;
    set_anim(a0, a0->type == 0xe0 ? level4_blob.walker_anim_e0 : level4_blob.walker_anim_other);
    a0->energy = (int16_t)level4_blob.walker_energy;
}

/* level4 $056D1C */
void l4_walker_update(Object *a0)
{
    if ((int16_t)(scroll_max + 0xd0) < a0->y) {
        kill_thunk(a0);
        return;
    }
    anim_step(a0);
    if (WALKER_STATE(a0) == 0) {
        if ((uint8_t)random_next() < level4_blob.walker_chance) {
            WALKER_STATE(a0) = 1;
            a0->anim_delay = 1;
        }
        return;
    }
    if (a0->type == 0xe0)
        a0->x = (int16_t)(a0->x + level4_blob.walker_step_x);
    else
        a0->x = (int16_t)(a0->x - level4_blob.walker_step_x);
    if (WALKER_STATE(a0) == 1) {
        if (a0->anim_delay == 0) {
            WALKER_STATE(a0) = 2;
            WALKER_DY(a0) = (int16_t)(PLAYER->y - a0->y + scroll_pos);
        }
        goto box;
    }
    if (a0->x >= 0x28 && a0->x <= 0x118) {
        if (WALKER_DY(a0) >= 0)
            a0->y = (int16_t)(a0->y + level4_blob.walker_step_x);
        else
            a0->y = (int16_t)(a0->y - level4_blob.walker_step_x);
    }
    if (a0->x >= -0x20 && a0->x < 0x160)
        goto box;
    walker_reset(a0);
    return;
box:
    bbox_from_frame(a0);
    a0->hit[1] = (int16_t)(a0->hit[1] - scroll_pos);
    a0->hit[3] = (int16_t)(a0->hit[3] - scroll_pos);
}

/* level4 $056E84: the hit of the walker: when it is shot to pieces it bursts, drops a pickup and goes back */
void l4_walker_hit(Object *a0, Object *a1, uint16_t d0)
{
    uint16_t old = (uint16_t)a1->energy;
    const uint8_t *a5;
    int16_t x, y;
    Object *p;

    a1->energy = (int16_t)(old - d0);
    if (old > d0) {
        a1->fn_draw = FN(l4_walker_flash);
        return;
    }
    a5 = (const uint8_t *)a1->gfx;
    x = (int16_t)(((int16_t)(a1->x - (int16_t)rd16(a5)) * 2 + (int16_t)rd16(a5 + 4)) >> 1);
    y = (int16_t)(((int16_t)(a1->y - (int16_t)rd16(a5 + 2)) * 2 + (int16_t)rd16(a5 + 6)) >> 1);
    y = (int16_t)(y - scroll_pos);
    spawn_effect(x, y, big_explosion_anim, draw_bob_masked);
    player_score += (uint16_t)a1->score;
    p = alloc_in(LIST_B);
    p->type = 0x18;
    PICKUP_MODE(p) = 7;
    p->fn_update = (obj_fn)pickup_update;
    p->fn_draw = draw_bob_masked;
    anim_header_set(p, anim_bonus_big);
    p->x = a1->x;
    p->y = (int16_t)(a1->y - scroll_pos);
    PICKUP_DIR(p) = (int16_t)random_next();
    walker_reset(a1);
}

/* the tiles of a walker's legs, by its side v30 */
static void walker_tiles(Object *a0)
{
    uint16_t d2 = (uint16_t)(WALKER_SIDE(a0) * 2);

    if (WALKER_STATE(a0) >= 2)
        return;
    draw_tile_grid(a0->type == 0xe0 ? level4_blob.walker_grid_e0 : level4_blob.walker_grid_e4, 0, 2, (int16_t)(level4_blob.walker_grid_y[d2 / 2] - scroll_pos), level4_blob.walker_grid_x[d2 / 2]);
}

/* level4 $056F3E: the walker's flash */
void l4_walker_flash(Object *a0)
{
    int16_t y = (int16_t)(a0->y - scroll_pos);

    a0->fn_draw = FN(l4_walker_draw);
    if (y > 0xdc)
        return;
    draw_bob_or_at((uint16_t)a0->x, (uint16_t)y, (const uint8_t *)a0->gfx);
    walker_tiles(a0);
}

/* level4 $056F6A: the walker's picture */
void l4_walker_draw(Object *a0)
{
    int16_t y = (int16_t)(a0->y - scroll_pos);

    if (y > 0xdc)
        return;
    draw_bob_masked_at((uint16_t)a0->x, (uint16_t)y, (const uint8_t *)a0->gfx);
    walker_tiles(a0);
}
