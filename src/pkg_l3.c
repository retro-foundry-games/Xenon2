/* pkg_l3: level 3's own code (the blob at $54E00 while level 3 is loaded), written like pkg_l2.c: raw big-endian memory through sh_r16 and the like;
 * a function has `level3 $ADDR` for its address. */
#include "pkg_d.h"
#include "pkg_i.h"
#include "game_all.h"

#define FN(f) ((obj_fn)(f))
#define W(a) sh_r16(a)
#define B(a) sh_r8(a)
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

static void kill_thunk(Object *o)
{
    o->type = 4;
    o->fn_update = (obj_fn)free_object;
    o->fn_draw = (obj_fn)obj_nop_thunk;
    o->fn_hit = (void *)obj_nop_thunk;
    o->flag34 = 0;
    o->flag60 = 0;
}

static void set_anim(Object *o, uint32_t address)
{
    anim_header_set(o, (const uint8_t *)GPTR(address));
}

void l3_spawn_snake(const uint8_t *a1);
void l3_spawn_diver(const uint8_t *a1);
void l3_worm_spawn(uint16_t d6);
void l3_end_draw(Object *a0);

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
        saved = v_0426;
        v_0426 = W(0x54e7a);
        spawn_formation(0, a1, (const uint8_t *)GPTR(0x54eb2));
        v_0426 = saved;
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
    }
}

/* level3 $054E24, $0551DE: the scroll: at the end ($AF0) the scroll's speed is stopped; above $D0 the limit is $A30 at least; below it, $20, the worm's
 * spawner runs and when its flags say so the bonuses come once */
void l3_scroll(void)
{
    if (v_0ce8 == 0xaf0 && v_0cd0 == 0xaf0) {
        v_0cea = 0xaf0;
        scroll_pos_update(v_0cda);
        stars_scroll(v_0cda);
        v_0cda = 0;
        if (B(0xd96) != 0)
            sh_w8(0xd96, 0);
        else
            v_0ce8 = 0;
    }
    if ((int16_t)v_0cd0 <= 0xd0) {
        if ((int16_t)v_0cea < 0x20)
            v_0cea = 0x20;
        if (B(0xd92) == 0 && B(0xd94) == 0)
            l3_worm_spawn(0);
        sh_w8(0xd94, 0);
        sh_w8(0xd95, 0);
        if (B(0xd92) != 0 && B(0xd93) != 0 && v_0a07 == 0) {
            spawn_bonuses(9);
            v_0a07 = 0xff;
        }
    } else if ((int16_t)v_0cea < 0xa30) {
        v_0cea = 0xa30;
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
    v_0ce8 = 0xaf0;
    v_0cea = 0x11ff;
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
    a0->flag60 = -1;
    v_0d26[16] = 0xe496;
    v_0d26[17] = 0xaa8a;
    v_0d26[18] = 0x5274;
    v_0d26[19] = 0xb949;
    sh_w8(0xd92, 0);
    sh_w8(0xd93, 0xff);
    sh_w8(0xd94, 0);
    sh_w8(0xd95, 0);
    sh_w16(0xd64, 0);
    sh_w16(0xd66, W(0x54e6a));
    sh_w16(0xd68, W(0x54e6a));
    sh_w8(0xd96, 0);
    sh_w16(0xd6a, 2);
    sh_w16(0xd60, W(0x54e6c));
    sh_w16(0xd62, W(0x54e6c));
    v_0424 = (uint16_t)(v_0e02 * 2);
    v_0426 = (uint16_t)(v_0e02 * 3);
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
        int16_t d2 = (int16_t)(W(t + 8) - v_0cd0);

        if (d2 >= 0xc0 || d2 <= -0x30)
            continue;
        draw_tile_grid((const uint8_t *)GPTR(sh_r32(t)), W(t + 4), W(t + 6), d2, W(t + 10));
    }
}

/* level3 $055310: the spawner of the end's worm (not written) */
void l3_worm_spawn(uint16_t d6)
{
    (void)d6;
}

static void l3_enemy(Object *a0, uint32_t table)
{
    enemy_update_no_anim(a0);
    a0->gfx = GPTR(sh_r32(table + ((((uint16_t)a0->v2c + 0x10) >> 3) & 0x1c)));
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
    a0->y = (int16_t)(a0->y + v_0cd8);
    if (a0->y >= 0xe8) {
        kill_thunk(a0);
        return 1;
    }
    return 0;
}

/* level3 $056490: the first piece of a snake: it waits (v30), looks for the player's height, then swings (v28 between 0 and v30) */
void l3_snake_head(Object *a0)
{
    if (l3_snake_off(a0))
        return;
    a0->fn_draw = (obj_fn)obj_nop_thunk;
    if (a0->v2a == 0) {
        int16_t d0;

        if (a0->v30 < 0) {
            a0->v30++;
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
        a0->v2a = 2;
        a0->v30 = (int16_t)((random_next() & 6) + 0xa);
    }
    a0->fn_draw = draw_bob_masked;
    a0->v28 = (int16_t)(a0->v2a + a0->v28);
    if (a0->v28 == 0) {
        a0->v2a = 0;
        a0->v30 = -5;
    } else if (a0->v28 == a0->v30) {
        a0->v2a = (int16_t)-a0->v2a;
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
    a0->x = (int16_t)((uint16_t)((uint16_t)a0->v30 * (uint16_t)a1->v28) + (uint16_t)a0->v2e);
    if (a1->v2a == 2 && a1->v28 == 2) {
        a0->anim_delay = 1;
    } else if (a1->v30 > 0 && a1->v30 == a1->v28) {
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
    a0->x = (int16_t)((uint16_t)((uint16_t)a0->v30 * (uint16_t)a1->v28) + (uint16_t)a0->v2e);
    if (a1->v2a != 0)
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
    d1 = (int16_t)(rd16(a1 + 0xa) - v_0cd0);
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
        a0->v2e = d0;
        a0->y = d1;
        a0->fn_hit = (void *)hit_part;
        a0->flag34 = 1;
        a0->energy = (int16_t)W(0x54e48);
        a0->flag35 = 0;
        a0->v42 = 0x190;
        a0->v28 = 0;
        a0->v44 = 0;
        a0->v2a = 0;
        d6 = (int16_t)(7 - d7);
        if (flag)
            d6 = (int16_t)-d6;
        a0->v30 = d6;
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
        a0->v2a = 3;
        a0->gfx = GPTR(0x5b330);
    }
    if (a0->x >= 0xc0) {
        a0->v2a = 5;
        a0->gfx = GPTR(0x5b386);
    }
}

/* level3 $056618: the diver ($D0): it crosses the screen sideways, turns at the edges, stops at x $88 and $B8 (v28 1) to shoot, with the shot
 * eleven steps into the animation */
void l3_diver_update(Object *a0)
{
    int16_t d0;
    uint32_t a2;
    int flip;

    a0->y = (int16_t)(a0->y + v_0cd8);
    d0 = (int16_t)((int16_t)v_0cea + 0xd0 - v_0cd0);
    if (d0 < a0->y) {
        kill_thunk(a0);
        return;
    }
    anim_step(a0);
    if (a0->v28 == 0) {
        d0 = (int16_t)(a0->v2a + a0->x);
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
            a0->v28 = 1;
            a2 = 0x567f2;
            flip = 0;
        } else if (d0 == 0xb8) {
            a0->v28 = 1;
            a2 = 0x56840;
            flip = 0;
        } else {
            bbox_from_frame(a0);
            return;
        }
    } else if (a0->anim_delay == 0) {
        a0->v28 = 0;
        if (a0->x > 0xa0) {
            a0->x = 0xd8;
            a2 = 0x567ce;
        } else {
            a0->x = 0x68;
            a2 = 0x5681c;
        }
        flip = 1;
    } else {
        a0->v28++;
        if (a0->v28 == 0xb) {
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
            s->v28 = 0;
            s->v54 = (int16_t)W(0x54e54);
            s->v2a = 3;
            if (a0->x >= 0xa0)
                s->v2a = 5;
        }
        bbox_from_frame(a0);
        return;
    }
    if (flip)
        a0->v2a = (int16_t)-a0->v2a;
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
    a0->y = (int16_t)(rd16(a1 + 0xa) + 8 - v_0cd0);
    a0->v2a = 2;
    if (a0->x > 0xa0) {
        a2 = 0x5681c;
        a0->v2a = -2;
    }
    set_anim(a0, a2);
    a0->fn_hit = (void *)hit_enemy;
    a0->energy = (int16_t)W(0x54e4a);
    a0->flag35 = -1;
    a0->v42 = 0x12c;
    a0->v28 = 0;
    a0->v44 = 0;
    bbox_from_frame(a0);
}
