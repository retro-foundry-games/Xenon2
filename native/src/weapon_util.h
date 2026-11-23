#ifndef WEAPON_UTIL_H
#define WEAPON_UTIL_H
#include "game_all.h"

/* the same, as the level's code (orig $55000 and up) does it: it stores the jump-table entries ($E42, $E48), not the routines ($FEE, $3130) */
static inline void kill_object_thunk(Object *o)
{
    o->type = 4;
    o->fn_update = (obj_fn)free_object;
    o->fn_draw = (obj_fn)obj_nop_thunk;
    o->fn_hit = (void *)obj_nop_thunk;
    o->is_part = 0;
    o->persists = 0;
}

static inline Object *alloc_b(Object *caller)
{
    Object *o = obj_free_pop(), *tail = xenon2.obj_list[3].prev;

    xenon2.obj_list[3].prev = o;
    o->prev = tail;
    o->next = &xenon2.obj_list[3];
    tail->next = o;
    return o;
}

/* list B from the other end: the object goes first (baa), links swapped */
static inline Object *alloc_b_front(Object *caller)
{
    Object *o = obj_free_pop(), *head = xenon2.obj_list[3].next;

    xenon2.obj_list[3].next = o;
    o->next = head;
    o->prev = &xenon2.obj_list[3];
    head->prev = o;
    return o;
}

static inline void copy_long(uint16_t *dst, const uint16_t *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
}

/* move.l (a2)+,$16(a); move.w (a2)+,$1a(a); move.l a2,$1c(a) */
static inline void set_anim(Object *o, const uint16_t *a)
{
    anim_header_set(o, a);
}

/* the object from the free list put first in the list of enemies (list C, head $B4C) */
static inline Object *alloc_c(void)
{
    Object *o = obj_free_pop(), *tail = xenon2.obj_list[2].prev;

    xenon2.obj_list[2].prev = o;
    o->prev = tail;
    o->next = &xenon2.obj_list[2];
    tail->next = o;
    return o;
}

#endif
