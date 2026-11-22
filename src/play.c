#include "play.h"
#include "game_all.h"
#include "amiga/hw.h"


/* inlined at its callers: the pair of list links of an append at the end of a list (before the head) */
static void list_append(Object *head, Object *o)
{
    Object *last = head->prev;

    head->prev = o;
    o->prev = last;
    o->next = head;
    last->next = o;
}

/* orig $006bc8 and $006bce: clears both screens (last+1 longwords each; $6bc8 last=$1f3f, $6bce last=$1dff) */
static void clear_screens(uint16_t last)
{
    uint32_t *a = (uint32_t *)screen_b;
    uint32_t *b = (uint32_t *)screen_a;
    uint32_t i;

    for (i = 0; i <= last; i++) {
        if (i < 0x1e00) {
            a[i] = 0;
            b[i] = 0;
        } else {
            int k;

            for (k = 0; k < 4; k++) {
                panel_b[(i - 0x1e00) * 4 + k] = 0;
                panel_a[(i - 0x1e00) * 4 + k] = 0;
            }
        }
    }
}

/* orig $006bc8: clears $1F40 longwords of both screens */
void clear_screens_large(void)
{
    clear_screens(0x1f3f);
}

/* orig $006bce: clears $1E00 longwords of both screens */
void clear_screens_small(void)
{
    clear_screens(0x1dff);
}

/* orig $00668c: fills the score area of both screens at the current player's place with the first glyph rows */
void panel_clear_score(void)
{
    const uint8_t *gfx = digit_gfx;
    uint32_t off = cur_player ? 0x1e : 2;   /* from the panel's start */
    uint8_t *pb = (uint8_t *)panel_b;
    uint8_t *pa = (uint8_t *)panel_a;
    uint16_t first = cur_player ? 0 : 1;
    int row;

    for (row = 0; row < 8; row++) {
        uint16_t col_off = 0;
        do {
            uint8_t byte = *gfx++;
            uint16_t k;

            for (k = first; k < first + 7; k++) {
                pb[off + col_off + k] = byte;
                pa[off + col_off + k] = byte;
            }
            col_off += 0x28;
        } while ((int16_t)col_off <= 0x78);
        off += 0xa0;
    }
}

/* orig $006676: the score area for both players */
void panel_init(void)
{
    panel_clear_score();
    cur_player ^= 1;
    panel_clear_score();
    cur_player ^= 1;
}

/* orig $006766: counts the displayed score up towards the real one by 10, 100 or 1000 and draws the digit that changed */
void update_score_display(void)
{
    int32_t diff = (int32_t)(player_score - score_shown);
    uint8_t *digit;
    uint32_t off;   /* from the panel's start */
    uint8_t *pb = (uint8_t *)panel_b;
    uint8_t *pa = (uint8_t *)panel_a;

    if (diff == 0)
        return;
    if (diff < 0) {
        score_shown = player_score;
        return;
    }
    if (diff >= 0xc8) {
        if (diff >= 0x7d0) {
            digit = &score_digits[3];
            off = 6;
            score_shown += 0x3e8;
        } else {
            digit = &score_digits[4];
            off = 7;
            score_shown += 0x64;
        }
    } else {
        digit = &score_digits[5];
        off = 8;
        score_shown += 0xa;
    }
    if (cur_player)
        off += 0x1b;
    for (;;) {
        const uint8_t *g;
        int row, k;

        (*digit)++;
        if ((int8_t)*digit >= 10)
            *digit = 0;
        g = digit_gfx + ((uint16_t)*digit << 5);
        for (row = 0; row < 8; row++) {
            for (k = 0; k < 4; k++) {
                uint8_t b = *g++;

                pb[off + row * 0xa0 + k * 0x28] = b;
                pa[off + row * 0xa0 + k * 0x28] = b;
            }
        }
        if (*digit != 0)
            return;
        digit--;
        off--;
        if (digit < score_digits)
            return;
    }
}

/* orig $006bea: sets up a level: flags, both screens' panel, the free list of objects, the shot list, the objects */
void init_level_state(void)
{
    uint8_t *dst_b, *dst_a;
    const uint16_t *words = panel_words;
    uint16_t g, c, i;
    Object *o;

    player_died = 0;
    player_active[0] = 0xff;
    respawn_shield = 8;
    difficulty = 1;
    cur_player = 0;
    player_active[1] = (players == 2) ? 0xff : 0;
    player_active[2] = 0;
    player_active[3] = 0;
    clear_screens_small();
    dst_b = panel_b;   /* what $6bce leaves in dst_b and dst_a: the panels */
    dst_a = panel_a;
    for (g = 0; g < 8; g++) {
        for (c = 0; c < 20; c++) {
            uint16_t word;

            word = *words++; wr16(dst_b, word); wr16(dst_a, word);
            word = *words++; wr16(dst_b + 40, word); wr16(dst_a + 40, word);
            word = *words++; wr16(dst_b + 80, word); wr16(dst_a + 80, word);
            word = *words++; wr16(dst_b + 120, word); wr16(dst_a + 120, word);
            dst_b += 2;
            dst_a += 2;
        }
        dst_b += 120;
        dst_a += 120;
    }
    boss_dead = 0;
    o = obj_pool;
    free_obj = o;
    for (i = 0; i < 158; i++) {
        Object *n = o + 1;

        o->type = 0;
        o->next = n;
        o = n;
    }
    o->type = 0;
    o->next = 0;
    for (i = 0; i < 80; i++) {
        shots[i].x = -1;
        shots[i].y = -1;
        shots[i].dx = -1;
        shots[i].dy = -1;
    }
    panel_init();
    init_objects();
    if (player_active[1]) {
        swap_player_state();
        init_objects();
        swap_player_state();
    }
}

/* orig $006990: empties the five object lists, puts in the player and its four options, clears the game's counters */
void init_objects(void)
{
    uint16_t i;
    Object *o;

    for (i = 0; i < 5; i++) {
        o = &obj_list[i];
        o->type = 0;
        o->prev = o;
        o->next = o;
        o->fn_update = obj_nop;
        o->fn_draw = obj_nop;
        o->fn_hit = obj_nop;
    }
    o = &player;
    list_append(&obj_list[0], o);
    o->type = 8;
    o->fn_update = player_update;
    o->fn_draw = draw_bob_masked;
    o->gfx = &player_gfx;
    o->anim_delay = 0;
    o->x = 0xa0;
    o->y = 0xb0;
    for (i = 0; i < 4; i++) {
        const uint8_t *rec = &option_anims[i * 24];   /* the record the table at $6CB2 points to */
        Object *h = &obj_list[0];
        Object *first;

        o = obj_free_pop();
        first = h->next;
        h->next = o;
        o->next = first;
        o->prev = h;
        first->prev = o;
        o->type = 0xc4;
        o->fn_update = option_update;
        o->fn_draw = obj_nop;
        anim_header_set(o, rec);
        o->x = 0xa0;
        o->y = 0xb0;
        o->option.index = 3 - i;
    }
    for (i = 0; i < 7; i++)
        weapon_slots[i].type = 0;
    weapon_94(0, 0);
    for (i = 0; i < 14; i++)
        slot_snapshot[i] = 0;
    latch_slots(0);
    autofire_counter = 8;
    autofire_period = 8;
    autofire_level = 1;
    crush_timer = 0;
    shades_timer = 0;
    dive_phase = 0;
    dive_step = 0;
    player_score = 0;
    score_shown = 0;
    speed_level = 0;
    dive_charges = 0;
    dive_timer = 0;
    nashwan_timer = 0;
    for (i = 0; i < 7; i++)
        score_digits[i] = 0;
    level_end_items = 0;
    player_energy = 0x27;
    shield_timer = 0;
    protection = 0;
    player_dead = 0;
    player_lives = 3;
    scroll_step_target = 1;
    skip_map_draw = 0;
    continues_left = 3;
    draw_status_digit();
    energy_dirty = 0xff;
    redraw_energy_bar();
}

/* orig $0068b0: an option's update: follows the player at an offset given by player_vx, fires when the button says so */
void option_update(Object *option)
{
    uint16_t x, index;
    uint8_t bit;

    anim_step(option);
    x = player.x;
    index = (uint16_t)player_vx;
    if ((int16_t)index < 0)
        index = (uint16_t)-(int16_t)index;
    index += index;
    if (option->option.index & 1)
        index += 0xe;
    x += (uint16_t)option_offsets[index / 2];
    option->x = x;
    option->y = player.y;
    option->fn_draw = obj_nop;
    if (dive_phase)
        return;
    bit = (option->option.index < 2) ? 1 : 2;
    if (joy_stick & bit)
        option->fn_draw = draw_bob_masked;
}

/* orig $00615a: makes a shot object ($5850) for the object owner; dx and dy go into its $2A and $2E, off_x/off_y are offsets from the player;
 * gfx_slot points at its graphics pointer (the next ones when owner's $42 is 1 or more) */
void spawn_shot_object(Object *owner, uint16_t dx, uint16_t dy, uint16_t off_x, uint16_t off_y, void *const *gfx_slot)
{
    Object *shot_obj;

    if (sfx_cmd < 0)
        sfx_cmd = 0x89;
    if (owner->score != 0) {
        if (owner->score != 1)
            gfx_slot++;
        gfx_slot++;
    }
    shot_obj = obj_free_pop();
    list_append(&obj_list[3], shot_obj);
    shot_obj->shot.dx = dx;
    shot_obj->shot.dy = dy;
    shot_obj->x = (uint16_t)(off_x + player.x);
    shot_obj->y = (uint16_t)(off_y + player.y);
    shot_obj->type = 0x10;
    shot_obj->fn_update = shot_update;
    shot_obj->fn_draw = draw_bob_masked;
    shot_obj->gfx = *gfx_slot;
    shot_obj->anim_delay = 0;
    shot_obj->shot.phase = 0;
    shot_obj->score = owner->score;
}

/* orig $00644e: empties list 1 of its objects and restores its ends and the block at $C66 from their saved copies */
void reset_extra_objects(void)
{
    Object *obj = obj_list[1].prev;
    uint16_t i;

    while (obj->type) {
        Object *p = obj->prev;

        free_with_slot(obj);
        obj = p;
    }
    obj_list[1].prev = list_a_prev_saved;
    obj_list[1].next = list_a_next_saved;
    for (i = 0; i < 7; i++)
        weapon_slots[i] = weapon_slots_saved[i];
}

/* orig $006426: the read of the stick for the autofire counter (the lockstep hooks it to give the port the byte the original read) */
uint8_t autofire_stick(void)
{
    return joy_stick;
}

/* orig $006372: the per-frame counters: the panel's fade, the palette load, the two timers, the autofire counter */
void frame_update(void)
{
    if (dive_phase != 0 || player_dead != 0) {
        if (respawn_shield != 0x10)
            respawn_shield++;
    } else if (respawn_shield != 0) {
        respawn_shield--;
    }
    if (shades_timer != 0) {
        shades_timer--;
        if (shades_timer == 0) {
            const uint32_t *palette = level_palette;
            uint16_t i;

            for (i = 0; i < 8; i++) {
                uint32_t d = rd32(&palette[i]);

                palette_shadow[i * 2] = (uint16_t)(d >> 16);
                palette_shadow[i * 2 + 1] = (uint16_t)d;
                d += d;
                custom->color[i * 2] = (uint16_t)(d >> 16);
                custom->color[i * 2 + 1] = (uint16_t)d;
            }
        }
    }
    if (dive_timer != 0) {
        dive_timer--;
        draw_gauge(0x98, 8, dive_timer, 0x88);
        if (dive_timer == 0) {
            dive_step = 0xffff;
            sfx_cmd = 0x10;
        }
    }
    if (nashwan_timer != 0) {
        nashwan_timer--;
        draw_gauge(0x98, 0xa0, nashwan_timer, 0xaa);
        if (nashwan_timer == 0)
            reset_extra_objects();
    }
    if (autofire_stick() & 0x80) {
        int16_t a = (int16_t)autofire_counter;
        int16_t b = (int16_t)autofire_level;

        autofire_counter = (uint16_t)(a - b);
        if (!(a > b)) {
            autofire_counter += autofire_period;
            key_pressed = 0xff;
        }
    } else {
        autofire_counter = autofire_period;
    }
}

/* orig $0061e6: moves the 80 shot dots and draws them into the screen being drawn */
void update_shots(void)
{
    uint16_t i;

    for (i = 0; i < 80; i++) {
        Shot *s = &shots[i];
        uint16_t x, y, x16;
        uint8_t *dst;
        uint32_t mask2, mask3;
        uint16_t lo2, hi2, lo3, hi3;

        if (s->x < 0)
            continue;
        x = (uint16_t)(s->x + s->dx);
        y = (uint16_t)(s->y + s->dy);
        if (x >= 0x13c || y >= 0xbc) {
            s->x = -1;
            continue;
        }
        s->x = x;
        s->y = y;
        if (!point_in_enemy(x, y)) {
            s->x = -1;
            continue;
        }
        dst = draw_screen;
        dst += (int16_t)row_offset[y];
        x16 = x & 0xf;
        dst += (uint16_t)((x ^ x16) >> 3);
#define ORW(off, v) wr16(dst + (off), (uint16_t)(rd16(dst + (off)) | (v)))
#define ANDW(off, v) wr16(dst + (off), (uint16_t)(rd16(dst + (off)) & (v)))
        if ((int16_t)x16 <= 0xc) {
            lo2 = (uint16_t)(0xe000u >> x16 | 0xe000u << (16 - x16));
            lo3 = (uint16_t)(0x4000u >> x16 | 0x4000u << (16 - x16));
            hi2 = hi3 = 0;
        } else {
            mask2 = 0xe000u;
            mask3 = 0x4000u;
            mask2 = mask2 >> x16 | mask2 << (32 - x16);
            mask3 = mask3 >> x16 | mask3 << (32 - x16);
            lo2 = (uint16_t)mask2;
            hi2 = (uint16_t)(mask2 >> 16);
            lo3 = (uint16_t)mask3;
            hi3 = (uint16_t)(mask3 >> 16);
        }
        ORW(0, lo2);
        ORW(0x28, lo3);
        ORW(0x50, lo2);
        ORW(0x140, lo2);
        ORW(0x168, lo3);
        ORW(0x190, lo2);
        ANDW(0x78, (uint16_t)~lo3);
        ANDW(0x1b8, (uint16_t)~lo3);
        ORW(0xa0, lo2);
        ORW(0xc8, lo2);
        ORW(0xf0, lo2);
        ANDW(0x118, (uint16_t)~lo2);
        if ((int16_t)x16 > 0xc) {
            ORW(2, hi2);
            ORW(0x2a, hi3);
            ORW(0x52, hi2);
            ORW(0x1e2, hi2);
            ORW(0x20a, hi3);
            ORW(0x232, hi2);
            ANDW(0x7a, (uint16_t)~hi3);
            ANDW(0x25a, (uint16_t)~hi3);
            ORW(0xa2, hi2);
            ORW(0xca, hi2);
            ORW(0xf2, hi2);
            ORW(0x142, hi2);
            ORW(0x16a, hi2);
            ORW(0x192, hi2);
            ANDW(0x11a, (uint16_t)~hi2);
            ANDW(0x1ba, (uint16_t)~hi2);
        }
#undef ORW
#undef ANDW
    }
}

/* orig $00683c: draws the objects of list 2 and counts those that are not of type $50 or $54 */
void draw_enemy_list(void)
{
    Object *obj = obj_list[2].prev;
    uint16_t count = 0;
    uint16_t type;

    while ((type = obj->type) != 0) {
        if (type != 0x50 && type != 0x54)
            count++;
        obj = owner_chain_next(obj);
    }
    target_count = count;
}

/* orig $F02, $006862: puts the objects of a chain (towards the previous ones) back on the free list */
void free_object_chain(Object *obj)
{
    while (obj->type) {
        Object *earlier = obj->prev;
        Object *after = obj->next;
        Object *before = obj->prev;

        after->prev = before;
        before->next = after;
        obj->next = free_obj;
        free_obj = obj;
        obj->type = 0;
        obj = earlier;
    }
}

/* orig $006d74: makes an object for each of the 7 queued (type, value) pairs; the value goes into the object's $42 */
void create_queued_objects(void)
{
    uint16_t i;

    for (i = 0; i < 7; i++) {
        uint16_t type = slot_snapshot[i * 2];
        uint16_t value = slot_snapshot[i * 2 + 1];

        if (type != 0) {
            Object *obj = weapon_select(type, 0, 0);

            obj->score = value;
        }
    }
}

/* orig $006d22: empties the object lists 2 to 4, and the shot dots */
void clear_level_objects(void)
{
    uint16_t scroll_delta;
    Object *obj;
    uint16_t i;

    sfx_stop_all();
    if (nashwan_timer)
        reset_extra_objects();
    scroll_delta = scroll_y - scroll_pos;
    free_chain(obj_list[4].prev, scroll_delta);
    free_chain(obj_list[3].prev, scroll_delta);
    free_chain(obj_list[2].prev, scroll_delta);
    for (obj = obj_list[1].prev; obj->type; obj = obj_list[1].prev)
        free_with_slot(obj);
    for (i = 0; i < 80; i++) {
        shots[i].x = -1;
        shots[i].y = -1;
        shots[i].dx = -1;
        shots[i].dy = -1;
    }
}

/* orig $006e70: exchanges the state of the two players: the block at $A76, the player object, the level data */
void swap_player_state(void)
{
    uint32_t *a = other_player_block;
    uint32_t *b = (uint32_t *)obj_list;   /* the block runs on over the variables that follow the lists */
    uint8_t *pa = (uint8_t *)&player;
    uint8_t *pb = other_player_object;
    uint8_t *la = level_init_src;
    uint8_t *lb = other_player_level;
    uint16_t i;

    for (i = 0; i < 202; i++) {
        uint32_t t = a[i];

        a[i] = b[i];
        b[i] = t;
    }
    for (i = 0; i < 98; i++) {
        uint8_t t = pa[i];

        pa[i] = pb[i];
        pb[i] = t;
    }
    for (i = 0; i < 12000; i++) {
        uint8_t t = la[i];

        la[i] = lb[i];
        lb[i] = t;
    }
    cur_player ^= 1;
    level_swap_hook();
}

/* orig $006fe8: all 16 palette entries, the copy and the chip's, to black */
void clear_palette(void)
{
    uint16_t i;

    for (i = 0; i < 16; i++) {
        palette_shadow[i] = 0;
        custom->color[i] = 0;
    }
}

/* orig $006d9c: puts the player back at the start of a level section (scroll positions, the shot trails, the "PLAYER n" text) */
void respawn_player(void)
{
    uint16_t pos, i;

    player_dead = 0;
    player.x = player_x_saved;
    player.y = 0xb0;
    pos = scroll_y;
    scroll_pos = pos;
    scroll_max = pos;
    pos++;
    next_event_a = pos;
    pos += 0xbf;
    next_event_b = pos;
    scroll_fine = scroll_y & 0xf;
    map_row_offset = (uint16_t)((scroll_y >> 4) * 0x28);
    create_queued_objects();
    player_money = player_money_saved;
    for (i = 0; i < 16; i++)
        pickup_groups[i] = 0;
    dive_phase = 0;
    crush_timer = 0;
    energy_dirty = 0xff;
    player_vx = 0;
    for (i = 0; i < 4; i++) {
        player_trail[i * 2] = player.x;
        player_trail[i * 2 + 1] = player.y;
    }
    for (i = 0; i < 17; i++) {
        player_history[i * 3] = scroll_pos;
        player_history[i * 3 + 1] = player.x;
        player_history[i * 3 + 2] = player.y;
    }
    ready_text[18] = (uint8_t)(cur_player + 0x31);
    text_wait_key = 0xff;
    title_message(ready_text);
    text_wait_key = 0;
    clear_palette();
    if (nashwan_timer)
        init_weapons(0);
}

/* orig $006ec6: after a death: clears the level objects and, if the other player is in, gives the turn to him */
void switch_player(void)
{
    clear_level_objects();
    switch_player_after_clear();
}

/* orig $006eca: switch_player after the level objects are cleared */
void switch_player_after_clear(void)
{
    uint16_t who;

    who = cur_player;
    if (player_active[who]) {
        who ^= 1;
        if (player_active[who]) {
            if (player_active[2 + who] == 0)
                swap_player_state();
        }
    } else {
        swap_player_state();
        who = cur_player;
        if (player_active[2 + who]) {
            boss_dead = 0xff;
            if (level_number == 5)
                level_over = 0xff;
        }
    }
    respawn_player();
    respawn_shield = 8;
}

/* orig $00635E: a gauge of ten steps: the picture for fill / (max + 1) * 10 at (x, y); the original's table has no picture for a full
 * gauge (it would draw code bytes), nothing is drawn then */
void draw_gauge(uint16_t x, uint16_t y, uint16_t fill, uint16_t max)
{
    uint32_t dividend = (uint32_t)fill * 10;
    uint16_t divisor = (uint16_t)(max + 1);
    uint32_t q = dividend / divisor;
    uint16_t pic = q > 0xffff ? (uint16_t)dividend : (uint16_t)q;

    if (pic < 10)
        draw_bob_masked_at(x, y, gauge_pics[pic]);
}

/* orig $006f1e: the scroll and timing variables at the start of a level */
void level_reset_scroll(void)
{
    map_row_offset = 0x2d00;
    scroll_fine = 0;
    scroll_pos = 0x1200;
    next_event_a = 0x7fee;
    next_event_b = 0x7fee;
    scroll_margin = 0x10;
    player_x_saved = player.x;
    player_y_saved = player.y;
    scroll_y = scroll_pos;
    player_money_saved = player_money;
    scroll_speed = 1;
    scroll_step = 0;
    player_vx = 0;
    level_init();
    scroll_accel_frames = 0;
    crush_timer = 0;
    frame_tick = 0;
    player_money = 0;
    player_money_saved = 0;
    level_end_items = 0;
    pickup_cycle = 1;
    advice_hints = 0;
}

/* orig $006f94: copies the level's initial data in and resets the start state of each player */
void level_start(void)
{
    const uint8_t *src = level_init_src;
    uint16_t i;

    for (i = 0; i < 12000; i++)
        other_player_level[i] = src[i];
    background_scroll = 0;
    target_count = 0;
    trail_count = 0;
    for (i = 0; i < 16; i++)
        pickup_groups[i] = 0;
    level_over = 0;
    joy_stick = 0;
    key_pressed = 0;
    level_reset_scroll();
    if (players == 2) {
        swap_player_state();
        level_reset_scroll();
        swap_player_state();
    }
}
