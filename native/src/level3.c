/* level3: level 3's own code (the blob at $54E00 while level 3 is loaded), written like level2.c: raw big-endian memory through sh_r16 and the like;
 * a function has `level3 $ADDR` for its address. */
#include "weapons.h"
#include "shop.h"
#include "game_all.h"

/* level 3: its parameter block at E42 (energies, fire rates, shot steps), by the object that reads them */
#define L3_WORM_SPAWN_STEPS 0x54e42
#define L3_WALKER_ENERGY 0x54e44
#define L3_PARTS_STEPS 0x54e46
#define L3_SNAKE_ENERGY 0x54e48
#define L3_DIVER_ENERGY 0x54e4a
#define L3_BOSS_ENERGY 0x54e4c
#define L3_WALKER_FIRE_RATE 0x54e4f
#define L3_WALKER_PARAM 0x54e50
#define L3_WALKER_SHOT_STEPS 0x54e52
#define L3_DIVER_STEPS 0x54e54
#define L3_BOSS_CHANCE 0x54e57
#define L3_BOSS_CHANCE_2 0x54e59
#define L3_BOSS_FIRE_RATE 0x54e5b
#define L3_BOSS_PARAM 0x54e5c
#define L3_GUN_ENERGY 0x54e5e
#define L3_BOSS_ENERGY_2 0x54e60
#define L3_BOSS2_FIRE_RATE 0x54e63
#define L3_BOSS2_SHOT_STEPS 0x54e64
#define L3_GUN_FIRE_RATE 0x54e67
#define L3_GUN_SHOT_STEPS 0x54e68
#define L3_INIT_PATCH 0x54e6a
#define L3_INIT_PATCH_2 0x54e6c
#define L3_WORM_HEAD_FIRE_RATE 0x54e6f
#define L3_WORM_HEAD_SHOT_STEPS 0x54e70
#define L3_PART_CORE_FIRE_RATE 0x54e75
#define L3_PART_CORE_SHOT_STEPS 0x54e76
#define L3_PART_GUN_FIRE_RATE 0x54e79
#define L3_EVENT_PARAM 0x54e7a
#define PART_SLOT(o) ((o)->v30)
#define TILE_COLS(o) ((o)->v3e)
#define TILE_ROWS(o) ((o)->v40)
#define PATH_WAIT(o) ((o)->v28)
#define ARM_SWING(o) ((o)->v3e)
#define WORM_DELAY(o) ((o)->v28)
#define WALKER_DIR(o) ((o)->v2a)

/* the scratch fields of Object, by the kind that uses them */


/* the path script view of an object (ObjMove) */
#define PATH_ANGLE_HI(o) ((o)->v2a)
#define PATH_ANGLE_LO(o) ((o)->v2c)
#define SNAKE_X(o) ((o)->v2e)
#define ARM_SWING_STEP(o) ((o)->v2e)

/* the scratch fields of Object, by the kind that uses them */
#define SNAKE_STEP(o) ((o)->v2a)

#define ARM_ANGLE_HI(o) ((o)->v2a)
#define ARM_ANGLE_LO(o) ((o)->v2c)

/* the scratch fields of Object, by the kind that uses them */


/* the scratch fields of Object, by the kind that uses them */
#define WALKER_STATE(o) ((o)->v28)


/* the scratch fields of Object, by the kind that uses them */
#define DIVER_SIDE(o) ((o)->v2a)
#define L3_PICTURE_PHASE(o) ((o)->v2c)
#define WORM_PHASE(o) ((o)->v2c)
#define WORM_SIDE(o) ((o)->v30)


#define FN(f) ((obj_fn)(f))
#define W(a) sh_r16(a)
#define B(a) sh_r8(a)
#define PLAYER_X (PLAYER->x)
#define PLAYER_Y (PLAYER->y)

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

static void set_anim(Object *o, uint32_t address)
{
    anim_header_set(o, (const uint8_t *)GPTR(address));
}

void l3_spawn_snake(const uint8_t *a1);
void l3_spawn_diver(const uint8_t *a1);
void l3_worm_spawn(uint16_t d6);
void l3_worm_hit(Object *a0, Object *a1, uint16_t d0);
void l3_end_draw(Object *a0);
void l3_spawn_gun(const uint8_t *a1);
void l3_spawn_boss(const uint8_t *a1);
void l3_spawn_walker(const uint8_t *a1);


/* ---- the entries the engine calls ($54E0C..$54E2C) ---- */

/* level3 $054E10, $056EFA: level events by d0 (a1 = the record that fired): 0 the wave of $326C, 1 to 5 formations */
void l3_event(uint16_t d0, const uint8_t *a1)
{
    uint16_t saved;

    switch (d0) {
    case 0:
        spawn_wave_326c(0, a1);   /* $E5A */
        break;
    case 1:
        spawn_formation(0, a1, (const uint8_t *)GPTR(0x54e7e));
        break;
    case 2:
        spawn_formation(0, a1, (const uint8_t *)GPTR(0x54e98));
        break;
    case 3:
        saved = difficulty_x2;
        difficulty_x2 = W(L3_EVENT_PARAM);
        spawn_formation(0, a1, (const uint8_t *)GPTR(0x54eb2));
        difficulty_x2 = saved;
        break;
    case 4:
        spawn_formation(0, a1, (const uint8_t *)GPTR(0x54f5c));
        break;
    case 5:
        spawn_formation(0, a1, (const uint8_t *)GPTR(0x54f76));
        break;
    }
}

/* level3 $054E18, $0556D0: level hooks by d0 (a1 = the record that fired, d2 its scroll value; w0c of the record says which side): 0 remembers
 * the record, 1 the snake, 2 the diver; 3 to 6 are not written */
void l3_hook(uint16_t d0, const uint8_t *a1, uint16_t d2)
{
    switch (d0) {
    case 0:
        note_spawn_record(a1, d2);   /* $EA2 */
        break;
    case 1:
        l3_spawn_snake(a1);
        break;
    case 2:
        l3_spawn_diver(a1);
        break;
    case 3:
        l3_spawn_parts();
        break;
    case 4:
        l3_spawn_gun(a1);
        break;
    case 5:
        l3_spawn_boss(a1);
        break;
    case 6:
        l3_spawn_walker(a1);
        break;
    }
}

/* level3 $054E24, $0551DE: the scroll: at the end ($AF0) the scroll's speed is stopped; above $D0 the limit is $A30 at least; below it, $20, the worm's
 * spawner runs and when its flags say so the bonuses come once */
void l3_scroll(void)
{
    if (scroll_min == 0xaf0 && scroll_pos == 0xaf0) {
        scroll_max = 0xaf0;
        scroll_pos_update(scroll_step);
        stars_scroll(scroll_step);
        scroll_step = 0;
        if (B(0xd96) != 0)
            sh_w8(0xd96, 0);
        else
            scroll_min = 0;
    }
    if ((int16_t)scroll_pos <= 0xd0) {
        if ((int16_t)scroll_max < 0x20)
            scroll_max = 0x20;
        if (B(0xd92) == 0 && B(0xd94) == 0)
            l3_worm_spawn(0);
        sh_w8(0xd94, 0);
        sh_w8(0xd95, 0);
        if (B(0xd92) != 0 && B(0xd93) != 0 && boss_dead == 0) {
            spawn_bonuses(9);
            boss_dead = 0xff;
        }
    } else if ((int16_t)scroll_max < 0xa30) {
        scroll_max = 0xa30;
    }
}

/* level3 $054E20, $056FEE: level 3 starts: its map is copied once to $7D120 (backwards, as the original does), the scroll's limits are made, the
 * object that draws the pieces of the end is put first in the front list, and the state of the end is cleared */
void l3_init(void)
{
    Object *a0, *s, *old;
    int i;

    if (level_init_src == (uint8_t *)GPTR(0x74386)) {
        for (i = 11999; i >= 0; i--)
            ((uint8_t *)GPTR(0x7d120))[i] = ((const uint8_t *)GPTR(0x74386))[i];
        level_init_src = (uint8_t *)GPTR(0x7d120);
    }
    scroll_min = 0xaf0;
    scroll_max = 0x11ff;
    a0 = obj_free_pop();
    s = LIST_SENTINEL(LIST_FRONT);
    old = s->next;
    s->next = a0;
    a0->next = old;
    a0->prev = s;
    old->prev = a0;
    a0->type = 0x50;
    a0->fn_update = (obj_fn)obj_nop_thunk;
    a0->fn_draw = FN(l3_end_draw);
    a0->persists = -1;
    level_vars[16] = 0xe496;
    level_vars[17] = 0xaa8a;
    level_vars[18] = 0x5274;
    level_vars[19] = 0xb949;
    sh_w8(0xd92, 0);
    sh_w8(0xd93, 0xff);
    sh_w8(0xd94, 0);
    sh_w8(0xd95, 0);
    sh_w16(0xd64, 0);
    sh_w16(0xd66, W(L3_INIT_PATCH));
    sh_w16(0xd68, W(L3_INIT_PATCH));
    sh_w8(0xd96, 0);
    sh_w16(0xd6a, 2);
    sh_w16(0xd60, W(L3_INIT_PATCH_2));
    sh_w16(0xd62, W(L3_INIT_PATCH_2));
    difficulty_x1 = (uint16_t)(difficulty * 2);
    difficulty_x2 = (uint16_t)(difficulty * 3);
}

/* ---- what the level 3's records put into the field ---- */

/* level3 $056F60: draws the tile grids of the table at $56F9A (the pieces of the end) that are on the screen */
void l3_end_draw(Object *a0)
{
    uint32_t t = 0x56f9a;

    (void)a0;
    if (B(0xd94) == 0 && B(0xd95) == 0)
        return;
    for (; sh_r32(t) != 0; t += 12) {
        int16_t d2 = (int16_t)(W(t + 8) - scroll_pos);

        if (d2 >= 0xc0 || d2 <= -0x30)
            continue;
        draw_tile_grid((const uint8_t *)GPTR(sh_r32(t)), W(t + 4), W(t + 6), d2, W(t + 10));
    }
}

/* level3 $055310: the worm of the end: eleven objects of the table at $55270 (update, picture, the delay v28 of each behind the one before) along a path from
 * the table at $6EF36: the first path is the first, then they come by the seed of d6 (1 to 6); d6 is the side (v30) */
void l3_worm_spawn(uint16_t d6)
{
    uint16_t d0;
    uint32_t t = 0x55270, path;
    int16_t d2 = 0, x, y;
    int n, count;
    Object *head = 0, *prev = 0;

    if (W(0xd64) != 1) {
        d0 = W(0xd64);
        sh_w16(0xd64, (uint16_t)(d0 + 1));
    } else {
        uint16_t *seed = d6 == 0 ? &level_vars[16] : &level_vars[18];

        do {
            uint32_t r = ((uint32_t)seed[0] << 16) | seed[1];

            r = (r >> 5) | (r << 27);
            seed[0] = (uint16_t)(r >> 16);
            seed[1] = (uint16_t)r;
            d0 = (uint16_t)(r & 7);
        } while (d0 < 1 || d0 == 7);
    }
    path = sh_r32(0x6ef36 + 4 * (uint32_t)d0);
    x = (int16_t)W(path + 2);
    y = (int16_t)(W(path + 4) - scroll_pos - scroll_step);
    count = W(t) + 1;
    t += 2;
    for (n = 0; n < count; n++) {
        Object *a0 = alloc_in(LIST_C);

        if (prev)
            prev->chain = a0;
        else
            head = a0;
        a0->fn_hit = prev ? GFN(0xf2c) : (void *)l3_worm_hit;
        a0->type = 0x50;
        WORM_DELAY(a0) = d2;
        d2 = (int16_t)(d2 - (int16_t)W(t));
        a0->fn_update = (obj_fn)GFN(sh_r32(t + 2));
        a0->fn_draw = draw_bob_masked;
        a0->gfx = GPTR(sh_r32(t + 6));
        a0->score = 0x7d0;
        SET_V5E(a0, 0);
        a0->big = 0;
        a0->is_part = 0;
        a0->anim_delay = 0;
        a0->script = (uint8_t *)GPTR(path);
        a0->cursor = (const uint8_t *)GPTR(path);
        WORM_PHASE(a0) = 0xc0;
        a0->x = x;
        a0->y = y;
        a0->kind = 0;
        a0->steps = (int16_t)W(L3_WORM_SPAWN_STEPS);
        WORM_SIDE(a0) = (int16_t)d6;
        bbox_from_frame(a0);
        a0->target = head;
        a0->chain = 0;
        t += 10;
        prev = a0;
    }
}

static void l3_enemy(Object *a0, uint32_t table)
{
    enemy_update_no_anim(a0);
    a0->gfx = GPTR(sh_r32(table + ((((uint16_t)L3_PICTURE_PHASE(a0) + 0x10) >> 3) & 0x1c)));
    bbox_from_frame(a0);
}

/* level3 $055044: an enemy whose picture follows its phase v2c (eight pictures of a table; $05504E and $055058 are the same with another table) */
void l3_enemy_a(Object *a0)
{
    l3_enemy(a0, 0x55078);
}

/* level3 $05504E */
void l3_enemy_b(Object *a0)
{
    l3_enemy(a0, 0x55098);
}

/* level3 $055058 */
void l3_enemy_c(Object *a0)
{
    l3_enemy(a0, 0x550b8);
}

/* a snake's piece leaves the field at y $E8 */
static int l3_snake_off(Object *a0)
{
    a0->y = (int16_t)(a0->y + scroll_speed);
    if (a0->y >= 0xe8) {
        kill_thunk(a0);
        return 1;
    }
    return 0;
}

#define SNAKE_LIMIT(o) ((o)->v30)
#define SNAKE_SWING(o) ((o)->v28)

/* level3 $056490: the first piece of a snake: it waits (v30), looks for the player's height, then swings (v28 between 0 and v30) */
void l3_snake_head(Object *a0)
{
    if (l3_snake_off(a0))
        return;
    a0->fn_draw = (obj_fn)obj_nop_thunk;
    if (SNAKE_STEP(a0) == 0) {
        int16_t d0;

        if (SNAKE_LIMIT(a0) < 0) {
            SNAKE_LIMIT(a0)++;
            bbox_from_frame(a0);
            return;
        }
        d0 = (int16_t)(a0->y - PLAYER_Y);
        if (d0 < 0)
            d0 = (int16_t)-d0;
        if (d0 > 0x20) {
            bbox_from_frame(a0);
            return;
        }
        SNAKE_STEP(a0) = 2;
        SNAKE_LIMIT(a0) = (int16_t)((random_next() & 6) + 0xa);
    }
    a0->fn_draw = draw_bob_masked;
    SNAKE_SWING(a0) = (int16_t)(SNAKE_STEP(a0) + SNAKE_SWING(a0));
    if (SNAKE_SWING(a0) == 0) {
        SNAKE_STEP(a0) = 0;
        SNAKE_LIMIT(a0) = -5;
    } else if (SNAKE_SWING(a0) == SNAKE_LIMIT(a0)) {
        SNAKE_STEP(a0) = (int16_t)-SNAKE_STEP(a0);
    }
    bbox_from_frame(a0);
}

/* level3 $05653C: the last piece of a snake: it follows the head's swing and shows its second picture at the ends of it */
void l3_snake_tail(Object *a0)
{
    Object *a1;

    if (l3_snake_off(a0))
        return;
    anim_step(a0);
    a1 = a0->target;
    a0->x = (int16_t)((uint16_t)((uint16_t)SNAKE_LIMIT(a0) * (uint16_t)SNAKE_SWING(a1)) + (uint16_t)SNAKE_X(a0));
    if (SNAKE_STEP(a1) == 2 && SNAKE_SWING(a1) == 2) {
        a0->anim_delay = 1;
    } else if (SNAKE_LIMIT(a1) > 0 && SNAKE_LIMIT(a1) == SNAKE_SWING(a1)) {
        a0->anim_delay = 1;
    }
    bbox_from_frame(a0);
}

/* level3 $0565B2: a piece of a snake between the head and the tail: it follows the head's swing, drawn while the head swings */
void l3_snake_body(Object *a0)
{
    Object *a1;

    if (l3_snake_off(a0))
        return;
    a0->fn_draw = (obj_fn)obj_nop_thunk;
    a1 = a0->target;
    a0->x = (int16_t)((uint16_t)((uint16_t)SNAKE_LIMIT(a0) * (uint16_t)SNAKE_SWING(a1)) + (uint16_t)SNAKE_X(a0));
    if (SNAKE_STEP(a1) != 0)
        a0->fn_draw = draw_bob_masked;
    bbox_from_frame(a0);
}

/* level3 $055EEA: a snake of eight pieces ($C8, $CC when w0c of the record is set): the pieces are chained, each follows the first, and a pair of
 * markers ($B8 and $BC) holds the box of the group */
void l3_spawn_snake(const uint8_t *a1)
{
    Object *m = alloc_in(LIST_C), *a4, *a0, *b;
    int flag = rd16(a1 + 0xc) != 0;
    int16_t d0 = (int16_t)(rd16(a1 + 8) - 8), d1;
    int d7;

    if (flag)
        d0 = (int16_t)(d0 + 0x20);
    d1 = (int16_t)(rd16(a1 + 0xa) - scroll_pos);
    a0 = alloc_in(LIST_C);
    a4 = a0;
    for (d7 = 7; d7 >= 0; d7--) {
        int16_t d6;

        a0->type = flag ? 0xcc : 0xc8;
        a0->gfx = GPTR(flag ? 0x5b1d0 : 0x5b280);
        a0->anim_delay = 0;
        SET_V5E(a0, 0);
        if (d7 == 0)
            set_anim(a0, flag ? 0x560de : 0x560b4);
        a0->fn_update = FN(l3_snake_body);
        if (d7 == 0)
            a0->fn_update = FN(l3_snake_tail);
        if (d7 == 7)
            a0->fn_update = FN(l3_snake_head);
        a0->fn_draw = draw_bob_masked;
        a0->x = d0;
        SNAKE_X(a0) = d0;
        a0->y = d1;
        a0->fn_hit = (void *)hit_part;
        a0->is_part = 1;
        a0->energy = (int16_t)W(L3_SNAKE_ENERGY);
        a0->big = 0;
        a0->score = 0x190;
        SNAKE_SWING(a0) = 0;
        a0->kind = 0;
        SNAKE_STEP(a0) = 0;
        d6 = (int16_t)(7 - d7);
        if (flag)
            d6 = (int16_t)-d6;
        SNAKE_LIMIT(a0) = d6;
        bbox_from_frame(a0);
        a0->target = a4;
        a0->chain = 0;
        if (d7 != 0) {
            Object *a2 = a0, *n = obj_free_pop(), *prev = a2->prev;

            a2->prev = n;
            n->prev = prev;
            n->next = a2;
            prev->next = n;
            a2->chain = n;
            a0 = n;
        }
    }
    b = alloc_in(LIST_C);
    b->type = 0xb8;
    b->fn_update = (obj_fn)obj_nop_thunk;
    b->fn_draw = (obj_fn)obj_nop_thunk;
    b->fn_hit = (void *)obj_nop_thunk;
    b->hit[0] = 0x3e8;
    b->hit[2] = 0x3e8;
    b->chain = m;
    m->type = 0xbc;
    m->fn_update = FN(group_box_update);
    m->fn_draw = (obj_fn)obj_nop_thunk;
    m->fn_hit = (void *)obj_nop_thunk;
    m->hit[0] = 0x3e8;
    m->hit[2] = 0x3e8;
    m->target = b;
}

/* level3 $056796: the diver's shot: the shot of an enemy, aimed left or right of the middle of the screen */
void l3_diver_shot(Object *a0)
{
    enemy_shot_update(a0);
    if (a0->type != 0x14)
        return;
    if (a0->x < 0x80) {
        DIVER_SIDE(a0) = 3;
        a0->gfx = GPTR(0x5b330);
    }
    if (a0->x >= 0xc0) {
        DIVER_SIDE(a0) = 5;
        a0->gfx = GPTR(0x5b386);
    }
}

#define DIVER_STATE(o) ((o)->v28)

/* level3 $056618: the diver ($D0): it crosses the screen sideways, turns at the edges, stops at x $88 and $B8 (v28 1) to shoot, with the shot
 * eleven steps into the animation */
void l3_diver_update(Object *a0)
{
    int16_t d0;
    uint32_t a2;
    int flip;

    a0->y = (int16_t)(a0->y + scroll_speed);
    d0 = (int16_t)((int16_t)scroll_max + 0xd0 - scroll_pos);
    if (d0 < a0->y) {
        kill_thunk(a0);
        return;
    }
    anim_step(a0);
    if (DIVER_STATE(a0) == 0) {
        d0 = (int16_t)(DIVER_SIDE(a0) + a0->x);
        a0->x = d0;
        if (d0 <= -0x20) {
            a0->x = 0;
            a2 = 0x567ce;
            flip = 1;
        } else if (d0 >= 0x160) {
            a0->x = 0x140;
            a2 = 0x5681c;
            flip = 1;
        } else if (d0 == 0x88) {
            DIVER_STATE(a0) = 1;
            a2 = 0x567f2;
            flip = 0;
        } else if (d0 == 0xb8) {
            DIVER_STATE(a0) = 1;
            a2 = 0x56840;
            flip = 0;
        } else {
            bbox_from_frame(a0);
            return;
        }
    } else if (a0->anim_delay == 0) {
        DIVER_STATE(a0) = 0;
        if (a0->x > 0xa0) {
            a0->x = 0xd8;
            a2 = 0x567ce;
        } else {
            a0->x = 0x68;
            a2 = 0x5681c;
        }
        flip = 1;
    } else {
        DIVER_STATE(a0)++;
        if (DIVER_STATE(a0) == 0xb) {
            int16_t sx = (int16_t)(a0->x - 5), sy = (int16_t)(a0->y + 4);
            uint32_t gfx = 0x5b330;
            Object *s;

            if (sx > 0xa0) {
                sx = (int16_t)(sx + 0xa);
                gfx = 0x5b386;
            }
            s = alloc_in(LIST_B);
            s->type = 0x14;
            s->fn_update = FN(l3_diver_shot);
            s->fn_draw = draw_bob_masked;
            s->gfx = GPTR(gfx);
            s->anim_delay = 0;
            s->x = sx;
            s->y = sy;
            DIVER_STATE(s) = 0;
            s->steps = (int16_t)W(L3_DIVER_STEPS);
            DIVER_SIDE(s) = 3;
            if (a0->x >= 0xa0)
                DIVER_SIDE(s) = 5;
        }
        bbox_from_frame(a0);
        return;
    }
    if (flip)
        DIVER_SIDE(a0) = (int16_t)-DIVER_SIDE(a0);
    set_anim(a0, a2);
    bbox_from_frame(a0);
}

/* level3 $056108: a diver ($D0) from a record: it flies towards the middle from the side it is on */
void l3_spawn_diver(const uint8_t *a1)
{
    Object *a0 = alloc_in(LIST_C);
    uint32_t a2 = 0x567ce;

    a0->type = 0xd0;
    a0->fn_update = FN(l3_diver_update);
    a0->fn_draw = draw_bob_masked;
    SET_V5E(a0, 0);
    a0->x = (int16_t)(rd16(a1 + 8) - 8);
    a0->y = (int16_t)(rd16(a1 + 0xa) + 8 - scroll_pos);
    DIVER_SIDE(a0) = 2;
    if (a0->x > 0xa0) {
        a2 = 0x5681c;
        DIVER_SIDE(a0) = -2;
    }
    set_anim(a0, a2);
    a0->fn_hit = (void *)hit_enemy;
    a0->energy = (int16_t)W(L3_DIVER_ENERGY);
    a0->big = -1;
    a0->score = 0x12c;
    DIVER_STATE(a0) = 0;
    a0->kind = 0;
    bbox_from_frame(a0);
}

/* ---- the parts of the end ---- */

static void l3_centre(const Object *o, int16_t *x, int16_t *y)
{
    const uint8_t *a5 = (const uint8_t *)o->gfx;
    int16_t dx = (int16_t)(o->x - (int16_t)rd16(a5)), dy = (int16_t)(o->y - (int16_t)rd16(a5 + 2));

    dx = (int16_t)(dx + dx);
    dy = (int16_t)(dy + dy);
    *x = (int16_t)(dx + (int16_t)rd16(a5 + 4)) >> 1;
    *y = (int16_t)(dy + (int16_t)rd16(a5 + 6)) >> 1;
}

/* level3 $055DEC: the parts of the end, one object each from the table at $558F0 (update, hit, picture, offset x, offset y), put first in list C; the first
 * of them is the head of the chain; v30 is the place after the part's entry in the table */
void l3_spawn_parts(void)
{
    uint32_t t = 0x558f0;
    Object *first = 0, *prev = 0;

    do {
        Object *a0 = insert_head(LIST_C);

        if (prev)
            prev->chain = a0;
        else
            first = a0;
        a0->type = 0x54;
        a0->fn_update = (obj_fn)GFN(sh_r32(t));
        a0->fn_draw = draw_bob_masked;
        a0->fn_hit = GFN(sh_r32(t + 4));
        PATH_WAIT(a0) = 0;
        set_anim(a0, sh_r32(t + 8));
        a0->turn[0] = W(t + 12);
        a0->turn[1] = W(t + 14);
        SET_V5E(a0, 0);
        a0->score = 0x3e8;
        a0->big = -1;
        a0->script = (uint8_t *)GPTR(0x570c2);
        a0->x = (int16_t)(W(0x570c4) - 0x20);
        a0->y = (int16_t)(W(0x570c6) - 0x28);
        a0->x_frac = 0;
        a0->y_frac = 0;
        a0->cursor = (const uint8_t *)GPTR(0x570c8);
        PATH_ANGLE_HI(a0) = 0;
        PATH_ANGLE_LO(a0) = 0;
        a0->turn[2] = 0;
        a0->kind = 0;
        a0->steps = (int16_t)W(L3_PARTS_STEPS);
        bbox_from_frame(a0);
        a0->target = first;
        a0->chain = 0;
        t += 16;
        PART_SLOT(a0) = (int16_t)(t - 0x558f0);
        ARM_SWING(a0) = 0;
        prev = a0;
    } while (sh_r32(t) != 0);
    sh_w8(0xd96, 0xff);
    sh_w8(0xd85, 0xff);
}

/* level3 $055A88: the first part: it keeps the end's flag d96 set and fires a shot at the player now and then */
void l3_part_core(Object *a0)
{
    uint16_t sum;

    sh_w8(0xd96, 0xff);
    enemy_update(a0);
    sum = (uint16_t)(a0->v5e_b[0] + B(L3_PART_CORE_FIRE_RATE));
    a0->v5e_b[0] = (uint8_t)sum;
    if (sum > 0xff) {
        Object *s;
        uint16_t angle;

        a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
        angle = direction8((int16_t)(PLAYER_X - a0->x), (int16_t)(PLAYER_Y - a0->y));
        s = spawn_enemy_shot((uint16_t)(a0->x + 0x10), (uint16_t)(a0->y + 0x3c), angle, W(L3_PART_CORE_SHOT_STEPS), 0);
        set_anim(s, 0x55b14);
    }
}

/* level3 $055AF0: a part follows the first by its offsets */
void l3_part_follow(Object *a0)
{
    Object *a1;

    anim_step(a0);
    a1 = a0->target;
    a0->x = (int16_t)(a1->x + (int16_t)a0->turn[0]);
    a0->y = (int16_t)(a1->y + (int16_t)a0->turn[1]);
    bbox_from_frame(a0);
}

#define GUN_SIDE(o) ((o)->v30)

/* level3 $055B38: a gun part (v30 $40 the left one, the other the right): while the energy of its side (d60 / d62) is above zero it is shown
 * as it is and can be hit, then it is the picture of the wreck */
void l3_part_gun(Object *a0)
{
    int16_t side;

    l3_part_follow(a0);
    side = (int16_t)W(GUN_SIDE(a0) == 0x40 ? 0xd60 : 0xd62);
    if (side <= 0) {
        a0->gfx = GPTR(GUN_SIDE(a0) == 0x40 ? 0x5f05e : 0x5f0f0);
        a0->anim_delay = 0;
        a0->hit[0] = 0x3e8;
        a0->fn_hit = (void *)obj_nop_thunk;
        return;
    }
    if (a0->anim_delay != 0) {
        bbox_from_frame(a0);
        return;
    }
    {
        uint16_t sum = (uint16_t)(a0->v5e_b[0] + B(L3_PART_GUN_FIRE_RATE));

        a0->v5e_b[0] = (uint8_t)sum;
        if (sum > 0xff) {
            a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
            set_anim(a0, 0x55a40);
        }
    }
    a0->hit[0] = 0x3e8;
    a0->hit[2] = 0x3e8;
}

/* level3 $055BC0: the hit of a gun part: the energy of its side runs out, it explodes, and when d6a hits come to zero the whole end goes */
void l3_part_hit(Object *a0, Object *a1, uint16_t d0)
{
    Object *a2;
    uint32_t w = GUN_SIDE(a1) == 0x40 ? 0xd60 : 0xd62;
    uint16_t old = W(w);
    int16_t x, y;

    for (a2 = a1->target; a2; a2 = a2->chain)
        a2->fn_draw = draw_bob_or_first;
    sh_w16(w, (uint16_t)(old - d0));
    if (old > d0)
        return;
    a1->hit[0] = 0x3e8;
    a1->fn_hit = (void *)obj_nop_thunk;
    l3_centre(a1, &x, &y);
    spawn_effect(x, y, (const uint8_t *)GPTR(sh_r32(0xf70)), draw_bob_masked);
    sh_w16(0xd6a, (uint16_t)(W(0xd6a) - 1));
    if (W(0xd6a) != 0)
        return;
    a1->target->energy = 0;
    hit_part(a0, a1, 1);
    spawn_bonuses(4);
    sh_w8(0xd85, 0);
}

#define ARM_SWING(o) ((o)->v3e)

/* level3 $055C52: an arm's root: now and then it aims at the player and swings (v3e from 1 to $E and back), the next part's angle and speed follow */
void l3_part_arm(Object *a0)
{
    Object *a1;

    l3_part_follow(a0);
    a1 = a0->prev;
    if (ARM_SWING(a0) != 0) {
        ARM_SWING(a0) = (int16_t)(ARM_SWING(a0) + ARM_SWING_STEP(a0));
        if (ARM_SWING(a0) == 0xe)
            ARM_SWING_STEP(a0) = (int16_t)-ARM_SWING_STEP(a0);
        a1->steps = ARM_SWING(a0);
        bbox_from_frame(a0);
        return;
    }
    if ((random_next() & 0xff) >= 3) {
        a1->steps = 0;
        ARM_ANGLE_LO(a1) = 0x40;
        bbox_from_frame(a0);
        return;
    }
    sfx_cmd2 = 0xf;
    ARM_SWING(a0) = 1;
    ARM_SWING_STEP(a0) = 1;
    {
        uint16_t d0 = (uint16_t)(direction8((int16_t)(PLAYER_X - a0->x), (int16_t)(PLAYER_Y - a0->y)) << 5);
        uint32_t r;

        d0 = (uint16_t)((d0 & 0xff00) | (uint8_t)((d0 & 0xff) - 0x60));
        ARM_ANGLE_HI(a1) = 0;
        ARM_ANGLE_LO(a1) = (int16_t)d0;
        r = (((random_next() & 0x1f) + 0x20) << 8) << 2;
        a1->turn[0] = (uint16_t)(r >> 16);
        a1->turn[1] = (uint16_t)r;
        a1->turn[2] = 0;
        a1->steps = 1;
        if (PART_SLOT(a0) == 0x60) {
            ARM_ANGLE_LO(a1) = (int16_t)((ARM_ANGLE_LO(a1) & 0xff00) | (uint8_t)(ARM_ANGLE_LO(a1) + 0x40));
            r = -r;
            a1->turn[0] = (uint16_t)(r >> 16);
            a1->turn[1] = (uint16_t)r;
        }
    }
    bbox_from_frame(a0);
}

/* the arm's piece takes its place from the next piece and runs its path with the state of the piece before it */
static void l3_arm_follow(Object *a0)
{
    Object *a1 = a0->next;

    a0->x = a1->x;
    a0->x_frac = a1->x_frac;
    a0->y = a1->y;
    a0->y_frac = a1->y_frac;
    PATH_WAIT(a0) = 0x3e8;
}

#define ARM_ANGLE_HI(o) ((o)->v2a)
#define ARM_ANGLE_LO(o) ((o)->v2c)

/* level3 $055D12: a piece of an arm: it moves after the next piece by its own path (angle v2a:v2c, speed pad4e), and hands that state to the piece before it */
void l3_part_arm_piece(Object *a0)
{
    int16_t s2a, s2c, s4e0, s4e1, s52;
    Object *a1;

    l3_arm_follow(a0);
    s2a = ARM_ANGLE_HI(a0);
    s2c = ARM_ANGLE_LO(a0);
    s4e0 = (int16_t)a0->turn[0];
    s4e1 = (int16_t)a0->turn[1];
    s52 = (int16_t)a0->turn[2];
    obj_script_run(a0);
    a1 = a0->prev;
    ARM_ANGLE_HI(a1) = ARM_ANGLE_HI(a0);
    ARM_ANGLE_LO(a1) = ARM_ANGLE_LO(a0);
    a1->turn[0] = a0->turn[0];
    a1->turn[1] = a0->turn[1];
    a1->turn[2] = a0->turn[2];
    a1->steps = a0->steps;
    a0->turn[2] = (uint16_t)s52;
    a0->turn[1] = (uint16_t)s4e1;
    a0->turn[0] = (uint16_t)s4e0;
    ARM_ANGLE_LO(a0) = s2c;
    ARM_ANGLE_HI(a0) = s2a;
}

/* level3 $055D62: the tip of an arm: it takes its place and path like the pieces, and its picture follows the angle */
void l3_part_arm_tip(Object *a0)
{
    uint16_t d0, d1;

    l3_arm_follow(a0);
    obj_script_run(a0);
    d0 = (uint16_t)(((uint16_t)ARM_ANGLE_LO(a0) >> 2) & 0x38);
    if (a0->steps == 0xe)
        sfx_cmd = 0xc;
    d1 = (uint16_t)(frame_tick & 4);
    a0->gfx = GPTR(sh_r32(0x55dac + (uint16_t)(d0 + d1)));
    a0->anim_delay = 0;
    bbox_from_frame(a0);
}

/* level3 $0550D8: an enemy that waits off the screen: by the side of it (the corner, by which of its distances is the shorter) it takes the picture
 * of its direction from the table at $5513E and goes on as an ordinary enemy (enemy_update) */
void l3_enemy_enter(Object *a0)
{
    int16_t x, y, d0;

    enemy_update(a0);
    x = a0->x;
    y = a0->y;
    if (x < 0) {
        if (y < 0)
            d0 = y < x ? 4 : 0;
        else
            d0 = (int16_t)(0xc0 - y) < x ? 0xc : 0;
    } else if (y < 0) {
        d0 = y < (int16_t)(0x140 - x) ? 4 : 8;
    } else {
        d0 = (int16_t)(y - 0xc0) < (int16_t)(x - 0x140) ? 8 : 0xc;
    }
    set_anim(a0, sh_r32(0x5513e + d0));
    a0->fn_update = FN(enemy_update);
    bbox_from_frame(a0);
}

/* ---- the guns, the boss of tiles and the walker ---- */

static void copy2(uint8_t *dst, const uint8_t *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
}

static void copy4(uint8_t *dst, const uint8_t *src)
{
    copy2(dst, src);
    copy2(dst + 2, src + 2);
}

#define L3_GUN_OPEN(o) ((o)->v28)

/* level3 $056DF6: a gun of tiles ($F0 or $F4): its tiles open in turn (v28), at the eighth it fires */
void l3_gun_update(Object *a0)
{
    int16_t d0;
    uint32_t a2;

    if ((int16_t)(scroll_max + 0xd0) < a0->y) {
        kill_thunk(a0);
        return;
    }
    a0->hit[0] = (int16_t)(a0->x + 2);
    a0->hit[2] = (int16_t)(a0->x + 0xe);
    d0 = (int16_t)(a0->y - scroll_pos + 2);
    a0->hit[1] = d0;
    a0->hit[3] = (int16_t)(d0 + 0xc);
    if (L3_GUN_OPEN(a0) == 0) {
        uint16_t sum = (uint16_t)(a0->v5e_b[0] + B(L3_GUN_FIRE_RATE));

        a0->v5e_b[0] = (uint8_t)sum;
        if (sum > 0xff) {
            a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
            L3_GUN_OPEN(a0) = 1;
        }
        return;
    }
    a2 = a0->type == 0xf0 ? 0x56eda : 0x56ee8;
    copy2(a0->script, (const uint8_t *)GPTR(a2 + (uint16_t)(L3_GUN_OPEN(a0) & ~1)));
    L3_GUN_OPEN(a0)++;
    if (L3_GUN_OPEN(a0) == 8) {
        int16_t x = (int16_t)(a0->x + 8), y = (int16_t)(a0->y - scroll_pos), angle = 0;

        if (a0->type != 0xf0) {
            angle = 4;
            y = (int16_t)(y + 0xa);
        }
        spawn_enemy_shot((uint16_t)x, (uint16_t)y, (uint16_t)angle, W(L3_GUN_SHOT_STEPS), GPTR(0x5e69e));
        return;
    }
    if (L3_GUN_OPEN(a0) == 0xe)
        L3_GUN_OPEN(a0) = 0;
}

/* level3 $056220: a gun of tiles from a record ($F0, $F4 when w0c is set) */
void l3_spawn_gun(const uint8_t *a1)
{
    int flag = rd16(a1 + 0xc) != 0;
    Object *a0 = spawn_marker_enemy(flag ? 0xf4 : 0xf0, a1, (void (*)(void))l3_gun_update);
    uint32_t a2 = flag ? 0x56ee8 : 0x56eda;

    a0->fn_hit = (void *)hit_tile;
    a0->energy = (int16_t)W(L3_GUN_ENERGY);
    a0->hit[0] = 0x3e8;
    a0->hit[2] = 0x3e8;
    a0->gfx = GPTR(a2);
    a0->anim = (const uint8_t *)GPTR(flag ? 0x56ef8 : 0x56ef6);
    copy2(a0->script, (const uint8_t *)GPTR(a2));
    a0->score = 0x64;
    TILE_COLS(a0) = 0;
    TILE_ROWS(a0) = 0;
}

/* level3 $0561A6: the boss of tiles ($F8) from a record: six rows of its tiles are put into the map */
void l3_spawn_boss(const uint8_t *a1)
{
    Object *a0 = spawn_marker_enemy(0xf8, a1, (void (*)(void))l3_boss_update);
    uint8_t *m = a0->script;
    int i;

    a0->fn_draw = (obj_fn)obj_nop_thunk;
    a0->fn_hit = (void *)l3_boss_hit;
    a0->energy = (int16_t)W(L3_BOSS_ENERGY);
    a0->hit[0] = 0x3e8;
    a0->hit[2] = 0x3e8;
    for (i = 0; i < 6; i++) {
        copy4(m, (const uint8_t *)GPTR(0x561f0 + 8 * i));
        copy4(m + 4, (const uint8_t *)GPTR(0x561f0 + 8 * i + 4));
        m += 0x28;
    }
}

#define L3_BOSS_CELL(o) ((o)->v28)

/* level3 $05686A: the boss of tiles, first form: it opens and closes its mouth (v28, the cells of the table at $569A2) and fires from the sixth step on */
void l3_boss_update(Object *a0)
{
    uint16_t d0;
    uint8_t *m;

    if ((int16_t)(scroll_max + 0xd0) < a0->y) {
        kill_thunk(a0);
        return;
    }
    a0->hit[0] = (int16_t)(a0->x + 0x10);
    a0->hit[2] = (int16_t)(a0->hit[0] + 0x1f);
    d0 = (uint16_t)(a0->y - scroll_pos + 0x40);
    a0->hit[1] = (int16_t)d0;
    a0->hit[3] = (int16_t)(d0 + 0x1a);
    d0 = (uint16_t)L3_BOSS_CELL(a0);
    if (d0 == 0) {
        if ((uint8_t)random_next() >= B(L3_BOSS_CHANCE))
            return;
    }
    m = a0->script;
    copy4(m + 0xa2, (const uint8_t *)GPTR(0x569a2 + 4 * (d0 & ~1u)));
    copy4(m + 0xca, (const uint8_t *)GPTR(0x569a2 + 4 * (d0 & ~1u) + 4));
    L3_BOSS_CELL(a0)++;
    if (L3_BOSS_CELL(a0) == 0x12) {
        L3_BOSS_CELL(a0) = 0;
        return;
    }
    if (L3_BOSS_CELL(a0) < 4)
        return;
    if (L3_BOSS_CELL(a0) > 0xb) {
        if (L3_BOSS_CELL(a0) == 0xc && (uint8_t)random_next() < B(L3_BOSS_CHANCE_2))
            L3_BOSS_CELL(a0) = 4;
        return;
    }
    {
        uint16_t sum = (uint16_t)(a0->v5e_b[0] + B(L3_BOSS_FIRE_RATE));
        int16_t x, y;
        uint16_t angle = 4, step;
        uint32_t gfx;

        a0->v5e_b[0] = (uint8_t)sum;
        if (sum <= 0xff)
            return;
        a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
        x = (int16_t)(a0->x + 0x20);
        y = (int16_t)(a0->y - scroll_pos + 0x58);
        gfx = sh_r32(sh_r32(0x6ee26 + 4 * (uint32_t)W(L3_BOSS_PARAM)));
        step = (uint16_t)(L3_BOSS_CELL(a0) & ~1);
        if (step == 6)
            angle = 3;
        if (step == 8)
            angle = 5;
        spawn_enemy_shot((uint16_t)x, (uint16_t)y, angle, W(L3_GUN_ENERGY), GPTR(gfx));
    }
}

/* level3 $0569EA: the hit of the boss of tiles, first form: it flashes; when the energy is gone it bursts, its tiles go and it goes on in the second form */
void l3_boss_hit(Object *a0, Object *a1, uint16_t d0)
{
    uint16_t old = (uint16_t)a1->energy;
    uint8_t *m;

    (void)a0;
    a1->energy = (int16_t)(old - d0);
    if (old > d0) {
        a1->fn_draw = FN(l3_boss_flash);
        return;
    }
    spawn_effect((int16_t)(a1->x + 0x20), (int16_t)(a1->y - scroll_pos + 0x40), big_explosion_anim, draw_bob_masked);
    m = a1->script;
    wr32(m + 0x50, 0);
    wr32(m + 0x54, 0);
    wr32(m + 0x78, 0);
    wr32(m + 0x7c, 0);
    wr32(m + 0xa0, 0);
    wr32(m + 0xa4, 0);
    wr32(m + 0xca, 0);
    a1->fn_update = FN(l3_boss2_update);
    a1->fn_draw = (obj_fn)obj_nop_thunk;
    a1->fn_hit = (void *)l3_boss2_hit;
    a1->energy = (int16_t)W(L3_BOSS_ENERGY_2);
    a1->hit[0] = 0x3e8;
    a1->hit[2] = 0x3e8;
    L3_BOSS_CELL(a1) = 0;
    player_score += 0x1f4;
}

/* level3 $056A8C: the draw of the boss of tiles for the frame after a hit: its two grids as flashes */
void l3_boss_flash(Object *a0)
{
    int16_t y = (int16_t)(a0->y - scroll_pos);
    uint32_t cells = 0x569a2 + 4 * (uint32_t)(L3_BOSS_CELL(a0) & ~1);

    a0->fn_draw = (obj_fn)obj_nop_thunk;
    draw_tile_grid_flash((const uint8_t *)GPTR(0x56a74), 3, 2, (int16_t)(y + 0x20), (uint16_t)(((uint16_t)a0->x >> 1) & 0xfffc));
    draw_tile_grid_flash((const uint8_t *)GPTR(cells), 1, 1, (int16_t)(y + 0x40), (uint16_t)(((uint16_t)(a0->x + 0x10) >> 1) & 0xfffc));
}

#define L3_BOSS2_CELL(o) ((o)->v28)

/* level3 $056AF8: the boss of tiles, second form: the cells of the table at $56D2C in turn (v28), and at the fourth step eight shots */
void l3_boss2_update(Object *a0)
{
    uint16_t d0;
    uint8_t *m;

    if ((int16_t)(scroll_max + 0xd0) < a0->y) {
        kill_thunk(a0);
        return;
    }
    a0->hit[0] = (int16_t)(a0->x + 0x10);
    a0->hit[2] = (int16_t)(a0->hit[0] + 0x1f);
    d0 = (uint16_t)(a0->y + 4 - scroll_pos);
    a0->hit[1] = (int16_t)d0;
    a0->hit[3] = (int16_t)(d0 + 0x17);
    d0 = (uint16_t)L3_BOSS2_CELL(a0);
    if (d0 == 0) {
        uint16_t sum = (uint16_t)(a0->v5e_b[0] + B(L3_BOSS2_FIRE_RATE));

        a0->v5e_b[0] = (uint8_t)sum;
        if (sum <= 0xff)
            return;
        a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
    }
    m = a0->script;
    copy4(m + 2, (const uint8_t *)GPTR(0x56d2c + 4 * (d0 & ~1u)));
    copy4(m + 0x2a, (const uint8_t *)GPTR(0x56d2c + 4 * (d0 & ~1u) + 4));
    L3_BOSS2_CELL(a0)++;
    if (L3_BOSS2_CELL(a0) == 0xa) {
        L3_BOSS2_CELL(a0) = 0;
        return;
    }
    if (L3_BOSS2_CELL(a0) == 4) {
        int16_t x = (int16_t)(a0->x + 0x20), y = (int16_t)(a0->y - scroll_pos + 0x18);
        int d7;

        for (d7 = 7; d7 >= 0; d7--) {
            Object *s = spawn_enemy_shot((uint16_t)x, (uint16_t)y, (uint16_t)d7, W(L3_BOSS2_SHOT_STEPS), 0);

            set_anim(s, sh_r32(0x56bec + 4 * (uint32_t)(7 - d7)));
        }
    }
}

/* level3 $056D54: the hit of the boss of tiles, second form: it flashes; when the energy is gone it bursts and goes */
void l3_boss2_hit(Object *a0, Object *a1, uint16_t d0)
{
    uint16_t old = (uint16_t)a1->energy;
    uint8_t *m;

    (void)a0;
    a1->energy = (int16_t)(old - d0);
    if (old > d0) {
        a1->fn_draw = FN(l3_boss2_flash);
        return;
    }
    spawn_effect((int16_t)(a1->x + 0x20), (int16_t)(a1->y - scroll_pos + 0x10), big_explosion_anim, draw_bob_masked);
    m = a1->script;
    wr32(m, 0);
    wr32(m + 4, 0);
    wr32(m + 0x28, 0);
    wr32(m + 0x2c, 0);
    player_score += 0x1f4;
    kill_thunk(a1);
}

/* level3 $056DC4: the draw of the boss of tiles, second form, for the frame after a hit */
void l3_boss2_flash(Object *a0)
{
    a0->fn_draw = (obj_fn)obj_nop_thunk;
    draw_tile_grid_flash((const uint8_t *)GPTR(0x56d2c), 1, 1, (int16_t)(a0->y - scroll_pos), (uint16_t)(((uint16_t)(a0->x + 0x10) >> 1) & 0xfffc));
}

/* level3 $0556F6: a walker ($DC) from a record: the direction w6 of the record gives its first place and picture */
void l3_spawn_walker(const uint8_t *a1)
{
    Object *a0 = alloc_in(LIST_C);
    uint16_t i = rd16(a1 + 6);

    a0->type = 0xdc;
    a0->fn_update = FN(l3_walker_update);
    a0->fn_draw = draw_bob_masked;
    WALKER_DIR(a0) = (int16_t)i;
    SET_V5E(a0, 0);
    a0->x = (int16_t)(rd16(a1 + 8) - 8 + (int16_t)W(0x55790 + 2 * i));
    a0->y = (int16_t)(rd16(a1 + 10) - 8 - scroll_pos + (int16_t)W(0x557a0 + 2 * i));
    set_anim(a0, sh_r32(0x557b0 + 4 * (uint32_t)i));
    a0->fn_hit = (void *)hit_enemy;
    a0->energy = (int16_t)W(L3_WALKER_ENERGY);
    a0->big = 0;
    a0->score = 0x64;
    WALKER_STATE(a0) = 0;
    a0->kind = 0;
    bbox_from_frame(a0);
}

#define WALKER_DIR(o) ((o)->v2a)

/* level3 $0562B0: the walker: it goes on in its direction (v2a, 8 of them) by the steps of the tables at $56290 / $562A0 and at each tile of the map looks at
 * the cell ahead: a wall turns it by the tables at $56460, a free cell ahead lets it go on; now and then it fires at the player */
void l3_walker_update(Object *a0)
{
    uint32_t d2 = 2 * (uint32_t)(uint16_t)WALKER_DIR(a0);
    int16_t d0, d1;
    int at_tile = 0, turn = 0;

    anim_step(a0);
    a0->y = (int16_t)(a0->y + scroll_speed);
    if ((int16_t)(scroll_max + 0xd0 - scroll_pos) < a0->y) {
        kill_thunk(a0);
        return;
    }
    d0 = (int16_t)W(0x56290 + d2);
    if (d0 != 0) {
        a0->x = (int16_t)(a0->x + d0);
        at_tile = d0 < 0 ? (a0->x & 0xf) == 0xf : (a0->x & 0xf) == 0;
    } else {
        int16_t step = (int16_t)W(0x562a0 + d2);

        a0->y = (int16_t)(a0->y + step);
        d1 = (int16_t)(a0->y + scroll_pos);
        at_tile = step < 0 ? (d1 & 0xf) == 0xf : (d1 & 0xf) == 0;
    }
    if (at_tile) {
        d0 = a0->x;
        if (d0 == 0 || (uint16_t)d0 >= 0x140) {
            turn = 1;
        } else {
            uint16_t row = (uint16_t)((uint16_t)(a0->y + scroll_pos) >> 4);
            const uint8_t *a1 = level_init_src + (int16_t)(uint16_t)(row * 0x28) + (((uint16_t)d0 >> 3) & 0xfe);

            if (rd16(a1) != 0) {
                a0->x = (int16_t)(a0->x - (int16_t)W(0x56290 + d2));
                a0->y = (int16_t)(a0->y - (int16_t)W(0x562a0 + d2));
                turn = 1;
            } else {
                int16_t d3 = (int16_t)W(0x56450 + d2);

                turn = rd16(a1 + d3) == 0;
            }
        }
        if (turn) {
            uint16_t ni;

            a0->x = (int16_t)(a0->x + (int16_t)W(0x56460 + d2));
            a0->y = (int16_t)(a0->y + (int16_t)W(0x56470 + d2));
            ni = W(0x56480 + d2);
            WALKER_DIR(a0) = (int16_t)ni;
            set_anim(a0, sh_r32(0x557b0 + 4 * (uint32_t)ni));
        }
    }
    {
        uint16_t sum = (uint16_t)(a0->v5e_b[0] + B(L3_WALKER_FIRE_RATE));

        a0->v5e_b[0] = (uint8_t)sum;
        if (sum > 0xff) {
            uint32_t gfx = sh_r32(sh_r32(0x6ee26 + 4 * (uint32_t)W(L3_WALKER_PARAM)));
            uint16_t angle;

            a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
            angle = direction8((int16_t)(PLAYER_X - a0->x), (int16_t)(PLAYER_Y - a0->y));
            spawn_enemy_shot((uint16_t)a0->x, (uint16_t)a0->y, angle, W(L3_WALKER_SHOT_STEPS), GPTR(gfx));
        }
    }
    bbox_from_frame(a0);
}

/* the picture of a worm's piece follows its phase v2c by a table of eight */
static void l3_worm_follow(Object *a0, uint32_t table)
{
    a0->y = (int16_t)(a0->y + scroll_speed);
    if (table == 0x55622)
        sh_w8(0xd94 + (uint16_t)WORM_SIDE(a0), 0xff);
    enemy_update_no_anim(a0);
    a0->gfx = GPTR(sh_r32(table + ((((uint16_t)WORM_PHASE(a0) + 0x10) >> 3) & 0x1c)));
    bbox_from_frame(a0);
}

/* level3 $055420: the worm's head: its picture follows its phase and now and then it fires eight shots round it */
void l3_worm_head(Object *a0)
{
    uint16_t old = (uint16_t)WORM_PHASE(a0);
    uint16_t d0, d1, sum;

    a0->y = (int16_t)(a0->y + scroll_speed);
    enemy_update(a0);
    d0 = (uint16_t)((((uint16_t)WORM_PHASE(a0) + 0x10) >> 3) & 0x1c);
    d1 = (uint16_t)(((old + 0x10) >> 3) & 0x1c);
    if (d1 != d0)
        set_anim(a0, sh_r32(0x55400 + d0));
    sum = (uint16_t)(a0->v5e_b[0] + B(L3_WORM_HEAD_FIRE_RATE));
    a0->v5e_b[0] = (uint8_t)sum;
    if (sum > 0xff) {
        int d2;

        for (d2 = 7; d2 >= 0; d2--) {
            Object *s = spawn_enemy_shot((uint16_t)a0->x, (uint16_t)a0->y, (uint16_t)d2, W(L3_WORM_HEAD_SHOT_STEPS), 0);

            set_anim(s, 0x5549e);
        }
    }
    bbox_from_frame(a0);
}

/* level3 $05556A */
void l3_worm_a(Object *a0)
{
    l3_worm_follow(a0, 0x5558e);
}

/* level3 $0555AE */
void l3_worm_b(Object *a0)
{
    l3_worm_follow(a0, 0x555d2);
}

/* level3 $0555F2: the last piece: it shows the end's flag of its side (d94 / d95) */
void l3_worm_c(Object *a0)
{
    l3_worm_follow(a0, 0x55622);
}

/* level3 $055642: the hit of the worm's first piece: the energy of its side (d66 / d68) runs out and the whole worm bursts and the side's flag d92 / d93 is set */
void l3_worm_hit(Object *a0, Object *a1, uint16_t d0)
{
    Object *o;
    uint32_t w = 0xd66 + 2 * (uint32_t)(uint16_t)WORM_SIDE(a1);
    uint16_t old = W(w);

    for (o = a1; o; o = o->chain)
        o->fn_draw = draw_bob_or_first;
    sh_w16(w, (uint16_t)(old - d0));
    if (old > d0)
        return;
    sh_w8(0xd92 + (uint16_t)WORM_SIDE(a1), 0xff);
    sh_w8(0xd94 + (uint16_t)WORM_SIDE(a1), 0);
    for (o = a1->chain; o;) {
        Object *next = o->chain;

        kill_thunk(o);
        o = next;
    }
    kill_thunk(a1);
    debris_burst(a0, 0x13, 0, 0, 0x140, 0xc0, big_explosion_anim);
    free_object_chain(LIST_C->prev);
}
