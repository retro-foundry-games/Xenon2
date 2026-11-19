/* pkg_d: the weapons, pickups and the player of $4000-$5FFF, and level 1's own code (the blob at $54E00). */
#include "pkg_d.h"
#include "amiga/hw.h"

#define FN(f) ((obj_fn)(f))

/* the pattern `move.w #4,(a); move.l #$fee,2(a); move.l #$3130,6(a); move.l #$3130,$a(a); sf.b $34(a); sf.b $60(a)` */
static void kill_object(Object *o)
{
    o->type = 4;
    o->fn_update = fn_000fee;
    o->fn_draw = fn_003130;
    o->fn_hit = (void *)fn_003130;
    o->flag34 = 0;
    o->flag60 = 0;
}

/* inlined: $2418 and the insertion before the sentinel of list A / B (the object comes back in a0) */
static Object *alloc_a(Object *a0)
{
    Object *o = fn_002418(a0), *tail = LIST_A->prev;

    LIST_A->prev = o;
    o->prev = tail;
    o->next = LIST_A;
    tail->next = o;
    return o;
}

static Object *alloc_b(Object *a0)
{
    Object *o = fn_002418(a0), *tail = LIST_B->prev;

    LIST_B->prev = o;
    o->prev = tail;
    o->next = LIST_B;
    tail->next = o;
    return o;
}

/* list B from the other end: the object goes first (baa), links swapped */
static Object *alloc_b_front(Object *a0)
{
    Object *o = fn_002418(a0), *head = LIST_B->next;

    LIST_B->next = o;
    o->next = head;
    o->prev = LIST_B;
    head->prev = o;
    return o;
}

/* move.l (a2)+,$16(a); move.w (a2)+,$1a(a); move.l a2,$1c(a) */
static void set_anim(Object *o, const uint16_t *a)
{
    o->gfx = (const void *)(((uint32_t)a[0] << 16) | a[1]);
    o->anim_delay = a[2];
    o->anim = a + 3;
}

static void set_ptr_2a(Object *o, Object *p)
{
    uint32_t v = (uint32_t)(uintptr_t)p;

    o->v2a = (int16_t)(v >> 16);
    o->v2c = (int16_t)v;
}

static Object *get_ptr_2a(const Object *o)
{
    return (Object *)(uintptr_t)(((uint32_t)(uint16_t)o->v2a << 16) | (uint16_t)o->v2c);
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

/* orig $005920: the same with the greatest damage */
void apply_damage_max(Object *a0, Object *a1)
{
    apply_damage(a0, a1, 0x7f);
}

/* orig $005550: 1 when the object's box meets the player's (the flag the caller tests; inlined: $200A) */
int player_hits(Object *a0)
{
    const Object *p = PLAYER;

    if (v_0430)
        return 0;
    return !(p->hit[2] < a0->hit[0] || p->hit[0] > a0->hit[2] || p->hit[3] < a0->hit[1] || p->hit[1] > a0->hit[3]);
}

/* orig $00556C: 1 when the point (d0, d1) is in the player's box */
int point_in_player_box(int16_t d0, int16_t d1)
{
    const Object *p = PLAYER;

    if (v_0430)
        return 0;
    return !(p->hit[0] > d0 || p->hit[2] < d0 || p->hit[1] > d1 || p->hit[3] < d1);
}

/* orig $0057B6: an object that only animates */
void animate_only(Object *a0)
{
    fn_002c5a(a0);
}

/* orig $005908: an object that follows its target */
void follow_target(Object *a0)
{
    fn_002c5a(a0);
    a0->x = a0->target->x;
    a0->y = a0->target->y;
    fn_0034ea(a0);
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

/* orig $005C3E: a1 and what belongs to it die */
void kill_with_slot(Object *a1)
{
    Object *a2;

    if (a1->type == 0x34) {
        a2 = find_decoration(a1);
        if (a2->type != 0)
            kill_object(a2);
    }
    ((Slot *)a1->fn_hit)->type = 0;
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
    ((Slot *)a1->fn_hit)->type = 0;
    unlink_to_free(a1);
}

/* orig $005D8C: scrolls the objects of a list (their y by d7) and frees those that are not fixed */
void free_chain(Object *a1, int16_t d7)
{
    while (a1->type != 0) {
        Object *next = a1->prev;

        if (a1->flag60 == 0)
            unlink_to_free(a1);
        else if (a1->flag61 == 0)
            a1->y -= d7;
        a1 = next;
    }
}

/* the slot's object grows: orig $004B94 (slot 0), $004D14 (slot 6), $005000 (slot 5) */
static Object *grow_weapon(Slot *s)
{
    Object *a1 = s->obj;

    if (a1->energy > a1->v42)
        a1->v42++;
    return a1;
}

/* orig $004F36: the weapon of slot 5 is dismissed */
static void dismiss_slot5(void)
{
    if (SLOT(5)->type != 0)
        kill_object(SLOT(5)->obj);
}

/* orig $005540: the basic weapon comes back when slot 0 is empty */
static Object *refill_slot0(Object *a0, Object *a1)
{
    if (v_0c66[0] == 0)
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
    n->fn_draw = fn_003130;
    n->v42 = 0;
    n->energy = 2;
    n->v44 = 1;
    n->v5e = -1;
    v_0c62 = 8;
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
    n->fn_update = fn_003b4a;
    n->fn_draw = fn_00128e;
    n->fn_hit = s;
    n->gfx = v_192ba;
    n->anim_delay = 0;
    n->x = PLAYER->x;
    n->y = PLAYER->y;
    n->v42 = 0;
    n->energy = 2;
    n->v44 = 0xe;
    n->v5e = -1;
    v_0c62 = 8;
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
    n->fn_draw = fn_003130;
    n->fn_hit = s6;
    n->v42 = 0;
    n->energy = 2;
    n->v44 = 0xa;
    n->v5e = -1;
    v_0c62 = 8;
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
    n->fn_draw = fn_003130;
    n->fn_hit = s6;
    n->v42 = 0;
    n->energy = 0;
    n->v44 = 0x15;
    n->v5e = -1;
    v_0c62 = 0xc;
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
    n->fn_draw = fn_003130;
    n->fn_hit = a2;
    n->v42 = 0;
    n->energy = 0;
    n->v44 = 0x17;
    n->v5e = -1;
    v_0c62 = 0x1e;
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
    n->fn_update = fn_003b94;
    n->fn_draw = fn_00128e;
    n->fn_hit = s;
    set_anim(n, anim_2540);
    n->x = PLAYER->x;
    n->y = PLAYER->y;
    n->v42 = 0;
    n->energy = 2;
    n->v44 = 0x14;
    n->v5e = -1;
    v_0c62 = 8;
    return n;
}

/* the weapons of slot 5 share their set-up: type, routine, animation, energy, kind, y offset 0x19 (or none) */
static Object *weapon_slot5(Object *a0, uint16_t type, obj_fn update, const uint16_t *anim, int16_t energy, int16_t v42, int16_t kind, int dy)
{
    Slot *s5 = SLOT(5);
    Object *n;

    n = alloc_a(a0);
    s5->type = type;
    s5->obj = n;
    n->type = type;
    n->fn_update = update;
    n->fn_draw = fn_00128e;
    n->fn_hit = s5;
    set_anim(n, anim);
    n->x = PLAYER->x;
    n->y = PLAYER->y + dy;
    n->v28 = 0;
    n->v42 = v42;
    n->energy = energy;
    n->v44 = kind;
    n->v5e = -1;
    return n;
}

/* orig $004F66: weapon $2C (slot 5) */
Object *weapon_2c(Object *a0, Object *a1)
{
    if (SLOT(5)->type == 0x2c)
        return grow_weapon(SLOT(5));
    dismiss_slot5();
    return weapon_slot5(a0, 0x2c, fn_003cdc, anim_25be, 2, 0, 0x13, 0x19);
}

/* orig $005014: weapon $4C (slot 5) */
Object *weapon_4c(Object *a0, Object *a1)
{
    if (SLOT(5)->type == 0x4c)
        return grow_weapon(SLOT(5));
    dismiss_slot5();
    return weapon_slot5(a0, 0x4c, fn_003e2c, anim_2a02, 0, 0, 0xb, 0x19);
}

/* orig $0050AC: weapon $38 (slot 5) */
Object *weapon_38(Object *a0, Object *a1)
{
    Object *n;

    if (SLOT(5)->type == 0x38 || SLOT(5)->type == 0x3c)
        return grow_weapon(SLOT(5));
    dismiss_slot5();
    n = weapon_slot5(a0, 0x38, fn_004340, anim_28b2, 1, 0, 9, 0x19);
    v_04e8 = -100;
    return n;
}

/* orig $005158: weapon $3C (slot 5) */
Object *weapon_3c(Object *a0, Object *a1)
{
    Object *n;

    if (SLOT(5)->type == 0x38 || SLOT(5)->type == 0x3c)
        return grow_weapon(SLOT(5));
    dismiss_slot5();
    n = weapon_slot5(a0, 0x3c, fn_004340, anim_28d0, 1, 1, 0xd, 0x19);
    v_04e8 = -100;
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
static Object *weapon_side(Object *a0, uint16_t type, obj_fn update, const uint16_t *anim, int16_t energy, int16_t kind, int clear28)
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
    n->fn_draw = fn_00128e;
    n->fn_hit = a2;
    n->v3e = v_2404[2 * d0];
    n->v40 = v_2404[2 * d0 + 1];
    set_anim(n, anim);
    n->x = PLAYER->x + n->v3e;
    n->y = PLAYER->y + n->v40;
    if (clear28)
        n->v28 = 0;
    n->v42 = 0;
    n->energy = energy;
    n->v44 = kind;
    n->v5e = -1;
    return n;
}

/* orig $0052E0 */
Object *weapon_90(Object *a0, Object *a1)
{
    Object *n = weapon_side(a0, 0x90, fn_00415a, anim_2c24, 0, 0x11, 1);

    return refill_slot0(a0, n ? n : a1);
}

/* orig $00538C */
Object *weapon_20(Object *a0, Object *a1)
{
    Object *n = weapon_side(a0, 0x20, fn_003f3a, anim_2612, 2, 0x12, 1);

    return refill_slot0(a0, n ? n : a1);
}

/* orig $00543A */
Object *weapon_34(Object *a0, Object *a1)
{
    Object *n = weapon_side(a0, 0x34, fn_004094, anim_280a, 0, 0xf, 0), *d;

    if (n == 0)
        return refill_slot0(a0, a1);
    if (n->v3e != 0) {
        d = alloc_b_front(a0);
        d->type = 0x40;
        d->fn_update = fn_003c7a;
        d->fn_draw = fn_00128e;
        d->flag60 = -1;
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
    if (v_0430)
        return;
    if (v_0a6a)
        fn_00615a(a0, 0, -9, 0, -6, &v_27c2[3]);
}

/* orig $0055B2: update of weapon $98 */
void weapon98_update(Object *a0)
{
    if (v_0430)
        return;
    if (v_0a6a) {
        fn_00615a(a0, -9, 0, -2, -3, &v_27c2[0]);
        fn_00615a(a0, 9, 0, 2, -3, &v_27c2[6]);
    }
}

/* orig $0055E2: update of weapon $44 */
void weapon44_update(Object *a0)
{
    fn_002c5a(a0);
    a0->x = PLAYER->x;
    a0->y = PLAYER->y;
    if (v_0e06) {
        fn_006912(a0);
        return;
    }
    a0->fn_draw = fn_00128e;
    if (v_0a6a) {
        a0->anim_delay = 1;
        fn_00615a(a0, 0, 9, 0, 0x12, &v_27c2[9]);
    }
}

/* orig $005624: update of weapon $9c (slot 6, type $28) */
void weapon9c_update(Object *a0)
{
    Object *a1, *n;
    int16_t d0;

    if (v_0430)
        return;
    if (a0->v5e >= 0)
        a0->v5e--;
    if (v_0a6a == 0 || a0->v5e >= 0)
        return;
    for (a1 = LIST_B->prev; a1->type != 0; a1 = a1->prev)
        if (a1->type == 0x28)
            return;
    a0->v5e = 0xf;
    n = alloc_b(a0);
    n->type = 0x28;
    n->fn_update = fn_0042a6;
    n->fn_draw = fn_00128e;
    n->flag60 = -1;
    set_anim(n, anim_25e2);
    n->x = PLAYER->x;
    d0 = PLAYER->y - 0xc;
    n->y = d0;
    n->v28 = -(int16_t)(((uint16_t)d0 >> 4) + 4);
    n->v5e = 6;
}

/* the eight pictures of the drill, by the loop counter */
static const uint16_t *const drill_anim[8] = { anim_2912, anim_2930, anim_294e, anim_296c, anim_298a, anim_29a8, anim_29c6, anim_29e4 };

/* orig $0056D6: update of weapon $a0 (slot 5 or 6, type $48) */
void weaponA0_update(Object *a0)
{
    Object *a1, *cur;
    int16_t cnt;

    if (v_0430)
        return;
    if (a0->v5e >= 0)
        a0->v5e--;
    if (v_0a6a == 0 || a0->v5e >= 0)
        return;
    for (a1 = LIST_B->prev; a1->type != 0; a1 = a1->prev)
        if (a1->type == 0x48)
            return;
    a0->v5e = 0x12;
    v_0414 = 9;
    cur = a0;
    for (cnt = 7; cnt >= 0; cnt -= 2) {
        cur = alloc_b(cur);
        cur->type = 0x48;
        cur->fn_update = fn_0039a2;
        cur->fn_draw = fn_00128e;
        cur->flag60 = -1;
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

    if (v_0d84) {
        shot_gone(a0);
        return;
    }
    fn_002c5a(a0);
    if (a0->type == 4)
        return;
    d0 = (int32_t)(((uint32_t)(uint16_t)a0->v2a << 16) | (uint16_t)a0->v2c) >> 6;
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
    fn_0034ea(a0);
    a1 = fn_003052(a0);
    if (a1 == 0)
        return;
    d = (uint16_t)(a0->v42 + v_7970[1]);
    shot_gone(a0);
    apply_damage(a0, a1, d);
}

/* orig $005850: update of a shot ($10) that moves by ($2A, $2E) */
void shot_update(Object *a0)
{
    Object *a1;
    uint16_t d;

    if (v_0d84) {
        shot_gone(a0);
        return;
    }
    fn_002c5a(a0);
    a0->x += a0->v2a;
    if ((uint16_t)a0->x >= 0x138) {
        shot_gone(a0);
        return;
    }
    a0->y += a0->v2e;
    if ((uint16_t)a0->y >= 0xc0) {
        shot_gone(a0);
        return;
    }
    a1 = fn_0030de(a0->x, a0->y);
    if (a1 == 0)
        return;
    d = (uint16_t)(a0->v42 + v_7970[0]);
    shot_gone(a0);
    apply_damage(a0, a1, d);
}

/* the centre of o's picture (the 16-bit arithmetic of the original) for an explosion */
static void centre(const Object *o, int16_t *x, int16_t *y)
{
    const int16_t *a5 = o->gfx;
    int16_t dx = (int16_t)(o->x - a5[0]), dy = (int16_t)(o->y - a5[1]);

    dx = (int16_t)(dx + dx);
    dy = (int16_t)(dy + dy);
    *x = (int16_t)(dx + a5[2]) >> 1;
    *y = (int16_t)(dy + a5[3]) >> 1;
}

/* orig $00592A: hit routine of the parts of a big enemy (a1 = a part; its chain is $56/$5A) */
void hit_part(Object *a0, Object *a1, uint16_t d0)
{
    Object *a2, *next;
    uint16_t old;

    a1 = a1->target;
    for (a2 = a1; ; a2 = a2->chain) {
        a2->fn_draw = fn_001368;
        if (a2->chain == 0)
            break;
    }
    old = (uint16_t)a1->energy;
    a1->energy = (int16_t)(old - d0);
    if (old > d0)
        return;
    for (next = a1->chain; next != 0; next = a2->chain) {
        a2 = next;
        if (a2->flag34 >= 0) {
            int16_t x, y;

            centre(a2, &x, &y);
            fn_0031ec(x, y, a1->flag35 ? anim_273a : anim_270e, fn_00128e);
        }
        kill_object(a2);
    }
    enemy_killed(a0, a1);
}

/* orig $0059C8: hit routine that only flashes the object */
void hit_flash(Object *a0, Object *a1, uint16_t d0)
{
    a1->fn_draw = fn_001368;
}

/* orig $0059D2: flashes a1 and kills a0 (the shot) */
void hit_flash_kill_shooter(Object *a0, Object *a1, uint16_t d0)
{
    a1->fn_draw = fn_001368;
    if (a0->type != 0 && a0->type != 8)
        kill_object(a0);
}

/* orig $005A0A: hit routine of an ordinary enemy */
void hit_enemy(Object *a0, Object *a1, uint16_t d0)
{
    uint16_t old;

    a1->fn_draw = fn_001368;
    old = (uint16_t)a1->energy;
    a1->energy = (int16_t)(old - d0);
    if (old > d0)
        return;
    enemy_killed(a0, a1);
}

static void explosion_at(Object *a1, const uint16_t *anim)
{
    int16_t x, y;

    centre(a1, &x, &y);
    fn_0031ec(x, y, anim, fn_00128e);
}

/* orig $005A1A: an enemy a1 dies: explosion, maybe a bonus, its score */
void enemy_killed(Object *a0, Object *a1)
{
    uint16_t *e = (uint16_t *)v_043c;
    Object *n;
    const uint16_t *a2;
    int i, found = 0;
    int16_t d0;

    explosion_at(a1, a1->flag35 ? anim_273a : anim_270e);
    d0 = a1->v44;
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
            if (a1->flag35) {
                a2 = anim_264e;
                n->type = 0x18;
            }
            n->v28 = 7;
            n->fn_update = FN(pickup_update);
            n->fn_draw = fn_00128e;
            set_anim(n, a2);
            n->x = a1->x;
            n->y = a1->y;
            n->v2a = (int16_t)random_next();
        }
    }
    v_0c92 += (uint16_t)a1->v42;
    kill_object(a1);
}

/* orig $005B16: hit routine: the object explodes and dies at once */
void hit_explode(Object *a0, Object *a1, uint16_t d0)
{
    explosion_at(a1, a1->flag35 ? anim_273a : anim_270e);
    kill_object(a1);
}

/* orig $005BFC: draw routine of the tile object */
void tile_draw(Object *a0)
{
    a0->fn_draw = fn_003130;
    fn_001bd0(a0->v3e, a0->v40, a0->y - v_0cd0, (uint16_t)a0->x >> 1, a0->gfx);
}

/* orig $005B72: hit routine of a destructible tile block: when it breaks the background is restored */
void hit_tile(Object *a0, Object *a1, uint16_t d0)
{
    uint16_t old;
    int16_t x, y, rows, cols;
    uint16_t *dst;
    const uint16_t *src;

    a1->fn_draw = FN(tile_draw);
    old = (uint16_t)a1->energy;
    a1->energy = (int16_t)(old - d0);
    if (old > d0)
        return;
    v_0c92 += (uint16_t)a1->v42;
    x = (int16_t)(((a1->v3e + 1) << 3) + a1->x);
    y = (int16_t)(((a1->v40 + 1) << 3) + a1->y - v_0cd0);
    fn_0031ec(x, y, anim_273a, fn_00128e);
    dst = a1->v46;
    src = a1->anim;
    for (rows = a1->v40; rows >= 0; rows--) {
        uint16_t *row = dst;

        for (cols = a1->v3e; cols >= 0; cols--)
            *row++ = *src++;
        dst += 20;
    }
    kill_object(a1);
}

/* orig $004894 (+$14) and $0048B4 (+$28): the shield grows (at most $27) */
static void shield_up(uint16_t d)
{
    v_0cc6 += d;
    v_0414 = 7;
    if ((int16_t)v_0cc6 > 0x27)
        v_0cc6 = 0x27;
    v_0a6b = 0xff;
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
            int16_t d2 = s->obj->v42;

            if (d2 != s->obj->energy && d2 < d0) {
                d0 = d2;
                a2 = s;
            }
        }
    }
    if (d0 != 0x7f) {
        a1 = a2->obj;
        a1->v42++;
    }
}

/* orig $0048F6 */
void pickup_speed(Object *a0)
{
    if ((int16_t)v_0c64 < 4)
        v_0c64++;
}

/* orig $004906 */
void pickup_move(Object *a0)
{
    if (v_0c9e != 2)
        v_0c9e++;
}

/* orig $004914 */
void pickup_flag_d83(Object *a0)
{
    v_0d83 = 0xff;
}

/* orig $00491A */
void pickup_ca0(Object *a0)
{
    v_0ca0 += 3;
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
    v_0414 = 2;
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
        a2 = fn_003286(a1);
        if (d0 != 0x50 && d0 != 0x54)
            apply_damage((Object *)&obj_list[1], a1, 0x7f);
    }
    for (a1 = LIST_B->prev; a1->type != 0; ) {
        Object *next = a1->prev;

        if (a1->type == 0x14) {
            a1->type = 4;
            a1->fn_update = fn_000fee;
            a1->fn_draw = fn_003130;
            a1->fn_hit = (void *)fn_003130;
            a1->flag34 = 0;
            a1->flag60 = 0;
        }
        a1 = next;
    }
    for (i = 0; i < 8; i++)
        v_043c[i] = 0;
}

/* orig $004A94: a pickup that makes a spinning object */
void pickup_spinner(Object *a0)
{
    Object *a1;

    if (v_0cc8 == 0) {
        a1 = alloc_b(a0);
        a1->type = 0x5c;
        a1->fn_update = fn_003868;
        a1->fn_draw = fn_00128e;
        a1->x = PLAYER->x;
        a1->y = PLAYER->y;
        set_anim(a1, anim_25a0);
    }
    v_0cc8 += 0xaa;
}

/* orig $004A0A: the weapons are set up for a new life/level */
void init_weapons(Object *a0)
{
    Object *a1 = 0;
    int i;

    v_0cf4 = LIST_A->prev;
    v_0cf8 = LIST_A->next;
    for (i = 0; i < 21; i++) {
        v_0cfc[i] = v_0c66[i];
        v_0c66[i] = 0;
    }
    LIST_A->type = 0;
    LIST_A->prev = LIST_A;
    LIST_A->next = LIST_A;
    LIST_A->fn_update = fn_003130;
    LIST_A->fn_draw = fn_003130;
    LIST_A->fn_hit = (void *)fn_003130;
    a1 = weapon_94(a0, a1);
    a1 = weapon_34(a0, a1);
    a1 = weapon_34(a0, a1);
    a1 = weapon_20(a0, a1);
    a1->v42 = 2;
    a1 = weapon_20(a0, a1);
    a1->v42 = 2;
    a1 = weapon_2c(a0, a1);
    a1->v42 = 2;
    a1 = weapon_98(a0, a1);
    a1->v42 = 2;
    a1 = weapon_c0(a0, a1);
    a1->v42 = 2;
}

/* the pickup routines, by $30(object): the table at $47D6 (the first byte of an entry says the sound: 4, else $81) */
static void pickup_call(int idx, Object *a0)
{
    static const uint8_t loud[19] = { 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0 };

    v_0414 = loud[idx] ? 4 : 0x81;
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

/* orig $004634: update of a pickup (type $18 or $60: a bonus; else a weapon pickup, $30 says which) */
void pickup_update(Object *a0)
{
    int16_t d0, d1;
    uint16_t d7;
    int idx;

    fn_002c5a(a0);
    if (a0->v28 == 0) {
        a0->y += 8;
        if (a0->y >= 0xc8) {
            if (v_0ca4 != 0) {
                v_0ca4--;
                if (v_0ca4 == 0)
                    v_0a6c = 0xff;
            }
            kill_object(a0);
            return;
        }
    } else {
        if (a0->v28 > 0) {
            a0->v28 &= 7;
            if (a0->v28 == 0)
                a0->v2a = (int16_t)fn_003132((int16_t)(0xa0 - a0->x), (int16_t)(0x64 - a0->y));
            a0->v28++;
            d0 = (int16_t)(0xa0 - a0->x);
            d1 = (int16_t)(0x64 - a0->y);
            if ((int32_t)d0 * d0 + (int32_t)d1 * d1 < 0x190)
                a0->v28 = -34;
        } else {
            a0->v28++;
            if ((a0->v28 & 3) == 0) {
                a0->v2a++;
                a0->v2a &= 7;
            }
        }
        idx = (a0->v2a << 2) & 0x1c;
        a0->x += v_476c[idx >> 1];
        a0->y += v_476c[(idx >> 1) + 1];
    }
    fn_0034ea(a0);
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
    v_0c9a += d7;
    v_0414 = 5;
    if (v_0ca4 != 0) {
        v_0ca4--;
        if (v_0ca4 == 0)
            v_0a6c = 0xff;
    }
    kill_object(a0);
}

/* orig $005D00: the player dies */
void player_dies(Object *a0)
{
    Object *a1;
    const uint16_t *a2 = anim_2760;

    if (v_0ca4 != 0)
        return;
    v_0d84 = 0xff;
    v_0cc6 = 0;
    v_0a6b = 0xff;
    v_0414 = 10;
    v_0418 = 10;
    v_0430 = 1;
    v_0434 = 0;
    PLAYER->gfx = (const void *)(((uint32_t)a2[0] << 16) | a2[1]);
    PLAYER->anim_delay = a2[2];
    PLAYER->anim = a2 + 3;
    PLAYER->fn_draw = fn_00128e;
    a1 = LIST_A->prev;
    if (a1->type == 0)
        return;
    if (a1->type == 0x94 && a1->v42 == 0) {
        a1 = a1->prev;
        if (a1->type == 0)
            return;
    }
    kill_with_slot(a1);
    refill_slot0(a0, a1);
}

/* orig $005D6E: the player takes d0 damage */
void player_damage(Object *a0, uint16_t d0)
{
    uint16_t old;

    if (v_0ca4 != 0)
        return;
    if (v_0d83 != 0)
        d0 >>= 1;
    v_0a6b = 0xff;
    old = v_0cc6;
    v_0cc6 = (uint16_t)(old - d0);
    if (old > d0)
        return;
    player_dies(a0);
}

/* orig $005DE6: the player's steering: v_0cde (the sideways speed), the stick in v_0a00 */
void player_move(Object *a0)
{
    int16_t d0;

    v_04e4 = a0->x;
    v_04e6 = a0->y;
    v_5dd0 = v_0cde;
    if (v_0a00 & 4) {
        if (v_0cde > 0)
            v_0cde = 0;
        if (v_0cde > -6)
            v_0cde--;
    } else if (v_0a00 & 8) {
        if (v_0cde < 0)
            v_0cde = 0;
        if (v_0cde < 6)
            v_0cde++;
    } else if (v_0cde != 0) {
        if (v_0cde < 0)
            v_0cde++;
        else
            v_0cde--;
    }
    v_0cda = v_0ccc;
    if (v_0a00 & 1) {
        if (a0->y <= 0x10) {
            a0->y = 0x10;
            v_0cda = 1;
            goto side;
        }
        a0->y -= 3;
        a0->y -= v_0c9e;
    }
    if (v_0a00 & 2) {
        if (a0->y < 0xb0) {
            a0->y += 3;
            a0->y += v_0c9e;
        } else if (v_0cd0 != v_0cea) {
            v_0cda = 0xffff;
            a0->y = 0xb0;
        }
    }
side:
    d0 = v_0cde;
    if (d0 != 0) {
        if (d0 < 0) {
            if (v_0a00 & 4)
                d0 = -6;
        } else if (v_0a00 & 8) {
            d0 = 6;
        }
    }
    if (v_0c9e == 0)
        d0 >>= 1;
    if ((int16_t)v_0c9e >= 2)
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

    a0->fn_draw = fn_00128e;
    if (v_0d84) {
        v_0cda = 0;
        fn_002c5a(a0);
        if (a0->anim_delay == 0) {
            a0->fn_draw = fn_003130;
            if ((int16_t)v_0e06 >= 0x10)
                v_0a02 = 0xff;
        }
        return;
    }
    if (v_0430 == 0) {
        a1 = 0;
        if (v_0c90 == 0) {
            fn_0034ea(a0);
            a1 = fn_003052(a0);
            if (a1 == 0)
                goto after_hit;
        } else {
            d0 = (int16_t)(a0->x - 0x40);
            a0->hit[0] = d0;
            a0->hit[2] = (int16_t)(d0 + 0x80);
            d0 = (int16_t)(a0->y - 0x40);
            a0->hit[1] = d0;
            a0->hit[3] = (int16_t)(d0 + 0x80);
            a1 = fn_003052(a0);
            if (a1 != 0)
                apply_damage(a0, a1, 0x7f);
            fn_0034ea(a0);
            goto after_hit;
        }
        if (v_0cc8 == 0) {
            if (a1->type == 0)
                goto after_hit;
            a0->fn_draw = fn_001368;
            player_damage(a0, a1->flag35 ? 0x10 : 8);
            if (v_0d84)
                return;
        }
        if (a1->type != 0x50 && a1->type != 0x54)
            apply_damage(a0, a1, 0x7f);
    }
after_hit:
    if (v_042e != 0) {
        if ((int16_t)v_042e < 0) {
            if (!fn_00300a(a0)) {
                v_042e = 0;
                goto move;
            }
            if (v_042e == 0xffef) {
                player_dies(a0);
                return;
            }
        }
        v_042e--;
        v_0cda = v_0ccc;
        d0 = (int16_t)(v_047c[3] - v_0cd0);
        a0->x = v_047c[4];
        d0 += v_047c[5];
        if (d0 > 0xb0) {
            d0 -= 0xb0;
            v_0cda -= d0;
            d0 = 0xb0;
        }
        if (d0 < 0x10) {
            d0 -= 0x10;
            v_0cda -= d0;
            d0 = 0x10;
        }
        a0->y = d0;
        for (i = 0; i < 48; i++)
            v_047c[i] = v_047c[i + 3];
        if (v_042e == 0)
            v_042e = 0xffff;
    } else {
move:
        player_move(a0);
        for (i = 50; i >= 3; i--)
            v_047c[i] = v_047c[i - 3];
        v_047c[0] = v_0cd0;
        v_047c[1] = a0->x;
        v_047c[2] = a0->y;
        if (v_0430 == 0 && (int16_t)v_042e >= 0 && fn_00300a(a0)) {
            v_042e = v_041e;
            v_0cde = 0;
            d0 = (int16_t)(v_0cd0 + v_0cee);
            if (d0 > (int16_t)v_0cea)
                v_0cea = d0;
        }
    }
    for (i = 0; i < 6; i++)
        v_046c[i] = v_046c[i + 2];
    v_046c[6] = v_04e4;
    v_046c[7] = v_04e6;
    if (v_0430 == 0) {
        a0->gfx = (const void *)v_60d0[v_0cde + 6];
        return;
    }
    a0->gfx = (const void *)v_614a[v_0430 - 1];
    a0->fn_draw = fn_00128e;
    v_0430 += v_0432;
    if (v_0430 == 0) {
        v_0432 = 0;
        v_042e = 0xfff1;
        return;
    }
    if (v_0430 == 4) {
        if (v_0432 == 0)
            a0->fn_draw = fn_001438;
        v_0432 = 0;
    }
}

/* ---- level 1's blob ($54E00): the jump table at $54E0C and the routines the engine calls ---- */

/* orig $054E0C (-> $5566A4): the level's extra collision test finds nothing (d4 = 0, Z) */
int level1_collision_hook(void)
{
    return 0;
}

/* orig $054E10 (-> $055B94): level events by d0: a table of the level engine ($E60) for each */
void level1_event(uint16_t d0)
{
    static const uint16_t *const tabs[10] = { level_54e66, level_54e80, level_54e9a, level_54eb4, level_54ece,
                                              level_54ee8, level_54f02, level_54f34, level_54f4e, level_54f68 };

    if (d0 == 0) {
        fn_000e5a();
        return;
    }
    if (d0 <= 10)
        fn_000e60(tabs[d0 - 1]);
}

/* orig $054E18 (-> $055942): level hooks by d0 */
void level1_hook(uint16_t d0)
{
    if (d0 == 0) {
        fn_000ea2();
        return;
    }
    /* goes on at $5594C + 4 * d0: $559EC, $55960, $55A64, $55AE4 */
}

/* orig $054E24 (-> $055806): the level's scroll events */
void level1_scroll(void)
{
    const uint16_t *a1 = level_558a2;
    uint16_t *a4 = v_0d26;
    int d7, i;

    if (v_0d8c == 0 && (int16_t)(v_0cd0 + PLAYER->y) < 0xa80) {
        v_0a6c = 0xff;
        v_0d8c = 0xff;
        v_0cd0 = 0x9c0;
        v_0cea = 0x9c0;
        v_0cce = 0x1860;
        v_0cd6 = 0;
    }
    if ((int16_t)v_0cd0 >= 0xa40 && (int16_t)v_0cd0 <= 0xd10) {
        if ((int16_t)v_0cea < 0xd10)
            v_0cea = 0xd10;
        fn_055e2c();
        for (d7 = 15; d7 >= 0; d7--) {
            if (*a4 != 0) {
                uint16_t *a2 = (uint16_t *)((uint8_t *)level_init_src + (int16_t)a1[0]);

                (*a4)--;
                *a2 = a1[1 + (*a4 & 3)];
            }
            a4++;
            a1 += 5;
        }
    }
    for (i = 0; i < 5; i++)
        v_0d86[i] = 0;
}

/* orig $0563AA: update of the level 1 boss (type $50) */
void level1_boss_update(Object *a0)
{
    Object *a1, *a2, *a5;
    int d7;
    int16_t d0, d5;
    int32_t l;
    const uint32_t *src;
    static const uint32_t *const sources[4] = { level_56810, level_56810 + 4, level_56810 + 8, level_56810 + 4 };

    if ((int16_t)v_0cd0 > 0x1c0)
        goto hide;
    if (v_0d8b == 0) {
        v_0d8b = 0xff;
        a2 = a0;
        for (d7 = 7; d7 >= 0; d7--) {
            a1 = fn_000e54();
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
            a1->fn_update = fn_056278;
            a1->fn_draw = fn_000e12;
            a1->fn_hit = (void *)fn_000f2c;
            a1->gfx = level_59e28;
            a1->anim_delay = 0;
            a1->v5e = 0;
            a1->anim = (const uint16_t *)a2;
            a2 = a1;
            a1->flag35 = 0;
            a1->flag60 = -1;
            a1->y = -100;
            a1->hit[0] = 0x3e8;
            a1->hit[2] = 0x3e8;
        }
        a0->v2a = 0;
        a0->v2c = 0;
        a0->pad4a[2] = 0;
        a0->pad4a[3] = 0;
        a0->pad4a[4] = 0;
    }
    if ((int16_t)v_0cea <= 0x1c0)
        v_0cea = 0x1c0;
    a0->hit[0] = 0x80;
    a0->hit[2] = 0xc8;
    d5 = (int16_t)(a0->y - v_0cd0 + 4);
    a0->hit[1] = d5;
    a0->hit[3] = (int16_t)(d5 + 0x50);
    if (a0->v28 == 0) {
        if ((int16_t)v_0cd0 > 0x30)
            goto draw_table;
        a0->v28 = 1;
        a0->v2e = 0;
        a0->v30 = 0xaa;
    }
    if (a0->v28 < 0) {
        if (a0->v54 != 0)
            a0->v54--;
    } else if (a0->v54 < 0xb) {
        if (a0->v54 == 0)
            v_0418 = 0xf;
        a0->v54++;
    }
    a0->v28++;
    if (a0->v28 == 0)
        a0->v28++;
    if (a0->v28 >= 0x64)
        a0->v28 = -40;
    d0 = v_0cdc & 0x7f;
    if (d0 & 0x40)
        d0 = (int16_t)-(d0 - 0x7f);
    d0 = (int8_t)d0;
    a0->v2c = (int16_t)(d0 + 0x20);
    d0 = (int16_t)((v_0cdc + 0x50) & 0xff);
    if (d0 & 0x80)
        d0 = (~d0) & 0xff;
    l = (int32_t)(int16_t)(d0 - 0x40) << 10;
    a0->pad4a[2] = (uint16_t)(l >> 16);
    a0->pad4a[3] = (uint16_t)l;
    d0 = (int16_t)((v_0cdc + 0xa0) & 0x1ff);
    if (d0 & 0x100)
        d0 ^= 0x1ff;
    d0 -= 0x80;
    a0->pad4a[4] = (uint16_t)(d0 << 5);
    d0 = (int16_t)(a0->v2e + a0->y);
    if (d0 < 0x10) {
        d0 = 0x10;
        a0->v30 = 0;
    }
    if (d0 > 0x1e0) {
        d0 = 0x1e0;
        a0->v30 = 0;
    }
    a0->y = d0;
    a0->v30--;
    if (a0->v30 < 0) {
        if (a0->v2e == -1) {
            a0->v2e = 0;
            a0->v30 = 0xaa;
        } else if (a0->v2e == 1) {
            a0->v2e = 0;
            a0->v30 = 0x3c;
        } else {
            a0->v30 = 0x32;
            a0->v2e = 1;
            if (a0->y != 0x10)
                a0->v2e = -1;
        }
    }
    d0 = (int16_t)(0x1e - a0->energy);
    v_0d5a = (uint16_t)((v_0d5a & 0xff00) | ((v_0d5a + d0) & 0xff));
draw_table:
    src = sources[(v_0cdc & 0xc) >> 2];
    level_567fa[0] = src[0];
    level_567fa[1] = src[1];
    level_567fa[3] = src[2];
    level_567fa[4] = src[3];
    if (a0->v28 != 0)
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

    if ((int16_t)v_0cd0 >= 0x280)
        return;
    fn_000e8a(level_567bc, 5, 6, (uint16_t)(a0->y - v_0cd0), 0x38);
    d7 = (v_0d5a >> 4) & 0xc;
    fn_000e90(eyes[d7 >> 2], 0x98, (uint16_t)(a0->y - v_0cd0 + 0x42));
}

/* orig $054E20 (-> $5566A8): level 1 starts: its data is copied once, the boss is made */
void level1_init(void)
{
    Object *a0, *a5;
    int i;

    if (level_init_src == level_data_src) {
        for (i = 2999; i >= 0; i--)
            level_data_dst[i] = level_data_src[i];
        level_init_src = level_data_dst;
    }
    v_0ce8 = 0;
    v_0cea = 0x11ff;
    v_0d5a = 0;
    a0 = fn_000e54();
    a5 = LIST_C->prev;
    LIST_C->prev = a0;
    a0->prev = a5;
    a0->next = LIST_C;
    a5->next = a0;
    a0->type = 0x50;
    a0->fn_update = FN(level1_boss_update);
    a0->fn_draw = FN(level1_boss_draw);
    a0->fn_hit = (void *)fn_0561da;
    a0->v5e = 0;
    a0->flag60 = -1;
    a0->flag61 = -1;
    a0->y = 0x10;
    a0->energy = level_54e58[0];
    a0->v28 = 0;
    a0->v54 = 0;
    v_0d8b = 0;
    for (i = 0; i < 5; i++)
        v_0d86[i] = 0;
    v_0d8c = 0;
    for (i = 0; i < 16; i++)
        v_0d26[i] = 0;
    v_0d26[16] = 0x45d2; v_0d26[17] = 0x72ba;
    v_0d26[18] = 0xaa6a; v_0d26[19] = 0x4a56;
    v_0d26[20] = 0x4aba; v_0d26[21] = 0x5d4d;
    v_0d26[22] = 0xd225; v_0d26[23] = 0x12c5;
    v_0d26[24] = 0x28e5; v_0d26[25] = 0x1aec;
    v_0424 = (uint16_t)(v_0e02 * 1);
    v_0426 = (uint16_t)(v_0e02 * 2);
}
