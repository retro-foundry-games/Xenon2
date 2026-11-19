#include "pkg_c.h"

/* ---- helpers ---- */

/* the object from the free list, put at the head of list i (the code after each call of $2418 that does this) */
static Object *list_insert(int list)
{
    Object *a0 = fn_002418();
    Object *old = LIST_HEAD(list);

    LIST_HEAD(list) = a0;
    OBJ_FWD(a0) = old;
    OBJ_BACK(a0) = LIST_SENTINEL(list);
    OBJ_BACK(old) = a0;
    return a0;
}

/* word $4A of an object (a pointer; struct Object has it among its padding) */
static void set_ptr4a(Object *o, const uint16_t *p)
{
    typedef const uint16_t *__attribute__((may_alias)) word_ptr;

    *(word_ptr *)(void *)&o->pad4a[0] = p;
}

/* an object that does nothing any more */
static void kill_object(Object *o)
{
    o->type = 4;
    o->fn_update = (obj_fn)fn_000fee;
    o->fn_draw = (obj_fn)obj_nop;
    o->fn_hit = (void *)obj_nop;
    o->flag34 = 0;
    o->flag60 = 0;
}

/* ---- animation scripts ---- */

/* orig $002C5A: advance the animation of object a0: count the delay down, then take the next step of the script (a picture and
 * a delay, or a zero picture followed by the address of a routine that is called with the script position) */
void anim_step(Object *o)
{
    const uint16_t *w;
    const uint16_t *gfx;
    int16_t t = (int16_t)(o->anim_delay - 1);

    o->anim_delay = t;
    if (t < 0) {
        o->anim_delay = 0;
        return;
    }
    if (t != 0)
        return;
    w = o->anim;
    gfx = *(const uint16_t *const *)w;
    w += 2;
    if (gfx == 0) {
        AnimOp op = *(const AnimOp *)w;

        w += 2;
        op(o, w);
        return;
    }
    o->gfx = gfx;
    o->anim_delay = (int16_t)*w++;
    o->anim = w;
}

/* orig $002C82: script step "jump": the next longword is the address of the script to go on with */
void anim_op_jump(Object *o, const uint16_t *a1)
{
    const uint16_t *w = *(const uint16_t *const *)a1;

    o->gfx = *(const void *const *)w;
    w += 2;
    o->anim_delay = (int16_t)*w++;
    o->anim = w;
}

/* orig $002C92: script step "end": the object (and the pieces chained to it, if it is one of a group) goes dead */
void anim_op_free(Object *o, const uint16_t *a1)
{
    Object *p;
    Object *next;

    if (o->flag34 == 0) {
        kill_object(o);
        return;
    }
    p = o->target->chain;
    while (p != 0) {
        next = p->chain;
        kill_object(p);
        p = next;
    }
}

/* ---- the map under the player ---- */

/* the three words of tile pointers for the next 16 pixel row of the map: 0 = empty tile mask, a negative word = a mask in
 * tile_gfx, else a full one; steps a1 to the next row of cells */
static const uint8_t *tile_mask_ptr(uint16_t w)
{
    if (w == 0)
        return mask_empty;
    if ((int16_t)w < 0)
        return tile_gfx + ((uint32_t)(w & 0x7fff) << 4);
    return mask_full;
}

/* orig $002D1A: fill tile_mask_buf with the tile masks (three words a row) of the map at pixel (x, y), rows + 1 rows high.
 * Inlined: $2D7A (the three pointers of a row of cells, results in a4, a5, a6). */
void map_tile_mask(uint16_t x, uint16_t y, uint16_t rows)
{
    const uint16_t *a1 = (const uint16_t *)((const uint8_t *)level_init_src + (int16_t)v_0cce);
    const uint8_t *a4, *a5, *a6;
    uint16_t d3, d4;
    uint16_t *out = tile_mask_buf;

    y = (uint16_t)(y + v_0cd6);
    d3 = y & 0xf;
    d4 = d3;
    d3 = (uint16_t)(d3 + d3);
    d3 = (uint16_t)(d3 + d3);
    d3 = (uint16_t)(d3 + d4);
    d3 = (uint16_t)(d3 + d3);
    a1 = (const uint16_t *)((const uint8_t *)a1 + (int16_t)(uint16_t)((uint32_t)(y >> 4) * 0x28));
    a1 = (const uint16_t *)((const uint8_t *)a1 + (uint16_t)((x >> 3) & 0xfe));
    a4 = tile_mask_ptr(a1[0]);
    a5 = tile_mask_ptr(a1[1]);
    a6 = tile_mask_ptr(a1[2]);
    a1 += 20;
    do {
        *out++ = *(const uint16_t *)(a4 + d3);
        *out++ = *(const uint16_t *)(a5 + d3);
        *out++ = *(const uint16_t *)(a6 + d3);
        d3 = (uint16_t)(d3 + 10);
        if (d3 == 0xa0) {
            a4 = tile_mask_ptr(a1[0]);
            a5 = tile_mask_ptr(a1[1]);
            a6 = tile_mask_ptr(a1[2]);
            a1 += 20;
            d3 = 0;
        }
    } while ((uint16_t)--rows != 0xffff);
}

/* orig $00300A: does the 24-row sprite mask of the player's position meet a solid tile?  (the original returns the condition
 * codes: not equal = hit) */
uint16_t player_tile_hit(Object *o)
{
    uint16_t x = (uint16_t)(o->x - 14);
    uint16_t y = (uint16_t)(o->y - 14);
    const uint16_t *m = hit_mask_data + 75 * (x & 0xf);
    const uint16_t *b = tile_mask_buf;
    const uint32_t *ml, *bl;
    uint32_t r;
    uint16_t n = 0x24;

    map_tile_mask(x, y, 24);
    if (*m++ & *b++)
        return 1;
    ml = (const uint32_t *)m;
    bl = (const uint32_t *)b;
    do {
        r = *ml++ & *bl++;
        if (r)
            break;
    } while ((uint16_t)--n != 0xffff);
    return r != 0;
}

/* ---- collisions ---- */

/* the boxes (x0, y0, x1, y1) of a and b overlap (inlined: $200A) */
static int boxes_overlap(int16_t ax0, int16_t ay0, int16_t ax1, int16_t ay1, const int16_t *b)
{
    return !(b[2] < ax0 || b[0] > ax1 || b[3] < ay0 || b[1] > ay1);
}

/* orig $00306E: the first object of the list from a1 whose box meets the box; objects of kind $B8 are skipped (and their chain
 * is stepped over).  Returns the object, or the object that ends the list (kind 0) when none does. */
Object *scan_boxes(Object *a1, int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    while (a1->type != 0) {
        if (boxes_overlap(x0, y0, x1, y1, a1->hit)) {
            if (a1->type != 0xb8)
                return a1;
        } else if (a1->type == 0xb8) {
            a1 = a1->chain;
        }
        a1 = OBJ_FWD(a1);
    }
    return a1;
}

/* orig $003052: the enemy that the box of object a0 meets (the original leaves it in a1 and returns not-equal), or 0 when none
 * does and the level blob has nothing either */
Object *collide_enemies(Object *o)
{
    int16_t x0 = o->hit[0], y0 = o->hit[1], x1 = o->hit[2], y1 = o->hit[3];
    Object *a1;

    hit_box[0] = (uint16_t)x0;
    hit_box[1] = (uint16_t)y0;
    hit_box[2] = (uint16_t)x1;
    hit_box[3] = (uint16_t)y1;
    a1 = scan_boxes(LIST_HEAD(LIST_ENEMY), x0, y0, x1, y1);
    if (a1->type != 0)
        return a1;
    return fn_054e0c(x0, y0, x1, y1) ? a1 : 0;
}

/* orig $0030DE: the enemy whose box holds the point (x, y) (in a1 in the original), or 0 when none does and the level blob has
 * nothing either */
Object *point_in_enemy(int16_t x, int16_t y)
{
    Object *a1 = LIST_HEAD(LIST_ENEMY);

    hit_box[0] = (uint16_t)x;
    hit_box[2] = (uint16_t)x;
    hit_box[1] = (uint16_t)y;
    hit_box[3] = (uint16_t)y;
    while (a1->type != 0) {
        if (x < a1->hit[0] || x > a1->hit[2] || y < a1->hit[1] || y > a1->hit[3]) {
            if (a1->type == 0xb8)
                a1 = a1->chain;
        } else if (a1->type != 0xb8) {
            return a1;
        }
        a1 = OBJ_FWD(a1);
    }
    return fn_054e0c(x, y, x, y) ? a1 : 0;
}

/* orig $003132: the direction (0 to 7) of (dx, dy) */
static uint16_t direction_octant(int16_t d0, int16_t d1)   /* $3156: both not negative */
{
    uint32_t q;

    if (d0 == 0)
        return 4;
    if (d1 == 0)
        return 2;
    if (d0 == d1)
        return 3;
    if (d0 > d1) {
        q = ((uint32_t)((uint16_t)d1 >> 1)) << 16;
        q = q / (uint16_t)d0;
        if (q > 0xffff)
            q = 0;
        return (int16_t)q >= 0x3504 ? 3 : 2;
    }
    q = ((uint32_t)((uint16_t)d0 >> 1)) << 16;
    q = q / (uint16_t)d1;
    if (q > 0xffff)
        q = 0;
    return (int16_t)q >= 0x3504 ? 3 : 4;
}

static uint16_t direction_half(int16_t d0, int16_t d1)     /* $3144 */
{
    uint16_t r;

    if (d0 >= 0)
        return direction_octant(d0, d1);
    r = direction_octant((int16_t)(0 - (uint16_t)d0), d1);
    return (uint16_t)(8 - r) & 7;
}

uint16_t direction8(int16_t dx, int16_t dy)
{
    uint16_t r;

    if (dy >= 0)
        return direction_half(dx, dy);
    r = direction_half(dx, (int16_t)(0 - (uint16_t)dy));
    return (uint16_t)(4 - r) & 7;
}

/* ---- objects ---- */

/* orig $003130: a routine that does nothing (the "no update", "no draw" of a dead object) */
void obj_nop(Object *o)
{
}

/* orig $003286: the object after a1 in its list, or, for a part of a group (flag $34), the first one after the group's other
 * parts (parts of one owner follow each other) */
Object *owner_chain_next(Object *a1)
{
    Object *a2 = OBJ_FWD(a1);

    if (a1->flag34 != 0) {
        a2 = a1->target;
        for (;;) {
            a2 = OBJ_FWD(a2);
            if (a2->type == 0 || a2->flag34 == 0 || a2->target != a1)
                break;
        }
    }
    return a2;
}

/* orig $0032AA: spawn rec->count copies of the formation a2 describes (a chain of objects, one for each definition), the copies
 * rec->x apart; at the end the kind is recorded in the table at $43C.  Returns the last object made (a0 itself when none). */
Object *spawn_formation(Object *a0, Object *rec_obj, const struct SpawnDef *a2)
{
    const struct SpawnRec *a1 = (const struct SpawnRec *)rec_obj;
    uint16_t d7 = a1->count;
    uint16_t d6 = 0;
    uint16_t *slot;
    uint16_t d1;
    int i;

    if (d7 == 0)
        return a0;

    while (d7-- != 0) {
        const struct SpawnDef *a4 = a2;
        Object *a5;
        const uint16_t *a3;
        const int16_t *e;
        uint16_t d0;

        a0 = list_insert(LIST_ENEMY);
        a5 = a0;
        for (;;) {
            a0->type = a4->kind;
            a0->fn_update = (obj_fn)a4->update;
            a0->fn_draw = (obj_fn)a4->draw;
            a0->fn_hit = (void *)a4->hit;
            a3 = a4->anim;
            a0->gfx = *(const void *const *)a3;
            a0->anim_delay = (int16_t)a3[2];
            a0->anim = a3 + 3;
            a0->x = (int16_t)d6;
            a0->x_frac = 0;
            a0->y = 0;
            a0->y_frac = 0;
            a0->v2a = 0;
            a0->v2c = 0;
            a0->v42 = (int16_t)a4->w2;
            a0->v28 = 0;
            if ((int16_t)a1->x < 100) {
                d6 = (uint16_t)(0 - d6);
                a0->v28 = (int16_t)d6;
                d6 = (uint16_t)(0 - d6);
                a0->x = 0;
            }
            if (a5 != a0)
                a0->v28 = (int16_t)(a0->v28 - (int16_t)a4->w2);
            e = (const int16_t *)level_formations[(int16_t)(uint16_t)((a1->index - 1) * 4) / 4];
            if ((const void *)e < (const void *)level_data_start || (const void *)e >= (const void *)level_events_a)
                return a0;
            a0->v46 = (uint16_t *)e;
            if (*(const uint16_t *)e == 0xa) {
                a0->x = e[1];
                a0->y = e[2];
                if ((int16_t)a1->x >= 100)
                    a0->x = (int16_t)(a0->x + (int16_t)d6);
                e += 3;
            }
            set_ptr4a(a0, (const uint16_t *)e);
            a0->v54 = (int16_t)a1->w0c;
            a0->v5e = (int16_t)a1->y;
            d0 = (uint16_t)random_next();
            a0->v5e = (int16_t)((a0->v5e & 0x00ff) | ((d0 & 0xff) << 8));
            a0->energy = (int16_t)v_0426;
            a0->v44 = (int16_t)v_045e;
            a0->flag35 = (int8_t)a4->to_flag35;
            if (a4->to_flag35 == 0) {
                a0->energy = (int16_t)v_0424;
                a0->v44 = (int16_t)v_045c;
            }
            a0->flag34 = (int8_t)a4->to_flag34;
            bbox_from_frame(a0);
            a0->target = a5;
            a4++;
            if (a4->kind != 0) {
                Object *a6 = a0;
                Object *nxt;

                a0 = fn_002418();
                a6->chain = a0;
                nxt = OBJ_FWD(a6);
                OBJ_FWD(a6) = a0;
                OBJ_FWD(a0) = nxt;
                OBJ_BACK(a0) = a6;
                OBJ_BACK(nxt) = a0;
                continue;
            }
            break;
        }
        d0 = a1->x;
        while ((int16_t)d0 >= 100)
            d0 = (uint16_t)(d0 - 100);
        d6 = (uint16_t)(d6 + d0);
        a0->chain = 0;
    }
    d1 = v_045e;
    if (a0->flag35 == 0)
        d1 = v_045c;
    slot = (uint16_t *)v_043c;
    for (i = 0; i < 8; i++, slot += 2) {
        if (slot[0] == d1) {
            slot[1] = (uint16_t)(slot[1] + a1->count);
            return a0;
        }
    }
    slot = (uint16_t *)v_043c;
    for (i = 0; i < 8; i++, slot += 2) {
        if (slot[0] == 0) {
            slot[0] = d1;
            slot[1] = a1->count;
            v_0cec = (uint16_t)(v_0cec + 1);
            return a0;
        }
    }
    return a0;
}

/* orig $0034EA: the box of object o (hit[]) from its picture: the four words before the picture are the box's offset and size,
 * the two words of the picture the hot spot */
void bbox_from_frame(Object *o)
{
    const int16_t *f = (const int16_t *)o->gfx;
    int16_t x0 = (int16_t)(f[-4] + o->x - f[0]);
    int16_t y0 = (int16_t)(f[-3] + o->y - f[1]);

    o->hit[0] = x0;
    o->hit[1] = y0;
    o->hit[2] = (int16_t)(x0 + f[-2] - 1);
    o->hit[3] = (int16_t)(y0 + f[-1] - 1);
}

/* orig $0031EC: an effect (list of effects) at (x, y) that plays the animation a2 (header: picture long, delay, script) and is
 * drawn by a3; it asks for the sound $85 when the animation is $270E, else $83 */
void spawn_effect(int16_t x, int16_t y, const uint16_t *a2, ObjFn a3)
{
    Object *a0;

    v_0418 = (a2 == anim_270e) ? 0x85 : 0x83;
    a0 = list_insert(LIST_EFFECT);
    a0->type = 0xc;
    a0->fn_update = (obj_fn)fn_0057b6;
    a0->fn_draw = (obj_fn)a3;
    a0->x = x;
    a0->y = y;
    a0->gfx = *(const void *const *)a2;
    a0->anim_delay = (int16_t)a2[2];
    a0->anim = a2 + 3;
}

/* orig $003518 (reached by a jump; goes on in $352E): remember the record of a spawn */
void note_spawn_record(const struct SpawnRec *a1, uint16_t d2)
{
    v_0ce2 = a1->x;
    v_0ce4 = a1->y;
    v_0ce6 = (uint16_t)a1->scroll;
    v_0ca6 = v_0c9a;
    latch_slots(d2);
}

/* orig $00352E: copy the type and the score ($42) of the object of each of the 7 weapon slots to v_0caa unless v_0ca2 is set
 * (d2 is what an empty slot gets after the first one that was not: the caller's register) */
void latch_slots(uint16_t d2)
{
    const Slot *s = (const Slot *)v_0c66;
    uint16_t *out = v_0caa;
    int i;

    if (v_0ca2 != 0)
        return;
    for (i = 0; i < 7; i++, s++) {
        *out++ = s->type;
        if (s->type != 0)
            d2 = (uint16_t)s->obj->v42;
        *out++ = d2;
    }
}

/* orig $003554: fire the level's scroll events: each record of the two lists whose scroll value lies between the new scroll
 * position (v_0cd0) and the old one calls the level's routine ($54E10 for the first list, $54E18 for the second) */
void scroll_events(void)
{
    const struct SpawnRec *a1 = (const struct SpawnRec *)level_events_a;
    int16_t d0 = (int16_t)v_0cd2;
    int16_t d1 = (int16_t)v_0cd0;

    for (;; a1++) {
        int16_t d2 = a1->scroll;

        if (d2 < 0)
            break;
        if (d0 > d2 && d1 <= d2) {
            fn_054e10(a1->w2, a1);
            d0 = (int16_t)v_0cd2;
            d1 = (int16_t)v_0cd0;
        }
    }
    if (d1 >= d0)
        return;
    v_0cd2 = (uint16_t)d1;
    a1 = (const struct SpawnRec *)level_events_b;
    d0 = (int16_t)v_0cd4;
    d1 = (int16_t)v_0cd0;
    for (;; a1++) {
        int16_t d2 = a1->scroll;

        if (d2 < 0)
            break;
        if (d0 > d2 && d1 <= d2) {
            fn_054e18(a1->w2, a1);
            d0 = (int16_t)v_0cd4;
            d1 = (int16_t)v_0cd0;
        }
    }
    if (d1 >= d0)
        return;
    v_0cd4 = (uint16_t)d1;
}

/* orig $003656: call the update routine of each object of the list from a0 */
void run_updates(Object *a0)
{
    while (a0->type != 0) {
        Object *volatile next = OBJ_FWD(a0);   /* kept on the stack: the handlers do not preserve registers */

        ((ObjFn)a0->fn_update)(a0);
        a0 = next;
    }
}

/* orig $00366A: call the draw routine of each object of the list from a0 */
void run_draws(Object *a0)
{
    while (a0->type != 0) {
        Object *volatile next = OBJ_FWD(a0);   /* kept on the stack: the handlers do not preserve registers */

        ((ObjFn)a0->fn_draw)(a0);
        a0 = next;
    }
}

/* orig $0035D4: one frame of the objects: update all lists, draw all lists, then forget the kinds of the table at $43C that no
 * enemy has any more */
void update_all(void)
{
    uint16_t *slot;
    int i;

    run_updates(LIST_HEAD(LIST_PLAYER));
    run_updates(LIST_HEAD(LIST_ENEMY));
    v_0a6a = v_0a01;
    v_0a01 = 0;
    run_updates(LIST_HEAD(LIST_MISC));
    run_updates(LIST_HEAD(LIST_EFFECT));
    run_updates(LIST_HEAD(LIST_FRONT));
    run_draws(LIST_HEAD(LIST_PLAYER));
    run_draws(LIST_HEAD(LIST_MISC));
    run_draws(LIST_HEAD(LIST_ENEMY));
    run_draws(LIST_HEAD(LIST_EFFECT));
    run_draws(LIST_HEAD(LIST_FRONT));
    slot = (uint16_t *)v_043c;
    for (i = 0; i < 8; i++, slot += 2) {
        Object *a1;

        if (slot[0] == 0)
            continue;
        for (a1 = LIST_HEAD(LIST_ENEMY);; a1 = OBJ_FWD(a1)) {
            if (a1->type == 0) {
                slot[0] = 0;
                break;
            }
            if ((uint16_t)a1->v44 == slot[0])
                break;
        }
    }
}

/* orig $00367E (reached by a jump): the object a1 breaks up into the debris of kind a1->v30 and goes dead */
void spawn_debris(Object *a1)
{
    Object *a0 = list_insert(LIST_EFFECT);
    const struct SpawnKind *k;
    const uint16_t *a2;

    a0->v30 = a1->v30;
    k = (const struct SpawnKind *)((const uint8_t *)spawn_kinds + (int16_t)(uint16_t)(6 * (uint16_t)a1->v30));
    a0->type = k->kind;
    v_0414 = 0x11;
    a0->fn_update = (obj_fn)fn_004634;
    a0->fn_draw = (obj_fn)fn_00128e;
    a0->x = a1->x;
    a0->y = a1->y;
    a2 = k->anim;
    a0->gfx = *(const void *const *)a2;
    a0->anim_delay = (int16_t)a2[2];
    a0->anim = a2 + 3;
    a0->v28 = 7;
    kill_object(a1);
}

/* ---- entries the recorded run did not reach ---- */

/* orig $003252: spawn the formation at $326C for record a1, then remember its word $54 in $30 */
Object *spawn_wave_326c(Object *a0, Object *a1)
{
    a0 = spawn_formation(a0, a1, (const struct SpawnDef *)spawn_def_326c);
    a0->v30 = a0->v54;
    a0->v54 = 3;
    a0->v44 = 0;
    return a0;
}

/* the end of $345E and $3480: a marker object that sits on the map cell under the record's position */
static Object *marker_tail(Object *a0, uint16_t d0, const struct SpawnRec *a1, void (*a2)(void))
{
    uint16_t x, y;

    a0->type = d0;
    a0->fn_update = (obj_fn)a2;
    a0->fn_draw = (obj_fn)obj_nop;
    a0->v5e = 0;
    x = (uint16_t)(a1->x - 8);
    a0->x = (int16_t)x;
    y = (uint16_t)(a1->y - 8);
    a0->y = (int16_t)y;
    a0->v46 = (uint16_t *)((const uint8_t *)level_init_src + (uint16_t)((x >> 3) & 0xfe) +
                           (int16_t)(uint16_t)((uint32_t)(y >> 4) * 0x28));
    a0->v28 = 0;
    a0->v30 = 0;
    return a0;
}

/* orig $00345E: a marker in the list of enemies */
Object *spawn_marker_enemy(uint16_t d0, const struct SpawnRec *a1, void (*a2)(void))
{
    return marker_tail(list_insert(LIST_ENEMY), d0, a1, a2);
}

/* orig $003480: a marker in the list at the front */
Object *spawn_marker_front(uint16_t d0, const struct SpawnRec *a1, void (*a2)(void))
{
    return marker_tail(list_insert(LIST_FRONT), d0, a1, a2);
}

/* ---- link stand-ins (weak: the owner's C replaces them) ---- */

/* $2418: the first object of the free list (the pool-empty case, which takes one from the effects, is not here) */
__attribute__((weak)) Object *fn_002418(void)
{
    Object *a0 = free_obj;

    if (a0 == 0)
        return 0;
    free_obj = a0->next;
    a0->flag34 = 0;
    a0->flag60 = 0;
    a0->flag61 = 0;
    return a0;
}

/* the level blob's entries: not decompiled with the blob */
__attribute__((weak)) uint16_t fn_054e0c(int16_t d0, int16_t d1, int16_t d2, int16_t d3) { return 0; }
__attribute__((weak)) void fn_054e10(uint16_t d0, const struct SpawnRec *a1) { }
__attribute__((weak)) void fn_054e18(uint16_t d0, const struct SpawnRec *a1) { }
