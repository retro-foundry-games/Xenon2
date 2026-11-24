/* weapons: the weapons, pickups and the player of $4000-$5FFF. */
#include "weapons.h"
#include "shop.h"
#include "game_all.h"
#include "weapon_util.h"
#include "amiga/hw.h"

/* the scratch fields of Object, by the kind that uses them */


/* a weapon object sits at its offset from the player */
/* a tile block object: its size in cells less one */

/* the scratch fields of Object, by the kind that uses them */



/* orig $005C22: the object of list B (type $40) that belongs to pod, or the sentinel */
static Object *find_decoration(Object *pod)
{
    Object *deco = xenon2.obj_list[LIST_EFFECT].prev;

    while (deco->type != 0) {
        if (deco->type == 0x40 && deco->link.ptr == pod)
            return deco;
        deco = deco->prev;
    }
    return deco;
}

/* the weapon slot that owns an object is freed (fn_hit is its address); an object that no longer has one (a dying shot: fn_hit is obj_nop) makes the original clear a word of its own code, which has no effect */
static void slot_free(void *fn_hit)
{
    Slot *s = (Slot *)fn_hit;

    if ((s >= xenon2.weapon_slots && s < xenon2.weapon_slots + 7) || (s >= xenon2.weapon_slots_saved && s < xenon2.weapon_slots_saved + 7))
        s->type = 0;
}

/* orig $005C3E: obj and what belongs to it die */
void kill_with_slot(Object *obj)
{
    Object *deco;

    if (obj->type == 0x34) {
        deco = find_decoration(obj);
        if (deco->type != 0)
            kill_object(deco);
    }
    slot_free(obj->fn_hit);
    kill_object(obj);
}

/* orig $005C9E: obj and what belongs to it go back to the free list */
void free_with_slot(Object *obj)
{
    Object *deco;

    if (obj->type == 0x34) {
        deco = find_decoration(obj);
        if (deco->type != 0)
            free_object(deco);
    }
    slot_free(obj->fn_hit);
    free_object(obj);
}

/* the slot's object grows: orig $004B94 (slot 0), $004D14 (slot 6), $005000 (slot 5) */
static Object *grow_weapon(Slot *s)
{
    Object *obj = s->obj;

    if (obj->energy > obj->score)
        obj->score++;
    return obj;
}

/* orig $004F36: the weapon of slot 5 is dismissed */
static void dismiss_slot5(void)
{
    if (xenon2.weapon_slots[5].type != 0)
        kill_object(xenon2.weapon_slots[5].obj);
}

/* orig $005540: the basic weapon comes back when slot 0 is empty */
Object *refill_slot0(Object *caller, Object *last)
{
    if (xenon2.weapon_slots[0].type == 0)
        give_forward_shot(caller, last);
    return last;
}

/* orig $0052D0 (with $23F0): the first free of the four side slots, or 0; *rev_index = 3 - its index */
static Slot *free_side_slot(int *rev_index)
{
    int i;

    for (i = 0; i < 4; i++) {
        if (xenon2.weapon_slots[1 + i].type == 0) {
            *rev_index = 3 - i;
            return &xenon2.weapon_slots[1 + i];
        }
    }
    return 0;
}

/* orig $004AFC: weapon $94 (slot 0); returns last */
Object *give_forward_shot(Object *caller, Object *last)
{
    Slot *s = &xenon2.weapon_slots[0];
    Object *n;

    if (s->type != 0)
        kill_object(s->obj);
    s->type = 0x94;
    n = alloc_a(caller);
    s->obj = n;
    n->type = 0x94;
    n->fn_update = (obj_fn)forward_shot_update;
    n->fn_hit = s;
    n->fn_draw = obj_nop;
    n->score = 0;
    n->energy = 2;
    n->kind = 1;
    n->shot_count = n->shot_rate = 0xff;
    xenon2.autofire_period = 8;
    return n;
}

/* orig $004BA8: weapon $C0 (slot 0) */
Object *give_double_shot(Object *caller, Object *last)
{
    Slot *s = &xenon2.weapon_slots[0];
    Object *n;

    if (s->type == 0xc0)
        return grow_weapon(s);
    if (s->type != 0)
        kill_object(s->obj);
    s->type = 0xc0;
    n = alloc_a(caller);
    s->obj = n;
    n->type = 0xc0;
    n->fn_update = double_shot_update;
    n->fn_draw = draw_bob_masked;
    n->fn_hit = s;
    n->gfx = xenon2.weaponc0_picture;
    n->anim_delay = 0;
    n->x = xenon2.player.x;
    n->y = xenon2.player.y;
    n->score = 0;
    n->energy = 2;
    n->kind = 0xe;
    n->shot_count = n->shot_rate = 0xff;
    xenon2.autofire_period = 8;
    return n;
}

/* orig $004C64: weapon $98 (slot 6; slot 5 loses $44) */
Object *give_side_shot(Object *caller, Object *last)
{
    Slot *s5 = &xenon2.weapon_slots[5], *s6 = &xenon2.weapon_slots[6];
    Object *n;

    if (s6->type == 0x98)
        return grow_weapon(s6);
    if (s6->type != 0)
        return last;
    if (s5->type == 0x44) {
        kill_object(s5->obj);
        s5->type = 0;
    }
    n = alloc_a(caller);
    s6->type = 0x98;
    s6->obj = n;
    n->type = 0x98;
    n->fn_update = (obj_fn)side_shot_update;
    n->fn_draw = obj_nop;
    n->fn_hit = s6;
    n->score = 0;
    n->energy = 2;
    n->kind = 0xa;
    n->shot_count = n->shot_rate = 0xff;
    xenon2.autofire_period = 8;
    return n;
}

/* orig $004D28: weapon $28 (slot 6) */
Object *give_bomb(Object *caller, Object *last)
{
    Slot *s6 = &xenon2.weapon_slots[6];
    Object *n;

    if (s6->type == 0x28)
        return grow_weapon(s6);
    if (s6->type != 0)
        kill_object(s6->obj);
    s6->type = 0x28;
    n = alloc_a(caller);
    s6->obj = n;
    n->type = 0x9c;
    n->fn_update = (obj_fn)bomb_update;
    n->fn_draw = obj_nop;
    n->fn_hit = s6;
    n->score = 0;
    n->energy = 0;
    n->kind = 0x15;
    n->shot_count = n->shot_rate = 0xff;
    xenon2.autofire_period = 0xc;
    return n;
}

/* orig $004DC6: weapon $48 (slot 5 or 6) */
Object *give_homing_missile(Object *caller, Object *last)
{
    Slot *s5 = &xenon2.weapon_slots[5], *s6 = &xenon2.weapon_slots[6], *slot;
    Object *n;

    if (s5->type == 0x48)
        return grow_weapon(s5);
    if (s6->type == 0x48)
        return grow_weapon(s6);
    slot = s5;
    if (slot->type != 0) {
        slot = s6;
        if (slot->type != 0)
            kill_object(slot->obj);
    }
    slot->type = 0x48;
    n = alloc_a(caller);
    slot->obj = n;
    n->type = 0xa0;
    n->fn_update = (obj_fn)homing_missile_update;
    n->fn_draw = obj_nop;
    n->fn_hit = slot;
    n->score = 0;
    n->energy = 0;
    n->kind = 0x17;
    n->shot_count = n->shot_rate = 0xff;
    xenon2.autofire_period = 0x1e;
    return n;
}

/* orig $004E74: weapon $B0 (slot 0) */
Object *give_flamer(Object *caller, Object *last)
{
    Slot *s = &xenon2.weapon_slots[0];
    Object *n;

    if (s->type == 0xb0)
        return grow_weapon(s);
    if (s->type != 0)
        kill_object(s->obj);
    s->type = 0xb0;
    n = alloc_a(caller);
    s->obj = n;
    n->type = 0xb0;
    n->fn_update = flamer_update;
    n->fn_draw = draw_bob_masked;
    n->fn_hit = s;
    set_anim(n, xenon2.anim_weaponb0);
    n->x = xenon2.player.x;
    n->y = xenon2.player.y;
    n->score = 0;
    n->energy = 2;
    n->kind = 0x14;
    n->shot_count = n->shot_rate = 0xff;
    xenon2.autofire_period = 8;
    return n;
}

/* the weapons of slot 5 share their set-up: type, routine, animation, energy, kind, y offset 0x19 (or none) */
static Object *weapon_slot5(Object *caller, uint16_t type, obj_fn update, const uint16_t *anim, int16_t energy, int16_t score, int16_t kind, int dy)
{
    Slot *s5 = &xenon2.weapon_slots[5];
    Object *n;

    n = alloc_a(caller);
    s5->type = type;
    s5->obj = n;
    n->type = type;
    n->fn_update = update;
    n->fn_draw = draw_bob_masked;
    n->fn_hit = s5;
    set_anim(n, anim);
    n->x = xenon2.player.x;
    n->y = xenon2.player.y + dy;
    n->shot.phase = 0;
    n->score = score;
    n->energy = energy;
    n->kind = kind;
    n->shot_count = n->shot_rate = 0xff;
    return n;
}

/* orig $004F66: weapon $2C (slot 5) */
Object *give_drone(Object *caller, Object *last)
{
    if (xenon2.weapon_slots[5].type == 0x2c)
        return grow_weapon(&xenon2.weapon_slots[5]);
    dismiss_slot5();
    return weapon_slot5(caller, 0x2c, drone_update, xenon2.anim_weapon2c, 2, 0, 0x13, 0x19);
}

/* orig $005014: weapon $4C (slot 5) */
Object *give_electro_ball(Object *caller, Object *last)
{
    if (xenon2.weapon_slots[5].type == 0x4c)
        return grow_weapon(&xenon2.weapon_slots[5]);
    dismiss_slot5();
    return weapon_slot5(caller, 0x4c, electro_ball_update, xenon2.anim_weapon4c, 0, 0, 0xb, 0x19);
}

/* orig $0050AC: weapon $38 (slot 5) */
Object *give_mine_1(Object *caller, Object *last)
{
    Object *n;

    if (xenon2.weapon_slots[5].type == 0x38 || xenon2.weapon_slots[5].type == 0x3c)
        return grow_weapon(&xenon2.weapon_slots[5]);
    dismiss_slot5();
    n = weapon_slot5(caller, 0x38, mine_update, xenon2.anim_weapon38, 1, 0, 9, 0x19);
    xenon2.trail_x = -100;
    return n;
}

/* orig $005158: weapon $3C (slot 5) */
Object *give_mine_2(Object *caller, Object *last)
{
    Object *n;

    if (xenon2.weapon_slots[5].type == 0x38 || xenon2.weapon_slots[5].type == 0x3c)
        return grow_weapon(&xenon2.weapon_slots[5]);
    dismiss_slot5();
    n = weapon_slot5(caller, 0x3c, mine_update, xenon2.anim_weapon3c, 1, 1, 0xd, 0x19);
    xenon2.trail_x = -100;
    return n;
}

/* orig $005206: weapon $44 (slot 5; slot 6 loses $98) */
Object *give_rear_shot(Object *caller, Object *last)
{
    Slot *s6 = &xenon2.weapon_slots[6];

    if (xenon2.weapon_slots[5].type == 0x44)
        return grow_weapon(&xenon2.weapon_slots[5]);
    if (s6->type == 0x98) {
        kill_object(s6->obj);
        s6->type = 0;
    }
    dismiss_slot5();
    return weapon_slot5(caller, 0x44, (obj_fn)rear_shot_update, xenon2.anim_weapon44, 2, 0, 8, 0);
}

/* the weapons of the side slots: orig $0052E0 ($90), $00538C ($20), $00543A ($34); 0 when the four slots are taken */
static Object *weapon_side(Object *caller, uint16_t type, obj_fn update, const uint16_t *anim, int16_t energy, int16_t kind, int clear28)
{
    int rev_index;
    Slot *slot = free_side_slot(&rev_index);
    Object *n;

    if (slot == 0)
        return 0;
    n = alloc_a(caller);
    slot->type = type;
    slot->obj = n;
    n->type = type;
    n->fn_update = update;
    n->fn_draw = draw_bob_masked;
    n->fn_hit = slot;
    n->weapon.dx = xenon2.weapon_slot_offsets[2 * rev_index];
    n->weapon.dy = xenon2.weapon_slot_offsets[2 * rev_index + 1];
    set_anim(n, anim);
    n->x = xenon2.player.x + n->weapon.dx;
    n->y = xenon2.player.y + n->weapon.dy;
    if (clear28)
        n->shot.phase = 0;
    n->score = 0;
    n->energy = energy;
    n->kind = kind;
    n->shot_count = n->shot_rate = 0xff;
    return n;
}

/* orig $0052E0 */
Object *give_missile_launcher(Object *caller, Object *last)
{
    Object *n = weapon_side(caller, 0x90, missile_launcher_update, xenon2.anim_weapon90, 0, 0x11, 1);

    return refill_slot0(caller, n ? n : last);
}

/* orig $00538C */
Object *give_laser(Object *caller, Object *last)
{
    Object *n = weapon_side(caller, 0x20, laser_update, xenon2.anim_weapon20, 2, 0x12, 1);

    return refill_slot0(caller, n ? n : last);
}

/* orig $00543A */
Object *give_cannon(Object *caller, Object *last)
{
    Object *n = weapon_side(caller, 0x34, cannon_update, xenon2.anim_weapon34, 0, 0xf, 0), *d;

    if (n == 0)
        return refill_slot0(caller, last);
    if (n->weapon.dx != 0) {
        d = alloc_b_front(caller);
        d->type = 0x40;
        d->fn_update = pod_decoration_update;
        d->fn_draw = draw_bob_masked;
        d->persists = -1;
        set_anim(d, xenon2.anim_pod_decoration);
        d->link.ptr = n;
        d->x = n->x;
        d->y = n->y;
    }
    return refill_slot0(caller, n);
}

/* orig $004822: the weapon of the given type is given to the player; returns last */
Object *weapon_select(uint16_t type, Object *caller, Object *last)
{
    switch (type) {
    case WEAPON_REAR_SHOT: return give_rear_shot(caller, last);
    case WEAPON_SIDE_SHOT: return give_side_shot(caller, last);
    case WEAPON_CANNON: return give_cannon(caller, last);
    case WEAPON_DRONE: return give_drone(caller, last);
    case WEAPON_LASER: return give_laser(caller, last);
    case WEAPON_ELECTRO_BALL: return give_electro_ball(caller, last);
    case WEAPON_MINE_2: return give_mine_2(caller, last);
    case WEAPON_MINE_1: return give_mine_1(caller, last);
    case WEAPON_DOUBLE_SHOT: return give_double_shot(caller, last);
    case WEAPON_MISSILE_LAUNCHER: return give_missile_launcher(caller, last);
    case WEAPON_HOMING_MISSILE: return give_homing_missile(caller, last);
    case WEAPON_FLAMER: return give_flamer(caller, last);
    case WEAPON_BOMB: return give_bomb(caller, last);
    case WEAPON_FORWARD_SHOT: return give_forward_shot(caller, last);
    }
    return last;
}

/* orig $005592: update of weapon $94 */
void forward_shot_update(Object *obj)
{
    if (xenon2.dive_phase)
        return;
    if (xenon2.fire_press)
        spawn_shot_object(obj, 0, -9, 0, -6, (void *const *)&xenon2.player_shot_pictures[3]);
}

/* orig $0055B2: update of weapon $98 */
void side_shot_update(Object *obj)
{
    if (xenon2.dive_phase)
        return;
    if (xenon2.fire_press) {
        spawn_shot_object(obj, -9, 0, -2, -3, (void *const *)&xenon2.player_shot_pictures[0]);
        spawn_shot_object(obj, 9, 0, 2, -3, (void *const *)&xenon2.player_shot_pictures[6]);
    }
}

/* orig $0055E2: update of weapon $44 */
void rear_shot_update(Object *obj)
{
    anim_step(obj);
    obj->x = xenon2.player.x;
    obj->y = xenon2.player.y;
    if (xenon2.respawn_shield) {
        weapon_shield_draw(obj);
        return;
    }
    obj->fn_draw = draw_bob_masked;
    if (xenon2.fire_press) {
        obj->anim_delay = 1;
        spawn_shot_object(obj, 0, 9, 0, 0x12, (void *const *)&xenon2.player_shot_pictures[9]);
    }
}

/* orig $005624: update of weapon $9c (slot 6, type $28) */
void bomb_update(Object *obj)
{
    Object *other, *n;
    int16_t y;

    if (xenon2.dive_phase)
        return;
    int16_t timer = (int16_t)(obj->shot_count << 8 | obj->shot_rate);

    if (timer >= 0) {
        timer--;
        obj->shot_count = (uint8_t)(timer >> 8);
        obj->shot_rate = (uint8_t)timer;
    }
    if (xenon2.fire_press == 0 || timer >= 0)
        return;
    for (other = xenon2.obj_list[LIST_EFFECT].prev; other->type != 0; other = other->prev)
        if (other->type == 0x28)
            return;
    obj->shot_count = 0;
    obj->shot_rate = 0xf;
    n = alloc_b(obj);
    n->type = 0x28;
    n->fn_update = shell_update;
    n->fn_draw = draw_bob_masked;
    n->persists = -1;
    set_anim(n, xenon2.anim_shell_9c);
    n->x = xenon2.player.x;
    y = xenon2.player.y - 0xc;
    n->y = y;
    n->shell.phase = -(int16_t)(((uint16_t)y >> 4) + 4);
    n->shot_count = 0;
    n->shot_rate = 6;
}

/* the eight pictures of the drill, by the loop counter */
static const uint16_t *const drill_anim[8] = { xenon2.anim_drill_0, xenon2.anim_drill_1, xenon2.anim_drill_2, xenon2.anim_drill_3, xenon2.anim_drill_4, xenon2.anim_drill_5, xenon2.anim_drill_6, xenon2.anim_drill_7 };

/* orig $0056D6: update of weapon $a0 (slot 5 or 6, type $48) */
void homing_missile_update(Object *obj)
{
    Object *other, *cur;
    int16_t cnt;

    if (xenon2.dive_phase)
        return;
    int16_t timer = (int16_t)(obj->shot_count << 8 | obj->shot_rate);

    if (timer >= 0) {
        timer--;
        obj->shot_count = (uint8_t)(timer >> 8);
        obj->shot_rate = (uint8_t)timer;
    }
    if (xenon2.fire_press == 0 || timer >= 0)
        return;
    for (other = xenon2.obj_list[LIST_EFFECT].prev; other->type != 0; other = other->prev)
        if (other->type == 0x48)
            return;
    obj->shot_count = 0;
    obj->shot_rate = 0x12;
    xenon2.sfx_cmd = 9;
    cur = obj;
    for (cnt = 7; cnt >= 0; cnt -= 2) {
        cur = alloc_b(cur);
        cur->type = 0x48;
        cur->fn_update = drill_update;
        cur->fn_draw = draw_bob_masked;
        cur->persists = -1;
        set_anim(cur, drill_anim[cnt]);
        cur->x = xenon2.player.x;
        cur->y = xenon2.player.y;
        cur->drill.dir = (cur->drill.dir & 0xffffu) | (uint32_t)(uint16_t)(cnt) << 16;
        cur->drill_target.target = 0;
        cur->drill.state = 0xd;
    }
}

/* a shot that has hit or left the screen is a dying object */
static void shot_gone(Object *obj)
{
    kill_object(obj);
}

/* orig $0057BC: update of a shot that moves by a 16.16 speed ($2A) */
void shot_fixed_update(Object *obj)
{
    Object *enemy;
    uint32_t pos;
    int32_t speed;
    uint16_t damage;

    if (xenon2.player_dead) {
        shot_gone(obj);
        return;
    }
    anim_step(obj);
    if (obj->type == 4)
        return;
    speed = (int32_t)obj->shot.speed >> 6;
    pos = ((uint32_t)(uint16_t)obj->x << 16) | obj->x_frac;
    pos += (uint32_t)speed;
    obj->x = (uint16_t)(pos >> 16);
    obj->x_frac = (uint16_t)pos;
    if ((uint16_t)obj->x >= 0x140) {
        shot_gone(obj);
        return;
    }
    obj->y -= 0xc;
    if (obj->y < 0) {
        shot_gone(obj);
        return;
    }
    if (obj->shot.hits == 0)
        return;
    enemy = collide_enemies(obj);
    if (enemy == 0)
        return;
    damage = (uint16_t)(obj->score + xenon2.kill_scores[1]);
    shot_gone(obj);
    apply_damage(obj, enemy, damage);
}

/* orig $005850: update of a shot ($10) that moves by ($2A, $2E) */
void shot_update(Object *obj)
{
    Object *enemy;
    uint16_t damage;

    if (xenon2.player_dead) {
        shot_gone(obj);
        return;
    }
    anim_step(obj);
    obj->x += (int16_t)(obj->shot.speed >> 16);
    if ((uint16_t)obj->x >= 0x138) {
        shot_gone(obj);
        return;
    }
    obj->y += obj->shot.dy;
    if ((uint16_t)obj->y >= 0xc0) {
        shot_gone(obj);
        return;
    }
    enemy = point_in_enemy(obj->x, obj->y);
    if (enemy == 0)
        return;
    damage = (uint16_t)(obj->score + xenon2.kill_scores[0]);
    shot_gone(obj);
    apply_damage(obj, enemy, damage);
}

/* orig $004894 (+$14) and $0048B4 (+$28): the shield grows (at most $27) */
static void shield_up(uint16_t d)
{
    xenon2.player_energy += d;
    xenon2.sfx_cmd = 7;
    if ((int16_t)xenon2.player_energy > 0x27)
        xenon2.player_energy = 0x27;
    xenon2.energy_dirty = 0xff;
}

void pickup_shield_small(Object *obj) { shield_up(0x14); }
void pickup_shield_big(Object *obj) { shield_up(0x28); }

/* orig $0048BC: the weakest weapon of the seven slots grows */
void pickup_grow_weakest(Object *obj)
{
    int i;
    int16_t best = 0x7f;
    Slot *best_slot = &xenon2.weapon_slots[0];
    Object *weapon;

    for (i = 0; i < 7; i++) {
        Slot *s = &xenon2.weapon_slots[i];

        if (s->type != 0) {
            int16_t level = s->obj->score;

            if (level != s->obj->energy && level < best) {
                best = level;
                best_slot = s;
            }
        }
    }
    if (best != 0x7f) {
        weapon = best_slot->obj;
        weapon->score++;
    }
}

/* orig $0048F6 */
void pickup_speed(Object *obj)
{
    if ((int16_t)xenon2.autofire_level < 4)
        xenon2.autofire_level++;
}

/* orig $004906 */
void pickup_move(Object *obj)
{
    if (xenon2.speed_level != 2)
        xenon2.speed_level++;
}

/* orig $004914 */
void pickup_flag_d83(Object *obj)
{
    xenon2.protection = 0xff;
}

/* orig $00491A */
void pickup_ca0(Object *obj)
{
    xenon2.dive_charges += 3;
}

/* orig $004920 */
void pickup_side_shot(Object *obj)
{
    give_side_shot(obj, 0);
}

/* orig $004904, $004924: pickups with no effect */
void pickup_nothing(Object *obj)
{
}

/* orig $004926: a bomb: the screen flashes, every enemy but the bosses is hit, the bonus counters are cleared
 * (HLE: palette flash; the colours are written to the custom chip) */
void pickup_bomb(Object *obj)
{
    uint32_t saved[8];
    uint16_t bits, colour, frame, type;
    int i, k, n;
    Object *enemy, *after, *item;

    for (i = 0; i < 8; i++)
        saved[i] = ((uint32_t)xenon2.palette_shadow[2 * i] << 16) | xenon2.palette_shadow[2 * i + 1];
    xenon2.sfx_cmd = 2;
    for (n = 0x1e; n >= 0; n--) {
        bits = (uint16_t)random_next();
        for (k = 0; k < 16; k++) {
            colour = 0;
            if (bits & 1)
                colour = 0x777;
            bits >>= 1;
            xenon2.palette_shadow[k] = colour;
            custom->color[k] = (uint16_t)(colour + colour);
        }
        frame = xenon2.vbl_count;
        while (xenon2.vbl_count == frame)
            HOST_IDLE();
    }
    for (i = 0; i < 8; i++) {
        uint32_t v = saved[i];

        xenon2.palette_shadow[2 * i] = (uint16_t)(v >> 16);
        xenon2.palette_shadow[2 * i + 1] = (uint16_t)v;
        v += v;
        custom->color[2 * i] = (uint16_t)(v >> 16);
        custom->color[2 * i + 1] = (uint16_t)v;
    }
    for (enemy = xenon2.obj_list[LIST_ENEMY].prev; enemy->type != 0; enemy = after) {
        type = enemy->type;
        after = owner_chain_next(enemy);
        if (type != 0x50 && type != 0x54)
            apply_damage((Object *)&xenon2.obj_list[LIST_MISC], enemy, 0x7f);
    }
    for (item = xenon2.obj_list[LIST_EFFECT].prev; item->type != 0; ) {
        Object *next = item->prev;

        if (item->type == 0x14) {
            item->type = 4;
            item->fn_update = free_object;
            item->fn_draw = obj_nop;
            item->fn_hit = (void *)obj_nop;
            item->is_part = 0;
            item->persists = 0;
        }
        item = next;
    }
    for (i = 0; i < 16; i++)
        xenon2.pickup_groups[i] = 0;
}

/* orig $004A94: a pickup that makes a spinning object */
void pickup_spinner(Object *obj)
{
    Object *spinner;

    if (xenon2.shield_timer == 0) {
        spinner = alloc_b(obj);
        spinner->type = 0x5c;
        spinner->fn_update = spinner_update;
        spinner->fn_draw = draw_bob_masked;
        spinner->x = xenon2.player.x;
        spinner->y = xenon2.player.y;
        set_anim(spinner, xenon2.anim_spinner);
    }
    xenon2.shield_timer += 0xaa;
}

/* orig $004A0A: the weapons are set up for a new life/level */
void init_weapons(Object *caller)
{
    Object *last = 0;
    int i;

    xenon2.list_a_prev_saved = xenon2.obj_list[LIST_MISC].prev;
    xenon2.list_a_next_saved = xenon2.obj_list[LIST_MISC].next;
    for (i = 0; i < 7; i++) {
        xenon2.weapon_slots_saved[i] = xenon2.weapon_slots[i];
        xenon2.weapon_slots[i].type = 0;
        xenon2.weapon_slots[i].obj = 0;
    }
    xenon2.obj_list[LIST_MISC].type = 0;
    xenon2.obj_list[LIST_MISC].prev = &xenon2.obj_list[LIST_MISC];
    xenon2.obj_list[LIST_MISC].next = &xenon2.obj_list[LIST_MISC];
    xenon2.obj_list[LIST_MISC].fn_update = obj_nop;
    xenon2.obj_list[LIST_MISC].fn_draw = obj_nop;
    xenon2.obj_list[LIST_MISC].fn_hit = (void *)obj_nop;
    last = give_forward_shot(caller, last);
    last = give_cannon(caller, last);
    last = give_cannon(caller, last);
    last = give_laser(caller, last);
    last->score = 2;
    last = give_laser(caller, last);
    last->score = 2;
    last = give_drone(caller, last);
    last->score = 2;
    last = give_side_shot(caller, last);
    last->score = 2;
    last = give_double_shot(caller, last);
    last->score = 2;
}

/* the pickup routines, by $30(object): the table at $47D6 (the first byte of an entry says the sound: 4, else $81) */
static void pickup_call(int idx, Object *obj)
{
    static const uint8_t loud[19] = { 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0 };

    xenon2.sfx_cmd = loud[idx] ? 4 : 0x81;
    switch (idx) {
    case 0: pickup_move(obj); break;
    case 1: pickup_speed(obj); break;
    case 2: pickup_shield_small(obj); break;
    case 3: pickup_shield_big(obj); break;
    case 4: give_rear_shot(obj, 0); break;
    case 5: pickup_side_shot(obj); break;
    case 6: pickup_grow_weakest(obj); break;
    case 7: give_cannon(obj, 0); break;
    case 8: give_drone(obj, 0); break;
    case 9: give_laser(obj, 0); break;
    case 10: give_electro_ball(obj, 0); break;
    case 11: give_mine_1(obj, 0); break;
    case 12: give_missile_launcher(obj, 0); break;
    case 13: give_homing_missile(obj, 0); break;
    case 14: give_flamer(obj, 0); break;
    case 15: give_bomb(obj, 0); break;
    case 16: pickup_spinner(obj); break;
    case 17: pickup_ca0(obj); break;
    case 18: pickup_bomb(obj); break;
    }
}

/* orig $F3E, $004634: update of a pickup (type $18 or $60: a bonus; else a weapon pickup, $30 says which) */
void pickup_update(Object *obj)
{
    int16_t dx, dy, type;
    uint16_t value;
    int idx;

    anim_step(obj);
    if (obj->pickup.mode == 0) {
        obj->y += 8;
        if (obj->y >= 0xc8) {
            if (xenon2.level_end_items != 0) {
                xenon2.level_end_items--;
                if (xenon2.level_end_items == 0)
                    xenon2.level_over = 0xff;
            }
            kill_object(obj);
            return;
        }
    } else {
        if (obj->pickup.mode > 0) {
            obj->pickup.mode &= 7;
            if (obj->pickup.mode == 0)
                obj->pickup.dir = (obj->pickup.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)direction8((int16_t)(0xa0 - obj->x), (int16_t)(0x64 - obj->y))) << 16;
            obj->pickup.mode++;
            dx = (int16_t)(0xa0 - obj->x);
            dy = (int16_t)(0x64 - obj->y);
            if ((int32_t)dx * dx + (int32_t)dy * dy < 0x190)
                obj->pickup.mode = -34;
        } else {
            obj->pickup.mode++;
            if ((obj->pickup.mode & 3) == 0) {
                obj->pickup.dir = (obj->pickup.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)(obj->pickup.dir >> 16) + 1) << 16;
                obj->pickup.dir = (obj->pickup.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)(obj->pickup.dir >> 16) & (7)) << 16;
            }
        }
        idx = ((int16_t)(obj->pickup.dir >> 16) << 2) & 0x1c;
        obj->x += xenon2.pickup_circle_steps[idx >> 1];
        obj->y += xenon2.pickup_circle_steps[(idx >> 1) + 1];
    }
    bbox_from_frame(obj);
    if (!player_hits(obj))
        return;
    type = obj->type;
    value = 0x64;
    if (type != 0x18) {
        value = 0x32;
        if (type != 0x60) {
            idx = obj->pickup.kind;
            kill_object(obj);
            pickup_call(idx, obj);
            return;
        }
    }
    xenon2.player_money += value;
    xenon2.sfx_cmd = 5;
    if (xenon2.level_end_items != 0) {
        xenon2.level_end_items--;
        if (xenon2.level_end_items == 0)
            xenon2.level_over = 0xff;
    }
    kill_object(obj);
}

/* orig $005D00: the player dies */
void player_dies(Object *ship)
{
    Object *weapon;
    const uint16_t *death_anim = xenon2.anim_player_death;

    if (xenon2.level_end_items != 0)
        return;
    xenon2.player_dead = 0xff;
    xenon2.player_energy = 0;
    xenon2.energy_dirty = 0xff;
    xenon2.sfx_cmd = 10;
    xenon2.sfx_cmd2 = 10;
    xenon2.dive_phase = 1;
    xenon2.dive_timer = 0;
    set_anim(&xenon2.player, death_anim);
    xenon2.player.fn_draw = draw_bob_masked;
    weapon = xenon2.obj_list[LIST_MISC].prev;
    if (weapon->type == 0)
        return;
    if (weapon->type == 0x94 && weapon->score == 0) {
        weapon = weapon->prev;
        if (weapon->type == 0)
            return;
    }
    kill_with_slot(weapon);
    refill_slot0(ship, weapon);
}

/* orig $EB4, $005D6E: the player takes damage */
void player_damage(Object *ship, uint16_t damage)
{
    uint16_t old;

    if (xenon2.level_end_items != 0)
        return;
    if (xenon2.protection != 0)
        damage >>= 1;
    xenon2.energy_dirty = 0xff;
    old = xenon2.player_energy;
    xenon2.player_energy = (uint16_t)(old - damage);
    if (old > damage)
        return;
    player_dies(ship);
}

/* orig $005DE6: the player's steering: player_vx (the sideways speed), the stick in joy_stick */
void player_move(Object *ship)
{
    int16_t dx;

    xenon2.player_x_prev = ship->x;
    xenon2.player_y_prev = ship->y;
    xenon2.player_vx_prev = xenon2.player_vx;
    if (xenon2.joy_stick & 4) {
        if (xenon2.player_vx > 0)
            xenon2.player_vx = 0;
        if (xenon2.player_vx > -6)
            xenon2.player_vx--;
    } else if (xenon2.joy_stick & 8) {
        if (xenon2.player_vx < 0)
            xenon2.player_vx = 0;
        if (xenon2.player_vx < 6)
            xenon2.player_vx++;
    } else if (xenon2.player_vx != 0) {
        if (xenon2.player_vx < 0)
            xenon2.player_vx++;
        else
            xenon2.player_vx--;
    }
    xenon2.scroll_step = xenon2.scroll_step_target;
    if (xenon2.joy_stick & 1) {
        if (ship->y <= 0x10) {
            ship->y = 0x10;
            xenon2.scroll_step = 1;
            goto side;
        }
        ship->y -= 3;
        ship->y -= xenon2.speed_level;
    }
    if (xenon2.joy_stick & 2) {
        if (ship->y < 0xb0) {
            ship->y += 3;
            ship->y += xenon2.speed_level;
        } else if (xenon2.scroll_pos != xenon2.scroll_max) {
            xenon2.scroll_step = 0xffff;
            ship->y = 0xb0;
        }
    }
side:
    dx = xenon2.player_vx;
    if (dx != 0) {
        if (dx < 0) {
            if (xenon2.joy_stick & 4)
                dx = -6;
        } else if (xenon2.joy_stick & 8) {
            dx = 6;
        }
    }
    if (xenon2.speed_level == 0)
        dx >>= 1;
    if ((int16_t)xenon2.speed_level >= 2)
        dx += dx >> 1;
    ship->x += dx;
    if (ship->x < 0xe)
        ship->x = 0xe;
    if (ship->x > 0x130)
        ship->x = 0x130;
}

/* orig $005EFE: update of the player (ship = $3F01C) */
void player_update(Object *ship)
{
    Object *enemy;
    int16_t edge, y, limit;
    int i;

    ship->fn_draw = draw_bob_masked;
    if (xenon2.player_dead) {
        xenon2.scroll_step = 0;
        anim_step(ship);
        if (ship->anim_delay == 0) {
            ship->fn_draw = obj_nop;
            if ((int16_t)xenon2.respawn_shield >= 0x10)
                xenon2.player_died = 0xff;
        }
        return;
    }
    if (xenon2.dive_phase == 0) {
        enemy = 0;
        if (xenon2.shades_timer == 0) {
            bbox_from_frame(ship);   /* $5F42: the box of the player's picture, then the test */
            enemy = collide_enemies(ship);
            if (enemy == 0)
                goto after_hit;
        } else {
            edge = (int16_t)(ship->x - 0x40);
            ship->hit[0] = edge;
            ship->hit[2] = (int16_t)(edge + 0x80);
            edge = (int16_t)(ship->y - 0x40);
            ship->hit[1] = edge;
            ship->hit[3] = (int16_t)(edge + 0x80);
            enemy = collide_enemies(ship);
            if (enemy != 0)
                apply_damage(ship, enemy, 0x7f);
            bbox_from_frame(ship);
            goto after_hit;
        }
        if (xenon2.shield_timer == 0) {
            if (enemy->type == 0)
                goto after_hit;
            ship->fn_draw = draw_bob_or_first;
            player_damage(ship, enemy->big ? 0x10 : 8);
            if (xenon2.player_dead)
                return;
        }
        if (enemy->type != 0x50 && enemy->type != 0x54)
            apply_damage(ship, enemy, 0x7f);
    }
after_hit:
    if (xenon2.crush_timer != 0) {
        if ((int16_t)xenon2.crush_timer < 0) {
            if (!player_tile_hit(ship)) {
                xenon2.crush_timer = 0;
                goto move;
            }
            if (xenon2.crush_timer == 0xffef) {
                player_dies(ship);
                return;
            }
        }
        xenon2.crush_timer--;
        xenon2.scroll_step = xenon2.scroll_step_target;
        y = (int16_t)(xenon2.player_history[3] - xenon2.scroll_pos);
        ship->x = xenon2.player_history[4];
        y += xenon2.player_history[5];
        if (y > 0xb0) {
            y -= 0xb0;
            xenon2.scroll_step -= y;
            y = 0xb0;
        }
        if (y < 0x10) {
            y -= 0x10;
            xenon2.scroll_step -= y;
            y = 0x10;
        }
        ship->y = y;
        for (i = 0; i < 48; i++)
            xenon2.player_history[i] = xenon2.player_history[i + 3];
        if (xenon2.crush_timer == 0)
            xenon2.crush_timer = 0xffff;
    } else {
move:
        player_move(ship);
        for (i = 50; i >= 3; i--)
            xenon2.player_history[i] = xenon2.player_history[i - 3];
        xenon2.player_history[0] = xenon2.scroll_pos;
        xenon2.player_history[1] = ship->x;
        xenon2.player_history[2] = ship->y;
        if (xenon2.dive_phase == 0 && (int16_t)xenon2.crush_timer >= 0 && player_tile_hit(ship)) {
            xenon2.crush_timer = xenon2.crush_reload;
            xenon2.player_vx = 0;
            limit = (int16_t)(xenon2.scroll_pos + xenon2.scroll_margin);
            if (limit > (int16_t)xenon2.scroll_max)
                xenon2.scroll_max = limit;
        }
    }
    for (i = 0; i < 6; i++)
        xenon2.player_trail[i] = xenon2.player_trail[i + 2];
    xenon2.player_trail[6] = xenon2.player_x_prev;
    xenon2.player_trail[7] = xenon2.player_y_prev;
    if (xenon2.dive_phase == 0) {
        ship->gfx = (const void *)xenon2.player_pictures[xenon2.player_vx + 6];
        return;
    }
    ship->gfx = (const void *)xenon2.dive_pictures[xenon2.dive_phase - 1];
    ship->fn_draw = draw_bob_masked;
    xenon2.dive_phase += xenon2.dive_step;
    if (xenon2.dive_phase == 0) {
        xenon2.dive_step = 0;
        xenon2.crush_timer = 0xfff1;
        return;
    }
    if (xenon2.dive_phase == 4) {
        if (xenon2.dive_step == 0)
            ship->fn_draw = draw_bob_behind_map;
        xenon2.dive_step = 0;
    }
}

/* ---- weapons and level 1 routines the recorded run never reached (from the listing) ---- */

/* orig $006912: the draw of a weapon object while respawn_shield is not 0 (after a respawn: 8 down to 0; at $10 nothing is drawn): the position
 * becomes the player's centre plus the difference of the centres times respawn_shield (the centre of the picture, not its corner) */
void weapon_shield_draw(Object *obj)
{
    const struct Pic *g;
    int16_t cx, cy, dx, dy;

    if (xenon2.respawn_shield == 0x10) {
        obj->fn_draw = obj_nop;
        return;
    }
    obj->fn_draw = draw_bob_masked;
    g = obj->gfx;
    cx = (int16_t)(obj->x - (int16_t)g->hot_x);
    cy = (int16_t)(obj->y - (int16_t)g->hot_y);
    cx = (int16_t)(cx + cx);
    cy = (int16_t)(cy + cy);
    cx = (int16_t)(cx + (int16_t)g->width);
    cy = (int16_t)(cy + (int16_t)g->height);
    dx = (int16_t)(cx >> 1);
    dy = (int16_t)(cy >> 1);
    g = xenon2.player.gfx;
    cx = (int16_t)(xenon2.player.x - (int16_t)g->hot_x);
    cy = (int16_t)(xenon2.player.y - (int16_t)g->hot_y);
    cx = (int16_t)(cx + cx);
    cy = (int16_t)(cy + cy);
    cx = (int16_t)(cx + (int16_t)g->width);
    cy = (int16_t)(cy + (int16_t)g->height);
    cx = (int16_t)(cx >> 1);
    cy = (int16_t)(cy >> 1);
    dx = (int16_t)(dx - cx);
    dy = (int16_t)(dy - cy);
    cx = (int16_t)(cx + (int16_t)((int32_t)dx * (int16_t)xenon2.respawn_shield));
    cy = (int16_t)(cy + (int16_t)((int32_t)dy * (int16_t)xenon2.respawn_shield));
    obj->x = cx;
    obj->y = cy;
}

/* orig $00249C: a random enemy on the screen (the end of the list when there is none): the (random below target_count)th
 * one that is on the screen, groups ($B8) skipped, at most five passes over the list */
static Object *pick_target(void)
{
    uint16_t n = xenon2.target_count, pass;
    Object *enemy;

    if (n == 0)
        n = 1;
    n = random_below(n);
    for (pass = 4; (int16_t)pass >= 0; pass--) {
        enemy = LIST_HEAD(LIST_ENEMY);
        while (enemy->type != 0) {
            if (enemy->type != 0xb8 && !(enemy->hit[2] < 0 || enemy->hit[0] >= 0x140 || enemy->hit[3] < 0 || enemy->hit[1] >= 0xc0)) {
                if ((uint16_t)n-- == 0)
                    return enemy;
            }
            enemy = owner_chain_next(enemy);
        }
    }
    return &xenon2.obj_list[LIST_ENEMY];
}

/* orig $003096: every enemy whose box meets (x0, y0)-(x1, y1) takes dmg from the shooter; it ends when the shooter dies or level_end_items is
 * set.  The level's own test at the end (level 1: none) is $54E0C. */
void area_damage(Object *shooter, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t dmg)
{
    Object *enemy = LIST_HEAD(LIST_ENEMY);

    xenon2.area_damage_kept = dmg;
    xenon2.hit_box[0] = (uint16_t)x0;
    xenon2.hit_box[1] = (uint16_t)y0;
    xenon2.hit_box[2] = (uint16_t)x1;
    xenon2.hit_box[3] = (uint16_t)y1;
    for (;;) {
        Object *next_enemy;

        enemy = scan_boxes(enemy, x0, y0, x1, y1);
        if (enemy->type == 0) {
            Object *hit = level_collision(x0, y0, x1, y1, enemy);

            if (hit)
                apply_damage(shooter, hit, (uint16_t)x0);   /* x0 as the hook leaves it */
            return;
        }
        next_enemy = owner_chain_next(enemy);
        apply_damage(shooter, enemy, xenon2.area_damage_kept);
        enemy = next_enemy;
        if (shooter->type == 4 || xenon2.level_end_items != 0)
            return;
    }
}

/* orig $003868: update of the spinning shield (type $5C): follows the player for shield_timer frames, drawing the gauge */
void spinner_update(Object *obj)
{
    anim_step(obj);
    obj->x = xenon2.player.x;
    obj->y = xenon2.player.y;
    if (xenon2.respawn_shield) {
        weapon_shield_draw(obj);
        return;
    }
    obj->fn_draw = draw_bob_masked;
    xenon2.shield_timer--;
    draw_gauge(4, 0xa0, xenon2.shield_timer, 0xaa);
    if (xenon2.shield_timer == 0)
        kill_object(obj);
}


/* orig $0039A2: update of a drill bit (type $48): it turns (v2a, eight directions) towards an enemy it keeps in $30 and flies; it hits
 * the first enemy it meets and explodes when there is none or it leaves the screen */
void drill_update(Object *obj)
{
    static const uint16_t *const heads[8] = { xenon2.anim_drill_head_0, xenon2.anim_drill_head_1, xenon2.anim_drill_head_2, xenon2.anim_drill_head_3, xenon2.anim_drill_head_4, xenon2.anim_drill_head_5, xenon2.anim_drill_head_6, xenon2.anim_drill_head_7 };
    Object *target;
    int16_t x, y, dx, dy, turn;

    if (xenon2.player_dead) {
        kill_object(obj);
        return;
    }
    anim_step(obj);
    if (obj->drill.state > 0) {
        obj->drill.state--;
        if (obj->drill.state >= 8)
            goto fly;
        x = obj->x;
        y = obj->y;
        goto test;
    }
    target = obj->drill_target.target;
    if (target == 0)
        goto find;
    if ((int16_t)target->type < 0xc8 || target->hit[2] < 0 || target->hit[0] >= 0x140 || target->hit[3] < 0 || target->hit[1] >= 0xc0)
        goto find;
    goto have;
find:
    target = pick_target();
    if (target->type == 0) {
        spawn_effect(obj->x, obj->y, xenon2.anim_explosion, draw_bob_masked);
        kill_object(obj);
        return;
    }
    obj->drill_target.target = target;
have:
    if (obj->drill.state != 0) {
        obj->drill.state++;
        if (obj->drill.state != 0)
            goto fly;
    }
    dx = (int16_t)(target->hit[0] + 2 - obj->x);
    dy = (int16_t)(target->hit[1] + 2 - obj->y);
    turn = (int16_t)(direction8(dx, dy) - (int16_t)(obj->drill.dir >> 16));
    if (turn != 0) {
        if (turn < 0)
            turn = (int16_t)(turn + 8);
        if (turn < 4)
            obj->drill.dir = (obj->drill.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)(obj->drill.dir >> 16) + 1) << 16;
        else
            obj->drill.dir = (obj->drill.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)(obj->drill.dir >> 16) - 1) << 16;
        obj->drill.dir = (obj->drill.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)(obj->drill.dir >> 16) & (7)) << 16;
        obj->drill.state = -3;
        set_anim(obj, heads[(int16_t)(obj->drill.dir >> 16)]);
    }
fly:
    y = (int16_t)(xenon2.drill_dy[(int16_t)(obj->drill.dir >> 16)] + obj->y);
    if ((uint16_t)y >= 0xc0) {
        kill_object(obj);
        return;
    }
    obj->y = y;
    x = (int16_t)(xenon2.drill_dx[(int16_t)(obj->drill.dir >> 16)] + obj->x);
    if ((uint16_t)x >= 0x140) {
        kill_object(obj);
        return;
    }
    obj->x = x;
test:
    target = point_in_enemy(x, y);
    if (target) {
        kill_object(obj);
        apply_damage(obj, target, xenon2.weapon_damage[0]);
    }
}

/* orig $003B4A: update of weapon $C0 (slot 0): two shots a frame while fire is held */
void double_shot_update(Object *obj)
{
    obj->x = xenon2.player.x;
    obj->y = xenon2.player.y;
    if (xenon2.respawn_shield) {
        weapon_shield_draw(obj);
        return;
    }
    obj->fn_draw = draw_bob_masked;
    if (xenon2.fire_press) {
        spawn_shot_object(obj, 0, (uint16_t)-9, (uint16_t)-5, (uint16_t)-12, (void *const *)&xenon2.player_shot_pictures[3]);
        spawn_shot_object(obj, 0, (uint16_t)-9, 6, (uint16_t)-12, (void *const *)&xenon2.player_shot_pictures[3]);
    }
}

/* orig $003B94: update of weapon $B0 (slot 0): while fire is held two shots (type $B4) of random spread each frame and the sound */
void flamer_update(Object *obj)
{
    int16_t i;

    anim_step(obj);
    obj->x = xenon2.player.x;
    obj->y = xenon2.player.y;
    if (xenon2.respawn_shield) {
        weapon_shield_draw(obj);
        return;
    }
    obj->fn_draw = draw_bob_masked;
    if (!(xenon2.joy_stick & 0x80)) {
        if (obj->wb0.tick != 0) {
            sfx_stop_all();
            obj->wb0.tick = 0;
        }
        return;
    }
    if (obj->wb0.tick == 0 || xenon2.sfx_channels[2].active == 0) {
        xenon2.sfx_cmd = 1;
        obj->wb0.tick = 1;
    }
    for (i = 1; i >= 0; i--) {
        Object *shot = alloc_b(obj);
        int16_t y;

        shot->type = 0xb4;
        shot->fn_update = (obj_fn)shot_fixed_update;
        shot->fn_draw = draw_bob_masked;
        shot->x = obj->x;
        shot->x_frac = 0x8000;
        y = (int16_t)((random_next() & 3) + obj->y - 0x10);
        if (i == 0)
            y = (int16_t)(y - 6);
        shot->y = y;
        set_anim(shot, xenon2.anim_weaponb0_shot);
        shot->score = obj->score;
        shot->shot.hits = i;
        shot->shot.speed = (shot->shot.speed & 0xffffu) | (uint32_t)(uint16_t)((int8_t)random_next()) << 16;
    }
}

/* orig $003C7A: update of the decoration (type $40) of a pod: it follows the pod ($2A) while that is there */
void pod_decoration_update(Object *obj)
{
    Object *pod;

    anim_step(obj);
    pod = obj->link.ptr;
    if (pod->type == 0x34) {
        obj->x = (int16_t)(xenon2.player.x + pod->weapon.dx);
        obj->y = (int16_t)(xenon2.player.y + pod->weapon.dy);
        if (xenon2.respawn_shield) {
            weapon_shield_draw(obj);
            return;
        }
        obj->fn_draw = draw_bob_masked;
        return;
    }
    kill_object(obj);
}

/* orig $003CDC: update of weapon $2C (slot 5): while fire is held every 6 frames a fan of dots (the table by the weapon's level) goes into
 * the free places of the shots' table */
void drone_update(Object *obj)
{
    static const int16_t *const pat[3] = { &xenon2.weapon2c_patterns[0], &xenon2.weapon2c_patterns[17], &xenon2.weapon2c_patterns[50] };
    const int16_t *pattern;
    Shot *shot;
    int16_t remaining, i;

    anim_step(obj);
    obj->x = (int16_t)xenon2.player_trail[0];
    obj->y = (int16_t)(xenon2.player_trail[1] + 0x19);
    if (xenon2.respawn_shield) {
        weapon_shield_draw(obj);
        return;
    }
    obj->fn_draw = draw_bob_masked;
    if (obj->w2c.tick)
        obj->w2c.tick--;
    if (!(xenon2.joy_stick & 0x80) || obj->w2c.tick != 0)
        return;
    obj->w2c.tick = 6;
    pattern = pat[(uint16_t)obj->score];
    remaining = *pattern++;
    shot = xenon2.shots;
    for (i = 0x4f; i >= 0; i--, shot++) {
        if (shot->x >= 0)
            continue;
#ifdef GAME_SDL
        presentation_reuse(shot);
#endif
        shot->x = (int16_t)(obj->x + *pattern++);
        shot->y = (int16_t)(obj->y + *pattern++);
        shot->dx = *pattern++;
        shot->dy = *pattern++;
        if (--remaining < 0)
            return;
    }
}

/* a weapon's phase while fire is held: 1 flying out / following, 2 up, 3 coming back, negative a trail copy */

/* orig $003E2C: update of weapon $4C (slot 5): a beam that flies out while fire is held (v28 = 1), moved by the stick, hurting what it
 * meets, and comes back to the ship when fire is let go (v28 negative) */
void electro_ball_update(Object *obj)
{
    int16_t coord, step;
    Object *enemy;

    anim_step(obj);
    if (xenon2.respawn_shield) {
        weapon_shield_draw(obj);
        return;
    }
    obj->fn_draw = draw_bob_masked;
    if (obj->shot.phase == 0) {
        if (!(xenon2.joy_stick & 0x80)) {
            obj->x = (int16_t)xenon2.player_trail[0];
            obj->y = (int16_t)(xenon2.player_trail[1] + 0x19);
            return;
        }
        obj->shot.phase = 1;
    } else if (obj->shot.phase > 0) {
        if (!(xenon2.joy_stick & 0x80)) {
            obj->shot.phase = -1;
            return;
        }
        coord = (int16_t)(((xenon2.player.x - xenon2.player_x_prev) * 4) + obj->x);
        if (coord < 0)
            coord = 0;
        if (coord > 0x13f)
            coord = 0x13f;
        obj->x = coord;
        coord = (int16_t)(((xenon2.player.y - xenon2.player_y_prev) * 4) + obj->y);
        if (coord < 0)
            coord = 0;
        if (coord > 0xbf)
            coord = 0xbf;
        obj->y = coord;
    } else {
        step = (int16_t)(xenon2.player_trail[0] - obj->x);
        if (step > 8)
            step = 8;
        if (step < -8)
            step = -8;
        obj->x = (int16_t)(obj->x + step);
        step = (int16_t)(xenon2.player_trail[1] + 0x19 - obj->y);
        if (step > 8)
            step = 8;
        if (step < -8)
            step = -8;
        obj->y = (int16_t)(obj->y + step);
        if ((int16_t)xenon2.player_trail[0] == obj->x && (int16_t)(xenon2.player_trail[1] + 0x19) == obj->y)
            obj->shot.phase = 0;
        return;
    }
    bbox_from_frame(obj);
    enemy = collide_enemies(obj);
    if (enemy) {
        xenon2.sfx_cmd = 0x15;
        apply_damage(obj, enemy, xenon2.weapon_damage[1]);
    }
}

/* orig $003F3A: update of weapon $20 (side slot): every 7 frames while fire is held a beam (type $24) goes up from it */
void laser_update(Object *obj)
{
    Object *beam;

    anim_step(obj);
    obj->x = (int16_t)(xenon2.player.x + obj->weapon.dx);
    obj->y = (int16_t)(xenon2.player.y + obj->weapon.dy);
    if (xenon2.respawn_shield) {
        weapon_shield_draw(obj);
        return;
    }
    obj->fn_draw = draw_bob_masked;
    int16_t timer = (int16_t)(obj->shot_count << 8 | obj->shot_rate);

    if (timer >= 0) {
        timer--;
        obj->shot_count = (uint8_t)(timer >> 8);
        obj->shot_rate = (uint8_t)timer;
    }
    if (xenon2.fire_press && timer < 0) {
        xenon2.sfx_cmd = 6;
        beam = alloc_b(obj);
        beam->type = 0x24;
        beam->fn_draw = (obj_fn)beam_draw;
        beam->fn_update = (obj_fn)beam_update;
        beam->w20.tick = -0x20;
        beam->target = obj;
        beam->score = obj->score;
        obj->shot_count = 0;
        obj->shot_rate = 6;
    }
}


/* orig $003FDA: update of the beam of weapon $20: it grows (v28 -$20 to $60), then climbs; hurts what it covers */
void beam_update(Object *obj)
{
    int16_t x0, y0, x1, y1;
    uint16_t damage;

    if (xenon2.player_dead) {
        kill_object(obj);
        return;
    }
    if (obj->beam.growth != 0x41) {
        Object *owner = obj->target;

        obj->x = (int16_t)(owner->x - 7);
        obj->y = (int16_t)(owner->y - 0x18);
        obj->beam.growth = (int16_t)(obj->beam.growth + 0x20);
        if (obj->beam.growth != 0x60)
            goto hurt;
        obj->beam.growth = 0x41;
    }
    obj->y = (int16_t)(obj->y - 0x20);
    if (obj->y < 0) {
        kill_object(obj);
        return;
    }
hurt:
    x0 = obj->x;
    y0 = (int16_t)(obj->y - obj->beam.growth);
    x1 = obj->score;
    x0 = (int16_t)(x0 + 4);
    x1 = (int16_t)(x1 + x1);
    x0 = (int16_t)(x0 - x1);
    x1 = (int16_t)(x1 + x1);
    x1 = (int16_t)(x1 + 7);
    x1 = (int16_t)(x1 + x0);
    y1 = (int16_t)(y0 + 0x20);
    if (y1 < 0)
        return;
    if (y0 < 0)
        y0 = 0;
    damage = (uint16_t)((uint16_t)(obj->score + 1) * xenon2.blast_damage[0]);
    area_damage(obj, x0, y0, x1, y1, damage);
}

/* orig $004094: update of weapon $34 (side slot, the pod): with the first picture ($17336) the pod makes its little trail object
 * (type $30); with the picture $171D6 and fire held it plays the sound $80 and changes to the animation $27F2 */
void cannon_update(Object *obj)
{
    Object *trail;

    anim_step(obj);
    obj->x = (int16_t)(xenon2.player.x + obj->weapon.dx);
    obj->y = (int16_t)(xenon2.player.y + obj->weapon.dy);
    if (xenon2.respawn_shield) {
        weapon_shield_draw(obj);
        return;
    }
    obj->fn_draw = draw_bob_masked;
    if (obj->gfx == (const void *)xenon2.pic_weapon34_idle) {
        trail = alloc_b(obj);
        trail->type = 0x30;
        trail->fn_draw = draw_bob_masked;
        trail->fn_update = (obj_fn)pod_trail_update;
        set_anim(trail, xenon2.anim_pod_trail);
        trail->x = obj->x;
        trail->y = (int16_t)(obj->y - 0xb);
        trail->pod_trail.tick = 0;
        return;
    }
    if (xenon2.fire_press && obj->gfx == (const void *)xenon2.pic_weapon34_fire) {
        xenon2.sfx_cmd = 0x80;
        set_anim(obj, xenon2.anim_weapon34_fire);
    }
}

/* orig $0045E4: update of the pod's shot (type $30): flies up and hurts the first enemy it meets */
void pod_trail_update(Object *obj)
{
    Object *enemy;

    if (xenon2.player_dead) {
        kill_object(obj);
        return;
    }
    anim_step(obj);
    obj->y = (int16_t)(obj->y - 0xa);
    if (obj->y < 0) {
        kill_object(obj);
        return;
    }
    bbox_from_frame(obj);
    enemy = collide_enemies(obj);
    if (enemy)
        apply_damage(obj, enemy, xenon2.weapon_damage[2]);
}

/* orig $00415A: update of weapon $90 (side slot, twin gun): with the picture $1838C it fires two shots (type $10); with $182DC
 * and fire held it changes its delay */
void missile_launcher_update(Object *obj)
{
    int16_t dx, i;

    anim_step(obj);
    obj->x = (int16_t)(xenon2.player.x + obj->weapon.dx);
    obj->y = (int16_t)(xenon2.player.y + obj->weapon.dy);
    if (xenon2.respawn_shield) {
        weapon_shield_draw(obj);
        return;
    }
    obj->fn_draw = draw_bob_masked;
    if (obj->gfx == (const void *)xenon2.pic_weapon90_twin) {
        dx = -3;
        for (i = 1; i >= 0; i--) {
            Object *shot = alloc_b(obj);

            shot->shot.speed = (shot->shot.speed & 0xffffu) | (uint32_t)(uint16_t)(0) << 16;
            shot->shot.dy = -9;
            dx = (int16_t)(dx + obj->x);
            shot->x = dx;
            shot->y = (int16_t)(obj->y - 0x10);
            shot->type = 0x10;
            shot->fn_update = (obj_fn)shot_update;
            shot->fn_draw = draw_bob_masked;
            set_anim(shot, xenon2.anim_drill_head_0);
            shot->score = obj->score;
            dx = 5;
        }
        return;
    }
    if (xenon2.fire_press && obj->gfx == (const void *)xenon2.pic_weapon90_idle)
        obj->anim_delay = 1;
}


/* orig $0042A6: update of the shell of weapon $28 (type $28): flies up while v28 < 0, waits, then explodes after 3 frames and hurts
 * what is near */
void shell_update(Object *obj)
{
    if (xenon2.player_dead) {
        kill_object(obj);
        return;
    }
    anim_step(obj);
    if (obj->shell.phase < 0) {
        obj->shell.phase++;
        obj->y = (int16_t)(obj->y - 6);
        return;
    }
    if (obj->shell.phase == 0) {
        obj->shell.phase = 3;
        return;
    }
    obj->shell.phase--;
    if (obj->shell.phase != 0)
        return;
    xenon2.sfx_cmd = 0x82;
    debris_burst(obj, 3, (uint16_t)(obj->x - 0x14), (uint16_t)(obj->y - 0x14), 0x28, 0x28, xenon2.anim_explosion_big);
    kill_object(obj);
    area_damage(obj, (int16_t)(obj->x - 0x20), (int16_t)(obj->y - 0x20), (int16_t)(obj->x - 0x20 + 0x3f), (int16_t)(obj->y - 0x20 + 0x3f), xenon2.blast_damage[1]);
}


/* orig $004340: update of weapon $38 and $3C (slot 5): the shell follows the ship until fire is held (v28 = 1), flies up to the top
 * (v28 = 2), is steered by the stick, leaves a trail of copies of itself (v28 negative, they explode when fire is let go), comes back
 * (v28 = 3) when fire is let go */
void mine_update(Object *obj)
{
    int16_t x, y, dist, step;

    anim_step(obj);
    if (obj->shot.phase < 0) {
        int16_t left, top;
        uint16_t w, h;
        const uint16_t *explosion;

        if (xenon2.player_dead) {
            kill_object(obj);
            return;
        }
        if (xenon2.joy_stick & 0x80)
            return;
        obj->shot.phase++;
        if (obj->shot.phase != 0)
            return;
        left = (int16_t)(obj->x - 8);
        top = (int16_t)(obj->y - 8);
        w = h = 0x10;
        explosion = xenon2.anim_explosion;
        if (obj->score != 0) {
            left = (int16_t)(left - 0xa);
            top = (int16_t)(top - 0xa);
            w = h = 0x14;
            explosion = xenon2.anim_explosion_big;
        }
        debris_burst(obj, 2, (uint16_t)left, (uint16_t)top, w, h, explosion);
        if (obj->score == 0) {
            left = (int16_t)(obj->x - 0x10);
            top = (int16_t)(obj->y - 0x10);
            area_damage(obj, left, top, (int16_t)(left + 0x20), (int16_t)(top + 0x20), (uint16_t)(xenon2.blast_damage[2]));
        } else {
            left = (int16_t)(obj->x - 0x18);
            top = (int16_t)(obj->y - 0x18);
            area_damage(obj, left, top, (int16_t)(left + 0x2f), (int16_t)(top + 0x2f), (uint16_t)(obj->score * 2 + xenon2.blast_damage[2]));
        }
        kill_object(obj);
        return;
    }
    if (xenon2.respawn_shield) {
        weapon_shield_draw(obj);
        return;
    }
    obj->fn_draw = draw_bob_masked;
    if (obj->shot.phase == 0) {
        obj->x = xenon2.player.x;
        obj->y = (int16_t)(xenon2.player.y + 0x19);
        if (xenon2.joy_stick & 0x80)
            obj->shot.phase = 1;
        return;
    }
    if (obj->shot.phase == 1) {
        if (xenon2.joy_stick & 0x80) {
            if (obj->y > 0x10)
                obj->y = (int16_t)(obj->y - 0xc);
            else
                obj->shot.phase = 2;
        } else {
            obj->shot.phase = 3;
        }
        return;
    }
    if (obj->shot.phase == 2) {
        if (!(xenon2.joy_stick & 0x80)) {
            obj->shot.phase = 3;
            return;
        }
        x = obj->x;
        if (xenon2.joy_stick & 4) {
            x = (int16_t)(x - 8);
            if (x < 0)
                x = 0;
        }
        if (xenon2.joy_stick & 8) {
            x = (int16_t)(x + 8);
            if (x > 0x13f)
                x = 0x13f;
        }
        obj->x = x;
        y = obj->y;
        if (xenon2.joy_stick & 1) {
            y = (int16_t)(y - 8);
            if (y < 0)
                y = 0;
        }
        if (xenon2.joy_stick & 2) {
            y = (int16_t)(y + 8);
            if (y > 0xbf)
                y = 0xbf;
        }
        obj->y = y;
        if (xenon2.trail_count == 10)
            return;
        dist = (int16_t)(xenon2.trail_x - obj->x);
        if (dist < 0)
            dist = (int16_t)-dist;
        if (dist < 0x20) {
            dist = (int16_t)(xenon2.trail_y - obj->y);
            if (dist < 0)
                dist = (int16_t)-dist;
            if (dist < 0x20)
                return;
        }
        xenon2.trail_x = obj->x;
        xenon2.trail_y = obj->y;
        {
            Object *trail = alloc_b(obj);

            trail->type = obj->type;
            trail->fn_update = obj->fn_update;
            trail->fn_draw = draw_bob_masked;
            set_anim(trail, obj->anim - 3);
            trail->x = obj->x;
            trail->y = obj->y;
            trail->score = obj->score;
            xenon2.trail_count++;
            trail->shot.phase = (int16_t)(-(int16_t)(xenon2.trail_count * 4));
        }
        return;
    }
    step = (int16_t)(xenon2.player.x - obj->x);
    if (step > 8)
        step = 8;
    if (step < -8)
        step = -8;
    obj->x = (int16_t)(obj->x + step);
    step = (int16_t)(xenon2.player.y + 0x19 - obj->y);
    if (step > 8)
        step = 8;
    if (step < -8)
        step = -8;
    obj->y = (int16_t)(obj->y + step);
    if (xenon2.player.x == obj->x && (int16_t)(xenon2.player.y + 0x19) == obj->y)
        obj->shot.phase = 0;
}

/* orig $0064A0: draw routine of the beam of weapon $20, by CPU: a slice 32 pixels wide of the level's pattern (mask and four planes,
 * shifted by x & 15) for each of the length + 1 lines going up from y, and the head pictures with the blitter-less bob routine; the
 * left and right edges are clipped to one word */
void beam_draw(Object *obj)
{
    uint16_t level = (uint16_t)obj->score;
    int16_t x = (int16_t)(obj->x + 4 - level - level);
    int16_t y = obj->y;
    uint32_t rot[5];
    uint16_t *dst;
    uint16_t cnt;
    int lt, i;

    xenon2.beam_state[0] = (uint16_t)x;
    xenon2.beam_state[1] = (uint16_t)y;
    xenon2.beam_state[3] = level;
    xenon2.beam_state[2] = (uint16_t)obj->beam.growth;
    if (x <= -16 || x >= 0x140)
        return;
    if ((int16_t)xenon2.beam_state[2] >= 0x41) {
        draw_bob_masked_at((uint16_t)x, (uint16_t)(y + 1), xenon2.beam_heads[level]);
        x = (int16_t)xenon2.beam_state[0];
        y = (int16_t)xenon2.beam_state[1];
    }
    y = (int16_t)(y - (int16_t)xenon2.beam_state[2]);
    draw_bob_masked_at((uint16_t)x, (uint16_t)y, xenon2.beam_bodies[xenon2.beam_state[3]]);
    x = (int16_t)xenon2.beam_state[0];
    y = (int16_t)xenon2.beam_state[1];
    if (y < 0)
        return;
    dst = (uint16_t *)(xenon2.draw_screen + (int16_t)((uint16_t)((uint16_t)y) * SCREEN_ROW_BYTES));
    {
        const uint32_t *pattern = &xenon2.beam_pattern[xenon2.beam_state[3] * 5];
        uint16_t shift = (uint16_t)x & 0xf;

        for (i = 0; i < 5; i++)
            rot[i] = shift ? (pattern[i] >> shift) | (pattern[i] << (32 - shift)) : pattern[i];
    }
    dst += (int16_t)(((int16_t)x >> 3) & 0xfffe) / 2;
#ifdef GAME_SDL
    if (presentation_capturing)
        presentation_draw(PRESENT_BEAM, x, y);
#endif
    cnt = xenon2.beam_state[2];
    lt = 0;
    if ((int16_t)xenon2.beam_state[0] < 0) {
        dst++;
        while (!lt) {
            if (--cnt == 0xffff)
                break;
            for (i = 0; i < 4; i++) {
                uint16_t *plane = dst + SCREEN_PLANE_WORDS * i;
                draw_screen_word(plane, (uint16_t)~rot[0], (uint16_t)rot[1 + i]);
            }
            dst -= SCREEN_ROW_WORDS;
            lt = (int32_t)(uintptr_t)(dst) < (int32_t)(uintptr_t)(xenon2.draw_screen);
        }
    } else if ((int16_t)xenon2.beam_state[0] > 0x130) {
        while (!lt) {
            if (--cnt == 0xffff)
                break;
            for (i = 0; i < 4; i++) {
                uint16_t *plane = dst + SCREEN_PLANE_WORDS * i;
                draw_screen_word(plane, (uint16_t)~(rot[0] >> 16), (uint16_t)(rot[1 + i] >> 16));
            }
            dst -= SCREEN_ROW_WORDS;
            lt = (int32_t)(uintptr_t)(dst) < (int32_t)(uintptr_t)(xenon2.draw_screen);
        }
    } else {
        while (!lt) {
            if (--cnt == 0xffff)
                break;
            for (i = 0; i < 4; i++) {
                uint16_t *plane = dst + SCREEN_PLANE_WORDS * i;
                draw_screen_word(plane, (uint16_t)~(rot[0] >> 16), (uint16_t)(rot[1 + i] >> 16));
                draw_screen_word(plane + 1, (uint16_t)~rot[0], (uint16_t)rot[1 + i]);
            }
            dst -= SCREEN_ROW_WORDS;
            lt = (int32_t)(uintptr_t)(dst) < (int32_t)(uintptr_t)(xenon2.draw_screen);
        }
    }
}
