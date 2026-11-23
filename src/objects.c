#include "objects.h"
#include "shop.h"
#include "game_all.h"
#include "weapon_util.h"

/* the scratch fields of Object, by the kind that uses them */

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
static void set_cursor(Object *o, const struct PathCmd *p)
{
    o->cursor = p;
}

/* an object that does nothing any more */
void kill_object(Object *o)
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
    const struct AnimStep *step;
    int16_t t = (int16_t)(o->anim_delay - 1);

    o->anim_delay = t;
    if (t < 0) {
        o->anim_delay = 0;
        return;
    }
    if (t != 0)
        return;
    step = (const struct AnimStep *)o->anim;
    if (step->gfx == 0) {
        const struct AnimCall *call = (const struct AnimCall *)step;

        call->code(o, (const uint16_t *)(call + 1));
        return;
    }
    o->gfx = step->gfx;
    o->anim_delay = (int16_t)step->delay;
    o->anim = (const uint16_t *)(step + 1);
}

/* orig $E2A, $002C82: script step "jump": the next longword is the address of the script to go on with */
void anim_op_jump(Object *o, const uint16_t *script)
{
    anim_header_set(o, ((const struct AnimJump *)script)->script);
}

/* orig $E30, $002C92: script step "end": the object (and the pieces chained to it, if it is one of a group) goes dead */
void anim_op_free(Object *o, const uint16_t *script)
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

/* the three words of tile pointers for the next 16 pixel row of the map: 0 = empty tile mask, a negative word = a mask in
 * tile_gfx, else a full one */
static const struct MaskRow *tile_mask_ptr(uint16_t w)
{
    if (w == 0)
        return (const struct MaskRow *)xenon2.mask_empty;
    if ((int16_t)w < 0) {
        const uint8_t *m = xenon2.tile_gfx + ((uint32_t)(w & 0x7fff) << 4);

        return (const struct MaskRow *)m;
    }
    return (const struct MaskRow *)xenon2.mask_full;
}

static const struct MaskRow *tile_mask_ptr_at(const Cell *cell)
{
    return tile_mask_ptr(*cell);
}

/* orig $002D1A: fill tile_mask_buf with the tile masks (three words a row) of the map at pixel (x, y), rows + 1 rows high.
 * Inlined: $2D7A (the three pointers of a row of cells, results in mask0, mask1, mask2). */
void map_tile_mask(uint16_t x, uint16_t y, uint16_t rows)
{
    const Cell *cell = (const Cell *)(xenon2.level_init_src + (int16_t)xenon2.map_row_offset);
    const struct MaskRow *mask0, *mask1, *mask2;
    uint16_t row;
    uint16_t *out = xenon2.tile_mask_buf;

    y = (uint16_t)(y + xenon2.scroll_fine);
    row = y & 0xf;
    cell += (int16_t)(uint16_t)((uint32_t)(y >> 4) * MAP_COLS);
    cell += (x >> 4) & 0x7f;
    mask0 = tile_mask_ptr_at(cell);
    mask1 = tile_mask_ptr_at(cell + 1);
    mask2 = tile_mask_ptr_at(cell + 2);
    cell += MAP_COLS;
    do {
        *out++ = mask0[row].w[0];
        *out++ = mask1[row].w[0];
        *out++ = mask2[row].w[0];
        row++;
        if (row == 16) {
            mask0 = tile_mask_ptr_at(cell);
            mask1 = tile_mask_ptr_at(cell + 1);
            mask2 = tile_mask_ptr_at(cell + 2);
            cell += MAP_COLS;
            row = 0;
        }
    } while ((uint16_t)--rows != 0xffff);
}

/* orig $002F66: like map_tile_mask with two words a row (the masks of the cells at x and at x + 16), written as the blitter reads them
 * (inlined: $2FC0, the two pointers of a row of cells in mask0 and mask1) */
void map_tile_mask2(uint16_t x, uint16_t y, uint16_t rows)
{
    const Cell *cell = (const Cell *)(xenon2.level_init_src + (int16_t)xenon2.map_row_offset);
    const struct MaskRow *mask0, *mask1;
    uint16_t row;
    uint16_t *out = xenon2.tile_mask_buf;

    y = (uint16_t)(y + xenon2.scroll_fine);
    row = y & 0xf;
    cell += (int16_t)(uint16_t)((uint32_t)(y >> 4) * MAP_COLS);
    cell += (x >> 4) & 0x7f;
    mask0 = tile_mask_ptr_at(cell);
    mask1 = tile_mask_ptr_at(cell + 1);
    cell += MAP_COLS;
    do {
        out[0] = mask0[row].w[0];
        out[1] = mask1[row].w[0];
        out += 2;
        row++;
        if (row == 16) {
            mask0 = tile_mask_ptr_at(cell);
            mask1 = tile_mask_ptr_at(cell + 1);
            cell += MAP_COLS;
            row = 0;
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
Object *spawn_formation(Object *obj, const struct SpawnRec *rec, const struct SpawnDef *formation)
{
    uint16_t remaining;
    uint16_t offset = 0;
    uint16_t *slot;
    uint16_t kind;
    int i;

    remaining = rec->count;
    if (remaining == 0)
        return obj;

    while (remaining-- != 0) {
        const struct SpawnDef *def_ptr = formation;
        Object *leader;
        const struct PathCmd *e;
        uint16_t rnd, spacing;

        obj = list_insert(LIST_ENEMY);
        leader = obj;
        for (;;) {
            obj->type = def_ptr->kind;
            obj->fn_update = (obj_fn)def_ptr->update;
            obj->fn_draw = (obj_fn)def_ptr->draw;
            obj->fn_hit = (void *)def_ptr->hit;
            anim_header_set(obj, def_ptr->anim);
            obj->x = (int16_t)offset;
            obj->x_frac = 0;
            obj->y = 0;
            obj->y_frac = 0;
            obj->path.angle = 0;
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
            e = xenon2.level_formations[(uint16_t)(rec->index - 1)];
            if ((const void *)e < (const void *)xenon2.level_data_start || (const void *)e >= (const void *)xenon2.level_events_a)
                return obj;
            obj->script = (uint8_t *)e;
            if (e->op == 0xa) {
                obj->x = (int16_t)e->arg.pos.x;
                obj->y = (int16_t)e->arg.pos.y;
                if ((int16_t)rec->x >= 100)
                    obj->x = (int16_t)(obj->x + (int16_t)offset);
                e = (const struct PathCmd *)(&e->arg.pos + 1);
            }
            set_cursor(obj, e);
            obj->steps = (int16_t)rec->steps;
            obj->shot_rate = (uint8_t)rec->y;
            rnd = (uint16_t)random_next();
            obj->shot_count = (uint8_t)rnd;
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
            def_ptr++;
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
    const struct Pic *f = o->gfx;
    const struct PicBox *box = (const struct PicBox *)f - 1;
    int16_t x0 = (int16_t)((int16_t)box->dx + o->x - (int16_t)f->hot_x);
    int16_t y0 = (int16_t)((int16_t)box->dy + o->y - (int16_t)f->hot_y);

    o->hit[0] = x0;
    o->hit[1] = y0;
    o->hit[2] = (int16_t)(x0 + (int16_t)box->w - 1);
    o->hit[3] = (int16_t)(y0 + (int16_t)box->h - 1);
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
void spawn_effect(int16_t x, int16_t y, const uint16_t *anim, ObjFn draw)
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
void note_spawn_record(const struct SpawnRec *rec, uint16_t score)
{
    xenon2.player_x_saved = rec->x;
    xenon2.player_y_saved = rec->y;
    xenon2.scroll_y = rec->scroll;
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
    const struct SpawnRec *rec = xenon2.level_events_a;
    int16_t old_pos = (int16_t)xenon2.next_event_a;
    int16_t new_pos = (int16_t)xenon2.scroll_pos;

    for (;; rec++) {
        int16_t rec_scroll = (int16_t)rec->scroll;

        if (rec_scroll < 0)
            break;
        if (old_pos > rec_scroll && new_pos <= rec_scroll) {
            level_event(rec->arg, rec);
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
    for (;; rec++) {
        int16_t rec_scroll = (int16_t)rec->scroll;

        if (rec_scroll < 0)
            break;
        if (old_pos > rec_scroll && new_pos <= rec_scroll) {
            level_hook(rec->arg, rec, (uint16_t)rec_scroll);
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
    anim_header_set(debris, k->anim);
    debris->pickup.mode = 7;
    kill_object(target);
}

/* ---- entries the recorded run did not reach ---- */

/* orig $E5A, $003252: spawn the formation at $326C for record rec, then remember its word $54 in $30 */
Object *spawn_wave_326c(Object *obj, const struct SpawnRec *rec)
{
    static const struct SpawnDef wave[2] = {
        { .kind = 100, .update = (void (*)(void))enemy_update, .draw = (void (*)(void))draw_bob_masked, .hit = (void (*)(void))spawn_debris, .anim = xenon2.anim_wave_326c },
        { 0 } };

    obj = spawn_formation(obj, rec, wave);
    obj->marker.launches = obj->steps;
    obj->steps = 3;
    obj->kind = 0;
    return obj;
}

/* the end of $345E and $3480: a marker object that sits on the map cell under the record's position */
static Object *marker_tail(Object *marker, uint16_t type, const struct SpawnRec *rec, void (*update)(void))
{
    uint16_t x, y;

    marker->type = type;
    marker->fn_update = (obj_fn)update;
    marker->fn_draw = (obj_fn)obj_nop;
    marker->shot_count = marker->shot_rate = 0;
    x = (uint16_t)(rec->x - 8);
    marker->x = (int16_t)x;
    y = (uint16_t)(rec->y - 8);
    marker->y = (int16_t)y;
    marker->map = (Cell *)(xenon2.level_init_src + (uint16_t)((x >> 3) & 0xfe) + (int16_t)(uint16_t)((uint32_t)MAP_ROW(y >> 4)));
    marker->marker.step = 0;
    marker->marker.launches = 0;
    return marker;
}

/* orig $F56, $00345E: a marker in the list of enemies */
Object *spawn_marker_enemy(uint16_t type, const struct SpawnRec *rec, void (*update)(void))
{
    return marker_tail(list_insert(LIST_ENEMY), type, rec, update);
}

/* orig $F50, $003480: a marker in the list at the front */
Object *spawn_marker_front(uint16_t type, const struct SpawnRec *rec, void (*update)(void))
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
    dir = (uint16_t)(int16_t)(shot->enemy_shot.dir >> 16);
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
    shot->enemy_shot.dir = (shot->enemy_shot.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)angle) << 16;
    shot->steps = (int16_t)steps;
    shot->hit[0] = 0x3e8;
    shot->hit[2] = 0x3e8;
    return shot;
}

/* ---- from the weapons module: the plumbing of objects, the hits of enemies and tiles, the spawns ---- */

/* inlined: $2418 and the insertion before the sentinel of list A / B (the new object is returned) */
Object *alloc_a(Object *caller)
{
    Object *o = obj_free_pop(), *tail = LIST_A->prev;

    LIST_A->prev = o;
    o->prev = tail;
    o->next = LIST_A;
    tail->next = o;
    return o;
}

/* orig $E42, $FEE, $FF2, $1002 and $100E: take an object out of its list and put it on the free list */
void free_object(Object *obj)
{
    Object *next = obj->next, *prev = obj->prev;

    next->prev = prev;
    prev->next = next;
    obj->next = xenon2.free_obj;
    xenon2.free_obj = obj;
    obj->type = 0;
}

/* orig $005924: the hit routine of victim takes damage (the original jumps to it) */
void apply_damage(Object *shooter, Object *victim, uint16_t damage)
{
    ((hit_fn)victim->fn_hit)(shooter, victim, damage);
}

/* orig $ECC, $005920: the same with the greatest damage */
void apply_damage_max(Object *shooter, Object *victim)
{
    apply_damage(shooter, victim, 0x7f);
}

/* orig $F0E, $005550: 1 when the object's box meets the player's (the flag the caller tests; inlined: $200A) */
int player_hits(Object *obj)
{
    const Object *p = PLAYER;

    if (xenon2.dive_phase)
        return 0;
    return !(p->hit[2] < obj->hit[0] || p->hit[0] > obj->hit[2] || p->hit[3] < obj->hit[1] || p->hit[1] > obj->hit[3]);
}

/* orig $EAE, $00556C: 1 when the point (x, y) is in the player's box */
int point_in_player_box(int16_t x, int16_t y)
{
    const Object *p = PLAYER;

    if (xenon2.dive_phase)
        return 0;
    return !(p->hit[0] > x || p->hit[2] < x || p->hit[1] > y || p->hit[3] < y);
}

/* orig $F44, $0057B6: an object that only animates */
void animate_only(Object *obj)
{
    anim_step(obj);
}

/* orig $E24, $005908: an object that follows its target */
void follow_target(Object *obj)
{
    anim_step(obj);
    obj->x = obj->target->x;
    obj->y = obj->target->y;
    bbox_from_frame(obj);
}

/* orig $005D8C: scrolls the objects of a list (their y by dy) and frees those that are not fixed */
void free_chain(Object *obj, int16_t dy)
{
    while (obj->type != 0) {
        Object *next = obj->prev;

        if (obj->persists == 0)
            free_object(obj);
        else if (obj->screen_fixed == 0)
            obj->y -= dy;
        obj = next;
    }
}

/* the centre of o's picture (the 16-bit arithmetic of the original) for an explosion */
static void centre(const Object *o, int16_t *x, int16_t *y)
{
    const struct Pic *gfx = o->gfx;
    int16_t dx = (int16_t)(o->x - (int16_t)gfx->hot_x), dy = (int16_t)(o->y - (int16_t)gfx->hot_y);

    dx = (int16_t)(dx + dx);
    dy = (int16_t)(dy + dy);
    *x = (int16_t)(dx + (int16_t)gfx->width) >> 1;
    *y = (int16_t)(dy + (int16_t)gfx->height) >> 1;
}

/* orig $E1E, $00592A: hit routine of the parts of a big enemy (enemy is a part; its chain is $56/$5A) */
void hit_part(Object *shooter, Object *enemy, uint16_t damage)
{
    Object *part, *next;
    uint16_t old;

    enemy = enemy->target;
    for (part = enemy; ; part = part->chain) {
        part->fn_draw = draw_bob_or_first;
        if (part->chain == 0)
            break;
    }
    old = (uint16_t)enemy->energy;
    enemy->energy = (int16_t)(old - damage);
    if (old > damage)
        return;
    for (next = enemy->chain; next != 0; next = part->chain) {
        part = next;
        if (part->is_part >= 0) {
            int16_t x, y;

            centre(part, &x, &y);
            spawn_effect(x, y, enemy->big ? xenon2.anim_explosion_big : xenon2.anim_explosion, draw_bob_masked);
        }
        kill_object(part);
    }
    enemy_killed(shooter, enemy);
}

/* orig $E84, $0059C8: hit routine that only flashes the object */
void hit_flash(Object *shooter, Object *victim, uint16_t damage)
{
    victim->fn_draw = draw_bob_or_first;
}

/* orig $F2C, $0059DA: kills shooter (the shot) unless it is free or the player */
void hit_kill_shooter(Object *shooter, Object *victim, uint16_t damage)
{
    if (shooter->type != 0 && shooter->type != 8)
        kill_object(shooter);
}

/* orig $F26, $0059D2: flashes victim and kills shooter (the shot) */
void hit_flash_kill_shooter(Object *shooter, Object *victim, uint16_t damage)
{
    victim->fn_draw = draw_bob_or_first;
    hit_kill_shooter(shooter, victim, damage);
}

/* orig $E18, $005A0A: hit routine of an ordinary enemy */
void hit_enemy(Object *shooter, Object *enemy, uint16_t damage)
{
    uint16_t old;

    enemy->fn_draw = draw_bob_or_first;
    old = (uint16_t)enemy->energy;
    enemy->energy = (int16_t)(old - damage);
    if (old > damage)
        return;
    enemy_killed(shooter, enemy);
}

static void explosion_at(Object *obj, const uint16_t *anim)
{
    int16_t x, y;

    centre(obj, &x, &y);
    spawn_effect(x, y, anim, draw_bob_masked);
}

/* orig $EE4, $005A1A: an enemy dies: explosion, maybe a bonus, its score */
void enemy_killed(Object *shooter, Object *enemy)
{
    uint16_t *e = xenon2.pickup_groups;
    Object *n;
    const uint16_t *bonus_anim;
    int i, found = 0;
    int16_t kind;

    explosion_at(enemy, enemy->big ? xenon2.anim_explosion_big : xenon2.anim_explosion);
    kind = enemy->kind;
    if (kind != 0) {
        for (i = 0; i < 8; i++) {
            if (e[2 * i] == (uint16_t)kind) {
                found = 1;
                break;
            }
        }
        if (found && --e[2 * i + 1] == 0) {
            e[2 * i] = 0;
            n = alloc_b(shooter);
            bonus_anim = xenon2.anim_bonus_small;
            n->type = 0x60;
            if (enemy->big) {
                bonus_anim = xenon2.anim_bonus_big;
                n->type = 0x18;
            }
            n->pickup.mode = 7;
            n->fn_update = (obj_fn)pickup_update;
            n->fn_draw = draw_bob_masked;
            set_anim(n, bonus_anim);
            n->x = enemy->x;
            n->y = enemy->y;
            n->pickup.dir = (n->pickup.dir & 0xffffu) | (uint32_t)(uint16_t)((int16_t)random_next()) << 16;
        }
    }
    xenon2.player_score += (uint16_t)enemy->score;
    kill_object(enemy);
}

/* orig $EA8, $005B16: hit routine: the object explodes and dies at once */
void hit_explode(Object *shooter, Object *enemy, uint16_t damage)
{
    explosion_at(enemy, enemy->big ? xenon2.anim_explosion_big : xenon2.anim_explosion);
    kill_object(enemy);
}

/* orig $005BFC: draw routine of the tile object */
void tile_draw(Object *obj)
{
    obj->fn_draw = obj_nop;
    draw_tile_grid_flash((const uint16_t *)obj->gfx, (uint16_t)obj->tile.cols, (uint16_t)obj->tile.rows, (int16_t)(obj->y - xenon2.scroll_pos), (uint16_t)obj->x >> 1);
}

/* orig $F20, $005B72: hit routine of a destructible tile block: when it breaks the background is restored */
void hit_tile(Object *shooter, Object *tile, uint16_t damage)
{
    uint16_t old;
    int16_t x, y, rows, cols;
    Cell *dst;
    const uint16_t *src;

    tile->fn_draw = (obj_fn)tile_draw;
    old = (uint16_t)tile->energy;
    tile->energy = (int16_t)(old - damage);
    if (old > damage)
        return;
    xenon2.player_score += (uint16_t)tile->score;
    x = (int16_t)(((tile->tile.cols + 1) << 3) + tile->x);
    y = (int16_t)(((tile->tile.rows + 1) << 3) + tile->y - xenon2.scroll_pos);
    spawn_effect(x, y, xenon2.anim_explosion_big, draw_bob_masked);
    dst = tile->map;
    src = tile->anim;
    for (rows = tile->tile.rows; rows >= 0; rows--) {
        Cell *row = dst;

        for (cols = tile->tile.cols; cols >= 0; cols--) {
            row[0] = *src++;
            row++;
        }
        dst += MAP_COLS;
    }
    kill_object(tile);
}

/* orig $E72, $00422A: count + 1 pieces (type $C, only animating) at random places in the box (x, y, w, h), animation anim; the sound $83 */
void debris_burst(Object *obj, int16_t count, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *anim)
{
    do {
        Object *n = alloc_b(obj);

        n->type = 0xc;
        n->fn_update = (obj_fn)animate_only;
        n->fn_draw = draw_bob_masked;
        n->x = (int16_t)(random_below(w) + x);
        n->y = (int16_t)(random_below(h) + y);
        set_anim(n, anim);
    } while (--count != -1);
    xenon2.sfx_cmd = 0x83;
    xenon2.sfx_cmd2 = 0x83;
    sfx_play(0x83);
}

/* orig $E6C, $0021B6: count + 1 times two bonuses (types $60 and $18) fall in at random places at the front of list B */
void spawn_bonuses(int16_t count)
{
    do {
        Object *n = alloc_b_front(0);

        n->type = 0x60;
        n->fn_update = (obj_fn)pickup_update;
        n->fn_draw = draw_bob_masked;
        set_anim(n, xenon2.anim_bonus_small);
        n->x = (int16_t)(random_below(0x12c) + 0xa);
        n->y = (int16_t)(random_below(0xb4) + 6);
        n->pickup.mode = 7;
        xenon2.level_end_items++;
        n = alloc_b_front(0);
        n->type = 0x18;
        n->fn_update = (obj_fn)pickup_update;
        n->fn_draw = draw_bob_masked;
        set_anim(n, xenon2.anim_bonus_big);
        n->x = (int16_t)(random_below(0x12c) + 0xa);
        n->y = (int16_t)(random_below(0xb4) + 6);
        n->pickup.mode = 7;
        xenon2.level_end_items++;
    } while (--count != -1);
}
