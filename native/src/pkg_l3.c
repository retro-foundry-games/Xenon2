/* pkg_l3: level 3's own code (the blob at $54E00 while level 3 is loaded), written like pkg_l2.c: raw big-endian memory through sh_r16 and the like;
 * a function has `level3 $ADDR` for its address. */
#include "pkg_d.h"
#include "pkg_i.h"
#include "game_all.h"

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
    case 3:
        l3_spawn_parts();
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
        a0->v28 = 0;
        set_anim(a0, sh_r32(t + 8));
        a0->pad4e[0] = W(t + 12);
        a0->pad4e[1] = W(t + 14);
        SET_V5E(a0, 0);
        a0->v42 = 0x3e8;
        a0->flag35 = -1;
        a0->v46 = (uint8_t *)GPTR(0x570c2);
        a0->x = (int16_t)(W(0x570c4) - 0x20);
        a0->y = (int16_t)(W(0x570c6) - 0x28);
        a0->x_frac = 0;
        a0->y_frac = 0;
        a0->ptr4a = (const uint8_t *)GPTR(0x570c8);
        a0->v2a = 0;
        a0->v2c = 0;
        a0->pad4e[2] = 0;
        a0->v44 = 0;
        a0->v54 = (int16_t)W(0x54e46);
        bbox_from_frame(a0);
        a0->target = first;
        a0->chain = 0;
        t += 16;
        a0->v30 = (int16_t)(t - 0x558f0);
        a0->v3e = 0;
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
    sum = (uint16_t)(a0->v5e_b[0] + B(0x54e75));
    a0->v5e_b[0] = (uint8_t)sum;
    if (sum > 0xff) {
        Object *s;
        uint16_t angle;

        a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
        angle = direction8((int16_t)(PLAYER_X - a0->x), (int16_t)(PLAYER_Y - a0->y));
        s = spawn_enemy_shot((uint16_t)(a0->x + 0x10), (uint16_t)(a0->y + 0x3c), angle, W(0x54e76), 0);
        set_anim(s, 0x55b14);
    }
}

/* level3 $055AF0: a part follows the first by its offsets */
void l3_part_follow(Object *a0)
{
    Object *a1;

    anim_step(a0);
    a1 = a0->target;
    a0->x = (int16_t)(a1->x + (int16_t)a0->pad4e[0]);
    a0->y = (int16_t)(a1->y + (int16_t)a0->pad4e[1]);
    bbox_from_frame(a0);
}

/* level3 $055B38: a gun part (v30 $40 the left one, the other the right): while the energy of its side (d60 / d62) is above zero it is shown
 * as it is and can be hit, then it is the picture of the wreck */
void l3_part_gun(Object *a0)
{
    int16_t side;

    l3_part_follow(a0);
    side = (int16_t)W(a0->v30 == 0x40 ? 0xd60 : 0xd62);
    if (side <= 0) {
        a0->gfx = GPTR(a0->v30 == 0x40 ? 0x5f05e : 0x5f0f0);
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
        uint16_t sum = (uint16_t)(a0->v5e_b[0] + B(0x54e79));

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
    uint32_t w = a1->v30 == 0x40 ? 0xd60 : 0xd62;
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

/* level3 $055C52: an arm's root: now and then it aims at the player and swings (v3e from 1 to $E and back), the next part's angle and speed follow */
void l3_part_arm(Object *a0)
{
    Object *a1;

    l3_part_follow(a0);
    a1 = a0->prev;
    if (a0->v3e != 0) {
        a0->v3e = (int16_t)(a0->v3e + a0->v2e);
        if (a0->v3e == 0xe)
            a0->v2e = (int16_t)-a0->v2e;
        a1->v54 = a0->v3e;
        bbox_from_frame(a0);
        return;
    }
    if ((random_next() & 0xff) >= 3) {
        a1->v54 = 0;
        a1->v2c = 0x40;
        bbox_from_frame(a0);
        return;
    }
    v_0418 = 0xf;
    a0->v3e = 1;
    a0->v2e = 1;
    {
        uint16_t d0 = (uint16_t)(direction8((int16_t)(W(0x3f03c) - a0->x), (int16_t)(W(0x3f040) - a0->y)) << 5);
        uint32_t r;

        d0 = (uint16_t)((d0 & 0xff00) | (uint8_t)((d0 & 0xff) - 0x60));
        a1->v2a = 0;
        a1->v2c = (int16_t)d0;
        r = (((random_next() & 0x1f) + 0x20) << 8) << 2;
        a1->pad4e[0] = (uint16_t)(r >> 16);
        a1->pad4e[1] = (uint16_t)r;
        a1->pad4e[2] = 0;
        a1->v54 = 1;
        if (a0->v30 == 0x60) {
            a1->v2c = (int16_t)((a1->v2c & 0xff00) | (uint8_t)(a1->v2c + 0x40));
            r = -r;
            a1->pad4e[0] = (uint16_t)(r >> 16);
            a1->pad4e[1] = (uint16_t)r;
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
    a0->v28 = 0x3e8;
}

/* level3 $055D12: a piece of an arm: it moves after the next piece by its own path (angle v2a:v2c, speed pad4e), and hands that state to the piece before it */
void l3_part_arm_piece(Object *a0)
{
    int16_t s2a, s2c, s4e0, s4e1, s52;
    Object *a1;

    l3_arm_follow(a0);
    s2a = a0->v2a;
    s2c = a0->v2c;
    s4e0 = (int16_t)a0->pad4e[0];
    s4e1 = (int16_t)a0->pad4e[1];
    s52 = (int16_t)a0->pad4e[2];
    obj_script_run(a0);
    a1 = a0->prev;
    a1->v2a = a0->v2a;
    a1->v2c = a0->v2c;
    a1->pad4e[0] = a0->pad4e[0];
    a1->pad4e[1] = a0->pad4e[1];
    a1->pad4e[2] = a0->pad4e[2];
    a1->v54 = a0->v54;
    a0->pad4e[2] = (uint16_t)s52;
    a0->pad4e[1] = (uint16_t)s4e1;
    a0->pad4e[0] = (uint16_t)s4e0;
    a0->v2c = s2c;
    a0->v2a = s2a;
}

/* level3 $055D62: the tip of an arm: it takes its place and path like the pieces, and its picture follows the angle */
void l3_part_arm_tip(Object *a0)
{
    uint16_t d0, d1;

    l3_arm_follow(a0);
    obj_script_run(a0);
    d0 = (uint16_t)(((uint16_t)a0->v2c >> 2) & 0x38);
    if (a0->v54 == 0xe)
        v_0414 = 0xc;
    d1 = (uint16_t)(v_0cdc & 4);
    a0->gfx = GPTR(sh_r32(0x55dac + (uint16_t)(d0 + d1)));
    a0->anim_delay = 0;
    bbox_from_frame(a0);
}
