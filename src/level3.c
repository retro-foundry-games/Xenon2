/* level3: level 3's own code (the blob at $54E40 while level 3 is loaded), as level3_data (struct Level3Data, types_blobs.h), written like level2.c;
 * a function has `level3 $ADDR` for its address. */
#include "weapons.h"
#include "shop.h"
#include "game_all.h"



/* the scratch fields of Object, by the kind that uses them */


/* the path script view of an object (ObjMove) */

/* the scratch fields of Object, by the kind that uses them */


/* the scratch fields of Object, by the kind that uses them */


/* the scratch fields of Object, by the kind that uses them */


/* the scratch fields of Object, by the kind that uses them */


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
    o->is_part = 0;
    o->persists = 0;
}

static void set_anim(Object *o, const uint8_t *anim)
{
    anim_header_set(o, anim);
}

void l3_spawn_snake(const uint8_t *rec);
void l3_spawn_diver(const uint8_t *rec);
void l3_worm_spawn(uint16_t side);
void l3_worm_hit(Object *shot, Object *worm, uint16_t damage);
void l3_end_draw(Object *obj);
void l3_spawn_gun(const uint8_t *rec);
void l3_spawn_boss(const uint8_t *rec);
void l3_spawn_walker(const uint8_t *rec);


/* ---- the entries the engine calls ($54E0C..$54E2C) ---- */

/* level3 $054E10, $056EFA: level events by event (rec = the record that fired): 0 the wave of $326C, 1 to 5 formations */
void l3_event(uint16_t event, const uint8_t *rec)
{
    uint16_t saved;

    switch (event) {
    case 0:
        spawn_wave_326c(0, rec);   /* $E5A */
        break;
    case 1:
        spawn_formation(0, rec, level3_data.formation_1);
        break;
    case 2:
        spawn_formation(0, rec, level3_data.formation_2);
        break;
    case 3:
        saved = difficulty_x2;
        difficulty_x2 = level3_data.event_param;
        spawn_formation(0, rec, level3_data.formation_3);
        difficulty_x2 = saved;
        break;
    case 4:
        spawn_formation(0, rec, level3_data.formation_4);
        break;
    case 5:
        spawn_formation(0, rec, level3_data.formation_5);
        break;
    }
}

/* level3 $054E18, $0556D0: level hooks by hook (rec = the record that fired, scroll_value its scroll value; w0c of the record says which side): 0 remembers
 * the record, 1 the snake, 2 the diver; 3 to 6 are not written */
void l3_hook(uint16_t hook, const uint8_t *rec, uint16_t scroll_value)
{
    switch (hook) {
    case 0:
        note_spawn_record(rec, scroll_value);   /* $EA2 */
        break;
    case 1:
        l3_spawn_snake(rec);
        break;
    case 2:
        l3_spawn_diver(rec);
        break;
    case 3:
        l3_spawn_parts();
        break;
    case 4:
        l3_spawn_gun(rec);
        break;
    case 5:
        l3_spawn_boss(rec);
        break;
    case 6:
        l3_spawn_walker(rec);
        break;
    }
}

/* level3 $054E24, $0551DE: the scroll: at the end ($AF0) the scroll's speed is stopped; above $D0 the limit is $A30 at least; below it, $20, the worm's
 * spawner runs and when its flags say so the bonuses come once */
void l3_scroll(void)
{
    if (scroll_min == 0xaf0 && scroll_pos == 0xaf0) {
        scroll_max = 0xaf0;
        scroll_pos_update(scroll_step);
        stars_scroll(scroll_step);
        scroll_step = 0;
        if (B(0xd96) != 0)
            sh_w8(0xd96, 0);
        else
            scroll_min = 0;
    }
    if ((int16_t)scroll_pos <= 0xd0) {
        if ((int16_t)scroll_max < 0x20)
            scroll_max = 0x20;
        if (B(0xd92) == 0 && B(0xd94) == 0)
            l3_worm_spawn(0);
        sh_w8(0xd94, 0);
        sh_w8(0xd95, 0);
        if (B(0xd92) != 0 && B(0xd93) != 0 && boss_dead == 0) {
            spawn_bonuses(9);
            boss_dead = 0xff;
        }
    } else if ((int16_t)scroll_max < 0xa30) {
        scroll_max = 0xa30;
    }
}

/* level3 $054E20, $056FEE: level 3 starts: its map is copied once to $7D120 (backwards, as the original does), the scroll'front limits are made, the
 * object that draws the pieces of the end is put first in the front list, and the state of the end is cleared */
void l3_init(void)
{
    Object *end_obj, *front, *old;
    int i;

    if (level_init_src == (uint8_t *)GPTR(0x74386)) {
        for (i = 11999; i >= 0; i--)
            ((uint8_t *)GPTR(0x7d120))[i] = ((const uint8_t *)GPTR(0x74386))[i];
        level_init_src = (uint8_t *)GPTR(0x7d120);
    }
    scroll_min = 0xaf0;
    scroll_max = 0x11ff;
    end_obj = obj_free_pop();
    front = LIST_SENTINEL(LIST_FRONT);
    old = front->next;
    front->next = end_obj;
    end_obj->next = old;
    end_obj->prev = front;
    old->prev = end_obj;
    end_obj->type = 0x50;
    end_obj->fn_update = (obj_fn)obj_nop_thunk;
    end_obj->fn_draw = FN(l3_end_draw);
    end_obj->persists = -1;
    unit_seed[0] = 0xe496aa8a;
    unit_seed[1] = 0x5274b949;
    sh_w8(0xd92, 0);
    sh_w8(0xd93, 0xff);
    sh_w8(0xd94, 0);
    sh_w8(0xd95, 0);
    sh_w16(0xd64, 0);
    sh_w16(0xd66, level3_data.init_patch);
    sh_w16(0xd68, level3_data.init_patch);
    sh_w8(0xd96, 0);
    sh_w16(0xd6a, 2);
    sh_w16(0xd60, level3_data.init_patch_2);
    sh_w16(0xd62, level3_data.init_patch_2);
    difficulty_x1 = (uint16_t)(difficulty * 2);
    difficulty_x2 = (uint16_t)(difficulty * 3);
}

/* ---- what the level 3's records put into the field ---- */

/* level3 $056F60: draws the tile grids of the table at $56F9A (the pieces of the end) that are on the screen */
void l3_end_draw(Object *obj)
{
    const struct Level3EndPiece *piece = level3_data.end_pieces;

    (void)obj;
    if (B(0xd94) == 0 && B(0xd95) == 0)
        return;
    for (; piece->cells != 0; piece++) {
        int16_t sy = (int16_t)(piece->y - scroll_pos);

        if (sy >= 0xc0 || sy <= -0x30)
            continue;
        draw_tile_grid(piece->cells, piece->w, piece->h, sy, piece->x);
    }
}

/* level3 $055310: the worm of the end: eleven objects of the table at $55270 (update, picture, the delay v28 of each behind the one before) along a path from
 * the table at $6EF36: the first path is the first, then they come by the seed of the side (1 to 6); side is v30 */
void l3_worm_spawn(uint16_t side)
{
    uint16_t path_index;
    const struct Level3WormPiece *entry = level3_data.worm_spawn.pieces;
    uint32_t path;
    int16_t delay = 0, x, y;
    int n, count;
    Object *head = 0, *prev = 0;

    if (W(0xd64) != 1) {
        path_index = W(0xd64);
        sh_w16(0xd64, (uint16_t)(path_index + 1));
    } else {
        uint32_t *seed = &unit_seed[side == 0 ? 0 : 1];

        do {
            *seed = (*seed >> 5) | (*seed << 27);
            path_index = (uint16_t)(*seed & 7);
        } while (path_index < 1 || path_index == 7);
    }
    path = sh_r32(0x6ef36 + 4 * (uint32_t)path_index);
    x = (int16_t)W(path + 2);
    y = (int16_t)(W(path + 4) - scroll_pos - scroll_step);
    count = level3_data.worm_spawn.last + 1;
    for (n = 0; n < count; n++) {
        Object *seg = alloc_in(LIST_C);

        if (prev)
            prev->chain = seg;
        else
            head = seg;
        seg->fn_hit = prev ? GFN(0xf2c) : (void *)l3_worm_hit;
        seg->type = 0x50;
        seg->worm.delay = delay;
        delay = (int16_t)(delay - (int16_t)entry->delay);
        seg->fn_update = (obj_fn)entry->update;
        seg->fn_draw = draw_bob_masked;
        seg->gfx = (void *)entry->gfx;
        seg->score = 0x7d0;
        SET_V5E(seg, 0);
        seg->big = 0;
        seg->is_part = 0;
        seg->anim_delay = 0;
        seg->script = (uint8_t *)GPTR(path);
        seg->cursor = (const uint8_t *)GPTR(path);
        seg->worm.phase = 0xc0;
        seg->x = x;
        seg->y = y;
        seg->kind = 0;
        seg->steps = (int16_t)level3_data.worm_spawn_steps;
        seg->worm.side = (int16_t)side;
        bbox_from_frame(seg);
        seg->target = head;
        seg->chain = 0;
        entry++;
        prev = seg;
    }
}

static void l3_enemy(Object *obj, const uint8_t *const *table)
{
    enemy_update_no_anim(obj);
    obj->gfx = (void *)table[(((uint16_t)obj->l3_picture.phase + 0x10) >> 5) & 7];
    bbox_from_frame(obj);
}

/* level3 $055044: an enemy whose picture follows its phase v2c (eight pictures of a table; $05504E and $055058 are the same with another table) */
void l3_enemy_a(Object *obj)
{
    l3_enemy(obj, level3_data.enemy_a_pictures);
}

/* level3 $05504E */
void l3_enemy_b(Object *obj)
{
    l3_enemy(obj, level3_data.enemy_b_pictures);
}

/* level3 $055058 */
void l3_enemy_c(Object *obj)
{
    l3_enemy(obj, level3_data.enemy_c_pictures);
}

/* a snake's piece leaves the field at y $E8 */
static int l3_snake_off(Object *obj)
{
    obj->y = (int16_t)(obj->y + scroll_speed);
    if (obj->y >= 0xe8) {
        kill_thunk(obj);
        return 1;
    }
    return 0;
}


/* level3 $056490: the first piece of a snake: it waits (v30), looks for the player's height, then swings (v28 between 0 and v30) */
void l3_snake_head(Object *obj)
{
    if (l3_snake_off(obj))
        return;
    obj->fn_draw = (obj_fn)obj_nop_thunk;
    if (obj->snake.step == 0) {
        int16_t dy;

        if (obj->snake.limit < 0) {
            obj->snake.limit++;
            bbox_from_frame(obj);
            return;
        }
        dy = (int16_t)(obj->y - PLAYER_Y);
        if (dy < 0)
            dy = (int16_t)-dy;
        if (dy > 0x20) {
            bbox_from_frame(obj);
            return;
        }
        obj->snake.step = 2;
        obj->snake.limit = (int16_t)((random_next() & 6) + 0xa);
    }
    obj->fn_draw = draw_bob_masked;
    obj->snake.swing = (int16_t)(obj->snake.step + obj->snake.swing);
    if (obj->snake.swing == 0) {
        obj->snake.step = 0;
        obj->snake.limit = -5;
    } else if (obj->snake.swing == obj->snake.limit) {
        obj->snake.step = (int16_t)-obj->snake.step;
    }
    bbox_from_frame(obj);
}

/* level3 $05653C: the last piece of a snake: it follows the head's swing and shows its second picture at the ends of it */
void l3_snake_tail(Object *obj)
{
    Object *head;

    if (l3_snake_off(obj))
        return;
    anim_step(obj);
    head = obj->target;
    obj->x = (int16_t)((uint16_t)((uint16_t)obj->snake.limit * (uint16_t)head->snake.swing) + (uint16_t)obj->snake.x);
    if (head->snake.step == 2 && head->snake.swing == 2) {
        obj->anim_delay = 1;
    } else if (head->snake.limit > 0 && head->snake.limit == head->snake.swing) {
        obj->anim_delay = 1;
    }
    bbox_from_frame(obj);
}

/* level3 $0565B2: a piece of a snake between the head and the tail: it follows the head's swing, drawn while the head swings */
void l3_snake_body(Object *obj)
{
    Object *head;

    if (l3_snake_off(obj))
        return;
    obj->fn_draw = (obj_fn)obj_nop_thunk;
    head = obj->target;
    obj->x = (int16_t)((uint16_t)((uint16_t)obj->snake.limit * (uint16_t)head->snake.swing) + (uint16_t)obj->snake.x);
    if (head->snake.step != 0)
        obj->fn_draw = draw_bob_masked;
    bbox_from_frame(obj);
}

/* level3 $055EEA: a snake of eight pieces ($C8, $CC when w0c of the record is set): the pieces are chained, each follows the first, and a pair of
 * markers ($B8 and $BC) holds the box of the group */
void l3_spawn_snake(const uint8_t *rec)
{
    Object *box = alloc_in(LIST_C), *head, *seg, *anchor;
    int flag = rd16(rec + 0xc) != 0;
    int16_t x = (int16_t)(rd16(rec + 8) - 8), y;
    int i;

    if (flag)
        x = (int16_t)(x + 0x20);
    y = (int16_t)(rd16(rec + 0xa) - scroll_pos);
    seg = alloc_in(LIST_C);
    head = seg;
    for (i = 7; i >= 0; i--) {
        int16_t offset;

        seg->type = flag ? 0xcc : 0xc8;
        seg->gfx = (void *)(flag ? level3_data.snake_picture_a : level3_data.snake_picture_b);
        seg->anim_delay = 0;
        SET_V5E(seg, 0);
        if (i == 0)
            set_anim(seg, flag ? level3_data.snake_anim_a : level3_data.snake_anim_b);
        seg->fn_update = FN(l3_snake_body);
        if (i == 0)
            seg->fn_update = FN(l3_snake_tail);
        if (i == 7)
            seg->fn_update = FN(l3_snake_head);
        seg->fn_draw = draw_bob_masked;
        seg->x = x;
        seg->snake.x = x;
        seg->y = y;
        seg->fn_hit = (void *)hit_part;
        seg->is_part = 1;
        seg->energy = (int16_t)level3_data.snake_energy;
        seg->big = 0;
        seg->score = 0x190;
        seg->snake.swing = 0;
        seg->kind = 0;
        seg->snake.step = 0;
        offset = (int16_t)(7 - i);
        if (flag)
            offset = (int16_t)-offset;
        seg->snake.limit = offset;
        bbox_from_frame(seg);
        seg->target = head;
        seg->chain = 0;
        if (i != 0) {
            Object *piece = seg, *n = obj_free_pop(), *prev = piece->prev;

            piece->prev = n;
            n->prev = prev;
            n->next = piece;
            prev->next = n;
            piece->chain = n;
            seg = n;
        }
    }
    anchor = alloc_in(LIST_C);
    anchor->type = 0xb8;
    anchor->fn_update = (obj_fn)obj_nop_thunk;
    anchor->fn_draw = (obj_fn)obj_nop_thunk;
    anchor->fn_hit = (void *)obj_nop_thunk;
    anchor->hit[0] = 0x3e8;
    anchor->hit[2] = 0x3e8;
    anchor->chain = box;
    box->type = 0xbc;
    box->fn_update = FN(group_box_update);
    box->fn_draw = (obj_fn)obj_nop_thunk;
    box->fn_hit = (void *)obj_nop_thunk;
    box->hit[0] = 0x3e8;
    box->hit[2] = 0x3e8;
    box->target = anchor;
}

/* level3 $056796: the diver's shot: the shot of an enemy, aimed left or right of the middle of the screen */
void l3_diver_shot(Object *obj)
{
    enemy_shot_update(obj);
    if (obj->type != 0x14)
        return;
    if (obj->x < 0x80) {
        obj->diver.side = 3;
        obj->gfx = (void *)level3_data.diver_shot_picture_a;
    }
    if (obj->x >= 0xc0) {
        obj->diver.side = 5;
        obj->gfx = (void *)level3_data.diver_shot_picture_b;
    }
}


/* level3 $056618: the diver ($D0): it crosses the screen sideways, turns at the edges, stops at x $88 and $B8 (v28 1) to shoot, with the shot
 * eleven steps into the animation */
void l3_diver_update(Object *obj)
{
    int16_t x;
    const uint8_t *anim;
    int flip;

    obj->y = (int16_t)(obj->y + scroll_speed);
    x = (int16_t)((int16_t)scroll_max + 0xd0 - scroll_pos);
    if (x < obj->y) {
        kill_thunk(obj);
        return;
    }
    anim_step(obj);
    if (obj->diver.state == 0) {
        x = (int16_t)(obj->diver.side + obj->x);
        obj->x = x;
        if (x <= -0x20) {
            obj->x = 0;
            anim = level3_data.diver_update_data;
            flip = 1;
        } else if (x >= 0x160) {
            obj->x = 0x140;
            anim = level3_data.diver_update_data_2;
            flip = 1;
        } else if (x == 0x88) {
            obj->diver.state = 1;
            anim = level3_data.diver_update_data_3;
            flip = 0;
        } else if (x == 0xb8) {
            obj->diver.state = 1;
            anim = level3_data.diver_update_data_4;
            flip = 0;
        } else {
            bbox_from_frame(obj);
            return;
        }
    } else if (obj->anim_delay == 0) {
        obj->diver.state = 0;
        if (obj->x > 0xa0) {
            obj->x = 0xd8;
            anim = level3_data.diver_update_data;
        } else {
            obj->x = 0x68;
            anim = level3_data.diver_update_data_2;
        }
        flip = 1;
    } else {
        obj->diver.state++;
        if (obj->diver.state == 0xb) {
            int16_t sx = (int16_t)(obj->x - 5), sy = (int16_t)(obj->y + 4);
            const uint8_t *gfx = level3_data.diver_shot_picture_a;
            Object *shot;

            if (sx > 0xa0) {
                sx = (int16_t)(sx + 0xa);
                gfx = level3_data.diver_shot_picture_b;
            }
            shot = alloc_in(LIST_B);
            shot->type = 0x14;
            shot->fn_update = FN(l3_diver_shot);
            shot->fn_draw = draw_bob_masked;
            shot->gfx = (void *)gfx;
            shot->anim_delay = 0;
            shot->x = sx;
            shot->y = sy;
            shot->diver.state = 0;
            shot->steps = (int16_t)level3_data.diver_steps;
            shot->diver.side = 3;
            if (obj->x >= 0xa0)
                shot->diver.side = 5;
        }
        bbox_from_frame(obj);
        return;
    }
    if (flip)
        obj->diver.side = (int16_t)-obj->diver.side;
    set_anim(obj, anim);
    bbox_from_frame(obj);
}

/* level3 $056108: a diver ($D0) from a record: it flies towards the middle from the side it is on */
void l3_spawn_diver(const uint8_t *rec)
{
    Object *obj = alloc_in(LIST_C);
    const uint8_t *anim = level3_data.diver_update_data;

    obj->type = 0xd0;
    obj->fn_update = FN(l3_diver_update);
    obj->fn_draw = draw_bob_masked;
    SET_V5E(obj, 0);
    obj->x = (int16_t)(rd16(rec + 8) - 8);
    obj->y = (int16_t)(rd16(rec + 0xa) + 8 - scroll_pos);
    obj->diver.side = 2;
    if (obj->x > 0xa0) {
        anim = level3_data.diver_update_data_2;
        obj->diver.side = -2;
    }
    set_anim(obj, anim);
    obj->fn_hit = (void *)hit_enemy;
    obj->energy = (int16_t)level3_data.diver_energy;
    obj->big = -1;
    obj->score = 0x12c;
    obj->diver.state = 0;
    obj->kind = 0;
    bbox_from_frame(obj);
}

/* ---- the parts of the end ---- */

static void l3_centre(const Object *o, int16_t *x, int16_t *y)
{
    const uint8_t *frame = (const uint8_t *)o->gfx;
    int16_t dx = (int16_t)(o->x - (int16_t)rd16(frame)), dy = (int16_t)(o->y - (int16_t)rd16(frame + 2));

    dx = (int16_t)(dx + dx);
    dy = (int16_t)(dy + dy);
    *x = (int16_t)(dx + (int16_t)rd16(frame + 4)) >> 1;
    *y = (int16_t)(dy + (int16_t)rd16(frame + 6)) >> 1;
}

/* level3 $055DEC: the parts of the end, one object each from the table at $558F0 (update, hit, picture, offset x, offset y), put first in list C; the first
 * of them is the head of the chain; v30 is the place after the part's entry in the table */
void l3_spawn_parts(void)
{
    const struct Level3Part *entry = level3_data.parts;
    Object *first = 0, *prev = 0;

    do {
        Object *part = insert_head(LIST_C);

        if (prev)
            prev->chain = part;
        else
            first = part;
        part->type = 0x54;
        part->fn_update = (obj_fn)entry->update;
        part->fn_draw = draw_bob_masked;
        part->fn_hit = (void *)entry->hit;
        part->path.wait = 0;
        set_anim(part, entry->anim);
        part->turn_hi = entry->x;
        part->turn_lo = entry->y;
        SET_V5E(part, 0);
        part->score = 0x3e8;
        part->big = -1;
        part->script = level3_data.spawn_parts_data_2;
        part->x = (int16_t)(level3_data.spawn_parts_words - 0x20);
        part->y = (int16_t)(level3_data.spawn_parts_words_2 - 0x28);
        part->x_frac = 0;
        part->y_frac = 0;
        part->cursor = level3_data.spawn_parts_data_3;
        part->path.angle_hi = 0;
        part->path.angle_lo = 0;
        part->turn_step = 0;
        part->kind = 0;
        part->steps = (int16_t)level3_data.parts_steps;
        bbox_from_frame(part);
        part->target = first;
        part->chain = 0;
        entry++;
        part->part.slot = (int16_t)((entry - level3_data.parts) * 16);
        part->arm_b.swing = 0;
        prev = part;
    } while (entry->update != 0);
    sh_w8(0xd96, 0xff);
    sh_w8(0xd85, 0xff);
}

/* level3 $055A88: the first part: it keeps the end'shot flag d96 set and fires a shot at the player now and then */
void l3_part_core(Object *obj)
{
    uint16_t sum;

    sh_w8(0xd96, 0xff);
    enemy_update(obj);
    sum = (uint16_t)(obj->v5e_b[0] + level3_data.part_core_fire_rate);
    obj->v5e_b[0] = (uint8_t)sum;
    if (sum > 0xff) {
        Object *shot;
        uint16_t angle;

        obj->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
        angle = direction8((int16_t)(PLAYER_X - obj->x), (int16_t)(PLAYER_Y - obj->y));
        shot = spawn_enemy_shot((uint16_t)(obj->x + 0x10), (uint16_t)(obj->y + 0x3c), angle, level3_data.part_core_shot_steps, 0);
        set_anim(shot, level3_data.part_core_data);
    }
}

/* level3 $055AF0: a part follows the first by its offsets */
void l3_part_follow(Object *obj)
{
    Object *core;

    anim_step(obj);
    core = obj->target;
    obj->x = (int16_t)(core->x + (int16_t)obj->turn_hi);
    obj->y = (int16_t)(core->y + (int16_t)obj->turn_lo);
    bbox_from_frame(obj);
}


/* level3 $055B38: a gun part (v30 $40 the left one, the other the right): while the energy of its side (d60 / d62) is above zero it is shown
 * as it is and can be hit, then it is the picture of the wreck */
void l3_part_gun(Object *obj)
{
    int16_t side;

    l3_part_follow(obj);
    side = (int16_t)W(obj->gun.side == 0x40 ? 0xd60 : 0xd62);
    if (side <= 0) {
        obj->gfx = (void *)(obj->gun.side == 0x40 ? level3_data.part_gun_picture_side_40 : level3_data.part_gun_picture_other);
        obj->anim_delay = 0;
        obj->hit[0] = 0x3e8;
        obj->fn_hit = (void *)obj_nop_thunk;
        return;
    }
    if (obj->anim_delay != 0) {
        bbox_from_frame(obj);
        return;
    }
    {
        uint16_t sum = (uint16_t)(obj->v5e_b[0] + level3_data.part_gun_fire_rate);

        obj->v5e_b[0] = (uint8_t)sum;
        if (sum > 0xff) {
            obj->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
            set_anim(obj, level3_data.part_gun_data_3);
        }
    }
    obj->hit[0] = 0x3e8;
    obj->hit[2] = 0x3e8;
}

/* level3 $055BC0: the hit of a gun part: the energy of its side runs out, it explodes, and when d6a hits come to zero the whole end goes */
void l3_part_hit(Object *shot, Object *part, uint16_t damage)
{
    Object *piece;
    uint32_t addr = part->gun.side == 0x40 ? 0xd60 : 0xd62;
    uint16_t old = W(addr);
    int16_t x, y;

    for (piece = part->target; piece; piece = piece->chain)
        piece->fn_draw = draw_bob_or_first;
    sh_w16(addr, (uint16_t)(old - damage));
    if (old > damage)
        return;
    part->hit[0] = 0x3e8;
    part->fn_hit = (void *)obj_nop_thunk;
    l3_centre(part, &x, &y);
    spawn_effect(x, y, (const uint8_t *)GPTR(sh_r32(0xf70)), draw_bob_masked);
    sh_w16(0xd6a, (uint16_t)(W(0xd6a) - 1));
    if (W(0xd6a) != 0)
        return;
    part->target->energy = 0;
    hit_part(shot, part, 1);
    spawn_bonuses(4);
    sh_w8(0xd85, 0);
}


/* level3 $055C52: an arm's root: now and then it aims at the player and swings (v3e from 1 to $E and back), the next part's angle and speed follow */
void l3_part_arm(Object *obj)
{
    Object *piece;

    l3_part_follow(obj);
    piece = obj->prev;
    if (obj->arm_b.swing != 0) {
        obj->arm_b.swing = (int16_t)(obj->arm_b.swing + obj->arm.swing_step);
        if (obj->arm_b.swing == 0xe)
            obj->arm.swing_step = (int16_t)-obj->arm.swing_step;
        piece->steps = obj->arm_b.swing;
        bbox_from_frame(obj);
        return;
    }
    if ((random_next() & 0xff) >= 3) {
        piece->steps = 0;
        piece->arm.angle_lo = 0x40;
        bbox_from_frame(obj);
        return;
    }
    sfx_cmd2 = 0xf;
    obj->arm_b.swing = 1;
    obj->arm.swing_step = 1;
    {
        uint16_t aim = (uint16_t)(direction8((int16_t)(PLAYER_X - obj->x), (int16_t)(PLAYER_Y - obj->y)) << 5);
        uint32_t r;

        aim = (uint16_t)((aim & 0xff00) | (uint8_t)((aim & 0xff) - 0x60));
        piece->arm.angle_hi = 0;
        piece->arm.angle_lo = (int16_t)aim;
        r = (((random_next() & 0x1f) + 0x20) << 8) << 2;
        piece->turn_fixed = r;
        piece->turn_step = 0;
        piece->steps = 1;
        if (obj->part.slot == 0x60) {
            piece->arm.angle_lo = (int16_t)((piece->arm.angle_lo & 0xff00) | (uint8_t)(piece->arm.angle_lo + 0x40));
            r = -r;
            piece->turn_fixed = r;
        }
    }
    bbox_from_frame(obj);
}

/* the arm's piece takes its place from the next piece and runs its path with the state of the piece before it */
static void l3_arm_follow(Object *obj)
{
    Object *owner = obj->next;

    obj->x = owner->x;
    obj->x_frac = owner->x_frac;
    obj->y = owner->y;
    obj->y_frac = owner->y_frac;
    obj->path.wait = 0x3e8;
}


/* level3 $055D12: a piece of an arm: it moves after the next piece by its own path (angle v2a:v2c, speed pad4e), and hands that state to the piece before it */
void l3_part_arm_piece(Object *obj)
{
    int16_t saved_angle_hi, saved_angle_lo, saved_turn_hi, saved_turn_lo, saved_turn_step;
    Object *tip;

    l3_arm_follow(obj);
    saved_angle_hi = obj->arm.angle_hi;
    saved_angle_lo = obj->arm.angle_lo;
    saved_turn_hi = (int16_t)obj->turn_hi;
    saved_turn_lo = (int16_t)obj->turn_lo;
    saved_turn_step = (int16_t)obj->turn_step;
    obj_script_run(obj);
    tip = obj->prev;
    tip->arm.angle_hi = obj->arm.angle_hi;
    tip->arm.angle_lo = obj->arm.angle_lo;
    tip->turn_hi = obj->turn_hi;
    tip->turn_lo = obj->turn_lo;
    tip->turn_step = obj->turn_step;
    tip->steps = obj->steps;
    obj->turn_step = (uint16_t)saved_turn_step;
    obj->turn_lo = (uint16_t)saved_turn_lo;
    obj->turn_hi = (uint16_t)saved_turn_hi;
    obj->arm.angle_lo = saved_angle_lo;
    obj->arm.angle_hi = saved_angle_hi;
}

/* level3 $055D62: the tip of an arm: it takes its place and path like the pieces, and its picture follows the angle */
void l3_part_arm_tip(Object *obj)
{
    uint16_t angle_bits, tick_bits;

    l3_arm_follow(obj);
    obj_script_run(obj);
    angle_bits = (uint16_t)(((uint16_t)obj->arm.angle_lo >> 2) & 0x38);
    if (obj->steps == 0xe)
        sfx_cmd = 0xc;
    tick_bits = (uint16_t)(frame_tick & 4);
    obj->gfx = (void *)level3_data.arm_tip_pictures[(uint16_t)(angle_bits + tick_bits) / 4];
    obj->anim_delay = 0;
    bbox_from_frame(obj);
}

/* level3 $0550D8: an enemy that waits off the screen: by the side of it (the corner, by which of its distances is the shorter) it takes the picture
 * of its direction from the table at $5513E and goes on as an ordinary enemy (enemy_update) */
void l3_enemy_enter(Object *obj)
{
    int16_t x, y, anim_off;

    enemy_update(obj);
    x = obj->x;
    y = obj->y;
    if (x < 0) {
        if (y < 0)
            anim_off = y < x ? 4 : 0;
        else
            anim_off = (int16_t)(0xc0 - y) < x ? 0xc : 0;
    } else if (y < 0) {
        anim_off = y < (int16_t)(0x140 - x) ? 4 : 8;
    } else {
        anim_off = (int16_t)(y - 0xc0) < (int16_t)(x - 0x140) ? 8 : 0xc;
    }
    set_anim(obj, level3_data.enemy_enter_anims[anim_off / 4]);
    obj->fn_update = FN(enemy_update);
    bbox_from_frame(obj);
}

/* ---- the guns, the boss of tiles and the walker ---- */

static void copy2(uint8_t *dst, const uint8_t *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
}

static void copy4(uint8_t *dst, const uint8_t *src)
{
    copy2(dst, src);
    copy2(dst + 2, src + 2);
}


/* level3 $056DF6: a gun of tiles ($F0 or $F4): its tiles open in turn (v28), at the eighth it fires */
void l3_gun_update(Object *obj)
{
    int16_t y;
    const uint8_t *cells;

    if ((int16_t)(scroll_max + 0xd0) < obj->y) {
        kill_thunk(obj);
        return;
    }
    obj->hit[0] = (int16_t)(obj->x + 2);
    obj->hit[2] = (int16_t)(obj->x + 0xe);
    y = (int16_t)(obj->y - scroll_pos + 2);
    obj->hit[1] = y;
    obj->hit[3] = (int16_t)(y + 0xc);
    if (obj->l3_gun.open == 0) {
        uint16_t sum = (uint16_t)(obj->v5e_b[0] + level3_data.gun_fire_rate);

        obj->v5e_b[0] = (uint8_t)sum;
        if (sum > 0xff) {
            obj->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
            obj->l3_gun.open = 1;
        }
        return;
    }
    cells = obj->type == 0xf0 ? level3_data.gun_cells_f0 : level3_data.gun_cells_other;
    copy2(obj->script, cells + (uint16_t)(obj->l3_gun.open & ~1));
    obj->l3_gun.open++;
    if (obj->l3_gun.open == 8) {
        int16_t x = (int16_t)(obj->x + 8), y = (int16_t)(obj->y - scroll_pos), angle = 0;

        if (obj->type != 0xf0) {
            angle = 4;
            y = (int16_t)(y + 0xa);
        }
        spawn_enemy_shot((uint16_t)x, (uint16_t)y, (uint16_t)angle, level3_data.gun_shot_steps, (void *)level3_data.gun_shot_picture);
        return;
    }
    if (obj->l3_gun.open == 0xe)
        obj->l3_gun.open = 0;
}

/* level3 $056220: a gun of tiles from a record ($F0, $F4 when w0c is set) */
void l3_spawn_gun(const uint8_t *rec)
{
    int flag = rd16(rec + 0xc) != 0;
    Object *obj = spawn_marker_enemy(flag ? 0xf4 : 0xf0, rec, (void (*)(void))l3_gun_update);
    const uint8_t *cells = flag ? level3_data.gun_cells_other : level3_data.gun_cells_f0;

    obj->fn_hit = (void *)hit_tile;
    obj->energy = (int16_t)level3_data.gun_energy;
    obj->hit[0] = 0x3e8;
    obj->hit[2] = 0x3e8;
    obj->gfx = (void *)cells;
    obj->anim = flag ? level3_data.gun_anim_a : level3_data.gun_anim_b;
    copy2(obj->script, cells);
    obj->score = 0x64;
    obj->tile.cols = 0;
    obj->tile.rows = 0;
}

/* level3 $0561A6: the boss of tiles ($F8) from a record: six rows of its tiles are put into the map */
void l3_spawn_boss(const uint8_t *rec)
{
    Object *obj = spawn_marker_enemy(0xf8, rec, (void (*)(void))l3_boss_update);
    uint8_t *script = obj->script;
    int i;

    obj->fn_draw = (obj_fn)obj_nop_thunk;
    obj->fn_hit = (void *)l3_boss_hit;
    obj->energy = (int16_t)level3_data.boss_energy;
    obj->hit[0] = 0x3e8;
    obj->hit[2] = 0x3e8;
    for (i = 0; i < 6; i++) {
        copy4(script, level3_data.spawn_boss_data + 8 * i);
        copy4(script + 4, level3_data.spawn_boss_data + 8 * i + 4);
        script += 0x28;
    }
}


/* level3 $05686A: the boss of tiles, first form: it opens and closes its mouth (v28, the cells of the table at $569A2) and fires from the sixth step on */
void l3_boss_update(Object *obj)
{
    uint16_t cell;
    uint8_t *script;

    if ((int16_t)(scroll_max + 0xd0) < obj->y) {
        kill_thunk(obj);
        return;
    }
    obj->hit[0] = (int16_t)(obj->x + 0x10);
    obj->hit[2] = (int16_t)(obj->hit[0] + 0x1f);
    cell = (uint16_t)(obj->y - scroll_pos + 0x40);
    obj->hit[1] = (int16_t)cell;
    obj->hit[3] = (int16_t)(cell + 0x1a);
    cell = (uint16_t)obj->l3_boss.cell;
    if (cell == 0) {
        if ((uint8_t)random_next() >= level3_data.boss_chance)
            return;
    }
    script = obj->script;
    copy4(script + 0xa2, level3_data.boss_update_data + 4 * (cell & ~1u));
    copy4(script + 0xca, level3_data.boss_update_data + 4 * (cell & ~1u) + 4);
    obj->l3_boss.cell++;
    if (obj->l3_boss.cell == 0x12) {
        obj->l3_boss.cell = 0;
        return;
    }
    if (obj->l3_boss.cell < 4)
        return;
    if (obj->l3_boss.cell > 0xb) {
        if (obj->l3_boss.cell == 0xc && (uint8_t)random_next() < level3_data.boss_chance_2)
            obj->l3_boss.cell = 4;
        return;
    }
    {
        uint16_t sum = (uint16_t)(obj->v5e_b[0] + level3_data.boss_fire_rate);
        int16_t x, y;
        uint16_t angle = 4, step;
        uint32_t gfx;

        obj->v5e_b[0] = (uint8_t)sum;
        if (sum <= 0xff)
            return;
        obj->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
        x = (int16_t)(obj->x + 0x20);
        y = (int16_t)(obj->y - scroll_pos + 0x58);
        gfx = sh_r32(sh_r32(0x6ee26 + 4 * (uint32_t)level3_data.boss_param));
        step = (uint16_t)(obj->l3_boss.cell & ~1);
        if (step == 6)
            angle = 3;
        if (step == 8)
            angle = 5;
        spawn_enemy_shot((uint16_t)x, (uint16_t)y, angle, level3_data.gun_energy, GPTR(gfx));
    }
}

/* level3 $0569EA: the hit of the boss of tiles, first form: it flashes; when the energy is gone it bursts, its tiles go and it goes on in the second form */
void l3_boss_hit(Object *shot, Object *boss, uint16_t damage)
{
    uint16_t old = (uint16_t)boss->energy;
    uint8_t *script;

    (void)shot;
    boss->energy = (int16_t)(old - damage);
    if (old > damage) {
        boss->fn_draw = FN(l3_boss_flash);
        return;
    }
    spawn_effect((int16_t)(boss->x + 0x20), (int16_t)(boss->y - scroll_pos + 0x40), big_explosion_anim, draw_bob_masked);
    script = boss->script;
    wr32(script + 0x50, 0);
    wr32(script + 0x54, 0);
    wr32(script + 0x78, 0);
    wr32(script + 0x7c, 0);
    wr32(script + 0xa0, 0);
    wr32(script + 0xa4, 0);
    wr32(script + 0xca, 0);
    boss->fn_update = FN(l3_boss2_update);
    boss->fn_draw = (obj_fn)obj_nop_thunk;
    boss->fn_hit = (void *)l3_boss2_hit;
    boss->energy = (int16_t)level3_data.boss_energy_2;
    boss->hit[0] = 0x3e8;
    boss->hit[2] = 0x3e8;
    boss->l3_boss.cell = 0;
    player_score += 0x1f4;
}

/* level3 $056A8C: the draw of the boss of tiles for the frame after a hit: its two grids as flashes */
void l3_boss_flash(Object *obj)
{
    int16_t y = (int16_t)(obj->y - scroll_pos);
    const uint8_t *cells = level3_data.boss_update_data + 4 * (uint32_t)(obj->l3_boss.cell & ~1);

    obj->fn_draw = (obj_fn)obj_nop_thunk;
    draw_tile_grid_flash(level3_data.boss_flash_grid, 3, 2, (int16_t)(y + 0x20), (uint16_t)(((uint16_t)obj->x >> 1) & 0xfffc));
    draw_tile_grid_flash(cells, 1, 1, (int16_t)(y + 0x40), (uint16_t)(((uint16_t)(obj->x + 0x10) >> 1) & 0xfffc));
}


/* level3 $056AF8: the boss of tiles, second form: the cells of the table at $56D2C in turn (v28), and at the fourth step eight shots */
void l3_boss2_update(Object *obj)
{
    uint16_t cell;
    uint8_t *script;

    if ((int16_t)(scroll_max + 0xd0) < obj->y) {
        kill_thunk(obj);
        return;
    }
    obj->hit[0] = (int16_t)(obj->x + 0x10);
    obj->hit[2] = (int16_t)(obj->hit[0] + 0x1f);
    cell = (uint16_t)(obj->y + 4 - scroll_pos);
    obj->hit[1] = (int16_t)cell;
    obj->hit[3] = (int16_t)(cell + 0x17);
    cell = (uint16_t)obj->l3_boss2.cell;
    if (cell == 0) {
        uint16_t sum = (uint16_t)(obj->v5e_b[0] + level3_data.boss2_fire_rate);

        obj->v5e_b[0] = (uint8_t)sum;
        if (sum <= 0xff)
            return;
        obj->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
    }
    script = obj->script;
    copy4(script + 2, level3_data.boss2_update_data + 4 * (cell & ~1u));
    copy4(script + 0x2a, level3_data.boss2_update_data + 4 * (cell & ~1u) + 4);
    obj->l3_boss2.cell++;
    if (obj->l3_boss2.cell == 0xa) {
        obj->l3_boss2.cell = 0;
        return;
    }
    if (obj->l3_boss2.cell == 4) {
        int16_t x = (int16_t)(obj->x + 0x20), y = (int16_t)(obj->y - scroll_pos + 0x18);
        int i;

        for (i = 7; i >= 0; i--) {
            Object *shot = spawn_enemy_shot((uint16_t)x, (uint16_t)y, (uint16_t)i, level3_data.boss2_shot_steps, 0);

            set_anim(shot, level3_data.boss2_shot_anims[7 - i]);
        }
    }
}

/* level3 $056D54: the hit of the boss of tiles, second form: it flashes; when the energy is gone it bursts and goes */
void l3_boss2_hit(Object *shot, Object *boss, uint16_t damage)
{
    uint16_t old = (uint16_t)boss->energy;
    uint8_t *script;

    (void)shot;
    boss->energy = (int16_t)(old - damage);
    if (old > damage) {
        boss->fn_draw = FN(l3_boss2_flash);
        return;
    }
    spawn_effect((int16_t)(boss->x + 0x20), (int16_t)(boss->y - scroll_pos + 0x10), big_explosion_anim, draw_bob_masked);
    script = boss->script;
    wr32(script, 0);
    wr32(script + 4, 0);
    wr32(script + 0x28, 0);
    wr32(script + 0x2c, 0);
    player_score += 0x1f4;
    kill_thunk(boss);
}

/* level3 $056DC4: the draw of the boss of tiles, second form, for the frame after a hit */
void l3_boss2_flash(Object *obj)
{
    obj->fn_draw = (obj_fn)obj_nop_thunk;
    draw_tile_grid_flash(level3_data.boss2_update_data, 1, 1, (int16_t)(obj->y - scroll_pos), (uint16_t)(((uint16_t)(obj->x + 0x10) >> 1) & 0xfffc));
}

/* level3 $0556F6: a walker ($DC) from a record: the direction w6 of the record gives its first place and picture */
void l3_spawn_walker(const uint8_t *rec)
{
    Object *obj = alloc_in(LIST_C);
    uint16_t i = rd16(rec + 6);

    obj->type = 0xdc;
    obj->fn_update = FN(l3_walker_update);
    obj->fn_draw = draw_bob_masked;
    obj->walker.dir = (int16_t)i;
    SET_V5E(obj, 0);
    obj->x = (int16_t)(rd16(rec + 8) - 8 + level3_data.walker_spawn_dx[i]);
    obj->y = (int16_t)(rd16(rec + 10) - 8 - scroll_pos + level3_data.walker_spawn_dy[i]);
    set_anim(obj, level3_data.walker_spawn_anims[i]);
    obj->fn_hit = (void *)hit_enemy;
    obj->energy = (int16_t)level3_data.walker_energy;
    obj->big = 0;
    obj->score = 0x64;
    obj->walker.state = 0;
    obj->kind = 0;
    bbox_from_frame(obj);
}


/* level3 $0562B0: the walker: it goes on in its direction (v2a, 8 of them) by the steps of the tables at $56290 / $562A0 and at each tile of the map looks at
 * the cell ahead: a wall turns it by the tables at $56460, a free cell ahead lets it go on; now and then it fires at the player */
void l3_walker_update(Object *obj)
{
    uint32_t dir_offset = 2 * (uint32_t)(uint16_t)obj->walker.dir;
    int16_t dx, map_y;
    int at_tile = 0, turn = 0;

    anim_step(obj);
    obj->y = (int16_t)(obj->y + scroll_speed);
    if ((int16_t)(scroll_max + 0xd0 - scroll_pos) < obj->y) {
        kill_thunk(obj);
        return;
    }
    dx = level3_data.walker_update_words[dir_offset / 2];
    if (dx != 0) {
        obj->x = (int16_t)(obj->x + dx);
        at_tile = dx < 0 ? (obj->x & 0xf) == 0xf : (obj->x & 0xf) == 0;
    } else {
        int16_t step = level3_data.walker_update_words_2[dir_offset / 2];

        obj->y = (int16_t)(obj->y + step);
        map_y = (int16_t)(obj->y + scroll_pos);
        at_tile = step < 0 ? (map_y & 0xf) == 0xf : (map_y & 0xf) == 0;
    }
    if (at_tile) {
        int16_t x = obj->x;

        if (x == 0 || (uint16_t)x >= 0x140) {
            turn = 1;
        } else {
            uint16_t row = (uint16_t)((uint16_t)(obj->y + scroll_pos) >> 4);
            const uint8_t *tile = level_init_src + (int16_t)(uint16_t)(row * 0x28) + (((uint16_t)x >> 3) & 0xfe);

            if (rd16(tile) != 0) {
                obj->x = (int16_t)(obj->x - level3_data.walker_update_words[dir_offset / 2]);
                obj->y = (int16_t)(obj->y - level3_data.walker_update_words_2[dir_offset / 2]);
                turn = 1;
            } else {
                int16_t offset = level3_data.walker_update_words_3[dir_offset / 2];

                turn = rd16(tile + offset) == 0;
            }
        }
        if (turn) {
            uint16_t ni;

            obj->x = (int16_t)(obj->x + level3_data.walker_update_words_4[dir_offset / 2]);
            obj->y = (int16_t)(obj->y + level3_data.walker_update_words_5[dir_offset / 2]);
            ni = level3_data.walker_update_words_6[dir_offset / 2];
            obj->walker.dir = (int16_t)ni;
            set_anim(obj, level3_data.walker_spawn_anims[ni]);
        }
    }
    {
        uint16_t sum = (uint16_t)(obj->v5e_b[0] + level3_data.walker_fire_rate);

        obj->v5e_b[0] = (uint8_t)sum;
        if (sum > 0xff) {
            uint32_t gfx = sh_r32(sh_r32(0x6ee26 + 4 * (uint32_t)level3_data.walker_param));
            uint16_t angle;

            obj->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
            angle = direction8((int16_t)(PLAYER_X - obj->x), (int16_t)(PLAYER_Y - obj->y));
            spawn_enemy_shot((uint16_t)obj->x, (uint16_t)obj->y, angle, level3_data.walker_shot_steps, GPTR(gfx));
        }
    }
    bbox_from_frame(obj);
}

/* the picture of a worm's piece follows its phase v2c by a table of eight */
static void l3_worm_follow(Object *obj, const uint8_t *const *table)
{
    obj->y = (int16_t)(obj->y + scroll_speed);
    if (table == level3_data.worm_follow_pictures)
        sh_w8(0xd94 + (uint16_t)obj->worm.side, 0xff);
    enemy_update_no_anim(obj);
    obj->gfx = (void *)table[(((uint16_t)obj->worm.phase + 0x10) >> 5) & 7];
    bbox_from_frame(obj);
}

/* level3 $055420: the worm'shot head: its picture follows its phase and now and then it fires eight shots round it */
void l3_worm_head(Object *obj)
{
    uint16_t old = (uint16_t)obj->worm.phase;
    uint16_t frame, old_frame, sum;

    obj->y = (int16_t)(obj->y + scroll_speed);
    enemy_update(obj);
    frame = (uint16_t)((((uint16_t)obj->worm.phase + 0x10) >> 3) & 0x1c);
    old_frame = (uint16_t)(((old + 0x10) >> 3) & 0x1c);
    if (old_frame != frame)
        set_anim(obj, level3_data.worm_head_anims[frame / 4]);
    sum = (uint16_t)(obj->v5e_b[0] + level3_data.worm_head_fire_rate);
    obj->v5e_b[0] = (uint8_t)sum;
    if (sum > 0xff) {
        int i;

        for (i = 7; i >= 0; i--) {
            Object *shot = spawn_enemy_shot((uint16_t)obj->x, (uint16_t)obj->y, (uint16_t)i, level3_data.worm_head_shot_steps, 0);

            set_anim(shot, level3_data.worm_head_data);
        }
    }
    bbox_from_frame(obj);
}

/* level3 $05556A */
void l3_worm_a(Object *obj)
{
    l3_worm_follow(obj, level3_data.worm_a_pictures);
}

/* level3 $0555AE */
void l3_worm_b(Object *obj)
{
    l3_worm_follow(obj, level3_data.worm_b_pictures);
}

/* level3 $0555F2: the last piece: it shows the end's flag of its side (d94 / d95) */
void l3_worm_c(Object *obj)
{
    l3_worm_follow(obj, level3_data.worm_follow_pictures);
}

/* level3 $055642: the hit of the worm's first piece: the energy of its side (d66 / d68) runs out and the whole worm bursts and the side's flag d92 / d93 is set */
void l3_worm_hit(Object *shot, Object *worm, uint16_t damage)
{
    Object *o;
    uint32_t addr = 0xd66 + 2 * (uint32_t)(uint16_t)worm->worm.side;
    uint16_t old = W(addr);

    for (o = worm; o; o = o->chain)
        o->fn_draw = draw_bob_or_first;
    sh_w16(addr, (uint16_t)(old - damage));
    if (old > damage)
        return;
    sh_w8(0xd92 + (uint16_t)worm->worm.side, 0xff);
    sh_w8(0xd94 + (uint16_t)worm->worm.side, 0);
    for (o = worm->chain; o;) {
        Object *next = o->chain;

        kill_thunk(o);
        o = next;
    }
    kill_thunk(worm);
    debris_burst(shot, 0x13, 0, 0, 0x140, 0xc0, big_explosion_anim);
    free_object_chain(LIST_C->prev);
}
