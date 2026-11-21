#include "objects.h"
#include "shop.h"
#include "game_all.h"
#ifdef GAME_SDL
uint8_t guest_a6a(uint8_t a01);
#endif

/* ---- helpers ---- */

/* the object from the free list, put at the head of list i (the code after each call of $2418 that does this) */
static Object *list_insert(int list)
{
    Object *a0 = obj_free_pop();
    Object *old = LIST_HEAD(list);

    LIST_HEAD(list) = a0;
    OBJ_FWD(a0) = old;
    OBJ_BACK(a0) = LIST_SENTINEL(list);
    OBJ_BACK(old) = a0;
    return a0;
}

/* long $4A of an object: a pointer */
static void set_cursor(Object *o, const uint8_t *p)
{
    o->cursor = p;
}

/* the records of the level's data are raw bytes (big endian): read into a record of this machine */
static void spawnrec_load(struct SpawnRec *r, const uint8_t *p)
{
    r->scroll = (int16_t)rd16(p);
    r->w2 = rd16(p + 2);
    r->count = rd16(p + 4);
    r->index = rd16(p + 6);
    r->x = (int16_t)rd16(p + 8);
    r->y = (int16_t)rd16(p + 10);
    r->w0c = rd16(p + 12);
}

static void spawndef_load(struct SpawnDef *d, const uint8_t *p)
{
    d->kind = rd16(p);
    d->w2 = rd16(p + 2);
    d->update = (void (*)(void))GFN(rd32(p + 4));
    d->draw = (void (*)(void))GFN(rd32(p + 8));
    d->hit = (void (*)(void))GFN(rd32(p + 12));
    d->anim = (const uint8_t *)GPTR(rd32(p + 0x10));
    d->to_flag35 = p[0x16];
    d->to_flag34 = p[0x17];
}

/* an object that does nothing any more */
static void kill_object(Object *o)
{
    o->type = 4;
    o->fn_update = (obj_fn)free_object;
    o->fn_draw = (obj_fn)obj_nop;
    o->fn_hit = (void *)obj_nop;
    o->flag34 = 0;
    o->flag60 = 0;
}

/* ---- animation scripts ---- */

/* orig $E9C, $002C5A: advance the animation of object a0: count the delay down, then take the next step of the script (a picture and
 * a delay, or a zero picture followed by the address of a routine that is called with the script position) */
void anim_step(Object *o)
{
    const uint8_t *w;
    uint32_t gfx;
    int16_t t = (int16_t)(o->anim_delay - 1);

    o->anim_delay = t;
    if (t < 0) {
        o->anim_delay = 0;
        return;
    }
    if (t != 0)
        return;
    w = o->anim;
    gfx = rd32(w);
    w += 4;
    if (gfx == 0) {
        AnimOp op = (AnimOp)GFN(rd32(w));

        w += 4;
        op(o, w);
        return;
    }
    o->gfx = GPTR(gfx);
    o->anim_delay = (int16_t)rd16(w);
    o->anim = w + 2;
}

/* orig $E2A, $002C82: script step "jump": the next longword is the address of the script to go on with */
void anim_op_jump(Object *o, const uint8_t *a1)
{
    anim_header_set(o, (const uint8_t *)GPTR(rd32(a1)));
}

/* orig $E30, $002C92: script step "end": the object (and the pieces chained to it, if it is one of a group) goes dead */
void anim_op_free(Object *o, const uint8_t *a1)
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

#ifdef GAME_SDL
extern const uint8_t *guest_high_ptr(uint32_t address);   /* sdl/guest.c */
#endif

/* the three words of tile pointers for the next 16 pixel row of the map: 0 = empty tile mask, a negative word = a mask in
 * tile_gfx, else a full one; steps a1 to the next row of cells */
static const uint8_t *tile_mask_ptr(uint16_t w)
{
    if (w == 0)
        return mask_empty;
    if ((int16_t)w < 0) {
        const uint8_t *m = tile_gfx + ((uint32_t)(w & 0x7fff) << 4);

#ifdef GAME_SDL
        if (GADDR(m) + 0xa0 > 0x80000) {   /* the original's memory beyond 512 KB (the lockstep gives it) */
            m = guest_high_ptr(GADDR(m));
            return m ? m : mask_empty;
        }
#endif
        return m;
    }
    return mask_full;
}

/* A row of cells can lie past the end of the memory the port has (bobs above the screen read the original's memory beyond 512 KB: the lockstep supplies
 * it, otherwise it is zeros). */
#ifdef GAME_SDL
extern uint16_t guest_high_word(uint32_t address);   /* sdl/guest.c */
#endif
static const uint8_t *tile_mask_ptr_at(const uint8_t *cell)
{
#ifdef GAME_SDL
    if (GADDR(cell) + 2 > 0x80000)
        return tile_mask_ptr(guest_high_word(GADDR(cell)));
#endif
    return tile_mask_ptr(rd16(cell));
}

/* orig $002D1A: fill tile_mask_buf with the tile masks (three words a row) of the map at pixel (x, y), rows + 1 rows high.
 * Inlined: $2D7A (the three pointers of a row of cells, results in a4, a5, a6). */
void map_tile_mask(uint16_t x, uint16_t y, uint16_t rows)
{
    const uint8_t *a1 = level_init_src + (int16_t)map_row_offset;
    const uint8_t *a4, *a5, *a6;
    uint16_t d3, d4;
    uint16_t *out = tile_mask_buf;

    y = (uint16_t)(y + scroll_fine);
    d3 = y & 0xf;
    d4 = d3;
    d3 = (uint16_t)(d3 + d3);
    d3 = (uint16_t)(d3 + d3);
    d3 = (uint16_t)(d3 + d4);
    d3 = (uint16_t)(d3 + d3);
    a1 += (int16_t)(uint16_t)((uint32_t)(y >> 4) * 0x28);
    a1 += (uint16_t)((x >> 3) & 0xfe);
    a4 = tile_mask_ptr_at(a1);
    a5 = tile_mask_ptr_at(a1 + 2);
    a6 = tile_mask_ptr_at(a1 + 4);
    a1 += 40;
    do {
        *out++ = rd16(a4 + d3);
        *out++ = rd16(a5 + d3);
        *out++ = rd16(a6 + d3);
        d3 = (uint16_t)(d3 + 10);
        if (d3 == 0xa0) {
            a4 = tile_mask_ptr_at(a1);
            a5 = tile_mask_ptr_at(a1 + 2);
            a6 = tile_mask_ptr_at(a1 + 4);
            a1 += 40;
            d3 = 0;
        }
    } while ((uint16_t)--rows != 0xffff);
}

/* orig $002F66: like map_tile_mask with two words a row (the masks of the cells at x and at x + 16), written as the blitter reads them
 * (inlined: $2FC0, the two pointers of a row of cells in a4 and a5) */
uint16_t tile_mask2_words;   /* the words of tile_mask_buf the last map_tile_mask2 wrote (as the blitter reads them) */

void map_tile_mask2(uint16_t x, uint16_t y, uint16_t rows)
{
    const uint8_t *a1 = level_init_src + (int16_t)map_row_offset;
    const uint8_t *a4, *a5;
    uint16_t d3, d4;
    uint8_t *out = (uint8_t *)tile_mask_buf;

    tile_mask2_words = (uint16_t)((rows + 1) * 2);
    y = (uint16_t)(y + scroll_fine);
    d3 = y & 0xf;
    d4 = d3;
    d3 = (uint16_t)(d3 + d3);
    d3 = (uint16_t)(d3 + d3);
    d3 = (uint16_t)(d3 + d4);
    d3 = (uint16_t)(d3 + d3);
    a1 += (int16_t)(uint16_t)((uint32_t)(y >> 4) * 0x28);
    a1 += (uint16_t)((x >> 3) & 0xfe);
    a4 = tile_mask_ptr_at(a1);
    a5 = tile_mask_ptr_at(a1 + 2);
    a1 += 40;
    do {
        wr16(out, rd16(a4 + d3));
        wr16(out + 2, rd16(a5 + d3));
        out += 4;
        d3 = (uint16_t)(d3 + 10);
        if (d3 == 0xa0) {
            a4 = tile_mask_ptr_at(a1);
            a5 = tile_mask_ptr_at(a1 + 2);
            a1 += 40;
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
    return level_collision(x0, y0, x1, y1, a1);
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
    return level_collision(x, y, x, y, a1);
}

/* orig $003156: the direction (0 to 7) of (dx, dy), both not negative */
static uint16_t direction_octant(int16_t d0, int16_t d1)
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

/* orig $003144: the direction of (dx, dy) with dy not negative */
static uint16_t direction_half(int16_t d0, int16_t d1)
{
    uint16_t r;

    if (d0 >= 0)
        return direction_octant(d0, d1);
    r = direction_octant((int16_t)(0 - (uint16_t)d0), d1);
    return (uint16_t)(8 - r) & 7;
}

/* orig $E7E, $003132: the direction (0 to 7) of (dx, dy) */
uint16_t direction8(int16_t dx, int16_t dy)
{
    uint16_t r;

    if (dy >= 0)
        return direction_half(dx, dy);
    r = direction_half(dx, (int16_t)(0 - (uint16_t)dy));
    return (uint16_t)(4 - r) & 7;
}

/* ---- objects ---- */

/* orig $003130: a routine that does nothing (the "no update", "no draw" of a dead object).  The code that makes an object a nop stores this address, and the
 * code that asks compares with it. */
void obj_nop(Object *o)
{
}

/* orig $E48: the jump-table entry of obj_nop (`jmp $3130`), the address the level's data tables give the objects they spawn.  It is a different value from obj_nop's:
 * an object that has it is not "nop" to the code that compares with $3130 (group_box_update counts its box). */
void obj_nop_thunk(Object *o)
{
    obj_nop(o);
}

/* orig $EC6, $003286: the object after a1 in its list, or, for a part of a group (flag $34), the first one after the group's other
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

/* orig $E60, $0032AA: spawn rec->count copies of the formation a2 describes (a chain of objects, one for each definition), the copies
 * rec->x apart; at the end the kind is recorded in the table at $43C.  Returns the last object made (a0 itself when none). */
Object *spawn_formation(Object *a0, const uint8_t *rec_raw, const uint8_t *a2)
{
    struct SpawnRec rec_data;
    const struct SpawnRec *a1 = &rec_data;
    uint16_t d7;
    uint16_t d6 = 0;
    uint16_t *slot;
    uint16_t d1;
    int i;

    spawnrec_load(&rec_data, rec_raw);
    d7 = a1->count;
    if (d7 == 0)
        return a0;

    while (d7-- != 0) {
        const uint8_t *a4raw = a2;
        struct SpawnDef def;
        const struct SpawnDef *a4 = &def;
        Object *a5;
        const uint8_t *e;
        uint16_t d0;

        a0 = list_insert(LIST_ENEMY);
        a5 = a0;
        for (;;) {
            spawndef_load(&def, a4raw);
            a0->type = a4->kind;
            a0->fn_update = (obj_fn)a4->update;
            a0->fn_draw = (obj_fn)a4->draw;
            a0->fn_hit = (void *)a4->hit;
            anim_header_set(a0, a4->anim);
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
            e = (const uint8_t *)GPTR(rd32(level_formations + (int16_t)(uint16_t)((a1->index - 1) * 4)));
            if ((const void *)e < (const void *)level_data_start || (const void *)e >= (const void *)level_events_a)
                return a0;
            a0->v46 = (uint8_t *)e;
            if (rd16(e) == 0xa) {
                a0->x = (int16_t)rd16(e + 2);
                a0->y = (int16_t)rd16(e + 4);
                if ((int16_t)a1->x >= 100)
                    a0->x = (int16_t)(a0->x + (int16_t)d6);
                e += 6;
            }
            set_cursor(a0, e);
            a0->v54 = (int16_t)a1->w0c;
            SET_V5E(a0, (int16_t)a1->y);
            d0 = (uint16_t)random_next();
            SET_V5E(a0, (int16_t)((V5E(a0) & 0x00ff) | ((d0 & 0xff) << 8)));
            a0->energy = (int16_t)v_0426;
            a0->kind = (int16_t)v_045e;
            a0->flag35 = (int8_t)a4->to_flag35;
            if (a4->to_flag35 == 0) {
                a0->energy = (int16_t)v_0424;
                a0->kind = (int16_t)v_045c;
            }
            a0->flag34 = (int8_t)a4->to_flag34;
            bbox_from_frame(a0);
            a0->target = a5;
            a4raw += 0x18;
            spawndef_load(&def, a4raw);
            if (a4->kind != 0) {
                Object *a6 = a0;
                Object *nxt;

                a0 = obj_free_pop();
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
    slot = v_043c;
    for (i = 0; i < 8; i++, slot += 2) {
        if (slot[0] == d1) {
            slot[1] = (uint16_t)(slot[1] + a1->count);
            return a0;
        }
    }
    slot = v_043c;
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

/* orig $E4E, $0034EA: the box of object o (hit[]) from its picture: the four words before the picture are the box's offset and size,
 * the two words of the picture the hot spot */
void bbox_from_frame(Object *o)
{
    const uint8_t *f = (const uint8_t *)o->gfx;
    int16_t x0 = (int16_t)((int16_t)rd16(f - 8) + o->x - (int16_t)rd16(f));
    int16_t y0 = (int16_t)((int16_t)rd16(f - 6) + o->y - (int16_t)rd16(f + 2));

    o->hit[0] = x0;
    o->hit[1] = y0;
    o->hit[2] = (int16_t)(x0 + (int16_t)rd16(f - 4) - 1);
    o->hit[3] = (int16_t)(y0 + (int16_t)rd16(f - 2) - 1);
}

/* orig $F32, $0037B2: the update of the marker that ends a group (type $BC, its $56 the marker that starts it, type $B8): the box of the
 * start marker becomes the union of the boxes of the objects between them that are drawn (with none the box is moved off the screen);
 * with no object between them the two markers go back to the free list */
void group_box_update(Object *a0)
{
    Object *a1 = a0->target;
    Object *a2 = OBJ_FWD(a1);
    int16_t d0, d1, d2, d3;

    if (a2 == a0) {
        free_object(a0);
        free_object(a1);
        return;
    }
    while (a2->fn_draw == (obj_fn)obj_nop) {
        a2 = OBJ_FWD(a2);
        if (a2 == a0) {
            a1->hit[0] = 0x3e8;
            a1->hit[2] = 0x3e8;
            return;
        }
    }
    d0 = a2->hit[0];
    d1 = a2->hit[1];
    d2 = a2->hit[2];
    d3 = a2->hit[3];
    for (;;) {
        a2 = OBJ_FWD(a2);
        if (a2 == a0)
            break;
        if (a2->fn_draw == (obj_fn)obj_nop)
            continue;
        if (a2->hit[0] < d0)
            d0 = a2->hit[0];
        if (a2->hit[1] < d1)
            d1 = a2->hit[1];
        if (a2->hit[2] > d2)
            d2 = a2->hit[2];
        if (a2->hit[3] > d3)
            d3 = a2->hit[3];
    }
    a1->hit[0] = d0;
    a1->hit[1] = d1;
    a1->hit[2] = d2;
    a1->hit[3] = d3;
}

/* orig $EC0, $0031EC: an effect (list of effects) at (x, y) that plays the animation a2 (header: picture long, delay, script) and is
 * drawn by a3; it asks for the sound $85 when the animation is $270E, else $83 */
void spawn_effect(int16_t x, int16_t y, const uint8_t *a2, ObjFn a3)
{
    Object *a0;

    sfx_cmd2 = (a2 == anim_270e) ? 0x85 : 0x83;
    a0 = list_insert(LIST_EFFECT);
    a0->type = 0xc;
    a0->fn_update = (obj_fn)animate_only;
    a0->fn_draw = (obj_fn)a3;
    a0->x = x;
    a0->y = y;
    anim_header_set(a0, a2);
}

/* orig $EA2, $003518 (reached by a jump; goes on in $352E): remember the record of a spawn */
void note_spawn_record(const uint8_t *a1, uint16_t d2)
{
    player_x_saved = rd16(a1 + 8);
    player_y_saved = rd16(a1 + 10);
    scroll_y = rd16(a1);
    player_money_saved = player_money;
    latch_slots(d2);
}

/* orig $F4A, $00352E: copy the type and the score ($42) of the object of each of the 7 weapon slots to v_0caa unless v_0ca2 is set
 * (d2 is what an empty slot gets after the first one that was not: the caller's register) */
void latch_slots(uint16_t d2)
{
    const Slot *s = weapon_slots;
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
 * position (scroll_pos) and the old one calls the level's routine ($54E10 for the first list, $54E18 for the second) */
void scroll_events(void)
{
    const uint8_t *a1 = level_events_a;
    int16_t d0 = (int16_t)v_0cd2;
    int16_t d1 = (int16_t)scroll_pos;

    for (;; a1 += 14) {
        int16_t d2 = (int16_t)rd16(a1);

        if (d2 < 0)
            break;
        if (d0 > d2 && d1 <= d2) {
            level_event(rd16(a1 + 2), a1);
            d0 = (int16_t)v_0cd2;
            d1 = (int16_t)scroll_pos;
        }
    }
    if (d1 >= d0)
        return;
    v_0cd2 = (uint16_t)d1;
    a1 = level_events_b;
    d0 = (int16_t)v_0cd4;
    d1 = (int16_t)scroll_pos;
    for (;; a1 += 14) {
        int16_t d2 = (int16_t)rd16(a1);

        if (d2 < 0)
            break;
        if (d0 > d2 && d1 <= d2) {
            level_hook(rd16(a1 + 2), a1, (uint16_t)d2);
            d0 = (int16_t)v_0cd4;
            d1 = (int16_t)scroll_pos;
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
#ifdef GAME_SDL
    v_0a6a = guest_a6a(key_pressed);
#else
    v_0a6a = key_pressed;
#endif
    key_pressed = 0;
    run_updates(LIST_HEAD(LIST_MISC));
    run_updates(LIST_HEAD(LIST_EFFECT));
    run_updates(LIST_HEAD(LIST_FRONT));
    run_draws(LIST_HEAD(LIST_PLAYER));
    run_draws(LIST_HEAD(LIST_MISC));
    run_draws(LIST_HEAD(LIST_ENEMY));
    run_draws(LIST_HEAD(LIST_EFFECT));
    run_draws(LIST_HEAD(LIST_FRONT));
    slot = v_043c;
    for (i = 0; i < 8; i++, slot += 2) {
        Object *a1;

        if (slot[0] == 0)
            continue;
        for (a1 = LIST_HEAD(LIST_ENEMY);; a1 = OBJ_FWD(a1)) {
            if (a1->type == 0) {
                slot[0] = 0;
                break;
            }
            if ((uint16_t)a1->kind == slot[0])
                break;
        }
    }
}

/* orig $00367E: a hit routine (a0 the shooter, a1 the object hit, d0 the damage): the object a1 breaks up into the debris of kind
 * a1->v30 and goes dead */
void spawn_debris(Object *a0_unused, Object *a1, uint16_t d0_unused)
{
    Object *a0 = list_insert(LIST_EFFECT);
    const struct SpawnKind *k = &spawn_kinds[(uint16_t)a1->v30];

    a0->v30 = a1->v30;
    a0->type = k->kind;
    sfx_cmd = 0x11;
    a0->fn_update = (obj_fn)pickup_update;
    a0->fn_draw = (obj_fn)draw_bob_masked;
    a0->x = a1->x;
    a0->y = a1->y;
    anim_header_set(a0, (const uint8_t *)k->anim);
    a0->v28 = 7;
    kill_object(a1);
}

/* ---- entries the recorded run did not reach ---- */

/* orig $E5A, $003252: spawn the formation at $326C for record a1, then remember its word $54 in $30 */
Object *spawn_wave_326c(Object *a0, const uint8_t *a1)
{
    a0 = spawn_formation(a0, a1, spawn_def_326c);
    a0->v30 = a0->v54;
    a0->v54 = 3;
    a0->kind = 0;
    return a0;
}

/* the end of $345E and $3480: a marker object that sits on the map cell under the record's position */
static Object *marker_tail(Object *a0, uint16_t d0, const uint8_t *a1, void (*a2)(void))
{
    uint16_t x, y;

    a0->type = d0;
    a0->fn_update = (obj_fn)a2;
    a0->fn_draw = (obj_fn)obj_nop;
    SET_V5E(a0, 0);
    x = (uint16_t)(rd16(a1 + 8) - 8);
    a0->x = (int16_t)x;
    y = (uint16_t)(rd16(a1 + 10) - 8);
    a0->y = (int16_t)y;
    a0->v46 = level_init_src + (uint16_t)((x >> 3) & 0xfe) + (int16_t)(uint16_t)((uint32_t)(y >> 4) * 0x28);
    a0->v28 = 0;
    a0->v30 = 0;
    return a0;
}

/* orig $F56, $00345E: a marker in the list of enemies */
Object *spawn_marker_enemy(uint16_t d0, const uint8_t *a1, void (*a2)(void))
{
    return marker_tail(list_insert(LIST_ENEMY), d0, a1, a2);
}

/* orig $F50, $003480: a marker in the list at the front */
Object *spawn_marker_front(uint16_t d0, const uint8_t *a1, void (*a2)(void))
{
    return marker_tail(list_insert(LIST_FRONT), d0, a1, a2);
}

/* orig $EFC, $003912: update of a shot of an enemy: animation, moves by steps (v54) along one of 8 directions (v2a), gone when it leaves the
 * screen; when it meets the player the player is hit (unless v_0cc8) and the shot goes */
void enemy_shot_update(Object *a0)
{
    uint16_t d2;
    uint32_t d0, d1;
    int32_t m;

    anim_step(a0);
    d2 = (uint16_t)a0->v2a;
    d0 = ((uint32_t)(uint16_t)a0->x << 16) | a0->x_frac;
    d1 = ((uint32_t)(uint16_t)a0->y << 16) | a0->y_frac;
    m = (int32_t)enemy_shot_dx[d2] * (int16_t)a0->v54;
    d0 += (uint32_t)m << 2;
    m = (int32_t)enemy_shot_dy[d2] * (int16_t)a0->v54;
    d1 += (uint32_t)m << 2;
    a0->x = (int16_t)(d0 >> 16);
    a0->x_frac = (uint16_t)d0;
    a0->y_frac = (uint16_t)d1;
    d0 >>= 16;
    d1 = (uint16_t)((d1 >> 16) + scroll_speed);
    a0->y = (int16_t)d1;
    if (d0 < 0x140 && d1 < 0xc0) {
        bbox_from_frame(a0);
        if (!player_hits(a0))
            return;
        if (v_0cc8 == 0) {
            player_damage(a0, 4);
            PLAYER->fn_draw = (obj_fn)draw_bob_or_first;
        }
    }
    kill_object(a0);
}

/* orig $00318C: a shot of an enemy at (x, y), moving by steps in direction angle, drawn from gfx */
Object *spawn_enemy_shot(uint16_t x, uint16_t y, uint16_t angle, uint16_t steps, const void *gfx)
{
    Object *a0 = list_insert(LIST_EFFECT);

    a0->type = 0x14;
    a0->fn_update = (obj_fn)enemy_shot_update;
    a0->fn_draw = (obj_fn)draw_bob_masked;
    a0->gfx = gfx;
    a0->anim_delay = 0;
    a0->x = (int16_t)x;
    a0->y = (int16_t)y;
    a0->v2a = (int16_t)angle;
    a0->v54 = (int16_t)steps;
    a0->hit[0] = 0x3e8;
    a0->hit[2] = 0x3e8;
    return a0;
}
