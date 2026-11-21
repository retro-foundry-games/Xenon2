/* level5: level 5's own code (the blob at $54E00 while level 5 is loaded), written like level4.c; a function has `level5 $ADDR` for its address. */
#include "weapons.h"
#include "shop.h"
#include "game_all.h"
#include <string.h>
#define FN(f) ((obj_fn)(f))
void l5_unit_update(Object *a0);

void l5_beam_draw(Object *a0);
void l5_beam_update(Object *a0);
void l5_boss2_dead(Object *a0);
void l5_boss2_draw(Object *a0);
void l5_boss2_flash(Object *a0);
void l5_boss_draw(Object *a0);
void l5_bridge_flash(Object *a0);
void l5_bridge_hit(Object *a0, Object *a1, uint16_t d0);
void l5_bridge_update(Object *a0);
void l5_cannon_update(Object *a0);
void l5_gunner_update(Object *a0);
void l5_gun_update(Object *a0);
void l5_missile_update(Object *a0);
void l5_pellet_update(Object *a0);
void l5_unit_flash(Object *a0);
void l5_unit_hit(Object *a0, Object *a1, uint16_t d0);
void l5_wall_flash_a(Object *a0);
void l5_wall_flash_b(Object *a0);
#define CANNON_BOB(o) ((o)->v2a)

/* the scratch fields of Object, by the kind that uses them */


/* the path script view of an object (ObjMove) */
#define PATH_WAIT(o) ((o)->v28)
#define PATH_ANGLE_HI(o) ((o)->v2a)
#define PATH_ANGLE_LO(o) ((o)->v2c)

/* the scratch fields of Object, by the kind that uses them */
#define BEAM_DY(o) ((o)->v2a)


/* the scratch fields of Object, by the kind that uses them */
#define BEAM_LENGTH(o) ((o)->v28)
#define BOSS_PHASE(o) ((o)->v28)
#define GUNNER_PHASE(o) ((o)->v2c)
#define L5_CELL(o) ((o)->v28)
#define L5_TURRET_DIR(o) ((o)->v2a)
#define MISSILE_DIR(o) ((o)->v2a)
#define MISSILE_TICK(o) ((o)->v28)
#define MOVER_STEP(o) ((o)->v2a)
#define MOVER_TICK(o) ((o)->v28)
#define PART_DX(o) ((o)->v3e)
#define PART_DY(o) ((o)->v40)
#define PELLET_DIR(o) ((o)->v2a)
#define PELLET_TICK(o) ((o)->v28)
#define UNIT_DIR(o) ((o)->v2a)
#define UNIT_GROUP(o) ((o)->v30)
#define UNIT_TICK(o) ((o)->v28)
#define WHEEL_ANGLE(o) ((o)->v28)


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

static void copy4(uint8_t *dst, const uint8_t *src)
{
    int i;

    for (i = 0; i < 4; i++)
        dst[i] = src[i];
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
    return GPTR(sh_r32(sh_r32(0x72ec8 + 4 * (uint32_t)index)));
}

/* the object follows its target ($564C4) */
static void follow_target_offset(Object *a0)
{
    Object *a1 = a0->target;

    a0->x = (int16_t)(a1->x + PART_DX(a0));
    a0->y = (int16_t)(a1->y + PART_DY(a0));
}

/* the burst at the hot spot of the picture of o (the end of the hits: $55D1C... $561F4) */
static void explode_at(const Object *o, const uint8_t *anim)
{
    const uint8_t *g = (const uint8_t *)o->gfx;
    int16_t d0 = (int16_t)(o->x - (int16_t)rd16(g));
    int16_t d1 = (int16_t)(o->y - (int16_t)rd16(g + 2));

    d0 = (int16_t)(d0 + d0);
    d1 = (int16_t)(d1 + d1);
    d0 = (int16_t)(d0 + (int16_t)rd16(g + 4));
    d1 = (int16_t)(d1 + (int16_t)rd16(g + 6));
    spawn_effect((int16_t)(d0 >> 1), (int16_t)(d1 >> 1), anim, draw_bob_masked);
}

static int dies(Object *a1, uint16_t d0)
{
    uint16_t old = (uint16_t)a1->energy;

    a1->energy = (int16_t)(old - d0);
    return old <= d0;
}

void l5_spawn_gunners(const uint8_t *a1);
void l5_spawn_cannon(const uint8_t *a1);
void l5_spawn_boss1(uint16_t d2);
void l5_spawn_boss2(const uint8_t *a1);
void l5_spawn_gates(const uint8_t *a1);
void l5_spawn_gun(const uint8_t *a1);
void l5_spawn_bridge(const uint8_t *a1);
void l5_spawn_group(const uint8_t *a1);
Object *l5_spawn_unit(const uint8_t *a1);

/* level5 $054E10, $057568: level events by d0: 0 the wave of $326C, 1 to 8 formations */
void l5_event(uint16_t d0, const uint8_t *a1)
{
    if (d0 == 0)
        spawn_wave_326c(0, a1);   /* $E5A */
    else if (d0 < 9)
        spawn_formation(0, a1, (const uint8_t *)GPTR(0x54e92 + 0x1a * (uint32_t)(d0 - 1)));
}

/* level5 $054E18, $0551AA: the hooks by d0 */
void l5_hook(uint16_t d0, const uint8_t *a1, uint16_t d2)
{
    switch (d0) {
    case 0:
        note_spawn_record(a1, d2);   /* $EA2 */
        break;
    case 1:
        l5_spawn_gates(a1);
        break;
    case 2:
        l5_spawn_gun(a1);
        break;
    case 3:
        l5_spawn_unit(a1);
        break;
    case 4:
        l5_spawn_bridge(a1);
        break;
    case 5:
        l5_spawn_boss2(a1);
        break;
    case 6:
        l5_spawn_boss1(d2);
        break;
    case 7:
        l5_spawn_gunners(a1);
        break;
    case 8:
        l5_spawn_cannon(a1);
        break;
    case 9:
        l5_spawn_group(a1);
        break;
    }
}

/* level5 $054E2C, $054E80: the two limits are kept for the swap */
void l5_swap_hook(void)
{
    sh_w16(0x62d22, sh_r16(0xd72));
    sh_w16(0x7491c, sh_r16(0xd76));
}

/* level5 $054E20, $0575EA: the data is copied once (backwards: the copy lies above its source) */
void l5_init(void)
{
    int i;

    if (level_init_src == (uint8_t *)GPTR(0x78d44)) {
        for (i = 11999; i >= 0; i--)
            ((uint8_t *)GPTR(0x7d120))[i] = ((const uint8_t *)GPTR(0x78d44))[i];
        level_init_src = (uint8_t *)GPTR(0x7d120);
    }
    scroll_min = 0;
    scroll_max = 0x11ff;
    sh_w8(0xd99, 0);
    sh_w8(0xd9a, 0);
    sh_w16(0xd72, 0x1200);
    sh_w16(0xd76, 0x11a0);
    sh_w8(0x55964, B(0x54e71));
    sh_w16(0x563c8, W(0x54e76));
    sh_w8(0xd9b, 0);
    sh_w8(0xd9c, 0);
    difficulty_x1 = (uint16_t)(difficulty * 0xc);
    difficulty_x2 = (uint16_t)(difficulty * 0xe);
}

/* ---- the gunners ($551E8): ten of them from a formation table ---- */

/* level5 $0551E8: a formation of ten that fly the path of the table by the record's type */
void l5_spawn_gunners(const uint8_t *a1)
{
    uint32_t a2 = sh_r32(0x551dc + 4 * (uint32_t)rd16(a1 + 6));
    int16_t d6 = 0;
    int d7;

    for (d7 = 9; d7 >= 0; d7--) {
        Object *a0 = insert_head(LIST_C);

        a0->type = 0xdc;
        a0->fn_update = FN(l5_gunner_update);
        a0->fn_draw = (obj_fn)draw_bob_masked;
        a0->fn_hit = (void *)hit_enemy;
        a0->gfx = GPTR(0x5c1ea);
        a0->anim_delay = 0;
        a0->x = (int16_t)W(a2 + 2);
        a0->y = (int16_t)(W(a2 + 4) - scroll_pos);
        a0->energy = (int16_t)W(0x54e42);
        a0->big = 0;
        PATH_ANGLE_HI(a0) = 0;
        PATH_ANGLE_LO(a0) = 0;
        a0->turn[0] = 0;
        a0->turn[1] = 0;
        a0->turn[2] = 0;
        a0->script = (uint8_t *)GPTR(a2);
        a0->cursor = (const uint8_t *)GPTR(a2 + 6);
        PATH_WAIT(a0) = d6;
        d6 = (int16_t)(d6 - (int16_t)W(0x54e44));
        a0->hit[0] = 0x3e8;
        a0->hit[2] = 0x3e8;
        a0->score = 0xc8;
        a0->kind = 0;
        a0->steps = (int16_t)W(0x54e46);
        a0->v5e_b[0] = (uint8_t)(d7 * 0x19);
    }
}

/* level5 $0552DA: a gunner: it flies its script ($EF6), its picture follows the direction, and now and then it fires all round */
void l5_gunner_update(Object *a0)
{
    uint16_t old, d0, d1;

    a0->y = (int16_t)(a0->y + scroll_speed);
    if (a0->y >= 0x190) {
        kill_thunk(a0);
        return;
    }
    old = (uint16_t)GUNNER_PHASE(a0);
    obj_script_run(a0);
    d0 = (uint16_t)(((uint16_t)(GUNNER_PHASE(a0) + 0x10) >> 3) & 0x1c);
    d1 = (uint16_t)(((uint16_t)(old + 0x10) >> 3) & 0x1c);
    if (d0 != d1)
        set_anim(a0, sh_r32(0x552ba + d0));
    if (add_carry(&a0->v5e_b[0], B(0x54e79))) {
        int d7;

        a0->v5e_b[0] = 0;
        for (d7 = 7; d7 >= 0; d7--)
            spawn_enemy_shot((uint16_t)a0->x, (uint16_t)a0->y, (uint16_t)d7, W(0x54e7a), shot_gfx(W(0x54e7c)));
    }
    bbox_from_frame(a0);
}

/* ---- the cannons ($55498) ---- */

/* level5 $055498: a cannon at the side that goes up and down; the record's $C tells the side */
void l5_spawn_cannon(const uint8_t *a1)
{
    uint16_t d0 = rd16(a1 + 0xc) ? 0x104 : 0x100;
    Object *a0 = spawn_marker_enemy(d0, a1, (void (*)(void))FN(l5_cannon_update));
    uint32_t a2 = 0x55516;

    a0->y = (int16_t)(a0->y - scroll_pos);
    a0->fn_draw = (obj_fn)draw_bob_masked;
    a0->fn_hit = (void *)hit_enemy;
    a0->energy = (int16_t)W(0x54e48);
    a0->big = 0xff;
    a0->hit[0] = 0x3e8;
    a0->hit[2] = 0x3e8;
    a0->score = 0xfa;
    if (a0->type != 0x100) {
        a0->x = (int16_t)(a0->x + 0x10);
        a2 = 0x5553a;
    }
    set_anim(a0, a2);
    a0->kind = 0;
    CANNON_BOB(a0) = 0x14;
}

/* the pellet: type $E4 of the cannon and the unit ($55682) */
static void spawn_pellet(int16_t d0, int16_t d1, uint16_t d2)
{
    Object *a1 = alloc_in(LIST_C);

    a1->type = 0xe4;
    a1->fn_update = FN(l5_pellet_update);
    a1->fn_draw = (obj_fn)draw_bob_masked;
    a1->fn_hit = (void *)hit_enemy;
    a1->x = d0;
    a1->y = d1;
    PELLET_DIR(a1) = (int16_t)d2;
    set_anim(a1, sh_r32(0x557de + 4 * (uint32_t)d2));
    a1->energy = (int16_t)W(0x54e4c);
    a1->big = 0;
    a1->score = 0x64;
    PELLET_TICK(a1) = (int16_t)(random_next() & 0x1f);
    a1->kind = 0;
    a1->hit[0] = 0x3e8;
    a1->hit[2] = 0x3e8;
}

#define CANNON_BOB(o) ((o)->v2a)

/* level5 $0555A6: the cannon: it moves down by the scroll and slowly up and down by v2a, and fires pellets */
void l5_cannon_update(Object *a0)
{
    int16_t d1;

    a0->y = (int16_t)(a0->y + scroll_speed);
    if (a0->y >= 0x190) {
        kill_thunk(a0);
        return;
    }
    anim_step(a0);
    d1 = (int16_t)scroll_pos;
    if (CANNON_BOB(a0) >= 0) {
        a0->y = (int16_t)(a0->y + 1);
        d1 = (int16_t)(d1 + a0->y);
        if (d1 >= 0xf20) {
            CANNON_BOB(a0) = -1;
            set_anim(a0, a0->type == 0x100 ? 0x5555e : 0x55582);
        }
    } else {
        a0->y = (int16_t)(a0->y - 1);
        d1 = (int16_t)(d1 + a0->y);
        if (d1 <= 0xe30) {
            CANNON_BOB(a0) = 1;
            set_anim(a0, a0->type == 0x100 ? 0x55516 : 0x5553a);
        }
    }
    if (add_carry(&a0->v5e_b[0], B(0x54e4b))) {
        int16_t d0 = (int16_t)(a0->x + 0x20);
        uint16_t d2 = 2;

        a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
        if (a0->type != 0x100) {
            d2 = 6;
            d0 = (int16_t)(d0 - 0x40);
        }
        spawn_pellet(d0, a0->y, d2);
    }
    bbox_from_frame(a0);
}

/* level5 $05581E: the pellet: it turns towards the player every eighth frame and goes; it ends in a burst */
void l5_pellet_update(Object *a0)
{
    int16_t dx, dy;
    uint16_t d0;

    anim_step(a0);
    PELLET_TICK(a0) = (int16_t)(PELLET_TICK(a0) + 1);
    d0 = (uint16_t)PELLET_TICK(a0);
    if ((int16_t)d0 < (int16_t)W(0x54e4e)) {
        if ((d0 & 7) == 0) {
            d0 = direction8((int16_t)(PLAYER->x - a0->x), (int16_t)(PLAYER->y - a0->y));
            d0 = (uint16_t)(d0 - (uint16_t)PELLET_DIR(a0));
            if (d0 != 0) {
                if ((int16_t)d0 < 0)
                    d0 = (uint16_t)(d0 + 8);
                PELLET_DIR(a0) = (int16_t)(PELLET_DIR(a0) + 1);
                if (d0 >= 4 && (d0 != 4 || !((uint16_t)a0->x & 1)))
                    PELLET_DIR(a0) = (int16_t)(PELLET_DIR(a0) - 2);
                PELLET_DIR(a0) = (int16_t)(PELLET_DIR(a0) & 7);
                set_anim(a0, sh_r32(0x557de + 4 * (uint32_t)PELLET_DIR(a0)));
            }
        }
        dx = (int16_t)W(0x557fe + 2 * (uint32_t)PELLET_DIR(a0));
        dy = (int16_t)(W(0x5580e + 2 * (uint32_t)PELLET_DIR(a0)) + scroll_speed);
        a0->x = (int16_t)(a0->x + dx);
        a0->y = (int16_t)(a0->y + dy);
        bbox_from_frame(a0);
        if (!point_in_player_box(dx, dy))
            return;
        if (shield_timer == 0) {
            PLAYER->fn_draw = (obj_fn)draw_bob_or_first;
            player_damage(a0, 4);
            kill_thunk(a0);
            return;
        }
    }
    explode_at(a0, anim_explosion);
    kill_thunk(a0);
}

/* ---- the big boss of 22 pieces ($55BFA) ---- */

/* level5 $055BFA: the boss: its picture is cells in the level's data, and the 22 pieces hang on the first */
void l5_spawn_boss1(uint16_t d2)
{
    uint8_t *p;
    uint32_t rec = 0x5594e;
    Object *a6 = 0;
    int d7, i;

    if (B(0xd99))
        return;
    sh_w8(0xd99, 0xff);
    scroll_y = 0x1a0;
    player_x_saved = 0xa0;
    player_y_saved = 0xb0;
    latch_slots(d2);
    sh_w16(0x62d22, 0xffff);
    sh_w16(0x7491c, 0xffff);
    sh_w16(0xd72, 0xffff);
    sh_w16(0xd76, 0xffff);
    p = level_init_src;
    memset(p, 0, 240);
    p += 240;
    for (d7 = 0; d7 < 18; d7++) {
        memset(p, 0, 6);
        p += 6;
        for (i = 0; i < 15; i++) {
            p[2 * i] = ((const uint8_t *)GPTR(0x57666 + 30 * (uint32_t)d7))[2 * i];
            p[2 * i + 1] = ((const uint8_t *)GPTR(0x57666 + 30 * (uint32_t)d7))[2 * i + 1];
        }
        p += 30;
        memset(p, 0, 4);
        p += 4;
    }
    memset(p, 0, 600);
    map_row_offset = 0x410;
    scroll_pos = 0x1a0;
    scroll_max = 0x1a0;
    for (d7 = 21; d7 >= 0; d7--, rec += 24) {
        Object *a0 = insert_head(LIST_C);

        if (a6 == 0)
            a6 = a0;
        a0->type = W(rec);
        a0->fn_update = GFN(sh_r32(rec + 2));
        a0->fn_draw = GFN(sh_r32(rec + 6));
        a0->fn_hit = GFN(sh_r32(rec + 10));
        set_anim(a0, sh_r32(rec + 14));
        a0->x = 0x30;
        a0->y = (int16_t)0xfec0;
        PART_DX(a0) = (int16_t)W(rec + 18);
        PART_DY(a0) = (int16_t)W(rec + 20);
        BOSS_PHASE(a0) = 0;
        a0->persists = 0xff;
        a0->v2a = 4;
        a0->energy = B(rec + 22);
        a0->big = B(rec + 23);
        a0->hit[0] = 0x3e8;
        a0->hit[2] = 0x3e8;
        a0->target = a6;
        a0->kind = 0;
    }
    sh_w16(0xd7a, 0x12);
}

/* level5 $055D1C: the flash of the boss, one frame */
void l5_boss_flash(Object *a0)
{
    a0->fn_draw = FN(l5_boss_draw);
    draw_tile_grid_flash((const uint8_t *)GPTR(0x57666), 0xe, 0x11, a0->y, (uint16_t)((uint16_t)a0->x >> 1));
}

/* level5 $055D3C: a gun of the boss: it hangs on the boss and its box is a bar */
void l5_boss_gun(Object *a0)
{
    int16_t d0, d1;

    follow_target_offset(a0);
    d0 = a0->x;
    d1 = a0->y;
    a0->hit[0] = d0;
    a0->hit[1] = d1;
    d0 = (int16_t)(d0 + a0->energy);
    d1 = (int16_t)((d1 & 0xff00) | (uint8_t)((uint8_t)d1 + a0->big));
    a0->hit[2] = d0;
    a0->hit[3] = d1;
}

/* level5 $055D62: a piece of the boss that animates */
void l5_boss_piece(Object *a0)
{
    anim_step(a0);
    follow_target_offset(a0);
    bbox_from_frame(a0);
}

/* level5 $055D6E: the last piece: it has a box when the others are gone */
void l5_boss_last(Object *a0)
{
    follow_target_offset(a0);
    if (sh_r16(0xd7a) == 0)
        bbox_from_frame(a0);
}

/* level5 $055D7E: the boss: the scroll stops at its place, it shifts left and right, and it shakes the screen and fires */
void l5_boss_update(Object *a0)
{
    scroll_max = 0x1a0;
    if (scroll_pos == 0) {
        scroll_pos_update(scroll_step);
        stars_scroll(scroll_step);
    }
    a0->y = (int16_t)(a0->y + scroll_speed);
    if (BOSS_PHASE(a0) == 0) {
        if (!add_carry(&a0->v5e_b[0], B(0x54e51)))
            return;
        a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
        sfx_cmd2 = -1;
        BOSS_PHASE(a0) = (frame_tick & 1) ? 2 : -2;
        return;
    }
    if (BOSS_PHASE(a0) > 0) {
        BOSS_PHASE(a0) = (int16_t)(BOSS_PHASE(a0) + 2);
        if (BOSS_PHASE(a0) == 0x14) {
            sfx_cmd2 = 0x13;
            return;
        }
        if (BOSS_PHASE(a0) == 0x28)
            goto fire;
        if (BOSS_PHASE(a0) == 0x3c)
            BOSS_PHASE(a0) = 0;
        return;
    }
    BOSS_PHASE(a0) = (int16_t)(BOSS_PHASE(a0) - 2);
    if (BOSS_PHASE(a0) == (int16_t)0xffec) {
        sfx_cmd2 = 0x13;
        return;
    }
    if (BOSS_PHASE(a0) == (int16_t)0xffd8)
        goto fire;
    if (BOSS_PHASE(a0) == (int16_t)0xffc4)
        BOSS_PHASE(a0) = 0;
    return;
fire:
    {
        int16_t d0 = (int16_t)(a0->x + 0x77), d1 = 0x65;
        Object *a1;

        sfx_stop_all();
        if (BOSS_PHASE(a0) < 0)
            d1 = 0xf5;
        d1 = (int16_t)(d1 + a0->y);
        a1 = insert_head(LIST_C);
        a1->type = 0xec;
        a1->fn_update = FN(l5_missile_update);
        a1->fn_draw = (obj_fn)draw_bob_masked;
        a1->fn_hit = (void *)hit_enemy;
        a1->x = d0;
        a1->y = d1;
        MISSILE_DIR(a1) = 0;
        set_anim(a1, 0x55f7e);
        a1->energy = (int16_t)W(0x54e52);
        a1->big = 0;
        a1->score = 0x96;
        BOSS_PHASE(a1) = 1;
        a1->kind = 0;
        a1->hit[0] = 0x3e8;
        a1->hit[2] = 0x3e8;
    }
}

#define L5_ARM_STROKE(o) ((o)->v28)

/* level5 $055EDA: the arms of the boss (v28 their stroke): the pictures are drawn at its side */
void l5_boss_draw(Object *a0)
{
    int16_t d1 = 0x55;
    int16_t d2 = L5_ARM_STROKE(a0);
    int16_t d0;

    if (d2 == 0)
        return;
    if (d2 < 0) {
        d1 = 0xe5;
        d2 = (int16_t)-d2;
    }
    d2 = (int16_t)(d2 & 0xfffc);
    d0 = (int16_t)(a0->x + 0x69);
    d1 = (int16_t)(d1 + a0->y);
    draw_bob_masked_at((uint16_t)d0, (uint16_t)d1, (const uint8_t *)GPTR(sh_r32(0x55f42 + (uint32_t)(uint16_t)d2)));
    if (d2 < 0x14 || d2 >= 0x28)
        return;
    d0 = (int16_t)(d0 + 0xe);
    d1 = (int16_t)(d1 + 0x10);
    draw_bob_masked_at((uint16_t)d0, (uint16_t)d1, (const uint8_t *)GPTR(sh_r32(0x55f1a + (uint32_t)(uint16_t)d2)));
}

/* level5 $05605E: the missile of the boss: it turns to the player by eighths, and goes; it hits the player or the screen's edge */
void l5_missile_update(Object *a0)
{
    int16_t d0, d1;
    uint16_t dir;

    anim_step(a0);
    MISSILE_TICK(a0) = (int16_t)(MISSILE_TICK(a0) + 1);
    if ((MISSILE_TICK(a0) & 7) == 0) {
        dir = direction8((int16_t)(PLAYER->x - a0->x), (int16_t)(PLAYER->y - a0->y));
        dir = (uint16_t)(dir - (uint16_t)MISSILE_DIR(a0));
        if (dir != 0) {
            if ((int16_t)dir < 0)
                dir = (uint16_t)(dir + 8);
            MISSILE_DIR(a0) = (int16_t)(MISSILE_DIR(a0) + 1);
            if (dir >= 4)
                MISSILE_DIR(a0) = (int16_t)(MISSILE_DIR(a0) - 2);
            MISSILE_DIR(a0) = (int16_t)(MISSILE_DIR(a0) & 7);
            set_anim(a0, sh_r32(0x5603e + 4 * (uint32_t)MISSILE_DIR(a0)));
        }
    }
    d0 = (int16_t)W(0x557fe + 2 * (uint32_t)MISSILE_DIR(a0));
    d1 = (int16_t)W(0x5580e + 2 * (uint32_t)MISSILE_DIR(a0));
    if (sh_r16(0xd7a) == 0) {
        d0 = (int16_t)(d0 + d0);
        d1 = (int16_t)(d1 + d1);
    }
    a0->x = (int16_t)(a0->x + d0);
    a0->y = (int16_t)(a0->y + d1);
    bbox_from_frame(a0);
    if (!player_hits(a0))
        return;
    if (shield_timer == 0) {
        PLAYER->fn_draw = (obj_fn)draw_bob_or_first;
        player_damage(a0, 4);
    }
    kill_thunk(a0);
}

/* level5 $05612A: a gun on the boss: its picture at $6158C at the frame 1 fires a beam; now and then it begins to fire */
void l5_turret_update(Object *a0)
{
    anim_step(a0);
    follow_target_offset(a0);
    if (a0->gfx == GPTR(0x6158c) && a0->anim_delay == 1) {
        Object *a1 = alloc_in(LIST_B);

        a1->type = 0x110;
        a1->fn_update = FN(l5_beam_update);
        a1->fn_draw = FN(l5_beam_draw);
        a1->x = (int16_t)(a0->x - 2);
        a1->y = (int16_t)(a0->y - 0x18);
        BEAM_LENGTH(a1) = 0;
        BEAM_DY(a1) = (int16_t)-(int16_t)W(0x54e5c);
    }
    if (a0->anim_delay == 0 && a0->y >= 0 && add_carry(&a0->v5e_b[0], B(0x54e55))) {
        a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
        a0->anim_delay = 1;
    }
    bbox_from_frame(a0);
}

/* level5 $0561D2: the hit of a piece of the boss: the burst and the score when its energy is gone */
void l5_piece_hit(Object *a0, Object *a1, uint16_t d0)
{
    (void)a0;
    a1->fn_draw = (obj_fn)draw_bob_or_first;
    if (!dies(a1, d0))
        return;
    sh_w16(0xd7a, (uint16_t)(sh_r16(0xd7a) - 1));
    player_score += 0xc8;
    explode_at(a1, a1->big ? big_explosion_anim : anim_explosion);
    if (a1->type != 0x114) {
        kill_thunk(a1);
    } else {
        a1->fn_update = FN(l5_boss2_dead);
        a1->hit[0] = 0x3e8;
        a1->hit[2] = 0x3e8;
        a1->fn_hit = (void *)obj_nop_thunk;
    }
}

/* level5 $0562A6: the hit of the heart of the boss: its picture follows its energy; at the end the level's objects go */
void l5_heart_hit(Object *a0, Object *a1, uint16_t d0)
{
    Object *a2 = a1->target;

    a1->fn_draw = (obj_fn)draw_bob_or_first;
    a2->fn_draw = FN(l5_boss_flash);
    if (!dies(a2, d0)) {
        uint32_t q = ((uint32_t)(uint16_t)a2->energy * 0xb) / B(0x55964);

        a1->gfx = GPTR(sh_r32(0x56276 + 4 * (uint32_t)(uint16_t)q));
        return;
    }
    memset(level_init_src + 0x1e0, 0, 720);
    free_object_chain(LIST_C->prev);
    spawn_bonuses(9);
    debris_burst(a0, 0x27, 0, 0, 0x140, 0xc0, big_explosion_anim);
    boss_dead = 0xff;
    memset(level_init_src, 0, 1560);
}

/* ---- the second boss of ten pieces ($5642A) ---- */

/* level5 $05642A: the second boss */
void l5_spawn_boss2(const uint8_t *a1)
{
    uint32_t rec = 0x5633a;
    Object *a6 = 0;
    int16_t d1;
    int d7;

    if (B(0xd9a))
        return;
    sh_w8(0xd9a, 0xff);
    d1 = (int16_t)(rd16(a1 + 0xa) - 8 - scroll_pos);
    for (d7 = 9; d7 >= 0; d7--, rec += 24) {
        Object *a0 = insert_head(LIST_C);

        if (a6 == 0)
            a6 = a0;
        a0->type = W(rec);
        a0->fn_update = GFN(sh_r32(rec + 2));
        a0->fn_draw = GFN(sh_r32(rec + 6));
        a0->fn_hit = GFN(sh_r32(rec + 10));
        a0->gfx = GPTR(sh_r32(rec + 14));
        a0->anim_delay = 0;
        a0->persists = 0xff;
        a0->x = 0x70;
        a0->y = d1;
        PART_DX(a0) = (int16_t)W(rec + 18);
        PART_DY(a0) = (int16_t)W(rec + 20);
        a0->v28 = 0;
        a0->v2a = 4;
        a0->energy = (int16_t)W(rec + 22);
        a0->hit[0] = 0x3e8;
        a0->hit[2] = 0x3e8;
        a0->target = a6;
        a0->kind = 0;
    }
}

/* level5 $0564E2: the big wheel: its picture turns by its energy */
void l5_wheel_update(Object *a0)
{
    int16_t d0;

    follow_target_offset(a0);
    d0 = (int16_t)(0x100 - a0->energy + WHEEL_ANGLE(a0));
    if (d0 >= 0x600)
        d0 = (int16_t)(d0 - 0x600);
    WHEEL_ANGLE(a0) = d0;
    a0->gfx = GPTR(sh_r32(0x5650e + (uint32_t)(((uint16_t)d0 >> 6) & 0xfffc)));
    bbox_from_frame(a0);
}

/* level5 $056526: the hit of the wheel: at the end the boss goes with its burst and bonuses */
void l5_wheel_hit(Object *a0, Object *a1, uint16_t d0)
{
    Object *a2 = a1->target;
    int i;

    a2->fn_draw = FN(l5_boss2_flash);
    if (!dies(a1, d0))
        return;
    free_object_chain(LIST_C->prev);
    debris_burst(a0, 0x13, 0x70, (uint16_t)(0x8d0 - scroll_pos), 0x60, 0xa0, big_explosion_anim);
    spawn_bonuses(4);
    for (i = 0; i < 10; i++)
        memset(level_init_src + 0x1614 + 0x28 * i, 0, 16);
    player_score += 0x5dc;
}

/* level5 $056594: a small turning part: its picture by the scroll */
void l5_spinner_update(Object *a0)
{
    uint32_t t = a0->type == 0x118 ? 0x565c2 : 0x565d2;

    follow_target_offset(a0);
    a0->gfx = GPTR(sh_r32(t + 4 * (uint32_t)(uint16_t)((uint16_t)(a0->y + scroll_pos) & 3)));
}

/* level5 $0565E2: a turret of the boss: it turns to the player and fires */
void l5_boss2_turret(Object *a0)
{
    uint16_t d0;

    follow_target_offset(a0);
    d0 = direction8((int16_t)(PLAYER->x - a0->x), (int16_t)(PLAYER->y - a0->y));
    L5_TURRET_DIR(a0) = (int16_t)d0;
    a0->gfx = GPTR(sh_r32(0x56658 + 4 * (uint32_t)d0));
    if (add_carry(&a0->v5e_b[0], B(0x54e57))) {
        a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
        spawn_enemy_shot((uint16_t)(a0->x + 8), (uint16_t)(a0->y + 8), (uint16_t)L5_TURRET_DIR(a0), W(0x54e58), shot_gfx(W(0x54e6e)));
    }
    bbox_from_frame(a0);
}

/* level5 $056678: the hit of the turret: it stops with a burst */
void l5_boss2_turret_hit(Object *a0, Object *a1, uint16_t d0)
{
    (void)a0;
    a1->fn_draw = (obj_fn)draw_bob_or_first;
    if (!dies(a1, d0))
        return;
    a1->fn_update = FN(l5_boss2_dead);
    a1->hit[0] = 0x3e8;
    a1->hit[2] = 0x3e8;
    a1->fn_hit = (void *)obj_nop_thunk;
    player_score += 0xc8;
    spawn_effect((int16_t)(a1->x + 8), (int16_t)(a1->y + 8), anim_explosion, draw_bob_masked);
}

/* level5 $0566CE: the part that is dead */
void l5_boss2_dead(Object *a0)
{
    follow_target_offset(a0);
    a0->gfx = GPTR(0x5df96);
    a0->hit[0] = 0x3e8;
    a0->hit[2] = 0x3e8;
}

/* level5 $0566E8: the mover of the second boss: it goes up and down by random, fires its beam and its pellets */
void l5_boss2_mover(Object *a0)
{
    int16_t d0;

    if ((int16_t)scroll_max < 0x920)
        scroll_max = 0x920;
    a0->y = (int16_t)(a0->y + scroll_speed);
    if (MOVER_STEP(a0) != 0) {
        sfx_cmd = 0x16;
        if (MOVER_STEP(a0) > 0) {
            a0->y = (int16_t)(a0->y + 1);
            MOVER_STEP(a0) = (int16_t)(MOVER_STEP(a0) - 1);
        } else {
            a0->y = (int16_t)(a0->y - 1);
            MOVER_STEP(a0) = (int16_t)(MOVER_STEP(a0) + 1);
        }
    } else {
        uint32_t r = random_next();

        if ((uint8_t)r < 0xf) {
            if (r & 1) {
                d0 = (int16_t)(0x910 - a0->y - scroll_pos);
                if (d0 != 0)
                    MOVER_STEP(a0) = (int16_t)(random_below((uint16_t)d0) + 1);
            } else {
                d0 = (int16_t)(a0->y + scroll_pos - 0x8d0);
                if (d0 != 0)
                    MOVER_STEP(a0) = (int16_t)-(int16_t)(random_below((uint16_t)d0) + 1);
            }
        }
    }
    if (MOVER_TICK(a0) == 0) {
        if (!add_carry(&a0->v5e_b[0], B(0x54e5b)))
            goto pellets;
        a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
    }
    MOVER_TICK(a0) = (int16_t)(MOVER_TICK(a0) + 1);
    if (MOVER_TICK(a0) == 9) {
        MOVER_TICK(a0) = 0;
    } else if (MOVER_TICK(a0) == 3) {
        Object *a1;

        sfx_cmd = 0x84;
        a1 = alloc_in(LIST_B);
        a1->type = 0x110;
        a1->fn_update = FN(l5_beam_update);
        a1->fn_draw = FN(l5_beam_draw);
        a1->x = 0x96;
        a1->y = (int16_t)(a0->y + 0x30);
        MOVER_TICK(a1) = 0;
        MOVER_STEP(a1) = (int16_t)W(0x54e5c);
    }
pellets:
    if (add_carry(&a0->v5e_b[1], B(0x54e7f))) {
        a0->v5e_b[1] = (uint8_t)(random_next() & 0x3f);
        spawn_pellet(0x6e, (int16_t)(a0->y + 0x30), 6);
        spawn_pellet(0xd2, (int16_t)(a0->y + 0x30), 2);
    }
}

/* level5 $056840: the flash of the mover, one frame */
void l5_boss2_flash(Object *a0)
{
    a0->fn_draw = FN(l5_boss2_draw);
    draw_tile_grid_flash((const uint8_t *)GPTR(0x568f8), 5, 5, a0->y, 0x38);
}

#define MOVER_CELL(o) ((o)->v28)

/* level5 $05685A: the cells of the mover: three of them change with v28 */
void l5_boss2_draw(Object *a0)
{
    uint32_t d0 = (uint32_t)(uint16_t)MOVER_CELL(a0) * 0xc;

    sh_w32(0x56908, sh_r32(0x5688c + d0));
    sh_w32(0x56914, sh_r32(0x56890 + d0));
    sh_w32(0x56920, sh_r32(0x56894 + d0));
    draw_tile_grid((const uint8_t *)GPTR(0x568f8), 5, 5, a0->y, 0x38);
}

/* level5 $056940: the beam: it grows to its length, then it goes; it hurts the player */
void l5_beam_update(Object *a0)
{
    int16_t d0;

    a0->hit[0] = a0->x;
    a0->hit[2] = (int16_t)(a0->x + 0xb);
    if (BEAM_LENGTH(a0) != 0x30) {
        BEAM_LENGTH(a0) = (int16_t)(BEAM_LENGTH(a0) + (int16_t)W(0x54e5c));
        if (BEAM_LENGTH(a0) > 0x30)
            BEAM_LENGTH(a0) = 0x30;
        a0->y = (int16_t)(a0->y + scroll_speed);
        if (BEAM_DY(a0) < 0)
            a0->y = (int16_t)(a0->y - (int16_t)W(0x54e5c));
    } else {
        a0->y = (int16_t)(a0->y + BEAM_DY(a0));
        d0 = a0->y;
        if (BEAM_DY(a0) >= 0) {
            if (d0 >= 0xc0)
                goto gone;
        } else {
            d0 = (int16_t)(d0 + BEAM_LENGTH(a0));
            if (d0 < 0)
                goto gone;
        }
    }
    a0->hit[1] = a0->y;
    a0->hit[3] = (int16_t)(a0->y + BEAM_LENGTH(a0));
    if (!player_hits(a0))
        return;
    if (shield_timer == 0) {
        player_damage(a0, 6);
        PLAYER->fn_draw = (obj_fn)draw_bob_or_first;
    }
gone:
    kill_thunk(a0);
}

/* level5 $056A10: the beam's heads and the bits of its body in the screen's planes */
void l5_beam_draw(Object *a0)
{
    uint8_t *screen = draw_screen, *a1, *limit = screen + 0x7800;
    uint32_t d0 = 0xfff00000u, d1 = 0xbfd00000u, d2 = 0xfff00000u, d3 = 0xc0300000u;
    uint16_t sh;
    uint32_t r0, r1, r2, r3, r4, r5;
    int16_t d6, d7;

    if (BEAM_DY(a0) < 0 || BEAM_LENGTH(a0) == 0x30)
        draw_bob_masked_at((uint16_t)a0->x, (uint16_t)a0->y, (const uint8_t *)GPTR(0x5f2fe));
    if (BEAM_DY(a0) >= 0 || BEAM_LENGTH(a0) == 0x30)
        draw_bob_masked_at((uint16_t)a0->x, (uint16_t)(a0->y + BEAM_LENGTH(a0) - 1), (const uint8_t *)GPTR(0x5f340));
    a1 = screen + (((uint16_t)a0->x >> 3) & 0xfffe);
    sh = (uint16_t)a0->x & 0xf;
    d0 >>= sh;
    d1 >>= sh;
    d2 >>= sh;
    d3 >>= sh;
    d0 = ~d0;
    r0 = ((d2 >> 16) << 16) | (d3 >> 16);
    r1 = ((d1 >> 16) << 16) | (d2 >> 16);
    r2 = ((d0 >> 16) << 16) | (d0 >> 16);
    r3 = ((d2 & 0xffff) << 16) | (d3 & 0xffff);
    r4 = ((d1 & 0xffff) << 16) | (d2 & 0xffff);
    r5 = ((d0 & 0xffff) << 16) | (d0 & 0xffff);
    d7 = BEAM_LENGTH(a0);
    d6 = a0->y;
    if (d6 < 0) {
        d7 = (int16_t)(d7 + d6);
        if (d7 < 0)
            return;
        d6 = 0;
    }
    a1 += (uint32_t)((uint16_t)d6 * 0xa0u);
    d0 = r0;
    d1 = r1;
    d2 = r2;
    d3 = r3;
    {
        uint32_t d4 = r4, d5 = r5;

        while (a1 < limit && d7 != 0) {
            uint16_t m2 = (uint16_t)d2, h1 = (uint16_t)(d1 >> 16), l1 = (uint16_t)d1, h0 = (uint16_t)(d0 >> 16), l0 = (uint16_t)d0;
            int p;

            for (p = 0; p < 4; p++)
                wr16(a1 + 0x28 * p, (uint16_t)(rd16(a1 + 0x28 * p) & m2));
            wr16(a1, (uint16_t)(rd16(a1) | h1));
            wr16(a1 + 0x28, (uint16_t)(rd16(a1 + 0x28) | l1));
            wr16(a1 + 0x50, (uint16_t)(rd16(a1 + 0x50) | h0));
            wr16(a1 + 0x78, (uint16_t)(rd16(a1 + 0x78) | l0));
            for (p = 0; p < 4; p++)
                wr16(a1 + 2 + 0x28 * p, (uint16_t)(rd16(a1 + 2 + 0x28 * p) & (uint16_t)d5));
            wr16(a1 + 2, (uint16_t)(rd16(a1 + 2) | (uint16_t)(d4 >> 16)));
            wr16(a1 + 0x2a, (uint16_t)(rd16(a1 + 0x2a) | (uint16_t)d4));
            wr16(a1 + 0x52, (uint16_t)(rd16(a1 + 0x52) | (uint16_t)(d3 >> 16)));
            wr16(a1 + 0x7a, (uint16_t)(rd16(a1 + 0x7a) | (uint16_t)d3));
            a1 += 0xa0;
            d7--;
        }
    }
}

/* ---- the gates and the guns of tiles ---- */

/* level5 $056B50: three tiles that open and close (the record's position is the cell) */
void l5_spawn_gates(const uint8_t *a1)
{
    int16_t d0 = (int16_t)(rd16(a1 + 8) - 8), d1 = (int16_t)(rd16(a1 + 0xa) - 8);
    uint8_t *a2 = level_init_src + (uint16_t)(((uint16_t)d0 >> 3) & 0xfe) + (int16_t)(uint16_t)((uint32_t)((uint16_t)d1 >> 4) * 0x28);
    uint32_t rec = 0x56b2c;
    int d7;

    wr16(a2, 0x8ad3);
    wr32(a2 + 2, 0);
    wr16(a2 + 6, 0x8add);
    for (d7 = 2; d7 >= 0; d7--, rec += 12) {
        Object *a0 = insert_head(LIST_C);
        uint16_t d2;

        a0->type = W(rec);
        a0->fn_update = GFN(sh_r32(rec + 2));
        a0->fn_draw = (obj_fn)obj_nop_thunk;
        a0->fn_hit = GFN(sh_r32(rec + 6));
        a0->x = d0;
        a0->y = d1;
        L5_CELL(a0) = 0;
        a0->energy = (int16_t)W(0x54e5e);
        a0->hit[0] = 0x3e8;
        a0->hit[2] = 0x3e8;
        a0->script = a2;
        d2 = W(rec + 10);
        a2 += (int16_t)d2;
        d0 = (int16_t)(d0 + (int16_t)(d2 << 3));
    }
}

/* level5 $056BEE: a gate: its cells go round */
void l5_gate_update(Object *a0)
{
    int16_t d0 = (int16_t)(L5_CELL(a0) + 4);

    if (d0 >= 0x10)
        d0 = 0;
    L5_CELL(a0) = d0;
    copy4(a0->script, (const uint8_t *)GPTR(0x56c30 + (uint32_t)d0));
    a0->hit[0] = a0->x;
    a0->hit[2] = (int16_t)(a0->x + 0x20);
    a0->hit[1] = (int16_t)(a0->y - scroll_pos + 2);
    a0->hit[3] = (int16_t)(a0->hit[1] + 0xc);
}

/* level5 $056C40: a gun in the wall: its cell by four */
void l5_wall_update(Object *a0)
{
    uint32_t a2 = a0->type == 0x128 ? 0x56c94 : 0x56c9c;

    L5_CELL(a0) = (int16_t)((L5_CELL(a0) + 1) & 3);
    wr16(a0->script, W(a2 + 2 * (uint32_t)L5_CELL(a0)));
    a0->hit[0] = (int16_t)(a0->x + 3);
    a0->hit[2] = (int16_t)(a0->hit[0] + 0xa);
    a0->hit[1] = (int16_t)(a0->y - scroll_pos + 3);
    a0->hit[3] = (int16_t)(a0->hit[1] + 0xa);
}

/* level5 $056CA4: the hit of the gun on the left: at the end it and the two before it go */
void l5_wall_hit_a(Object *a0, Object *a1, uint16_t d0)
{
    Object *a2, *a3;
    uint8_t *c;

    (void)a0;
    a1->fn_draw = FN(l5_wall_flash_a);
    if (!dies(a1, d0))
        return;
    spawn_effect((int16_t)(a1->x + 8), (int16_t)(a1->y - scroll_pos + 8), anim_explosion, draw_bob_masked);
    c = a1->script;
    wr16(c, 0x87b5);
    wr32(c + 2, 0);
    wr16(c + 6, 0x87ab);
    a2 = a1->prev;
    kill_thunk(a1);
    a3 = a2->prev;
    kill_thunk(a2);
    kill_thunk(a3);
    player_score += 0xc8;
}

/* level5 $056D5C: the hit of the gun on the right: it and the two after it go */
void l5_wall_hit_b(Object *a0, Object *a1, uint16_t d0)
{
    Object *a2, *a3;
    uint8_t *c;

    (void)a0;
    a1->fn_draw = FN(l5_wall_flash_b);
    if (!dies(a1, d0))
        return;
    spawn_effect((int16_t)(a1->x + 8), (int16_t)(a1->y - scroll_pos + 8), anim_explosion, draw_bob_masked);
    c = a1->script;
    wr16(c, 0x87ab);
    wr32(c - 4, 0);
    wr16(c - 6, 0x87b5);
    a2 = a1->next;
    kill_thunk(a1);
    a3 = a2->next;
    kill_thunk(a2);
    kill_thunk(a3);
    player_score += 0xc8;
}

/* level5 $056E14: the flash of the gun on the left, one frame */
void l5_wall_flash_a(Object *a0)
{
    a0->fn_draw = (obj_fn)obj_nop_thunk;
    draw_tile_grid_flash((const uint8_t *)GPTR(0x56e3c), 0, 0, (int16_t)(a0->y - scroll_pos), (uint16_t)((uint16_t)a0->x >> 1));
}

/* level5 $056E1A: the flash of the gun on the right, one frame */
void l5_wall_flash_b(Object *a0)
{
    a0->fn_draw = (obj_fn)obj_nop_thunk;
    draw_tile_grid_flash((const uint8_t *)GPTR(0x56e3e), 0, 0, (int16_t)(a0->y - scroll_pos), (uint16_t)((uint16_t)a0->x >> 1));
}

/* ---- the bridge pieces ($56E40) ---- */

/* level5 $056E40: a piece of the bridge */
void l5_spawn_bridge(const uint8_t *a1)
{
    Object *a0 = spawn_marker_enemy(0xf8, a1, (void (*)(void))FN(l5_bridge_update));
    uint8_t *c;

    a0->fn_hit = FN(l5_bridge_hit);
    a0->energy = (int16_t)W(0x54e60);
    a0->hit[0] = 0x3e8;
    a0->hit[2] = 0x3e8;
    c = a0->script;
    if (rd16(c - 2) == 0x8669) {
        wr16(c - 2, 0xc7);
        wr16(c + 0x26, 0x177);
    }
    if (rd16(c + 4) == 0x8673) {
        wr16(c + 4, 0xc7);
        wr16(c + 0x2c, 0x177);
    }
    copy4(c, (const uint8_t *)GPTR(0x56ea2));
    copy4(c + 0x28, (const uint8_t *)GPTR(0x56ea6));
}

#define BRIDGE_CELL(o) ((o)->v28)

/* level5 $056EE2: the piece: its cells go round by v28 and now and then it fires all round */
void l5_bridge_update(Object *a0)
{
    uint8_t *c;
    uint32_t d0;

    if ((int16_t)(scroll_max + 0xd0) < a0->y) {
        kill_thunk(a0);
        return;
    }
    a0->hit[0] = (int16_t)(a0->x + 4);
    a0->hit[2] = (int16_t)(a0->hit[0] + 0x18);
    a0->hit[1] = (int16_t)(a0->y - scroll_pos + 4);
    a0->hit[3] = (int16_t)(a0->hit[1] + 0x18);
    BRIDGE_CELL(a0) = (int16_t)((BRIDGE_CELL(a0) + 1) & 0xf);
    if (add_carry(&a0->v5e_b[0], B(0x54e73))) {
        int d7;

        a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
        for (d7 = 7; d7 >= 0; d7--) {
            Object *a1 = spawn_enemy_shot((uint16_t)(a0->x + 0x10), (uint16_t)(a0->y - scroll_pos + 0x10), (uint16_t)d7, W(0x54e74), 0);

            set_anim(a1, 0x56fb8);
        }
    }
    c = a0->script;
    d0 = (uint32_t)((uint16_t)BRIDGE_CELL(a0) & 0xe) * 4;
    copy4(c, (const uint8_t *)GPTR(0x56ea2 + d0));
    copy4(c + 0x28, (const uint8_t *)GPTR(0x56ea6 + d0));
}

/* level5 $056FDC: the hit of the piece: the burst, the cells go back, the score */
void l5_bridge_hit(Object *a0, Object *a1, uint16_t d0)
{
    uint8_t *c;

    (void)a0;
    a1->fn_draw = FN(l5_bridge_flash);
    if (!dies(a1, d0))
        return;
    spawn_effect((int16_t)(a1->x + 0x10), (int16_t)(a1->y - scroll_pos + 0x10), big_explosion_anim, draw_bob_masked);
    c = a1->script;
    if (rd16(c - 2) == 0xc7) {
        wr16(c - 2, 0x8669);
        wr16(c + 0x26, 0x8707);
    }
    if (rd16(c + 4) == 0xc7) {
        wr16(c + 4, 0x8673);
        wr16(c + 0x2c, 0x8711);
    }
    wr32(c, 0);
    wr32(c + 0x28, 0);
    player_score += 0x1f4;
    kill_thunk(a1);
}

/* level5 $05706C: the flash of the piece, one frame */
void l5_bridge_flash(Object *a0)
{
    a0->fn_draw = (obj_fn)obj_nop_thunk;
    draw_tile_grid_flash((const uint8_t *)GPTR(0x56ea2), 1, 1, (int16_t)(a0->y - scroll_pos), (uint16_t)((uint16_t)a0->x >> 1));
}

/* ---- the units of eight directions ($570AA) ---- */

/* level5 $057090: the unit of the record's group, once (a flag by the group) */
void l5_spawn_group(const uint8_t *a1)
{
    uint16_t d0 = rd16(a1 + 6);

    if (sh_r8(0xd9b + d0) == 0)
        UNIT_GROUP(l5_spawn_unit(a1)) = (int16_t)d0;
}

/* level5 $0570AA: a unit of eight directions made of cells */
Object *l5_spawn_unit(const uint8_t *a1)
{
    Object *a0 = spawn_marker_enemy(0xf4, a1, (void (*)(void))FN(l5_unit_update));
    uint8_t *c;
    uint32_t d0;

    UNIT_DIR(a0) = (int16_t)rd16(a1 + 0xc);
    a0->fn_hit = FN(l5_unit_hit);
    a0->energy = (int16_t)W(0x54e62);
    a0->hit[0] = 0x3e8;
    a0->hit[2] = 0x3e8;
    c = a0->script;
    d0 = (uint32_t)(uint16_t)UNIT_DIR(a0) << 3;
    UNIT_GROUP(a0) = -1;
    if (rd16(c - 2) == 0x867d) {
        wr16(c - 2, 0x80d7);
        wr16(c + 0x26, 0x8187);
    }
    if (rd16(c + 4) == 0x8687) {
        wr16(c + 4, 0x8287);
        wr16(c + 0x2c, 0x8339);
    }
    copy4(c, (const uint8_t *)GPTR(0x5711e + d0));
    copy4(c + 0x28, (const uint8_t *)GPTR(0x57122 + d0));
    return a0;
}

/* level5 $05715E: the unit: it turns to the player by one step at a time and fires in its direction */
void l5_unit_update(Object *a0)
{
    uint8_t *c;
    uint32_t d0;

    if ((int16_t)(scroll_max + 0xd0) < a0->y) {
        kill_thunk(a0);
        return;
    }
    a0->hit[0] = (int16_t)(a0->x + 4);
    a0->hit[2] = (int16_t)(a0->hit[0] + 0x18);
    a0->hit[1] = (int16_t)(a0->y - scroll_pos + 4);
    a0->hit[3] = (int16_t)(a0->hit[1] + 0x18);
    if (UNIT_TICK(a0) == 0 && (int16_t)(a0->y - scroll_pos) < -16)
        goto cells;
    if (UNIT_TICK(a0) != 8) {
        UNIT_TICK(a0) = (int16_t)(UNIT_TICK(a0) + 1);
        UNIT_DIR(a0) = (int16_t)((UNIT_DIR(a0) + 1) & 7);
        goto cells;
    }
    {
        uint16_t dir = direction8((int16_t)(PLAYER->x - a0->x - 0x10), (int16_t)(PLAYER->y - a0->y + scroll_pos - 0x10));

        dir = (uint16_t)(dir - (uint16_t)UNIT_DIR(a0));
        if (dir != 0) {
            dir &= 7;
            if (dir < 4)
                UNIT_DIR(a0) = (int16_t)(UNIT_DIR(a0) + 1);
            else
                UNIT_DIR(a0) = (int16_t)(UNIT_DIR(a0) - 1);
        }
        UNIT_DIR(a0) = (int16_t)(UNIT_DIR(a0) & 7);
    }
    if (add_carry(&a0->v5e_b[0], B(0x54e65))) {
        int16_t x = (int16_t)(a0->x + 0x10), y = (int16_t)(a0->y - scroll_pos + 0x10);
        uint16_t d3 = (uint16_t)((UNIT_DIR(a0) * 4) & 0x1c);

        a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
        x = (int16_t)(x + (int16_t)W(0x572aa + d3));
        y = (int16_t)(y + (int16_t)W(0x572ac + d3));
        d3 &= 0xc;
        if (!(frame_tick & 1))
            d3 += 0x10;
        spawn_enemy_shot((uint16_t)x, (uint16_t)y, (uint16_t)UNIT_DIR(a0), W(0x54e66), GPTR(sh_r32(0x572ca + d3)));
    }
cells:
    c = a0->script;
    d0 = (uint32_t)(uint16_t)UNIT_DIR(a0) << 3;
    copy4(c, (const uint8_t *)GPTR(0x5711e + d0));
    copy4(c + 0x28, (const uint8_t *)GPTR(0x57122 + d0));
}

/* level5 $0572EA: the hit of the unit: at the end the burst, its flag, the cells go back */
void l5_unit_hit(Object *a0, Object *a1, uint16_t d0)
{
    uint8_t *c;

    (void)a0;
    a1->fn_draw = FN(l5_unit_flash);
    if (!dies(a1, d0))
        return;
    if (UNIT_GROUP(a1) >= 0)
        sh_w8(0xd9b + (uint32_t)(uint16_t)UNIT_GROUP(a1), 0xff);
    spawn_effect((int16_t)(a1->x + 0x10), (int16_t)(a1->y - scroll_pos + 0x10), big_explosion_anim, draw_bob_masked);
    c = a1->script;
    if (rd16(c - 2) == 0x80d7) {
        wr16(c - 2, 0x867d);
        wr16(c + 0x26, 0x871b);
    }
    if (rd16(c + 4) == 0x8287) {
        wr16(c + 4, 0x8687);
        wr16(c + 0x2c, 0x8725);
    }
    wr32(c, 0);
    wr32(c + 0x28, 0);
    player_score += 0x190;
    kill_thunk(a1);
}

/* level5 $057388: the flash of the unit, one frame */
void l5_unit_flash(Object *a0)
{
    a0->fn_draw = (obj_fn)obj_nop_thunk;
    draw_tile_grid_flash((const uint8_t *)GPTR(0x5711e), 1, 1, (int16_t)(a0->y - scroll_pos), (uint16_t)((uint16_t)a0->x >> 1));
}

/* ---- the guns of cells ($573AC) ---- */

/* level5 $0573AC: a gun in the wall that fires out */
void l5_spawn_gun(const uint8_t *a1)
{
    int right = rd16(a1 + 0xc) != 0;
    Object *a0 = spawn_marker_enemy(right ? 0x10c : 0x108, a1, (void (*)(void))FN(l5_gun_update));
    uint32_t a2 = right ? 0x57514 : 0x57524, a4 = right ? 0x57536 : 0x57534;

    a0->fn_hit = (void *)hit_tile;
    a0->energy = (int16_t)W(0x54e68);
    a0->hit[0] = 0x3e8;
    a0->hit[2] = 0x3e8;
    a0->gfx = GPTR(a2);
    a0->anim = (const uint8_t *)GPTR(a4);
    wr16(a0->script, W(a2));
    a0->score = 0x64;
    PART_DX(a0) = 0;
    PART_DY(a0) = 0;
}

/* level5 $05741C: the gun: its cell goes round and it fires every third step */
void l5_gun_update(Object *a0)
{
    uint32_t a2;

    if ((int16_t)(scroll_max + 0xd0) < a0->y) {
        kill_thunk(a0);
        return;
    }
    a0->hit[0] = (int16_t)(a0->x + 2);
    a0->hit[2] = (int16_t)(a0->hit[0] + 0xc);
    a0->hit[1] = (int16_t)(a0->y - scroll_pos + 2);
    a0->hit[3] = (int16_t)(a0->hit[1] + 0xc);
    if (L5_CELL(a0) == 0) {
        if (!add_carry(&a0->v5e_b[0], B(0x54e6b)))
            return;
        a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
    }
    L5_CELL(a0) = (int16_t)(L5_CELL(a0) + 1);
    a2 = a0->type == 0x108 ? 0x57524 : 0x57514;
    wr16(a0->script, W(a2 + 2 * (uint32_t)(uint16_t)L5_CELL(a0)));
    if (L5_CELL(a0) == 3) {
        int16_t d0 = (int16_t)(a0->x + 0x18), d1 = (int16_t)(a0->y - scroll_pos + 8);
        uint16_t d2 = 2;
        Object *a1;

        if (a0->type != 0x108) {
            d2 = 6;
            d0 = (int16_t)(d0 - 0x18);
        }
        a1 = spawn_enemy_shot((uint16_t)d0, (uint16_t)d1, d2, W(0x54e6c), 0);
        set_anim(a1, a0->type == 0x108 ? 0x57538 : 0x57550);
        return;
    }
    if (L5_CELL(a0) == 7)
        L5_CELL(a0) = 0;
}
