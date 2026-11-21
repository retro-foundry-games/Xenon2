/* weapons: the weapons, pickups and the player of $4000-$5FFF. */
#include "weapons.h"
#include "shop.h"
#include "game_all.h"
#include "weapon_util.h"
#include "amiga/hw.h"

/* the scratch fields of Object, by the kind that uses them */
#define DRILL_STATE(o) ((o)->v28)
#define PICKUP_MODE(o) ((o)->v28)
#define SHOT_DX(o) ((o)->v2a)
#define SHOT_DY(o) ((o)->v2e)
#define SHOT_SPEED_HI(o) ((o)->v2a)
#define SHOT_SPEED_LO(o) ((o)->v2c)
#define W20_TICK(o) ((o)->v28)
#define W2C_TICK(o) ((o)->v28)
#define WB0_TICK(o) ((o)->v28)


#define FN(f) ((obj_fn)(f))

/* the pattern `move.w #4,(a); move.l #$fee,2(a); move.l #$3130,6(a); move.l #$3130,$a(a); sf.b $34(a); sf.b $60(a)` */
static void kill_object(Object *o)
{
    o->type = 4;
    o->fn_update = free_object;
    o->fn_draw = obj_nop;
    o->fn_hit = (void *)obj_nop;
    o->is_part = 0;
    o->persists = 0;
}

/* inlined: $2418 and the insertion before the sentinel of list A / B (the object comes back in a0) */
static Object *alloc_a(Object *a0)
{
    Object *o = obj_free_pop(), *tail = LIST_A->prev;

    LIST_A->prev = o;
    o->prev = tail;
    o->next = LIST_A;
    tail->next = o;
    return o;
}

static void set_ptr_2a(Object *o, Object *p)
{
    uint32_t v = p ? GADDR(p) : 0;   /* (the original's address: the two words are compared and exported as they are) */

    o->v2a = (int16_t)(v >> 16);
    o->v2c = (int16_t)v;
}

static Object *get_ptr_2a(const Object *o)
{
    uint32_t v = ((uint32_t)(uint16_t)o->v2a << 16) | (uint16_t)o->v2c;

    return v ? (Object *)GPTR(v) : 0;
}

static void unlink_to_free(Object *o)
{
    Object *n = o->next, *p = o->prev;

    n->prev = p;
    p->next = n;
    o->next = free_obj;
    free_obj = o;
    o->type = 0;
}

/* orig $005924: the hit routine of a1 takes d0 damage (the original jumps to it) */
void apply_damage(Object *a0, Object *a1, uint16_t d0)
{
    ((hit_fn)a1->fn_hit)(a0, a1, d0);
}

/* orig $ECC, $005920: the same with the greatest damage */
void apply_damage_max(Object *a0, Object *a1)
{
    apply_damage(a0, a1, 0x7f);
}

/* orig $F0E, $005550: 1 when the object's box meets the player's (the flag the caller tests; inlined: $200A) */
int player_hits(Object *a0)
{
    const Object *p = PLAYER;

    if (dive_phase)
        return 0;
    return !(p->hit[2] < a0->hit[0] || p->hit[0] > a0->hit[2] || p->hit[3] < a0->hit[1] || p->hit[1] > a0->hit[3]);
}

/* orig $EAE, $00556C: 1 when the point (d0, d1) is in the player's box */
int point_in_player_box(int16_t d0, int16_t d1)
{
    const Object *p = PLAYER;

    if (dive_phase)
        return 0;
    return !(p->hit[0] > d0 || p->hit[2] < d0 || p->hit[1] > d1 || p->hit[3] < d1);
}

/* orig $F44, $0057B6: an object that only animates */
void animate_only(Object *a0)
{
    anim_step(a0);
}

/* orig $E24, $005908: an object that follows its target */
void follow_target(Object *a0)
{
    anim_step(a0);
    a0->x = a0->target->x;
    a0->y = a0->target->y;
    bbox_from_frame(a0);
}

/* orig $005C22: the object of list B (type $40) that belongs to a1, or the sentinel */
static Object *find_decoration(Object *a1)
{
    Object *a2 = LIST_B->prev;

    while (a2->type != 0) {
        if (a2->type == 0x40 && get_ptr_2a(a2) == a1)
            return a2;
        a2 = a2->prev;
    }
    return a2;
}

/* the weapon slot that owns an object is freed (fn_hit is its address); an object that no longer has one (a dying shot: fn_hit is obj_nop) makes the original clear a word of its own code, which has no effect */
static void slot_free(void *fn_hit)
{
    Slot *s = (Slot *)fn_hit;

    if ((s >= weapon_slots && s < weapon_slots + 7) || (s >= weapon_slots_saved && s < weapon_slots_saved + 7))
        s->type = 0;
}

/* orig $005C3E: a1 and what belongs to it die */
void kill_with_slot(Object *a1)
{
    Object *a2;

    if (a1->type == 0x34) {
        a2 = find_decoration(a1);
        if (a2->type != 0)
            kill_object(a2);
    }
    slot_free(a1->fn_hit);
    kill_object(a1);
}

/* orig $005C9E: a1 and what belongs to it go back to the free list */
void free_with_slot(Object *a1)
{
    Object *a2;

    if (a1->type == 0x34) {
        a2 = find_decoration(a1);
        if (a2->type != 0)
            unlink_to_free(a2);
    }
    slot_free(a1->fn_hit);
    unlink_to_free(a1);
}

/* orig $005D8C: scrolls the objects of a list (their y by d7) and frees those that are not fixed */
void free_chain(Object *a1, int16_t d7)
{
    while (a1->type != 0) {
        Object *next = a1->prev;

        if (a1->persists == 0)
            unlink_to_free(a1);
        else if (a1->screen_fixed == 0)
            a1->y -= d7;
        a1 = next;
    }
}

/* the slot's object grows: orig $004B94 (slot 0), $004D14 (slot 6), $005000 (slot 5) */
static Object *grow_weapon(Slot *s)
{
    Object *a1 = s->obj;

    if (a1->energy > a1->score)
        a1->score++;
    return a1;
}

/* orig $004F36: the weapon of slot 5 is dismissed */
static void dismiss_slot5(void)
{
    if (SLOT(5)->type != 0)
        kill_object(SLOT(5)->obj);
}

/* orig $005540: the basic weapon comes back when slot 0 is empty */
Object *refill_slot0(Object *a0, Object *a1)
{
    if (weapon_slots[0].type == 0)
        weapon_94(a0, a1);
    return a1;
}

/* orig $0052D0 (with $23F0): the first free of the four side slots, or 0; *d0 = 3 - its index */
static Slot *free_side_slot(int *d0)
{
    int i;

    for (i = 0; i < 4; i++) {
        if (SLOT(1 + i)->type == 0) {
            *d0 = 3 - i;
            return SLOT(1 + i);
        }
    }
    return 0;
}

/* orig $004AFC: weapon $94 (slot 0); returns a1 */
Object *weapon_94(Object *a0, Object *a1)
{
    Slot *s = SLOT(0);
    Object *n;

    if (s->type != 0)
        kill_object(s->obj);
    s->type = 0x94;
    n = alloc_a(a0);
    s->obj = n;
    n->type = 0x94;
    n->fn_update = FN(weapon94_update);
    n->fn_hit = s;
    n->fn_draw = obj_nop;
    n->score = 0;
    n->energy = 2;
    n->kind = 1;
    SET_V5E(n, -1);
    autofire_period = 8;
    return n;
}

/* orig $004BA8: weapon $C0 (slot 0) */
Object *weapon_c0(Object *a0, Object *a1)
{
    Slot *s = SLOT(0);
    Object *n;

    if (s->type == 0xc0)
        return grow_weapon(s);
    if (s->type != 0)
        kill_object(s->obj);
    s->type = 0xc0;
    n = alloc_a(a0);
    s->obj = n;
    n->type = 0xc0;
    n->fn_update = weaponc0_update;
    n->fn_draw = draw_bob_masked;
    n->fn_hit = s;
    n->gfx = v_192ba;
    n->anim_delay = 0;
    n->x = PLAYER->x;
    n->y = PLAYER->y;
    n->score = 0;
    n->energy = 2;
    n->kind = 0xe;
    SET_V5E(n, -1);
    autofire_period = 8;
    return n;
}

/* orig $004C64: weapon $98 (slot 6; slot 5 loses $44) */
Object *weapon_98(Object *a0, Object *a1)
{
    Slot *s5 = SLOT(5), *s6 = SLOT(6);
    Object *n;

    if (s6->type == 0x98)
        return grow_weapon(s6);
    if (s6->type != 0)
        return a1;
    if (s5->type == 0x44) {
        kill_object(s5->obj);
        s5->type = 0;
    }
    n = alloc_a(a0);
    s6->type = 0x98;
    s6->obj = n;
    n->type = 0x98;
    n->fn_update = FN(weapon98_update);
    n->fn_draw = obj_nop;
    n->fn_hit = s6;
    n->score = 0;
    n->energy = 2;
    n->kind = 0xa;
    SET_V5E(n, -1);
    autofire_period = 8;
    return n;
}

/* orig $004D28: weapon $28 (slot 6) */
Object *weapon_28(Object *a0, Object *a1)
{
    Slot *s6 = SLOT(6);
    Object *n;

    if (s6->type == 0x28)
        return grow_weapon(s6);
    if (s6->type != 0)
        kill_object(s6->obj);
    s6->type = 0x28;
    n = alloc_a(a0);
    s6->obj = n;
    n->type = 0x9c;
    n->fn_update = FN(weapon9c_update);
    n->fn_draw = obj_nop;
    n->fn_hit = s6;
    n->score = 0;
    n->energy = 0;
    n->kind = 0x15;
    SET_V5E(n, -1);
    autofire_period = 0xc;
    return n;
}

/* orig $004DC6: weapon $48 (slot 5 or 6) */
Object *weapon_48(Object *a0, Object *a1)
{
    Slot *s5 = SLOT(5), *s6 = SLOT(6), *a2;
    Object *n;

    if (s5->type == 0x48)
        return grow_weapon(s5);
    if (s6->type == 0x48)
        return grow_weapon(s6);
    a2 = s5;
    if (a2->type != 0) {
        a2 = s6;
        if (a2->type != 0)
            kill_object(a2->obj);
    }
    a2->type = 0x48;
    n = alloc_a(a0);
    a2->obj = n;
    n->type = 0xa0;
    n->fn_update = FN(weaponA0_update);
    n->fn_draw = obj_nop;
    n->fn_hit = a2;
    n->score = 0;
    n->energy = 0;
    n->kind = 0x17;
    SET_V5E(n, -1);
    autofire_period = 0x1e;
    return n;
}

/* orig $004E74: weapon $B0 (slot 0) */
Object *weapon_b0(Object *a0, Object *a1)
{
    Slot *s = SLOT(0);
    Object *n;

    if (s->type == 0xb0)
        return grow_weapon(s);
    if (s->type != 0)
        kill_object(s->obj);
    s->type = 0xb0;
    n = alloc_a(a0);
    s->obj = n;
    n->type = 0xb0;
    n->fn_update = weaponb0_update;
    n->fn_draw = draw_bob_masked;
    n->fn_hit = s;
    set_anim(n, anim_2540);
    n->x = PLAYER->x;
    n->y = PLAYER->y;
    n->score = 0;
    n->energy = 2;
    n->kind = 0x14;
    SET_V5E(n, -1);
    autofire_period = 8;
    return n;
}

/* the weapons of slot 5 share their set-up: type, routine, animation, energy, kind, y offset 0x19 (or none) */
static Object *weapon_slot5(Object *a0, uint16_t type, obj_fn update, const uint8_t *anim, int16_t energy, int16_t score, int16_t kind, int dy)
{
    Slot *s5 = SLOT(5);
    Object *n;

    n = alloc_a(a0);
    s5->type = type;
    s5->obj = n;
    n->type = type;
    n->fn_update = update;
    n->fn_draw = draw_bob_masked;
    n->fn_hit = s5;
    set_anim(n, anim);
    n->x = PLAYER->x;
    n->y = PLAYER->y + dy;
    n->v28 = 0;
    n->score = score;
    n->energy = energy;
    n->kind = kind;
    SET_V5E(n, -1);
    return n;
}

/* orig $004F66: weapon $2C (slot 5) */
Object *weapon_2c(Object *a0, Object *a1)
{
    if (SLOT(5)->type == 0x2c)
        return grow_weapon(SLOT(5));
    dismiss_slot5();
    return weapon_slot5(a0, 0x2c, weapon2c_update, anim_25be, 2, 0, 0x13, 0x19);
}

/* orig $005014: weapon $4C (slot 5) */
Object *weapon_4c(Object *a0, Object *a1)
{
    if (SLOT(5)->type == 0x4c)
        return grow_weapon(SLOT(5));
    dismiss_slot5();
    return weapon_slot5(a0, 0x4c, weapon4c_update, anim_2a02, 0, 0, 0xb, 0x19);
}

/* orig $0050AC: weapon $38 (slot 5) */
Object *weapon_38(Object *a0, Object *a1)
{
    Object *n;

    if (SLOT(5)->type == 0x38 || SLOT(5)->type == 0x3c)
        return grow_weapon(SLOT(5));
    dismiss_slot5();
    n = weapon_slot5(a0, 0x38, weapon38_update, anim_28b2, 1, 0, 9, 0x19);
    trail_x = -100;
    return n;
}

/* orig $005158: weapon $3C (slot 5) */
Object *weapon_3c(Object *a0, Object *a1)
{
    Object *n;

    if (SLOT(5)->type == 0x38 || SLOT(5)->type == 0x3c)
        return grow_weapon(SLOT(5));
    dismiss_slot5();
    n = weapon_slot5(a0, 0x3c, weapon38_update, anim_28d0, 1, 1, 0xd, 0x19);
    trail_x = -100;
    return n;
}

/* orig $005206: weapon $44 (slot 5; slot 6 loses $98) */
Object *weapon_44(Object *a0, Object *a1)
{
    Slot *s6 = SLOT(6);

    if (SLOT(5)->type == 0x44)
        return grow_weapon(SLOT(5));
    if (s6->type == 0x98) {
        kill_object(s6->obj);
        s6->type = 0;
    }
    dismiss_slot5();
    return weapon_slot5(a0, 0x44, FN(weapon44_update), anim_283a, 2, 0, 8, 0);
}

/* the weapons of the side slots: orig $0052E0 ($90), $00538C ($20), $00543A ($34); 0 when the four slots are taken */
static Object *weapon_side(Object *a0, uint16_t type, obj_fn update, const uint8_t *anim, int16_t energy, int16_t kind, int clear28)
{
    int d0;
    Slot *a2 = free_side_slot(&d0);
    Object *n;

    if (a2 == 0)
        return 0;
    n = alloc_a(a0);
    a2->type = type;
    a2->obj = n;
    n->type = type;
    n->fn_update = update;
    n->fn_draw = draw_bob_masked;
    n->fn_hit = a2;
    n->v3e = weapon_slot_offsets[2 * d0];
    n->v40 = weapon_slot_offsets[2 * d0 + 1];
    set_anim(n, anim);
    n->x = PLAYER->x + n->v3e;
    n->y = PLAYER->y + n->v40;
    if (clear28)
        n->v28 = 0;
    n->score = 0;
    n->energy = energy;
    n->kind = kind;
    SET_V5E(n, -1);
    return n;
}

/* orig $0052E0 */
Object *weapon_90(Object *a0, Object *a1)
{
    Object *n = weapon_side(a0, 0x90, weapon90_update, anim_2c24, 0, 0x11, 1);

    return refill_slot0(a0, n ? n : a1);
}

/* orig $00538C */
Object *weapon_20(Object *a0, Object *a1)
{
    Object *n = weapon_side(a0, 0x20, weapon20_update, anim_2612, 2, 0x12, 1);

    return refill_slot0(a0, n ? n : a1);
}

/* orig $00543A */
Object *weapon_34(Object *a0, Object *a1)
{
    Object *n = weapon_side(a0, 0x34, weapon34_update, anim_280a, 0, 0xf, 0), *d;

    if (n == 0)
        return refill_slot0(a0, a1);
    if (n->v3e != 0) {
        d = alloc_b_front(a0);
        d->type = 0x40;
        d->fn_update = pod_decoration_update;
        d->fn_draw = draw_bob_masked;
        d->persists = -1;
        set_anim(d, anim_28ee);
        set_ptr_2a(d, n);
        d->x = n->x;
        d->y = n->y;
    }
    return refill_slot0(a0, n);
}

/* orig $004822: the weapon of type d1 is given to the player; returns a1 */
Object *weapon_select(uint16_t d1, Object *a0, Object *a1)
{
    switch (d1) {
    case 0x44: return weapon_44(a0, a1);
    case 0x98: return weapon_98(a0, a1);
    case 0x34: return weapon_34(a0, a1);
    case 0x2c: return weapon_2c(a0, a1);
    case 0x20: return weapon_20(a0, a1);
    case 0x4c: return weapon_4c(a0, a1);
    case 0x3c: return weapon_3c(a0, a1);
    case 0x38: return weapon_38(a0, a1);
    case 0xc0: return weapon_c0(a0, a1);
    case 0x90: return weapon_90(a0, a1);
    case 0x48: return weapon_48(a0, a1);
    case 0xb0: return weapon_b0(a0, a1);
    case 0x28: return weapon_28(a0, a1);
    case 0x94: return weapon_94(a0, a1);
    }
    return a1;
}

/* orig $005592: update of weapon $94 */
void weapon94_update(Object *a0)
{
    if (dive_phase)
        return;
    if (fire_press)
        spawn_shot_object(a0, 0, -9, 0, -6, (void *const *)&player_shot_pictures[3]);
}

/* orig $0055B2: update of weapon $98 */
void weapon98_update(Object *a0)
{
    if (dive_phase)
        return;
    if (fire_press) {
        spawn_shot_object(a0, -9, 0, -2, -3, (void *const *)&player_shot_pictures[0]);
        spawn_shot_object(a0, 9, 0, 2, -3, (void *const *)&player_shot_pictures[6]);
    }
}

/* orig $0055E2: update of weapon $44 */
void weapon44_update(Object *a0)
{
    anim_step(a0);
    a0->x = PLAYER->x;
    a0->y = PLAYER->y;
    if (respawn_shield) {
        weapon_shield_draw(a0);
        return;
    }
    a0->fn_draw = draw_bob_masked;
    if (fire_press) {
        a0->anim_delay = 1;
        spawn_shot_object(a0, 0, 9, 0, 0x12, (void *const *)&player_shot_pictures[9]);
    }
}

/* orig $005624: update of weapon $9c (slot 6, type $28) */
void weapon9c_update(Object *a0)
{
    Object *a1, *n;
    int16_t d0;

    if (dive_phase)
        return;
    if (V5E(a0) >= 0)
        SET_V5E(a0, V5E(a0) - 1);
    if (fire_press == 0 || V5E(a0) >= 0)
        return;
    for (a1 = LIST_B->prev; a1->type != 0; a1 = a1->prev)
        if (a1->type == 0x28)
            return;
    SET_V5E(a0, 0xf);
    n = alloc_b(a0);
    n->type = 0x28;
    n->fn_update = shell_update;
    n->fn_draw = draw_bob_masked;
    n->persists = -1;
    set_anim(n, anim_25e2);
    n->x = PLAYER->x;
    d0 = PLAYER->y - 0xc;
    n->y = d0;
    n->v28 = -(int16_t)(((uint16_t)d0 >> 4) + 4);
    SET_V5E(n, 6);
}

/* the eight pictures of the drill, by the loop counter */
static const uint8_t *const drill_anim[8] = { anim_2912, anim_2930, anim_294e, anim_296c, anim_298a, anim_29a8, anim_29c6, anim_29e4 };

/* orig $0056D6: update of weapon $a0 (slot 5 or 6, type $48) */
void weaponA0_update(Object *a0)
{
    Object *a1, *cur;
    int16_t cnt;

    if (dive_phase)
        return;
    if (V5E(a0) >= 0)
        SET_V5E(a0, V5E(a0) - 1);
    if (fire_press == 0 || V5E(a0) >= 0)
        return;
    for (a1 = LIST_B->prev; a1->type != 0; a1 = a1->prev)
        if (a1->type == 0x48)
            return;
    SET_V5E(a0, 0x12);
    sfx_cmd = 9;
    cur = a0;
    for (cnt = 7; cnt >= 0; cnt -= 2) {
        cur = alloc_b(cur);
        cur->type = 0x48;
        cur->fn_update = drill_update;
        cur->fn_draw = draw_bob_masked;
        cur->persists = -1;
        set_anim(cur, drill_anim[cnt]);
        cur->x = PLAYER->x;
        cur->y = PLAYER->y;
        cur->v2a = cnt;
        cur->v30 = 0;
        cur->energy = 0;
        cur->v28 = 0xd;
    }
}

/* a shot that has hit or left the screen is a dying object */
static void shot_gone(Object *a0)
{
    kill_object(a0);
}

/* orig $0057BC: update of a shot that moves by a 16.16 speed ($2A) */
void shot_fixed_update(Object *a0)
{
    Object *a1;
    uint32_t pos;
    int32_t d0;
    uint16_t d;

    if (player_dead) {
        shot_gone(a0);
        return;
    }
    anim_step(a0);
    if (a0->type == 4)
        return;
    d0 = (int32_t)(((uint32_t)(uint16_t)SHOT_SPEED_HI(a0) << 16) | (uint16_t)SHOT_SPEED_LO(a0)) >> 6;
    pos = ((uint32_t)(uint16_t)a0->x << 16) | a0->x_frac;
    pos += (uint32_t)d0;
    a0->x = (int16_t)(pos >> 16);
    a0->x_frac = (uint16_t)pos;
    if ((uint16_t)a0->x >= 0x140) {
        shot_gone(a0);
        return;
    }
    a0->y -= 0xc;
    if (a0->y < 0) {
        shot_gone(a0);
        return;
    }
    if (a0->v30 == 0)
        return;
    a1 = collide_enemies(a0);
    if (a1 == 0)
        return;
    d = (uint16_t)(a0->score + kill_scores[1]);
    shot_gone(a0);
    apply_damage(a0, a1, d);
}

/* orig $005850: update of a shot ($10) that moves by ($2A, $2E) */
void shot_update(Object *a0)
{
    Object *a1;
    uint16_t d;

    if (player_dead) {
        shot_gone(a0);
        return;
    }
    anim_step(a0);
    a0->x += SHOT_DX(a0);
    if ((uint16_t)a0->x >= 0x138) {
        shot_gone(a0);
        return;
    }
    a0->y += SHOT_DY(a0);
    if ((uint16_t)a0->y >= 0xc0) {
        shot_gone(a0);
        return;
    }
    a1 = point_in_enemy(a0->x, a0->y);
    if (a1 == 0)
        return;
    d = (uint16_t)(a0->score + kill_scores[0]);
    shot_gone(a0);
    apply_damage(a0, a1, d);
}

/* the centre of o's picture (the 16-bit arithmetic of the original) for an explosion */
static void centre(const Object *o, int16_t *x, int16_t *y)
{
    const uint8_t *a5 = (const uint8_t *)o->gfx;
    int16_t dx = (int16_t)(o->x - (int16_t)rd16(a5)), dy = (int16_t)(o->y - (int16_t)rd16(a5 + 2));

    dx = (int16_t)(dx + dx);
    dy = (int16_t)(dy + dy);
    *x = (int16_t)(dx + (int16_t)rd16(a5 + 4)) >> 1;
    *y = (int16_t)(dy + (int16_t)rd16(a5 + 6)) >> 1;
}

/* orig $E1E, $00592A: hit routine of the parts of a big enemy (a1 = a part; its chain is $56/$5A) */
void hit_part(Object *a0, Object *a1, uint16_t d0)
{
    Object *a2, *next;
    uint16_t old;

    a1 = a1->target;
    for (a2 = a1; ; a2 = a2->chain) {
        a2->fn_draw = draw_bob_or_first;
        if (a2->chain == 0)
            break;
    }
    old = (uint16_t)a1->energy;
    a1->energy = (int16_t)(old - d0);
    if (old > d0)
        return;
    for (next = a1->chain; next != 0; next = a2->chain) {
        a2 = next;
        if (a2->is_part >= 0) {
            int16_t x, y;

            centre(a2, &x, &y);
            spawn_effect(x, y, a1->big ? anim_273a : anim_270e, draw_bob_masked);
        }
        kill_object(a2);
    }
    enemy_killed(a0, a1);
}

/* orig $E84, $0059C8: hit routine that only flashes the object */
void hit_flash(Object *a0, Object *a1, uint16_t d0)
{
    a1->fn_draw = draw_bob_or_first;
}

/* orig $F2C, $0059DA: kills a0 (the shot) unless it is free or the player */
void hit_kill_shooter(Object *a0, Object *a1, uint16_t d0)
{
    if (a0->type != 0 && a0->type != 8)
        kill_object(a0);
}

/* orig $F26, $0059D2: flashes a1 and kills a0 (the shot) */
void hit_flash_kill_shooter(Object *a0, Object *a1, uint16_t d0)
{
    a1->fn_draw = draw_bob_or_first;
    hit_kill_shooter(a0, a1, d0);
}

/* orig $E18, $005A0A: hit routine of an ordinary enemy */
void hit_enemy(Object *a0, Object *a1, uint16_t d0)
{
    uint16_t old;

    a1->fn_draw = draw_bob_or_first;
    old = (uint16_t)a1->energy;
    a1->energy = (int16_t)(old - d0);
    if (old > d0)
        return;
    enemy_killed(a0, a1);
}

static void explosion_at(Object *a1, const uint8_t *anim)
{
    int16_t x, y;

    centre(a1, &x, &y);
    spawn_effect(x, y, anim, draw_bob_masked);
}

/* orig $EE4, $005A1A: an enemy a1 dies: explosion, maybe a bonus, its score */
void enemy_killed(Object *a0, Object *a1)
{
    uint16_t *e = pickup_groups;
    Object *n;
    const uint8_t *a2;
    int i, found = 0;
    int16_t d0;

    explosion_at(a1, a1->big ? anim_273a : anim_270e);
    d0 = a1->kind;
    if (d0 != 0) {
        for (i = 0; i < 8; i++) {
            if (e[2 * i] == (uint16_t)d0) {
                found = 1;
                break;
            }
        }
        if (found && --e[2 * i + 1] == 0) {
            e[2 * i] = 0;
            n = alloc_b(a0);
            a2 = anim_26ae;
            n->type = 0x60;
            if (a1->big) {
                a2 = anim_264e;
                n->type = 0x18;
            }
            n->v28 = 7;
            n->fn_update = FN(pickup_update);
            n->fn_draw = draw_bob_masked;
            set_anim(n, a2);
            n->x = a1->x;
            n->y = a1->y;
            n->v2a = (int16_t)random_next();
        }
    }
    player_score += (uint16_t)a1->score;
    kill_object(a1);
}

/* orig $EA8, $005B16: hit routine: the object explodes and dies at once */
void hit_explode(Object *a0, Object *a1, uint16_t d0)
{
    explosion_at(a1, a1->big ? anim_273a : anim_270e);
    kill_object(a1);
}

/* orig $005BFC: draw routine of the tile object */
void tile_draw(Object *a0)
{
    a0->fn_draw = obj_nop;
    draw_tile_grid_flash((const uint8_t *)a0->gfx, (uint16_t)a0->v3e, (uint16_t)a0->v40, (int16_t)(a0->y - scroll_pos), (uint16_t)a0->x >> 1);
}

/* orig $F20, $005B72: hit routine of a destructible tile block: when it breaks the background is restored */
void hit_tile(Object *a0, Object *a1, uint16_t d0)
{
    uint16_t old;
    int16_t x, y, rows, cols;
    uint8_t *dst;
    const uint8_t *src;

    a1->fn_draw = FN(tile_draw);
    old = (uint16_t)a1->energy;
    a1->energy = (int16_t)(old - d0);
    if (old > d0)
        return;
    player_score += (uint16_t)a1->score;
    x = (int16_t)(((a1->v3e + 1) << 3) + a1->x);
    y = (int16_t)(((a1->v40 + 1) << 3) + a1->y - scroll_pos);
    spawn_effect(x, y, anim_273a, draw_bob_masked);
    dst = a1->script;
    src = a1->anim;
    for (rows = a1->v40; rows >= 0; rows--) {
        uint8_t *row = dst;

        for (cols = a1->v3e; cols >= 0; cols--) {
            wr16(row, rd16(src));
            row += 2;
            src += 2;
        }
        dst += 40;
    }
    kill_object(a1);
}

/* orig $004894 (+$14) and $0048B4 (+$28): the shield grows (at most $27) */
static void shield_up(uint16_t d)
{
    player_energy += d;
    sfx_cmd = 7;
    if ((int16_t)player_energy > 0x27)
        player_energy = 0x27;
    energy_dirty = 0xff;
}

void pickup_shield_small(Object *a0) { shield_up(0x14); }
void pickup_shield_big(Object *a0) { shield_up(0x28); }

/* orig $0048BC: the weakest weapon of the seven slots grows */
void pickup_grow_weakest(Object *a0)
{
    int i;
    int16_t d0 = 0x7f;
    Slot *a2 = SLOT(0);
    Object *a1;

    for (i = 0; i < 7; i++) {
        Slot *s = SLOT(i);

        if (s->type != 0) {
            int16_t d2 = s->obj->score;

            if (d2 != s->obj->energy && d2 < d0) {
                d0 = d2;
                a2 = s;
            }
        }
    }
    if (d0 != 0x7f) {
        a1 = a2->obj;
        a1->score++;
    }
}

/* orig $0048F6 */
void pickup_speed(Object *a0)
{
    if ((int16_t)autofire_level < 4)
        autofire_level++;
}

/* orig $004906 */
void pickup_move(Object *a0)
{
    if (speed_level != 2)
        speed_level++;
}

/* orig $004914 */
void pickup_flag_d83(Object *a0)
{
    protection = 0xff;
}

/* orig $00491A */
void pickup_ca0(Object *a0)
{
    dive_charges += 3;
}

/* orig $004920 */
void pickup_weapon_98(Object *a0)
{
    weapon_98(a0, 0);
}

/* orig $004904, $004924: pickups with no effect */
void pickup_nothing(Object *a0)
{
}

/* orig $004926: a bomb: the screen flashes, every enemy but the bosses is hit, the bonus counters are cleared
 * (HLE: palette flash; the colours are written to the custom chip) */
void pickup_bomb(Object *a0)
{
    uint32_t saved[8];
    uint16_t d0, d1;
    int i, k, n;
    Object *a1, *a2;

    for (i = 0; i < 8; i++)
        saved[i] = ((uint32_t)palette_shadow[2 * i] << 16) | palette_shadow[2 * i + 1];
    sfx_cmd = 2;
    for (n = 0x1e; n >= 0; n--) {
        d0 = (uint16_t)random_next();
        for (k = 0; k < 16; k++) {
            d1 = 0;
            if (d0 & 1)
                d1 = 0x777;
            d0 >>= 1;
            palette_shadow[k] = d1;
            custom->color[k] = (uint16_t)(d1 + d1);
        }
        d0 = vbl_count;
        while (vbl_count == d0)
            HOST_IDLE();
    }
    for (i = 0; i < 8; i++) {
        uint32_t v = saved[i];

        palette_shadow[2 * i] = (uint16_t)(v >> 16);
        palette_shadow[2 * i + 1] = (uint16_t)v;
        v += v;
        custom->color[2 * i] = (uint16_t)(v >> 16);
        custom->color[2 * i + 1] = (uint16_t)v;
    }
    for (a1 = LIST_C->prev; a1->type != 0; a1 = a2) {
        d0 = a1->type;
        a2 = owner_chain_next(a1);
        if (d0 != 0x50 && d0 != 0x54)
            apply_damage((Object *)&obj_list[1], a1, 0x7f);
    }
    for (a1 = LIST_B->prev; a1->type != 0; ) {
        Object *next = a1->prev;

        if (a1->type == 0x14) {
            a1->type = 4;
            a1->fn_update = free_object;
            a1->fn_draw = obj_nop;
            a1->fn_hit = (void *)obj_nop;
            a1->is_part = 0;
            a1->persists = 0;
        }
        a1 = next;
    }
    for (i = 0; i < 16; i++)
        pickup_groups[i] = 0;
}

/* orig $004A94: a pickup that makes a spinning object */
void pickup_spinner(Object *a0)
{
    Object *a1;

    if (shield_timer == 0) {
        a1 = alloc_b(a0);
        a1->type = 0x5c;
        a1->fn_update = spinner_update;
        a1->fn_draw = draw_bob_masked;
        a1->x = PLAYER->x;
        a1->y = PLAYER->y;
        set_anim(a1, anim_25a0);
    }
    shield_timer += 0xaa;
}

/* orig $004A0A: the weapons are set up for a new life/level */
void init_weapons(Object *a0)
{
    Object *a1 = 0;
    int i;

    list_a_prev_saved = LIST_A->prev;
    list_a_next_saved = LIST_A->next;
    for (i = 0; i < 7; i++) {
        weapon_slots_saved[i] = weapon_slots[i];
        weapon_slots[i].type = 0;
        weapon_slots[i].obj = 0;
    }
    LIST_A->type = 0;
    LIST_A->prev = LIST_A;
    LIST_A->next = LIST_A;
    LIST_A->fn_update = obj_nop;
    LIST_A->fn_draw = obj_nop;
    LIST_A->fn_hit = (void *)obj_nop;
    a1 = weapon_94(a0, a1);
    a1 = weapon_34(a0, a1);
    a1 = weapon_34(a0, a1);
    a1 = weapon_20(a0, a1);
    a1->score = 2;
    a1 = weapon_20(a0, a1);
    a1->score = 2;
    a1 = weapon_2c(a0, a1);
    a1->score = 2;
    a1 = weapon_98(a0, a1);
    a1->score = 2;
    a1 = weapon_c0(a0, a1);
    a1->score = 2;
}

/* the pickup routines, by $30(object): the table at $47D6 (the first byte of an entry says the sound: 4, else $81) */
static void pickup_call(int idx, Object *a0)
{
    static const uint8_t loud[19] = { 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0 };

    sfx_cmd = loud[idx] ? 4 : 0x81;
    switch (idx) {
    case 0: pickup_move(a0); break;
    case 1: pickup_speed(a0); break;
    case 2: pickup_shield_small(a0); break;
    case 3: pickup_shield_big(a0); break;
    case 4: weapon_44(a0, 0); break;
    case 5: pickup_weapon_98(a0); break;
    case 6: pickup_grow_weakest(a0); break;
    case 7: weapon_34(a0, 0); break;
    case 8: weapon_2c(a0, 0); break;
    case 9: weapon_20(a0, 0); break;
    case 10: weapon_4c(a0, 0); break;
    case 11: weapon_38(a0, 0); break;
    case 12: weapon_90(a0, 0); break;
    case 13: weapon_48(a0, 0); break;
    case 14: weapon_b0(a0, 0); break;
    case 15: weapon_28(a0, 0); break;
    case 16: pickup_spinner(a0); break;
    case 17: pickup_ca0(a0); break;
    case 18: pickup_bomb(a0); break;
    }
}

/* orig $F3E, $004634: update of a pickup (type $18 or $60: a bonus; else a weapon pickup, $30 says which) */
void pickup_update(Object *a0)
{
    int16_t d0, d1;
    uint16_t d7;
    int idx;

    anim_step(a0);
    if (PICKUP_MODE(a0) == 0) {
        a0->y += 8;
        if (a0->y >= 0xc8) {
            if (level_end_items != 0) {
                level_end_items--;
                if (level_end_items == 0)
                    level_over = 0xff;
            }
            kill_object(a0);
            return;
        }
    } else {
        if (PICKUP_MODE(a0) > 0) {
            PICKUP_MODE(a0) &= 7;
            if (PICKUP_MODE(a0) == 0)
                a0->v2a = (int16_t)direction8((int16_t)(0xa0 - a0->x), (int16_t)(0x64 - a0->y));
            PICKUP_MODE(a0)++;
            d0 = (int16_t)(0xa0 - a0->x);
            d1 = (int16_t)(0x64 - a0->y);
            if ((int32_t)d0 * d0 + (int32_t)d1 * d1 < 0x190)
                PICKUP_MODE(a0) = -34;
        } else {
            PICKUP_MODE(a0)++;
            if ((PICKUP_MODE(a0) & 3) == 0) {
                a0->v2a++;
                a0->v2a &= 7;
            }
        }
        idx = (a0->v2a << 2) & 0x1c;
        a0->x += pickup_circle_steps[idx >> 1];
        a0->y += pickup_circle_steps[(idx >> 1) + 1];
    }
    bbox_from_frame(a0);
    if (!player_hits(a0))
        return;
    d0 = a0->type;
    d7 = 0x64;
    if (d0 != 0x18) {
        d7 = 0x32;
        if (d0 != 0x60) {
            idx = a0->v30;
            kill_object(a0);
            pickup_call(idx, a0);
            return;
        }
    }
    player_money += d7;
    sfx_cmd = 5;
    if (level_end_items != 0) {
        level_end_items--;
        if (level_end_items == 0)
            level_over = 0xff;
    }
    kill_object(a0);
}

/* orig $005D00: the player dies */
void player_dies(Object *a0)
{
    Object *a1;
    const uint8_t *a2 = anim_2760;

    if (level_end_items != 0)
        return;
    player_dead = 0xff;
    player_energy = 0;
    energy_dirty = 0xff;
    sfx_cmd = 10;
    sfx_cmd2 = 10;
    dive_phase = 1;
    dive_timer = 0;
    set_anim(PLAYER, a2);
    PLAYER->fn_draw = draw_bob_masked;
    a1 = LIST_A->prev;
    if (a1->type == 0)
        return;
    if (a1->type == 0x94 && a1->score == 0) {
        a1 = a1->prev;
        if (a1->type == 0)
            return;
    }
    kill_with_slot(a1);
    refill_slot0(a0, a1);
}

/* orig $EB4, $005D6E: the player takes d0 damage */
void player_damage(Object *a0, uint16_t d0)
{
    uint16_t old;

    if (level_end_items != 0)
        return;
    if (protection != 0)
        d0 >>= 1;
    energy_dirty = 0xff;
    old = player_energy;
    player_energy = (uint16_t)(old - d0);
    if (old > d0)
        return;
    player_dies(a0);
}

/* orig $005DE6: the player's steering: player_vx (the sideways speed), the stick in joy_stick */
void player_move(Object *a0)
{
    int16_t d0;

    player_x_prev = a0->x;
    player_y_prev = a0->y;
    v_5dd0 = player_vx;
    if (joy_stick & 4) {
        if (player_vx > 0)
            player_vx = 0;
        if (player_vx > -6)
            player_vx--;
    } else if (joy_stick & 8) {
        if (player_vx < 0)
            player_vx = 0;
        if (player_vx < 6)
            player_vx++;
    } else if (player_vx != 0) {
        if (player_vx < 0)
            player_vx++;
        else
            player_vx--;
    }
    scroll_step = scroll_step_target;
    if (joy_stick & 1) {
        if (a0->y <= 0x10) {
            a0->y = 0x10;
            scroll_step = 1;
            goto side;
        }
        a0->y -= 3;
        a0->y -= speed_level;
    }
    if (joy_stick & 2) {
        if (a0->y < 0xb0) {
            a0->y += 3;
            a0->y += speed_level;
        } else if (scroll_pos != scroll_max) {
            scroll_step = 0xffff;
            a0->y = 0xb0;
        }
    }
side:
    d0 = player_vx;
    if (d0 != 0) {
        if (d0 < 0) {
            if (joy_stick & 4)
                d0 = -6;
        } else if (joy_stick & 8) {
            d0 = 6;
        }
    }
    if (speed_level == 0)
        d0 >>= 1;
    if ((int16_t)speed_level >= 2)
        d0 += d0 >> 1;
    a0->x += d0;
    if (a0->x < 0xe)
        a0->x = 0xe;
    if (a0->x > 0x130)
        a0->x = 0x130;
}

/* orig $005EFE: update of the player (a0 = $3F01C) */
void player_update(Object *a0)
{
    Object *a1;
    int16_t d0;
    int i;

    a0->fn_draw = draw_bob_masked;
    if (player_dead) {
        scroll_step = 0;
        anim_step(a0);
        if (a0->anim_delay == 0) {
            a0->fn_draw = obj_nop;
            if ((int16_t)respawn_shield >= 0x10)
                player_died = 0xff;
        }
        return;
    }
    if (dive_phase == 0) {
        a1 = 0;
        if (shades_timer == 0) {
            bbox_from_frame(a0);   /* $5F42: the box of the player's picture, then the test */
            a1 = collide_enemies(a0);
            if (a1 == 0)
                goto after_hit;
        } else {
            d0 = (int16_t)(a0->x - 0x40);
            a0->hit[0] = d0;
            a0->hit[2] = (int16_t)(d0 + 0x80);
            d0 = (int16_t)(a0->y - 0x40);
            a0->hit[1] = d0;
            a0->hit[3] = (int16_t)(d0 + 0x80);
            a1 = collide_enemies(a0);
            if (a1 != 0)
                apply_damage(a0, a1, 0x7f);
            bbox_from_frame(a0);
            goto after_hit;
        }
        if (shield_timer == 0) {
            if (a1->type == 0)
                goto after_hit;
            a0->fn_draw = draw_bob_or_first;
            player_damage(a0, a1->big ? 0x10 : 8);
            if (player_dead)
                return;
        }
        if (a1->type != 0x50 && a1->type != 0x54)
            apply_damage(a0, a1, 0x7f);
    }
after_hit:
    if (crush_timer != 0) {
        if ((int16_t)crush_timer < 0) {
            if (!player_tile_hit(a0)) {
                crush_timer = 0;
                goto move;
            }
            if (crush_timer == 0xffef) {
                player_dies(a0);
                return;
            }
        }
        crush_timer--;
        scroll_step = scroll_step_target;
        d0 = (int16_t)(player_history[3] - scroll_pos);
        a0->x = player_history[4];
        d0 += player_history[5];
        if (d0 > 0xb0) {
            d0 -= 0xb0;
            scroll_step -= d0;
            d0 = 0xb0;
        }
        if (d0 < 0x10) {
            d0 -= 0x10;
            scroll_step -= d0;
            d0 = 0x10;
        }
        a0->y = d0;
        for (i = 0; i < 48; i++)
            player_history[i] = player_history[i + 3];
        if (crush_timer == 0)
            crush_timer = 0xffff;
    } else {
move:
        player_move(a0);
        for (i = 50; i >= 3; i--)
            player_history[i] = player_history[i - 3];
        player_history[0] = scroll_pos;
        player_history[1] = a0->x;
        player_history[2] = a0->y;
        if (dive_phase == 0 && (int16_t)crush_timer >= 0 && player_tile_hit(a0)) {
            crush_timer = crush_reload;
            player_vx = 0;
            d0 = (int16_t)(scroll_pos + scroll_margin);
            if (d0 > (int16_t)scroll_max)
                scroll_max = d0;
        }
    }
    for (i = 0; i < 6; i++)
        player_trail[i] = player_trail[i + 2];
    player_trail[6] = player_x_prev;
    player_trail[7] = player_y_prev;
    if (dive_phase == 0) {
        a0->gfx = (const void *)player_pictures[player_vx + 6];
        return;
    }
    a0->gfx = (const void *)dive_pictures[dive_phase - 1];
    a0->fn_draw = draw_bob_masked;
    dive_phase += dive_step;
    if (dive_phase == 0) {
        dive_step = 0;
        crush_timer = 0xfff1;
        return;
    }
    if (dive_phase == 4) {
        if (dive_step == 0)
            a0->fn_draw = draw_bob_behind_map;
        dive_step = 0;
    }
}

/* ---- weapons and level 1 routines the recorded run never reached (from the listing) ---- */

/* the pointer in the words $30 and $32 (the drill's target) */
static Object *get_ptr_30(const Object *o)
{
    uint32_t v = ((uint32_t)(uint16_t)o->v30 << 16) | (uint16_t)o->energy;

    return v ? (Object *)GPTR(v) : 0;
}

static void set_ptr_30(Object *o, Object *p)
{
    uint32_t v = p ? GADDR(p) : 0;

    o->v30 = (int16_t)(v >> 16);
    o->energy = (int16_t)v;
}

/* orig $006912: the draw of a weapon object while respawn_shield is not 0 (after a respawn: 8 down to 0; at $10 nothing is drawn): the position
 * becomes the player's centre plus the difference of the centres times respawn_shield (the centre of the picture, not its corner) */
void weapon_shield_draw(Object *a0)
{
    const uint8_t *g;
    int16_t d0, d1, d2, d3;

    if (respawn_shield == 0x10) {
        a0->fn_draw = obj_nop;
        return;
    }
    a0->fn_draw = draw_bob_masked;
    g = (const uint8_t *)a0->gfx;
    d0 = (int16_t)(a0->x - (int16_t)rd16(g));
    d1 = (int16_t)(a0->y - (int16_t)rd16(g + 2));
    d0 = (int16_t)(d0 + d0);
    d1 = (int16_t)(d1 + d1);
    d0 = (int16_t)(d0 + (int16_t)rd16(g + 4));
    d1 = (int16_t)(d1 + (int16_t)rd16(g + 6));
    d2 = (int16_t)(d0 >> 1);
    d3 = (int16_t)(d1 >> 1);
    g = (const uint8_t *)PLAYER->gfx;
    d0 = (int16_t)(PLAYER->x - (int16_t)rd16(g));
    d1 = (int16_t)(PLAYER->y - (int16_t)rd16(g + 2));
    d0 = (int16_t)(d0 + d0);
    d1 = (int16_t)(d1 + d1);
    d0 = (int16_t)(d0 + (int16_t)rd16(g + 4));
    d1 = (int16_t)(d1 + (int16_t)rd16(g + 6));
    d0 = (int16_t)(d0 >> 1);
    d1 = (int16_t)(d1 >> 1);
    d2 = (int16_t)(d2 - d0);
    d3 = (int16_t)(d3 - d1);
    d0 = (int16_t)(d0 + (int16_t)((int32_t)d2 * (int16_t)respawn_shield));
    d1 = (int16_t)(d1 + (int16_t)((int32_t)d3 * (int16_t)respawn_shield));
    a0->x = d0;
    a0->y = d1;
}

/* orig $00249C: a random enemy on the screen (a1 in the original; the end of the list when there is none): the (random below v_04e2)th
 * one that is on the screen, groups ($B8) skipped, at most five passes over the list */
static Object *pick_target(void)
{
    uint16_t d0 = v_04e2, d1;
    Object *a1;

    if (d0 == 0)
        d0 = 1;
    d0 = random_below(d0);
    for (d1 = 4; (int16_t)d1 >= 0; d1--) {
        a1 = LIST_HEAD(LIST_ENEMY);
        while (a1->type != 0) {
            if (a1->type != 0xb8 && !(a1->hit[2] < 0 || a1->hit[0] >= 0x140 || a1->hit[3] < 0 || a1->hit[1] >= 0xc0)) {
                if ((uint16_t)d0-- == 0)
                    return a1;
            }
            a1 = owner_chain_next(a1);
        }
    }
    return LIST_C;
}

/* orig $003096: every enemy whose box meets (x0, y0)-(x1, y1) takes dmg from the shooter a0; it ends when the shooter dies or level_end_items is
 * set.  The level's own test at the end (level 1: none) is $54E0C. */
void area_damage(Object *a0, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t dmg)
{
    Object *a1 = LIST_HEAD(LIST_ENEMY);

    area_damage_kept = dmg;
    hit_box[0] = (uint16_t)x0;
    hit_box[1] = (uint16_t)y0;
    hit_box[2] = (uint16_t)x1;
    hit_box[3] = (uint16_t)y1;
    for (;;) {
        Object *a2;

        a1 = scan_boxes(a1, x0, y0, x1, y1);
        if (a1->type == 0) {
            Object *hit = level_collision(x0, y0, x1, y1, a1);

            if (hit)
                apply_damage(a0, hit, (uint16_t)x0);   /* d0 as the hook leaves it */
            return;
        }
        a2 = owner_chain_next(a1);
        apply_damage(a0, a1, area_damage_kept);
        a1 = a2;
        if (a0->type == 4 || level_end_items != 0)
            return;
    }
}

/* orig $E72, $00422A: d7 + 1 pieces (type $C, only animating) at random places in the box (x, y, w, h), animation a2; the sound $83 */
void debris_burst(Object *a0, int16_t d7, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *a2)
{
    do {
        Object *n = alloc_b(a0);

        n->type = 0xc;
        n->fn_update = FN(animate_only);
        n->fn_draw = draw_bob_masked;
        n->x = (int16_t)(random_below(w) + x);
        n->y = (int16_t)(random_below(h) + y);
        set_anim(n, a2);
    } while (--d7 != -1);
    sfx_cmd = 0x83;
    sfx_cmd2 = 0x83;
    sfx_play(0x83);
}

/* orig $E6C, $0021B6: d7 + 1 times two bonuses (types $60 and $18) fall in at random places at the front of list B */
void spawn_bonuses(int16_t d7)
{
    do {
        Object *n = alloc_b_front(0);

        n->type = 0x60;
        n->fn_update = FN(pickup_update);
        n->fn_draw = draw_bob_masked;
        set_anim(n, anim_26ae);
        n->x = (int16_t)(random_below(0x12c) + 0xa);
        n->y = (int16_t)(random_below(0xb4) + 6);
        PICKUP_MODE(n) = 7;
        level_end_items++;
        n = alloc_b_front(0);
        n->type = 0x18;
        n->fn_update = FN(pickup_update);
        n->fn_draw = draw_bob_masked;
        set_anim(n, anim_264e);
        n->x = (int16_t)(random_below(0x12c) + 0xa);
        n->y = (int16_t)(random_below(0xb4) + 6);
        PICKUP_MODE(n) = 7;
        level_end_items++;
    } while (--d7 != -1);
}

/* orig $003868: update of the spinning shield (type $5C): follows the player for shield_timer frames, drawing the gauge */
void spinner_update(Object *a0)
{
    anim_step(a0);
    a0->x = PLAYER->x;
    a0->y = PLAYER->y;
    if (respawn_shield) {
        weapon_shield_draw(a0);
        return;
    }
    a0->fn_draw = draw_bob_masked;
    shield_timer--;
    draw_gauge(4, 0xa0, shield_timer, 0xaa);
    if (shield_timer == 0)
        kill_object(a0);
}

#define DRILL_DIR(o) ((o)->v2a)

/* orig $0039A2: update of a drill bit (type $48): it turns (v2a, eight directions) towards an enemy it keeps in $30 and flies; it hits
 * the first enemy it meets and explodes when there is none or it leaves the screen */
void drill_update(Object *a0)
{
    static const uint8_t *const heads[8] = { anim_2918, anim_2936, anim_2954, anim_2972, anim_2990, anim_29ae, anim_29cc, anim_29ea };
    Object *a1;
    int16_t d0, d1;

    if (player_dead) {
        kill_object(a0);
        return;
    }
    anim_step(a0);
    if (DRILL_STATE(a0) > 0) {
        DRILL_STATE(a0)--;
        if (DRILL_STATE(a0) >= 8)
            goto fly;
        d0 = a0->x;
        d1 = a0->y;
        goto test;
    }
    a1 = get_ptr_30(a0);
    if (a1 == 0)
        goto find;
    if ((int16_t)a1->type < 0xc8 || a1->hit[2] < 0 || a1->hit[0] >= 0x140 || a1->hit[3] < 0 || a1->hit[1] >= 0xc0)
        goto find;
    goto have;
find:
    a1 = pick_target();
    if (a1->type == 0) {
        spawn_effect(a0->x, a0->y, anim_270e, draw_bob_masked);
        kill_object(a0);
        return;
    }
    set_ptr_30(a0, a1);
have:
    if (DRILL_STATE(a0) != 0) {
        DRILL_STATE(a0)++;
        if (DRILL_STATE(a0) != 0)
            goto fly;
    }
    d0 = (int16_t)(a1->hit[0] + 2 - a0->x);
    d1 = (int16_t)(a1->hit[1] + 2 - a0->y);
    d0 = (int16_t)(direction8(d0, d1) - DRILL_DIR(a0));
    if (d0 != 0) {
        if (d0 < 0)
            d0 = (int16_t)(d0 + 8);
        if (d0 < 4)
            DRILL_DIR(a0)++;
        else
            DRILL_DIR(a0)--;
        DRILL_DIR(a0) &= 7;
        DRILL_STATE(a0) = -3;
        set_anim(a0, heads[DRILL_DIR(a0)]);
    }
fly:
    d1 = (int16_t)(drill_dy[DRILL_DIR(a0)] + a0->y);
    if ((uint16_t)d1 >= 0xc0) {
        kill_object(a0);
        return;
    }
    a0->y = d1;
    d0 = (int16_t)(drill_dx[DRILL_DIR(a0)] + a0->x);
    if ((uint16_t)d0 >= 0x140) {
        kill_object(a0);
        return;
    }
    a0->x = d0;
test:
    a1 = point_in_enemy(d0, d1);
    if (a1) {
        kill_object(a0);
        apply_damage(a0, a1, weapon_damage[0]);
    }
}

/* orig $003B4A: update of weapon $C0 (slot 0): two shots a frame while fire is held */
void weaponc0_update(Object *a0)
{
    a0->x = PLAYER->x;
    a0->y = PLAYER->y;
    if (respawn_shield) {
        weapon_shield_draw(a0);
        return;
    }
    a0->fn_draw = draw_bob_masked;
    if (fire_press) {
        spawn_shot_object(a0, 0, (uint16_t)-9, (uint16_t)-5, (uint16_t)-12, (void *const *)&player_shot_pictures[3]);
        spawn_shot_object(a0, 0, (uint16_t)-9, 6, (uint16_t)-12, (void *const *)&player_shot_pictures[3]);
    }
}

/* orig $003B94: update of weapon $B0 (slot 0): while fire is held two shots (type $B4) of random spread each frame and the sound */
void weaponb0_update(Object *a0)
{
    int16_t d7;

    anim_step(a0);
    a0->x = PLAYER->x;
    a0->y = PLAYER->y;
    if (respawn_shield) {
        weapon_shield_draw(a0);
        return;
    }
    a0->fn_draw = draw_bob_masked;
    if (!(joy_stick & 0x80)) {
        if (WB0_TICK(a0) != 0) {
            sfx_stop_all();
            WB0_TICK(a0) = 0;
        }
        return;
    }
    if (WB0_TICK(a0) == 0 || sfx_channels[2].active == 0) {
        sfx_cmd = 1;
        WB0_TICK(a0) = 1;
    }
    for (d7 = 1; d7 >= 0; d7--) {
        Object *a1 = alloc_b(a0);
        int16_t d0;

        a1->type = 0xb4;
        a1->fn_update = FN(shot_fixed_update);
        a1->fn_draw = draw_bob_masked;
        a1->x = a0->x;
        a1->x_frac = 0x8000;
        d0 = (int16_t)((random_next() & 3) + a0->y - 0x10);
        if (d7 == 0)
            d0 = (int16_t)(d0 - 6);
        a1->y = d0;
        set_anim(a1, anim_2796);
        a1->score = a0->score;
        a1->v30 = d7;
        a1->v2a = (int8_t)random_next();
    }
}

/* orig $003C7A: update of the decoration (type $40) of a pod: it follows the pod ($2A) while that is there */
void pod_decoration_update(Object *a0)
{
    Object *a1;

    anim_step(a0);
    a1 = get_ptr_2a(a0);
    if (a1->type == 0x34) {
        a0->x = (int16_t)(PLAYER->x + a1->v3e);
        a0->y = (int16_t)(PLAYER->y + a1->v40);
        if (respawn_shield) {
            weapon_shield_draw(a0);
            return;
        }
        a0->fn_draw = draw_bob_masked;
        return;
    }
    kill_object(a0);
}

/* orig $003CDC: update of weapon $2C (slot 5): while fire is held every 6 frames a fan of dots (the table by the weapon's level) goes into
 * the free places of the shots' table */
void weapon2c_update(Object *a0)
{
    static const int16_t *const pat[3] = { &weapon2c_patterns[0], &weapon2c_patterns[17], &weapon2c_patterns[50] };
    const int16_t *a1;
    Shot *a2;
    int16_t d7, d6;

    anim_step(a0);
    a0->x = (int16_t)player_trail[0];
    a0->y = (int16_t)(player_trail[1] + 0x19);
    if (respawn_shield) {
        weapon_shield_draw(a0);
        return;
    }
    a0->fn_draw = draw_bob_masked;
    if (W2C_TICK(a0))
        W2C_TICK(a0)--;
    if (!(joy_stick & 0x80) || W2C_TICK(a0) != 0)
        return;
    W2C_TICK(a0) = 6;
    a1 = pat[(uint16_t)a0->score];
    d7 = *a1++;
    a2 = shots;
    for (d6 = 0x4f; d6 >= 0; d6--, a2++) {
        if (a2->x >= 0)
            continue;
        a2->x = (int16_t)(a0->x + *a1++);
        a2->y = (int16_t)(a0->y + *a1++);
        a2->dx = *a1++;
        a2->dy = *a1++;
        if (--d7 < 0)
            return;
    }
}

/* a weapon's phase while fire is held: 1 flying out / following, 2 up, 3 coming back, negative a trail copy */
#define SHOT_PHASE(o) ((o)->v28)

/* orig $003E2C: update of weapon $4C (slot 5): a beam that flies out while fire is held (v28 = 1), moved by the stick, hurting what it
 * meets, and comes back to the ship when fire is let go (v28 negative) */
void weapon4c_update(Object *a0)
{
    int16_t d0;
    Object *a1;

    anim_step(a0);
    if (respawn_shield) {
        weapon_shield_draw(a0);
        return;
    }
    a0->fn_draw = draw_bob_masked;
    if (SHOT_PHASE(a0) == 0) {
        if (!(joy_stick & 0x80)) {
            a0->x = (int16_t)player_trail[0];
            a0->y = (int16_t)(player_trail[1] + 0x19);
            return;
        }
        SHOT_PHASE(a0) = 1;
    } else if (SHOT_PHASE(a0) > 0) {
        if (!(joy_stick & 0x80)) {
            SHOT_PHASE(a0) = -1;
            return;
        }
        d0 = (int16_t)(((PLAYER->x - player_x_prev) * 4) + a0->x);
        if (d0 < 0)
            d0 = 0;
        if (d0 > 0x13f)
            d0 = 0x13f;
        a0->x = d0;
        d0 = (int16_t)(((PLAYER->y - player_y_prev) * 4) + a0->y);
        if (d0 < 0)
            d0 = 0;
        if (d0 > 0xbf)
            d0 = 0xbf;
        a0->y = d0;
    } else {
        d0 = (int16_t)(player_trail[0] - a0->x);
        if (d0 > 8)
            d0 = 8;
        if (d0 < -8)
            d0 = -8;
        a0->x = (int16_t)(a0->x + d0);
        d0 = (int16_t)(player_trail[1] + 0x19 - a0->y);
        if (d0 > 8)
            d0 = 8;
        if (d0 < -8)
            d0 = -8;
        a0->y = (int16_t)(a0->y + d0);
        if ((int16_t)player_trail[0] == a0->x && (int16_t)(player_trail[1] + 0x19) == a0->y)
            SHOT_PHASE(a0) = 0;
        return;
    }
    bbox_from_frame(a0);
    a1 = collide_enemies(a0);
    if (a1) {
        sfx_cmd = 0x15;
        apply_damage(a0, a1, weapon_damage[1]);
    }
}

/* orig $003F3A: update of weapon $20 (side slot): every 7 frames while fire is held a beam (type $24) goes up from it */
void weapon20_update(Object *a0)
{
    Object *a1;

    anim_step(a0);
    a0->x = (int16_t)(PLAYER->x + a0->v3e);
    a0->y = (int16_t)(PLAYER->y + a0->v40);
    if (respawn_shield) {
        weapon_shield_draw(a0);
        return;
    }
    a0->fn_draw = draw_bob_masked;
    if (V5E(a0) >= 0)
        SET_V5E(a0, V5E(a0) - 1);
    if (fire_press && V5E(a0) < 0) {
        sfx_cmd = 6;
        a1 = alloc_b(a0);
        a1->type = 0x24;
        a1->fn_draw = FN(beam_draw);
        a1->fn_update = FN(beam_update);
        W20_TICK(a1) = -0x20;
        a1->target = a0;
        a1->score = a0->score;
        SET_V5E(a0, 6);
    }
}

#define BEAM_GROWTH(o) ((o)->v28)

/* orig $003FDA: update of the beam of weapon $20: it grows (v28 -$20 to $60), then climbs; hurts what it covers */
void beam_update(Object *a0)
{
    int16_t d0, d1, d2, d3;
    uint16_t d7;

    if (player_dead) {
        kill_object(a0);
        return;
    }
    if (BEAM_GROWTH(a0) != 0x41) {
        Object *a1 = a0->target;

        a0->x = (int16_t)(a1->x - 7);
        a0->y = (int16_t)(a1->y - 0x18);
        BEAM_GROWTH(a0) = (int16_t)(BEAM_GROWTH(a0) + 0x20);
        if (BEAM_GROWTH(a0) != 0x60)
            goto hurt;
        BEAM_GROWTH(a0) = 0x41;
    }
    a0->y = (int16_t)(a0->y - 0x20);
    if (a0->y < 0) {
        kill_object(a0);
        return;
    }
hurt:
    d0 = a0->x;
    d1 = (int16_t)(a0->y - BEAM_GROWTH(a0));
    d2 = a0->score;
    d0 = (int16_t)(d0 + 4);
    d2 = (int16_t)(d2 + d2);
    d0 = (int16_t)(d0 - d2);
    d2 = (int16_t)(d2 + d2);
    d2 = (int16_t)(d2 + 7);
    d2 = (int16_t)(d2 + d0);
    d3 = (int16_t)(d1 + 0x20);
    if (d3 < 0)
        return;
    if (d1 < 0)
        d1 = 0;
    d7 = (uint16_t)((uint16_t)(a0->score + 1) * blast_damage[0]);
    area_damage(a0, d0, d1, d2, d3, d7);
}

/* orig $004094: update of weapon $34 (side slot, the pod): with the first picture ($17336) the pod makes its little trail object
 * (type $30); with the picture $171D6 and fire held it plays the sound $80 and changes to the animation $27F2 */
void weapon34_update(Object *a0)
{
    Object *a1;

    anim_step(a0);
    a0->x = (int16_t)(PLAYER->x + a0->v3e);
    a0->y = (int16_t)(PLAYER->y + a0->v40);
    if (respawn_shield) {
        weapon_shield_draw(a0);
        return;
    }
    a0->fn_draw = draw_bob_masked;
    if (a0->gfx == (const void *)pic_17336) {
        a1 = alloc_b(a0);
        a1->type = 0x30;
        a1->fn_draw = draw_bob_masked;
        a1->fn_update = FN(pod_trail_update);
        set_anim(a1, anim_288e);
        a1->x = a0->x;
        a1->y = (int16_t)(a0->y - 0xb);
        a1->v28 = 0;
        return;
    }
    if (fire_press && a0->gfx == (const void *)pic_171d6) {
        sfx_cmd = 0x80;
        set_anim(a0, anim_27f2);
    }
}

/* orig $0045E4: update of the pod's shot (type $30): flies up and hurts the first enemy it meets */
void pod_trail_update(Object *a0)
{
    Object *a1;

    if (player_dead) {
        kill_object(a0);
        return;
    }
    anim_step(a0);
    a0->y = (int16_t)(a0->y - 0xa);
    if (a0->y < 0) {
        kill_object(a0);
        return;
    }
    bbox_from_frame(a0);
    a1 = collide_enemies(a0);
    if (a1)
        apply_damage(a0, a1, weapon_damage[2]);
}

/* orig $00415A: update of weapon $90 (side slot, twin gun): with the picture $1838C it fires two shots (type $10); with $182DC
 * and fire held it changes its delay */
void weapon90_update(Object *a0)
{
    int16_t d6, d7;

    anim_step(a0);
    a0->x = (int16_t)(PLAYER->x + a0->v3e);
    a0->y = (int16_t)(PLAYER->y + a0->v40);
    if (respawn_shield) {
        weapon_shield_draw(a0);
        return;
    }
    a0->fn_draw = draw_bob_masked;
    if (a0->gfx == (const void *)pic_1838c) {
        d6 = -3;
        for (d7 = 1; d7 >= 0; d7--) {
            Object *a1 = alloc_b(a0);

            a1->v2a = 0;
            a1->v2e = -9;
            d6 = (int16_t)(d6 + a0->x);
            a1->x = d6;
            a1->y = (int16_t)(a0->y - 0x10);
            a1->type = 0x10;
            a1->fn_update = FN(shot_update);
            a1->fn_draw = draw_bob_masked;
            set_anim(a1, anim_2918);
            a1->score = a0->score;
            d6 = 5;
        }
        return;
    }
    if (fire_press && a0->gfx == (const void *)pic_182dc)
        a0->anim_delay = 1;
}

#define SHELL_PHASE(o) ((o)->v28)

/* orig $0042A6: update of the shell of weapon $28 (type $28): flies up while v28 < 0, waits, then explodes after 3 frames and hurts
 * what is near */
void shell_update(Object *a0)
{
    if (player_dead) {
        kill_object(a0);
        return;
    }
    anim_step(a0);
    if (SHELL_PHASE(a0) < 0) {
        SHELL_PHASE(a0)++;
        a0->y = (int16_t)(a0->y - 6);
        return;
    }
    if (SHELL_PHASE(a0) == 0) {
        SHELL_PHASE(a0) = 3;
        return;
    }
    SHELL_PHASE(a0)--;
    if (SHELL_PHASE(a0) != 0)
        return;
    sfx_cmd = 0x82;
    debris_burst(a0, 3, (uint16_t)(a0->x - 0x14), (uint16_t)(a0->y - 0x14), 0x28, 0x28, anim_273a);
    kill_object(a0);
    area_damage(a0, (int16_t)(a0->x - 0x20), (int16_t)(a0->y - 0x20), (int16_t)(a0->x - 0x20 + 0x3f), (int16_t)(a0->y - 0x20 + 0x3f), blast_damage[1]);
}


/* orig $004340: update of weapon $38 and $3C (slot 5): the shell follows the ship until fire is held (v28 = 1), flies up to the top
 * (v28 = 2), is steered by the stick, leaves a trail of copies of itself (v28 negative, they explode when fire is let go), comes back
 * (v28 = 3) when fire is let go */
void weapon38_update(Object *a0)
{
    int16_t d0;

    anim_step(a0);
    if (SHOT_PHASE(a0) < 0) {
        int16_t d1, d2;
        uint16_t w, h;
        const uint8_t *a2;

        if (player_dead) {
            kill_object(a0);
            return;
        }
        if (joy_stick & 0x80)
            return;
        SHOT_PHASE(a0)++;
        if (SHOT_PHASE(a0) != 0)
            return;
        d1 = (int16_t)(a0->x - 8);
        d2 = (int16_t)(a0->y - 8);
        w = h = 0x10;
        a2 = anim_270e;
        if (a0->score != 0) {
            d1 = (int16_t)(d1 - 0xa);
            d2 = (int16_t)(d2 - 0xa);
            w = h = 0x14;
            a2 = anim_273a;
        }
        debris_burst(a0, 2, (uint16_t)d1, (uint16_t)d2, w, h, a2);
        if (a0->score == 0) {
            d1 = (int16_t)(a0->x - 0x10);
            d2 = (int16_t)(a0->y - 0x10);
            area_damage(a0, d1, d2, (int16_t)(d1 + 0x20), (int16_t)(d2 + 0x20), (uint16_t)(blast_damage[2]));
        } else {
            d1 = (int16_t)(a0->x - 0x18);
            d2 = (int16_t)(a0->y - 0x18);
            area_damage(a0, d1, d2, (int16_t)(d1 + 0x2f), (int16_t)(d2 + 0x2f), (uint16_t)(a0->score * 2 + blast_damage[2]));
        }
        kill_object(a0);
        return;
    }
    if (respawn_shield) {
        weapon_shield_draw(a0);
        return;
    }
    a0->fn_draw = draw_bob_masked;
    if (SHOT_PHASE(a0) == 0) {
        a0->x = PLAYER->x;
        a0->y = (int16_t)(PLAYER->y + 0x19);
        if (joy_stick & 0x80)
            SHOT_PHASE(a0) = 1;
        return;
    }
    if (SHOT_PHASE(a0) == 1) {
        if (joy_stick & 0x80) {
            if (a0->y > 0x10)
                a0->y = (int16_t)(a0->y - 0xc);
            else
                SHOT_PHASE(a0) = 2;
        } else {
            SHOT_PHASE(a0) = 3;
        }
        return;
    }
    if (SHOT_PHASE(a0) == 2) {
        if (!(joy_stick & 0x80)) {
            SHOT_PHASE(a0) = 3;
            return;
        }
        d0 = a0->x;
        if (joy_stick & 4) {
            d0 = (int16_t)(d0 - 8);
            if (d0 < 0)
                d0 = 0;
        }
        if (joy_stick & 8) {
            d0 = (int16_t)(d0 + 8);
            if (d0 > 0x13f)
                d0 = 0x13f;
        }
        a0->x = d0;
        d0 = a0->y;
        if (joy_stick & 1) {
            d0 = (int16_t)(d0 - 8);
            if (d0 < 0)
                d0 = 0;
        }
        if (joy_stick & 2) {
            d0 = (int16_t)(d0 + 8);
            if (d0 > 0xbf)
                d0 = 0xbf;
        }
        a0->y = d0;
        if (trail_count == 10)
            return;
        d0 = (int16_t)(trail_x - a0->x);
        if (d0 < 0)
            d0 = (int16_t)-d0;
        if (d0 < 0x20) {
            d0 = (int16_t)(trail_y - a0->y);
            if (d0 < 0)
                d0 = (int16_t)-d0;
            if (d0 < 0x20)
                return;
        }
        trail_x = a0->x;
        trail_y = a0->y;
        {
            Object *a1 = alloc_b(a0);

            a1->type = a0->type;
            a1->fn_update = a0->fn_update;
            a1->fn_draw = draw_bob_masked;
            set_anim(a1, a0->anim - 6);
            a1->x = a0->x;
            a1->y = a0->y;
            a1->score = a0->score;
            trail_count++;
            SHOT_PHASE(a1) = (int16_t)(-(int16_t)(trail_count * 4));
        }
        return;
    }
    d0 = (int16_t)(PLAYER->x - a0->x);
    if (d0 > 8)
        d0 = 8;
    if (d0 < -8)
        d0 = -8;
    a0->x = (int16_t)(a0->x + d0);
    d0 = (int16_t)(PLAYER->y + 0x19 - a0->y);
    if (d0 > 8)
        d0 = 8;
    if (d0 < -8)
        d0 = -8;
    a0->y = (int16_t)(a0->y + d0);
    if (PLAYER->x == a0->x && (int16_t)(PLAYER->y + 0x19) == a0->y)
        SHOT_PHASE(a0) = 0;
}

/* orig $0064A0: draw routine of the beam of weapon $20, by CPU: a slice 32 pixels wide of the level's pattern (mask and four planes,
 * shifted by x & 15) for each of the length + 1 lines going up from y, and the head pictures with the blitter-less bob routine; the
 * left and right edges are clipped to one word */
void beam_draw(Object *a0)
{
    uint16_t level = (uint16_t)a0->score;
    int16_t d0 = (int16_t)(a0->x + 4 - level - level);
    int16_t d1 = a0->y;
    uint32_t r[5];
    uint8_t *dst;
    uint16_t cnt;
    int lt, i;

    beam_state[0] = (uint16_t)d0;
    beam_state[1] = (uint16_t)d1;
    beam_state[3] = level;
    beam_state[2] = (uint16_t)a0->v28;
    if (d0 <= -16 || d0 >= 0x140)
        return;
    if ((int16_t)beam_state[2] >= 0x41) {
        draw_bob_masked_at((uint16_t)d0, (uint16_t)(d1 + 1), beam_heads[level]);
        d0 = (int16_t)beam_state[0];
        d1 = (int16_t)beam_state[1];
    }
    d1 = (int16_t)(d1 - (int16_t)beam_state[2]);
    draw_bob_masked_at((uint16_t)d0, (uint16_t)d1, beam_bodies[beam_state[3]]);
    d0 = (int16_t)beam_state[0];
    d1 = (int16_t)beam_state[1];
    if (d1 < 0)
        return;
    dst = draw_screen + (int16_t)row_offset[(uint16_t)d1];
    {
        const uint32_t *p = &beam_pattern[beam_state[3] * 5];
        uint16_t sh = (uint16_t)d0 & 0xf;

        for (i = 0; i < 5; i++)
            r[i] = sh ? (p[i] >> sh) | (p[i] << (32 - sh)) : p[i];
    }
    dst += (int16_t)((uint16_t)((int16_t)d0 >> 3) & 0xfffe);
    cnt = beam_state[2];
    lt = 0;
    if ((int16_t)beam_state[0] < 0) {
        dst += 2;
        while (!lt) {
            if (--cnt == 0xffff)
                break;
            for (i = 0; i < 4; i++) {
                uint8_t *w = dst + 0x28 * (i);
                wr16(w, (uint16_t)((rd16(w) & (uint16_t)r[0]) | (uint16_t)r[1 + i]));
            }
            dst -= 0xa0;
            lt = (int32_t)ADDR(dst) < (int32_t)ADDR(draw_screen);
        }
    } else if ((int16_t)beam_state[0] > 0x130) {
        while (!lt) {
            if (--cnt == 0xffff)
                break;
            for (i = 0; i < 4; i++) {
                uint8_t *w = dst + 0x28 * (i);
                wr16(w, (uint16_t)((rd16(w) & (uint16_t)(r[0] >> 16)) | (uint16_t)(r[1 + i] >> 16)));
            }
            dst -= 0xa0;
            lt = (int32_t)ADDR(dst) < (int32_t)ADDR(draw_screen);
        }
    } else {
        while (!lt) {
            if (--cnt == 0xffff)
                break;
            for (i = 0; i < 4; i++) {
                uint8_t *w = dst + 0x28 * (i);
                wr16(w, (uint16_t)((rd16(w) & (uint16_t)(r[0] >> 16)) | (uint16_t)(r[1 + i] >> 16)));
                wr16(w + 2, (uint16_t)((rd16(w + 2) & (uint16_t)r[0]) | (uint16_t)r[1 + i]));
            }
            dst -= 0xa0;
            lt = (int32_t)ADDR(dst) < (int32_t)ADDR(draw_screen);
        }
    }
}
