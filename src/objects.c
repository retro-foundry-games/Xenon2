#include "objects.h"
#include "shop.h"
#include "game_all.h"

/* the path script view of an object (ObjMove) */

/* the scratch fields of Object, by the kind that uses them */

#ifdef GAME_SDL
uint8_t guest_a6a(uint8_t a01);
#endif

/* ---- helpers ---- */

/* the object from the free list, put at the head of list i (the code after each call of $2418 that does this) */
static Object *list_insert(int list)
{
    Object *obj = obj_free_pop();
    Object *old = LIST_HEAD(list);

    LIST_HEAD(list) = obj;
    OBJ_FWD(obj) = old;
    OBJ_BACK(obj) = LIST_SENTINEL(list);
    OBJ_BACK(old) = obj;
    return obj;
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
    r->arg = rd16(p + 2);
    r->count = rd16(p + 4);
    r->index = rd16(p + 6);
    r->x = (int16_t)rd16(p + 8);
    r->y = (int16_t)rd16(p + 10);
    r->steps = rd16(p + 12);
}

static void spawndef_load(struct SpawnDef *d, const uint8_t *p)
{
    d->kind = rd16(p);
    d->score = rd16(p + 2);
    d->update = (void (*)(void))GFN(rd32(p + 4));
    d->draw = (void (*)(void))GFN(rd32(p + 8));
    d->hit = (void (*)(void))GFN(rd32(p + 12));
    d->anim = (const uint8_t *)GPTR(rd32(p + 0x10));
    d->to_big = p[0x16];
    d->to_is_part = p[0x17];
}

/* an object that does nothing any more */
static void kill_object(Object *o)
{
    o->type = 4;
    o->fn_update = (obj_fn)free_object;
    o->fn_draw = (obj_fn)obj_nop;
    o->fn_hit = (void *)obj_nop;
    o->is_part = 0;
    o->persists = 0;
}

/* ---- animation scripts ---- */

/* orig $E9C, $002C5A: advance the animation of object o: count the delay down, then take the next step of the script (a picture and
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
void anim_op_jump(Object *o, const uint8_t *script)
{
    anim_header_set(o, (const uint8_t *)GPTR(rd32(script)));
}

/* orig $E30, $002C92: script step "end": the object (and the pieces chained to it, if it is one of a group) goes dead */
void anim_op_free(Object *o, const uint8_t *script)
{
    Object *p;
    Object *next;

    if (o->is_part == 0) {
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
 * tile_gfx, else a full one */
static const uint8_t *tile_mask_ptr(uint16_t w)
{
    if (w == 0)
        return xenon2.mask_empty;
    if ((int16_t)w < 0) {
        const uint8_t *m = xenon2.tile_gfx + ((uint32_t)(w & 0x7fff) << 4);

#ifdef GAME_SDL
        if (GADDR(m) + 0xa0 > 0x80000) {   /* the original's memory beyond 512 KB (the lockstep gives it) */
            m = guest_high_ptr(GADDR(m));
            return m ? m : xenon2.mask_empty;
        }
#endif
        return m;
    }
    return xenon2.mask_full;
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
 * Inlined: $2D7A (the three pointers of a row of cells, results in mask0, mask1, mask2). */
void map_tile_mask(uint16_t x, uint16_t y, uint16_t rows)
{
    const uint8_t *cell = xenon2.level_init_src + (int16_t)xenon2.map_row_offset;
    const uint8_t *mask0, *mask1, *mask2;
    uint16_t offset, line;
    uint16_t *out = xenon2.tile_mask_buf;

    y = (uint16_t)(y + xenon2.scroll_fine);
    offset = y & 0xf;
    line = offset;
    offset = (uint16_t)(offset + offset);
    offset = (uint16_t)(offset + offset);
    offset = (uint16_t)(offset + line);
    offset = (uint16_t)(offset + offset);
    cell += (int16_t)(uint16_t)((uint32_t)(y >> 4) * 0x28);
    cell += (uint16_t)((x >> 3) & 0xfe);
    mask0 = tile_mask_ptr_at(cell);
    mask1 = tile_mask_ptr_at(cell + 2);
    mask2 = tile_mask_ptr_at(cell + 4);
    cell += 40;
    do {
        *out++ = rd16(mask0 + offset);
        *out++ = rd16(mask1 + offset);
        *out++ = rd16(mask2 + offset);
        offset = (uint16_t)(offset + 10);
        if (offset == 0xa0) {
            mask0 = tile_mask_ptr_at(cell);
            mask1 = tile_mask_ptr_at(cell + 2);
            mask2 = tile_mask_ptr_at(cell + 4);
            cell += 40;
            offset = 0;
        }
    } while ((uint16_t)--rows != 0xffff);
}

/* orig $002F66: like map_tile_mask with two words a row (the masks of the cells at x and at x + 16), written as the blitter reads them
 * (inlined: $2FC0, the two pointers of a row of cells in mask0 and mask1) */
uint16_t tile_mask2_words;   /* the words of tile_mask_buf the last map_tile_mask2 wrote (as the blitter reads them) */

void map_tile_mask2(uint16_t x, uint16_t y, uint16_t rows)
{
    const uint8_t *cell = xenon2.level_init_src + (int16_t)xenon2.map_row_offset;
    const uint8_t *mask0, *mask1;
    uint16_t offset, line;
    uint8_t *out = (uint8_t *)xenon2.tile_mask_buf;

    tile_mask2_words = (uint16_t)((rows + 1) * 2);
    y = (uint16_t)(y + xenon2.scroll_fine);
    offset = y & 0xf;
    line = offset;
    offset = (uint16_t)(offset + offset);
    offset = (uint16_t)(offset + offset);
    offset = (uint16_t)(offset + line);
    offset = (uint16_t)(offset + offset);
    cell += (int16_t)(uint16_t)((uint32_t)(y >> 4) * 0x28);
    cell += (uint16_t)((x >> 3) & 0xfe);
    mask0 = tile_mask_ptr_at(cell);
    mask1 = tile_mask_ptr_at(cell + 2);
    cell += 40;
    do {
        wr16(out, rd16(mask0 + offset));
        wr16(out + 2, rd16(mask1 + offset));
        out += 4;
        offset = (uint16_t)(offset + 10);
        if (offset == 0xa0) {
            mask0 = tile_mask_ptr_at(cell);
            mask1 = tile_mask_ptr_at(cell + 2);
            cell += 40;
            offset = 0;
        }
    } while ((uint16_t)--rows != 0xffff);
}

/* orig $00300A: does the 24-row sprite mask of the player's position meet a solid tile?  (the original returns the condition
 * codes: not equal = hit) */
uint16_t player_tile_hit(Object *o)
{
    uint16_t x = (uint16_t)(o->x - 14);
    uint16_t y = (uint16_t)(o->y - 14);
    const uint16_t *m = xenon2.hit_mask_data + 75 * (x & 0xf);
    const uint16_t *b = xenon2.tile_mask_buf;
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

/* orig $00306E: the first object of the list from obj whose box meets the box; objects of kind $B8 are skipped (and their chain
 * is stepped over).  Returns the object, or the object that ends the list (kind 0) when none does. */
Object *scan_boxes(Object *obj, int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    while (obj->type != 0) {
        if (boxes_overlap(x0, y0, x1, y1, obj->hit)) {
            if (obj->type != 0xb8)
                return obj;
        } else if (obj->type == 0xb8) {
            obj = obj->chain;
        }
        obj = OBJ_FWD(obj);
    }
    return obj;
}

/* orig $003052: the enemy that the box of object o meets (the original returns not-equal), or 0 when none
 * does and the level blob has nothing either */
Object *collide_enemies(Object *o)
{
    int16_t x0 = o->hit[0], y0 = o->hit[1], x1 = o->hit[2], y1 = o->hit[3];
    Object *enemy;

    xenon2.hit_box[0] = (uint16_t)x0;
    xenon2.hit_box[1] = (uint16_t)y0;
    xenon2.hit_box[2] = (uint16_t)x1;
    xenon2.hit_box[3] = (uint16_t)y1;
    enemy = scan_boxes(LIST_HEAD(LIST_ENEMY), x0, y0, x1, y1);
    if (enemy->type != 0)
        return enemy;
    return level_collision(x0, y0, x1, y1, enemy);
}

/* orig $0030DE: the enemy whose box holds the point (x, y), or 0 when none does and the level blob has
 * nothing either */
Object *point_in_enemy(int16_t x, int16_t y)
{
    Object *enemy = LIST_HEAD(LIST_ENEMY);

    xenon2.hit_box[0] = (uint16_t)x;
    xenon2.hit_box[2] = (uint16_t)x;
    xenon2.hit_box[1] = (uint16_t)y;
    xenon2.hit_box[3] = (uint16_t)y;
    while (enemy->type != 0) {
        if (x < enemy->hit[0] || x > enemy->hit[2] || y < enemy->hit[1] || y > enemy->hit[3]) {
            if (enemy->type == 0xb8)
                enemy = enemy->chain;
        } else if (enemy->type != 0xb8) {
            return enemy;
        }
        enemy = OBJ_FWD(enemy);
    }
    return level_collision(x, y, x, y, enemy);
}

/* orig $003156: the direction (0 to 7) of (dx, dy), both not negative */
static uint16_t direction_octant(int16_t dx, int16_t dy)
{
    uint32_t ratio;

    if (dx == 0)
        return 4;
    if (dy == 0)
        return 2;
    if (dx == dy)
        return 3;
    if (dx > dy) {
        ratio = ((uint32_t)((uint16_t)dy >> 1)) << 16;
        ratio = ratio / (uint16_t)dx;
        if (ratio > 0xffff)
            ratio = 0;
        return (int16_t)ratio >= 0x3504 ? 3 : 2;
    }
    ratio = ((uint32_t)((uint16_t)dx >> 1)) << 16;
    ratio = ratio / (uint16_t)dy;
    if (ratio > 0xffff)
        ratio = 0;
    return (int16_t)ratio >= 0x3504 ? 3 : 4;
}

/* orig $003144: the direction of (dx, dy) with dy not negative */
static uint16_t direction_half(int16_t dx, int16_t dy)
{
    uint16_t dir;

    if (dx >= 0)
        return direction_octant(dx, dy);
    dir = direction_octant((int16_t)(0 - (uint16_t)dx), dy);
    return (uint16_t)(8 - dir) & 7;
}

/* orig $E7E, $003132: the direction (0 to 7) of (dx, dy) */
uint16_t direction8(int16_t dx, int16_t dy)
{
    uint16_t dir;

    if (dy >= 0)
        return direction_half(dx, dy);
    dir = direction_half(dx, (int16_t)(0 - (uint16_t)dy));
    return (uint16_t)(4 - dir) & 7;
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

/* orig $EC6, $003286: the object after obj in its list, or, for a part of a group (flag $34), the first one after the group's other
 * parts (parts of one owner follow each other) */
Object *owner_chain_next(Object *obj)
{
    Object *next = OBJ_FWD(obj);

    if (obj->is_part != 0) {
        next = obj->target;
        for (;;) {
            next = OBJ_FWD(next);
            if (next->type == 0 || next->is_part == 0 || next->target != obj)
                break;
        }
    }
    return next;
}

/* orig $E60, $0032AA: spawn rec->count copies of the formation (a chain of objects, one for each definition), the copies
 * rec->x apart; at the end the kind is recorded in the table at $43C.  Returns the last object made (obj itself when none). */
Object *spawn_formation(Object *obj, const uint8_t *rec_raw, const uint8_t *formation)
{
    struct SpawnRec rec_data;
    const struct SpawnRec *rec = &rec_data;
    uint16_t remaining;
    uint16_t offset = 0;
    uint16_t *slot;
    uint16_t kind;
    int i;

    spawnrec_load(&rec_data, rec_raw);
    remaining = rec->count;
    if (remaining == 0)
        return obj;

    while (remaining-- != 0) {
        const uint8_t *def_raw = formation;
        struct SpawnDef def;
        const struct SpawnDef *def_ptr = &def;
        Object *leader;
        const uint8_t *e;
        uint16_t rnd, spacing;

        obj = list_insert(LIST_ENEMY);
        leader = obj;
        for (;;) {
            spawndef_load(&def, def_raw);
            obj->type = def_ptr->kind;
            obj->fn_update = (obj_fn)def_ptr->update;
            obj->fn_draw = (obj_fn)def_ptr->draw;
            obj->fn_hit = (void *)def_ptr->hit;
            anim_header_set(obj, def_ptr->anim);
            obj->x = (int16_t)offset;
            obj->x_frac = 0;
            obj->y = 0;
            obj->y_frac = 0;
            obj->path.angle_hi = 0;
            obj->path.angle_lo = 0;
            obj->score = (int16_t)def_ptr->score;
            obj->path.wait = 0;
            if ((int16_t)rec->x < 100) {
                offset = (uint16_t)(0 - offset);
                obj->path.wait = (int16_t)offset;
                offset = (uint16_t)(0 - offset);
                obj->x = 0;
            }
            if (leader != obj)
                obj->path.wait = (int16_t)(obj->path.wait - (int16_t)def_ptr->score);
            e = (const uint8_t *)GPTR(rd32(xenon2.level_formations + (int16_t)(uint16_t)((rec->index - 1) * 4)));
            if ((const void *)e < (const void *)xenon2.level_data_start || (const void *)e >= (const void *)xenon2.level_events_a)
                return obj;
            obj->script = (uint8_t *)e;
            if (rd16(e) == 0xa) {
                obj->x = (int16_t)rd16(e + 2);
                obj->y = (int16_t)rd16(e + 4);
                if ((int16_t)rec->x >= 100)
                    obj->x = (int16_t)(obj->x + (int16_t)offset);
                e += 6;
            }
            set_cursor(obj, e);
            obj->steps = (int16_t)rec->steps;
            SET_V5E(obj, (int16_t)rec->y);
            rnd = (uint16_t)random_next();
            SET_V5E(obj, (int16_t)((V5E(obj) & 0x00ff) | ((rnd & 0xff) << 8)));
            obj->energy = (int16_t)xenon2.difficulty_x2;
            obj->kind = (int16_t)xenon2.pickup_kind_b;
            obj->big = (int8_t)def_ptr->to_big;
            if (def_ptr->to_big == 0) {
                obj->energy = (int16_t)xenon2.difficulty_x1;
                obj->kind = (int16_t)xenon2.pickup_kind_a;
            }
            obj->is_part = (int8_t)def_ptr->to_is_part;
            bbox_from_frame(obj);
            obj->target = leader;
            def_raw += 0x18;
            spawndef_load(&def, def_raw);
            if (def_ptr->kind != 0) {
                Object *prev = obj;
                Object *nxt;

                obj = obj_free_pop();
                prev->chain = obj;
                nxt = OBJ_FWD(prev);
                OBJ_FWD(prev) = obj;
                OBJ_FWD(obj) = nxt;
                OBJ_BACK(obj) = prev;
                OBJ_BACK(nxt) = obj;
                continue;
            }
            break;
        }
        spacing = rec->x;
        while ((int16_t)spacing >= 100)
            spacing = (uint16_t)(spacing - 100);
        offset = (uint16_t)(offset + spacing);
        obj->chain = 0;
    }
    kind = xenon2.pickup_kind_b;
    if (obj->big == 0)
        kind = xenon2.pickup_kind_a;
    slot = xenon2.pickup_groups;
    for (i = 0; i < 8; i++, slot += 2) {
        if (slot[0] == kind) {
            slot[1] = (uint16_t)(slot[1] + rec->count);
            return obj;
        }
    }
    slot = xenon2.pickup_groups;
    for (i = 0; i < 8; i++, slot += 2) {
        if (slot[0] == 0) {
            slot[0] = kind;
            slot[1] = rec->count;
            xenon2.pickup_cycle = (uint16_t)(xenon2.pickup_cycle + 1);
            return obj;
        }
    }
    return obj;
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
void group_box_update(Object *end)
{
    Object *start = end->target;
    Object *obj = OBJ_FWD(start);
    int16_t x0, y0, x1, y1;

    if (obj == end) {
        free_object(end);
        free_object(start);
        return;
    }
    while (obj->fn_draw == (obj_fn)obj_nop) {
        obj = OBJ_FWD(obj);
        if (obj == end) {
            start->hit[0] = 0x3e8;
            start->hit[2] = 0x3e8;
            return;
        }
    }
    x0 = obj->hit[0];
    y0 = obj->hit[1];
    x1 = obj->hit[2];
    y1 = obj->hit[3];
    for (;;) {
        obj = OBJ_FWD(obj);
        if (obj == end)
            break;
        if (obj->fn_draw == (obj_fn)obj_nop)
            continue;
        if (obj->hit[0] < x0)
            x0 = obj->hit[0];
        if (obj->hit[1] < y0)
            y0 = obj->hit[1];
        if (obj->hit[2] > x1)
            x1 = obj->hit[2];
        if (obj->hit[3] > y1)
            y1 = obj->hit[3];
    }
    start->hit[0] = x0;
    start->hit[1] = y0;
    start->hit[2] = x1;
    start->hit[3] = y1;
}

/* orig $EC0, $0031EC: an effect (list of effects) at (x, y) that plays the animation anim (header: picture long, delay, script) and is
 * drawn by draw; it asks for the sound $85 when the animation is $270E, else $83 */
void spawn_effect(int16_t x, int16_t y, const uint8_t *anim, ObjFn draw)
{
    Object *fx;

    xenon2.sfx_cmd2 = (anim == xenon2.anim_explosion) ? 0x85 : 0x83;
    fx = list_insert(LIST_EFFECT);
    fx->type = 0xc;
    fx->fn_update = (obj_fn)animate_only;
    fx->fn_draw = (obj_fn)draw;
    fx->x = x;
    fx->y = y;
    anim_header_set(fx, anim);
}

/* orig $EA2, $003518 (reached by a jump; goes on in $352E): remember the record of a spawn */
void note_spawn_record(const uint8_t *rec, uint16_t score)
{
    xenon2.player_x_saved = rd16(rec + 8);
    xenon2.player_y_saved = rd16(rec + 10);
    xenon2.scroll_y = rd16(rec);
    xenon2.player_money_saved = xenon2.player_money;
    latch_slots(score);
}

/* orig $F4A, $00352E: copy the type and the score ($42) of the object of each of the 7 weapon slots to slot_snapshot unless nashwan_timer is set
 * (score is what an empty slot gets after the first one that was not) */
void latch_slots(uint16_t score)
{
    const Slot *s = xenon2.weapon_slots;
    uint16_t *out = xenon2.slot_snapshot;
    int i;

    if (xenon2.nashwan_timer != 0)
        return;
    for (i = 0; i < 7; i++, s++) {
        *out++ = s->type;
        if (s->type != 0)
            score = (uint16_t)s->obj->score;
        *out++ = score;
    }
}

/* orig $003554: fire the level's scroll events: each record of the two lists whose scroll value lies between the new scroll
 * position (scroll_pos) and the old one calls the level's routine ($54E10 for the first list, $54E18 for the second) */
void scroll_events(void)
{
    const uint8_t *rec = xenon2.level_events_a;
    int16_t old_pos = (int16_t)xenon2.next_event_a;
    int16_t new_pos = (int16_t)xenon2.scroll_pos;

    for (;; rec += 14) {
        int16_t rec_scroll = (int16_t)rd16(rec);

        if (rec_scroll < 0)
            break;
        if (old_pos > rec_scroll && new_pos <= rec_scroll) {
            level_event(rd16(rec + 2), rec);
            old_pos = (int16_t)xenon2.next_event_a;
            new_pos = (int16_t)xenon2.scroll_pos;
        }
    }
    if (new_pos >= old_pos)
        return;
    xenon2.next_event_a = (uint16_t)new_pos;
    rec = xenon2.level_events_b;
    old_pos = (int16_t)xenon2.next_event_b;
    new_pos = (int16_t)xenon2.scroll_pos;
    for (;; rec += 14) {
        int16_t rec_scroll = (int16_t)rd16(rec);

        if (rec_scroll < 0)
            break;
        if (old_pos > rec_scroll && new_pos <= rec_scroll) {
            level_hook(rd16(rec + 2), rec, (uint16_t)rec_scroll);
            old_pos = (int16_t)xenon2.next_event_b;
            new_pos = (int16_t)xenon2.scroll_pos;
        }
    }
    if (new_pos >= old_pos)
        return;
    xenon2.next_event_b = (uint16_t)new_pos;
}

/* orig $003656: call the update routine of each object of the list from obj */
void run_updates(Object *obj)
{
    while (obj->type != 0) {
        Object *volatile next = OBJ_FWD(obj);   /* kept on the stack: the handlers do not preserve registers */

        ((ObjFn)obj->fn_update)(obj);
        obj = next;
    }
}

/* orig $00366A: call the draw routine of each object of the list from obj */
void run_draws(Object *obj)
{
    while (obj->type != 0) {
        Object *volatile next = OBJ_FWD(obj);   /* kept on the stack: the handlers do not preserve registers */

        ((ObjFn)obj->fn_draw)(obj);
        obj = next;
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
    xenon2.fire_press = guest_a6a(xenon2.key_pressed);
#else
    xenon2.fire_press = xenon2.key_pressed;
#endif
    xenon2.key_pressed = 0;
    run_updates(LIST_HEAD(LIST_MISC));
    run_updates(LIST_HEAD(LIST_EFFECT));
    run_updates(LIST_HEAD(LIST_FRONT));
    run_draws(LIST_HEAD(LIST_PLAYER));
    run_draws(LIST_HEAD(LIST_MISC));
    run_draws(LIST_HEAD(LIST_ENEMY));
    run_draws(LIST_HEAD(LIST_EFFECT));
    run_draws(LIST_HEAD(LIST_FRONT));
    slot = xenon2.pickup_groups;
    for (i = 0; i < 8; i++, slot += 2) {
        Object *enemy;

        if (slot[0] == 0)
            continue;
        for (enemy = LIST_HEAD(LIST_ENEMY);; enemy = OBJ_FWD(enemy)) {
            if (enemy->type == 0) {
                slot[0] = 0;
                break;
            }
            if ((uint16_t)enemy->kind == slot[0])
                break;
        }
    }
}

/* orig $00367E: a hit routine (the shooter, the object hit, the damage): the object hit breaks up into the debris of kind
 * target->v30 and goes dead */
void spawn_debris(Object *shooter_unused, Object *target, uint16_t damage_unused)
{
    Object *debris = list_insert(LIST_EFFECT);
    const struct SpawnKind *k = &xenon2.spawn_kinds[(uint16_t)target->debris.kind];

    debris->debris.kind = target->debris.kind;
    debris->type = k->kind;
    xenon2.sfx_cmd = 0x11;
    debris->fn_update = (obj_fn)pickup_update;
    debris->fn_draw = (obj_fn)draw_bob_masked;
    debris->x = target->x;
    debris->y = target->y;
    anim_header_set(debris, (const uint8_t *)k->anim);
    debris->pickup.mode = 7;
    kill_object(target);
}

/* ---- entries the recorded run did not reach ---- */

/* orig $E5A, $003252: spawn the formation at $326C for record rec, then remember its word $54 in $30 */
Object *spawn_wave_326c(Object *obj, const uint8_t *rec)
{
    obj = spawn_formation(obj, rec, xenon2.spawn_def_326c);
    obj->marker.launches = obj->steps;
    obj->steps = 3;
    obj->kind = 0;
    return obj;
}

/* the end of $345E and $3480: a marker object that sits on the map cell under the record's position */
static Object *marker_tail(Object *marker, uint16_t type, const uint8_t *rec, void (*update)(void))
{
    uint16_t x, y;

    marker->type = type;
    marker->fn_update = (obj_fn)update;
    marker->fn_draw = (obj_fn)obj_nop;
    SET_V5E(marker, 0);
    x = (uint16_t)(rd16(rec + 8) - 8);
    marker->x = (int16_t)x;
    y = (uint16_t)(rd16(rec + 10) - 8);
    marker->y = (int16_t)y;
    marker->script = xenon2.level_init_src + (uint16_t)((x >> 3) & 0xfe) + (int16_t)(uint16_t)((uint32_t)(y >> 4) * 0x28);
    marker->marker.step = 0;
    marker->marker.launches = 0;
    return marker;
}

/* orig $F56, $00345E: a marker in the list of enemies */
Object *spawn_marker_enemy(uint16_t type, const uint8_t *rec, void (*update)(void))
{
    return marker_tail(list_insert(LIST_ENEMY), type, rec, update);
}

/* orig $F50, $003480: a marker in the list at the front */
Object *spawn_marker_front(uint16_t type, const uint8_t *rec, void (*update)(void))
{
    return marker_tail(list_insert(LIST_FRONT), type, rec, update);
}

/* orig $EFC, $003912: update of a shot of an enemy: animation, moves by steps (steps) along one of 8 directions (v2a), gone when it leaves the
 * screen; when it meets the player the player is hit (unless shield_timer) and the shot goes */
void enemy_shot_update(Object *shot)
{
    uint16_t dir;
    uint32_t x_fixed, y_fixed;   /* 16.16 */
    uint32_t x, y;
    int32_t step;

    anim_step(shot);
    dir = (uint16_t)shot->enemy_shot.dir;
    x_fixed = ((uint32_t)(uint16_t)shot->x << 16) | shot->x_frac;
    y_fixed = ((uint32_t)(uint16_t)shot->y << 16) | shot->y_frac;
    step = (int32_t)xenon2.enemy_shot_dx[dir] * (int16_t)shot->steps;
    x_fixed += (uint32_t)step << 2;
    step = (int32_t)xenon2.enemy_shot_dy[dir] * (int16_t)shot->steps;
    y_fixed += (uint32_t)step << 2;
    shot->x = (uint16_t)(x_fixed >> 16);
    shot->x_frac = (uint16_t)x_fixed;
    shot->y_frac = (uint16_t)y_fixed;
    x = x_fixed >> 16;
    y = (uint16_t)((y_fixed >> 16) + xenon2.scroll_speed);
    shot->y = (int16_t)y;
    if (x < 0x140 && y < 0xc0) {
        bbox_from_frame(shot);
        if (!player_hits(shot))
            return;
        if (xenon2.shield_timer == 0) {
            player_damage(shot, 4);
            PLAYER->fn_draw = (obj_fn)draw_bob_or_first;
        }
    }
    kill_object(shot);
}

/* orig $00318C: a shot of an enemy at (x, y), moving by steps in direction angle, drawn from gfx */
Object *spawn_enemy_shot(uint16_t x, uint16_t y, uint16_t angle, uint16_t steps, const void *gfx)
{
    Object *shot = list_insert(LIST_EFFECT);

    shot->type = 0x14;
    shot->fn_update = (obj_fn)enemy_shot_update;
    shot->fn_draw = (obj_fn)draw_bob_masked;
    shot->gfx = gfx;
    shot->anim_delay = 0;
    shot->x = (int16_t)x;
    shot->y = (int16_t)y;
    shot->enemy_shot.dir = (int16_t)angle;
    shot->steps = (int16_t)steps;
    shot->hit[0] = 0x3e8;
    shot->hit[2] = 0x3e8;
    return shot;
}
