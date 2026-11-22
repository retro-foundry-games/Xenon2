#include "game.h"
#include "game_all.h"
#include "amiga/hw.h"
#include "mainloop.h"
#ifdef GAME_SDL
extern void guest_stick_restore(void);   /* sdl/guest.c: undoes what the lockstep put into joy_stick */
#endif


/* The starfield of the game keeps in each record of `stars` (struct Star of title): x = offset in the screen, y = bit mask, z = fractional line. */
#define STAR_OFF(s) ((s)->x)
#define STAR_MASK(s) ((s)->y)
#define STAR_POS(s) ((s)->z)

/* orig $ED8, $001F22: the background's scroll position (background_scroll, high word) moves back by half the scroll speed; speed = scroll speed */
void scroll_pos_update(uint16_t speed)
{
    uint16_t full_speed = speed;
    uint16_t odd = xenon2.frame_tick & 1;
    uint16_t step;

    step = (uint16_t)((int16_t)speed >> 1);
    odd &= full_speed;
    step += odd;
    if (step != 0) {
        uint16_t pos;

        pos = (uint16_t)(xenon2.background_scroll >> 16) - step;
        if ((int16_t)pos < 0)
            pos += 0xc0;
        if ((int16_t)pos >= 0xc0)
            pos -= 0xc0;
        xenon2.background_scroll = (xenon2.background_scroll & 0xffffu) | ((uint32_t)pos << 16);
    }
}

/* orig $ED2, $00200A: do two boxes overlap?  The original returns it in the Z flag: Z clear = overlap (returned here as 1).
 * a_left, a_top, a_right, a_bottom = the first; b_left, b_top, b_right, b_bottom = the second. */
uint16_t box_overlap(int16_t a_left, int16_t a_top, int16_t a_right, int16_t a_bottom, int16_t b_left, int16_t b_top, int16_t b_right, int16_t b_bottom)
{
    if (b_right < a_left || b_left > a_right || b_bottom < a_top || b_top > a_bottom)
        return 0;
    return 1;
}

/* orig $002034: wait until the minimum number of vertical blanks has passed since the last flip, wait for the beam below line 258,
 * swap the draw and shown screens and start the copper list of the other one.  Inside $2026 and $202E. */
void swap_screens(void)
{
    uint8_t *old_draw;
    uint32_t list;

    if ((uint16_t)(xenon2.vbl_count - xenon2.last_flip_vbl) >= xenon2.min_vbl_per_frame)
        HOST_IDLE();   /* (the original's frame then takes real time, vertical blanks the interrupt uses to read the stick; the port's takes none) */
    while ((uint16_t)(xenon2.vbl_count - xenon2.last_flip_vbl) < xenon2.min_vbl_per_frame)
        HOST_IDLE();
    xenon2.last_flip_vbl = xenon2.vbl_count;
    while (!(custom->vposr & 1))
        HOST_IDLE();
    while ((custom->vhposr >> 8) < 2)
        HOST_IDLE();
    old_draw = xenon2.draw_screen;
    xenon2.draw_screen = xenon2.show_screen;
    xenon2.show_screen = old_draw;
    list = ADDR(xenon2.copper_list_a) + ADDR(xenon2.copper_list_b) - ADDR(xenon2.copper_cur);   /* the lists alternate */
    xenon2.copper_cur = (uint16_t *)list;
    custom->cop1lc = list;
    custom->copjmp1 = (uint16_t)list;   /* HLE: copper */
}

/* orig $002026: swap_screens for a 200-line screen; returns the size of the screen in bytes per plane group (0x7D00) */
uint32_t swap_screens_200(void)
{
    swap_screens();
    return 0x7d00;
}

/* orig $00202E: swap_screens for a 192-line screen; returns 0x7800 */
uint32_t swap_screens_192(void)
{
    swap_screens();
    return 0x7800;
}

/* orig $EDE, $0022DA: scroll the starfield: each of the 48 stars moves by a step that grows with its index (speed = scroll speed);
 * the fractional line (z) wraps at 0x180 and moves the offset by one line (0xA0 bytes), the offset wraps at 0x7800. */
void stars_scroll(uint16_t speed)
{
    uint16_t vel = *(const uint16_t *)((const uint8_t *)(xenon2.star_speed + 16) + (int16_t)(uint16_t)(speed * 2));
    uint16_t step = (uint16_t)(speed << 3);

    for (int i = 0; i < 48; i++) {
        struct Star *s = &xenon2.stars[i];
        uint16_t frac = STAR_POS(s) + vel;

        while ((int16_t)frac < 0) {
            STAR_OFF(s) -= 0xa0;
            if ((int16_t)STAR_OFF(s) < 0)
                STAR_OFF(s) += SCREEN_ROWS * SCREEN_ROW_BYTES;
            frac += 0x180;
        }
        while ((int16_t)frac >= 0x180) {
            STAR_OFF(s) += 0xa0;
            if ((int16_t)STAR_OFF(s) >= 0x7800)
                STAR_OFF(s) -= 0x7800;
            frac -= 0x180;
        }
        STAR_POS(s) = frac;
        vel += step;
    }
}

/* orig $00233A: draw the starfield into the draw screen: a star is drawn only where its four planes are empty under the mask;
 * four groups of 12 stars in four colours (planes 2; 0+2; 1+2; 0+1+2). */
void stars_draw(void)
{
    uint8_t *screen = xenon2.draw_screen;

    stars_scroll(xenon2.scroll_speed);
    for (int g = 0; g < 4; g++) {
        for (int i = 0; i < 12; i++) {
            struct Star *s = &xenon2.stars[g * 12 + i];
            uint16_t *p = (uint16_t *)(screen + (int16_t)STAR_OFF(s));
            uint16_t mask = STAR_MASK(s);
            uint16_t planes = p[0] | p[SCREEN_PLANE_WORDS] | p[2 * SCREEN_PLANE_WORDS] | p[3 * SCREEN_PLANE_WORDS];

            if (planes & mask)
                continue;
            if (g == 1 || g == 3)
                p[0] = p[0] | mask;
            if (g == 2 || g == 3)
                p[SCREEN_PLANE_WORDS] = p[SCREEN_PLANE_WORDS] | mask;
            p[2 * SCREEN_PLANE_WORDS] = p[2 * SCREEN_PLANE_WORDS] | mask;
        }
    }
}

/* orig $E54, $002418: take an object off the free list (free_obj) and clear its flag bytes; the result is returned (0 here when the list is
 * empty: the original then searches the lists from $2436, goes on at $2436). */
Object *obj_free_pop(void)
{
    Object *obj = xenon2.free_obj;
    Object *next, *prev;

    if (obj != 0) {
        xenon2.free_obj = obj->next;
        goto clear;
    }
    /* $2436: the pool is empty: take an object away from the lists, the effects first (obj_list[3], obj_list[4] is the front list, obj_list[2] the enemies) */
    obj = &xenon2.obj_list[3];
    for (;;) {
        obj = obj->prev;
        if (obj->type == 4 || obj->type == 0xc || obj->type == 0x10)
            goto steal;
        if (obj->type == 0)
            break;
    }
    obj = xenon2.obj_list[3].prev;
    if (obj->type != 0)
        goto steal;
    obj = xenon2.obj_list[4].prev;
    if (obj->type != 0)
        goto steal;
    obj = xenon2.obj_list[2].prev;   /* $247E: from the enemies' list head on, the first dead one or one far enough down */
    for (;;) {
        if ((int16_t)obj->type >= 0xc8 || obj->type == 4)
            goto steal;
        obj = obj->prev;
        if (obj->type == 0)
            break;
    }
    obj = xenon2.obj_list[2].prev;
steal:
    next = obj->next;   /* $2464: take it out of its list */
    prev = obj->prev;
    next->prev = prev;
    prev->next = next;
clear:
    obj->is_part = 0;
    obj->persists = 0;
    obj->screen_fixed = 0;
    return obj;
}

#ifdef GAME_SDL
static int edge_defer, edge_pending;
static uint8_t edge_old;

/* the fire press the interrupt saw is put into key_pressed now (when the interrupt that sees it arrives later in the original's frame than it does here) */
void guest_edge_apply(uint8_t value)
{
    if (edge_pending && (value & 0x80)) {
        edge_pending = 0;
        if (xenon2.key_pressed == 0)
            xenon2.key_pressed = (edge_old & 0x80) ? 0 : 0xff;
    }
}

/* the frame's end: a press that no routine saw is in key_pressed for the next frame */
void guest_edge_defer(int on)
{
    if (edge_pending) {
        edge_pending = 0;
        if (xenon2.key_pressed == 0)
            xenon2.key_pressed = (edge_old & 0x80) ? 0 : 0xff;
    }
    edge_defer = on;
}
#endif

/* orig $0020C2: the level 3 interrupt (vertical blank): counts the frame, reads the joystick (port 2) and the fire button into
 * irq_joy / joy_stick, sends pending sound commands to the music player and calls its tick.  Ends with rte. */
void irq_vblank(void)
{
    if (custom->intreqr & 0x20) {
        uint16_t dat, raw;
        uint8_t joy = 0;

        xenon2.vbl_count++;
        xenon2.vbl_count++;
        dat = custom->joy1dat;
        raw = dat;
        if (dat & 0x0002) joy |= 0x08;
        if (dat & 0x0200) joy |= 0x04;
        dat = (uint16_t)(dat + dat) ^ raw;
        if (dat & 0x0002) joy |= 0x02;
        if (dat & 0x0200) joy |= 0x01;
        if (!(CIAA_PRA & 0x80))
            joy |= 0x80;
        xenon2.irq_joy = joy;
#ifdef GAME_SDL
        guest_stick_restore();
#endif
        if (joy != xenon2.irq_joy_prev) {
#ifdef GAME_SDL
            if (edge_defer && xenon2.key_pressed == 0 && (joy & 0x80)) {
                edge_pending = 1;
                edge_old = xenon2.joy_stick;
            } else
#endif
            if (xenon2.key_pressed == 0 && (joy & 0x80))
                xenon2.key_pressed = (xenon2.joy_stick & 0x80) ? 0 : 0xff;
            xenon2.joy_stick = joy;
            xenon2.irq_joy_prev = joy;
        }
        if (xenon2.sfx_cmd >= 0 || xenon2.sfx_cmd2 >= 0) {
            void (*cmd)(uint16_t) = (void (*)(uint16_t))xenon2.music_cmd_fn;   /* the command is the argument */

            if (xenon2.sfx_cmd >= 0) {
                cmd((uint16_t)xenon2.sfx_cmd | 0x200);
                xenon2.sfx_cmd = -1;
            }
            if (xenon2.sfx_cmd2 >= 0) {
                cmd((uint16_t)xenon2.sfx_cmd2 | 0x100);
                xenon2.sfx_cmd2 = -1;
            }
        }
        xenon2.music_tick_fn();
    }
    SETCLR_WRITE(intreq, 0x70);
}

/* orig $000FAC: the level 2 interrupt (the keyboard through CIA A's serial port): when the port's flag is set the key code, inverted and
 * rotated right so that bit 7 is the release flag, is kept in key_code unless it is a release; the handshake is a pulse of 85 us
 * with the port in output mode.  Ends with rte. */
void irq_keyboard(void)
{
    if (CIAA_ICR & 0x08) {
        uint8_t key = (uint8_t)~CIAA_SDR;
        volatile uint16_t n = 0x36;

        key = (uint8_t)((key >> 1) | (key << 7));
        if (!(key & 0x80))
            xenon2.key_code = key;
        CIAA_CRA |= 0x40;
        do {
        } while (n--);
        CIAA_CRA &= (uint8_t)~0x40;
    }
    SETCLR_WRITE(intreq, 0x0008);
}

/* orig $002098: the level 3 interrupt of the title and menu: every third vertical blank first runs two routines of the title on the
 * shown screen as the draw screen, then the normal interrupt. */
void irq_vblank_hooked(void)
{
    if ((int16_t)--xenon2.irq_div < 0) {
        uint8_t *saved = xenon2.draw_screen;

        xenon2.irq_div = 2;
        xenon2.draw_screen = xenon2.show_screen;
        stars_erase();
        stars_update();
        xenon2.draw_screen = saved;
    }
    irq_vblank();
}

/* The palette of the level (level_palette, 16 colours of 12 bits) goes to palette_shadow (halved per component when shades_timer is set) and,
 * doubled, to the colour registers 0-15. */
static void apply_palette(void)
{
    const uint16_t *pal = xenon2.level_palette;

    for (int i = 0; i < 16; i++) {
        uint16_t w = pal[i];

        if (xenon2.shades_timer)
            w = (uint16_t)((w >> 1) & 0x0777);
        xenon2.palette_shadow[i] = w;
        custom->color[i] = (uint16_t)(w + w);
    }
}

static void music_restart_if_enabled(void)
{
    if (xenon2.music_switch) {
        music_start(0);
        xenon2.music_volume_hi = 0x30;
    }
}

void camera_limit(uint16_t pos, uint16_t limit) __attribute__((noinline));

/* orig $007A56: the scroll limit follows the camera unless the camera stands at it with the stick down (the lockstep hooks it to give the port the
 * stick byte the original read) */
void camera_limit(uint16_t pos, uint16_t limit)
{
    if (pos != xenon2.scroll_max || (xenon2.joy_stick & 2) == 0)
        limit += xenon2.scroll_margin;
    if (!((int16_t)limit > (int16_t)xenon2.scroll_max))
        xenon2.scroll_max = limit;
}

/* the camera / scroll speed step of the main loop ($79D6-$7A72) */
static void camera_step(void)
{
    uint16_t old_pos = xenon2.scroll_pos;
    uint16_t step = xenon2.scroll_step;
    uint16_t fine, row_off, new_pos;

    if (step != xenon2.scroll_step_target) {
        xenon2.scroll_accel_frames++;
        if ((int16_t)xenon2.scroll_accel_frames >= 0x23) {
            step += step;
            xenon2.scroll_step = step;
        }
    } else {
        xenon2.scroll_accel_frames = 0;
    }
    fine = xenon2.scroll_fine;
    row_off = xenon2.map_row_offset;
    new_pos = xenon2.scroll_pos - step;
    if ((int16_t)new_pos < (int16_t)xenon2.scroll_min) {
        new_pos -= xenon2.scroll_min;
        step += new_pos;
        new_pos = xenon2.scroll_min;
    }
    if ((int16_t)new_pos > (int16_t)xenon2.scroll_max) {
        step += new_pos;
        step -= xenon2.scroll_max;
        new_pos = xenon2.scroll_max;
    }
    fine -= step;
    if ((int16_t)fine < 0) {
        fine += 0x10;
        row_off -= 0x28;
    } else if ((int16_t)fine >= 0x10) {
        fine -= 0x10;
        row_off += 0x28;
    }
    xenon2.scroll_speed = step;
    xenon2.scroll_fine = fine;
    xenon2.map_row_offset = row_off;
    xenon2.scroll_pos = new_pos;
    camera_limit(old_pos, new_pos);
}

/* The target of a key in key_table: the addresses inside the original's main loop */
enum { KEY_PAUSE = 0x79ba, KEY_RESTART = 0x7800, KEY_FIRE = 0x797a };

/* orig $0079BA, $00797A: the keys of the main loop.  Returns 1 for the restart key (the main loop goes back to $7800). */
static int key_command(uint8_t key)
{
    uint32_t target = 0;

    for (int i = 0; i < 4; i++) {
        if ((xenon2.key_table[i] >> 24) == key || i == 3) {   /* the original patches the last entry's code to the key */
            target = xenon2.key_table[i] & 0xffffff;
            break;
        }
    }
    if (target == KEY_RESTART)
        return 1;
    if (target == KEY_PAUSE) {
        xenon2.key_pressed = 0;
        while (*(volatile uint8_t *)&xenon2.key_pressed == 0 && xenon2.key_code == 0)
            HOST_IDLE();
        xenon2.key_code = 0;
        xenon2.key_pressed = 0;
    } else if (target == KEY_FIRE) {
        if (xenon2.player_dead == 0) {
            if (xenon2.dive_phase == 0) {
                if (xenon2.dive_charges != 0) {
                    xenon2.dive_step = 1;
                    xenon2.sfx_cmd = 0x10;
                    xenon2.dive_phase = 1;
                    xenon2.dive_charges--;
                    xenon2.dive_timer = 0x88;
                }
            } else {
                xenon2.dive_timer = 1;
            }
        }
    }
    return 0;
}

/* wait for the next vertical blank, flip, draw the starfield, show the level palette ($7D60 and $78A4 do the same) */
static void wait_vbl_and_show(void)
{
    uint16_t vbl_start = xenon2.vbl_count;
    uint16_t colors[16];
    int i;

    while (vbl_start == xenon2.vbl_count)
        HOST_IDLE();
    draw_background();
    stars_draw();
    swap_screens_192();
    for (i = 0; i < 16; i++)
        colors[i] = xenon2.level_palette[i];
    palette_fade_in(colors);
}

/* orig $0078CE-$007D9A: the main loop, one pass for each frame.  Returns when the restart key was pressed or a player's game
 * ended with no one left (the original jumps back to $7800). */
static void main_loop(void)
{
    for (;;) {
        xenon2.frame_tick++;
        if ((xenon2.frame_tick & 3) == 0) {
            xenon2.pickup_kind_a = xenon2.pickup_cycle;
            xenon2.pickup_kind_b = xenon2.pickup_cycle;
            xenon2.pickup_kind_b++;
            xenon2.pickup_cycle += 2;
        }
        scroll_pos_update(xenon2.scroll_speed);
        draw_background();
        xenon2.scroll_step = 1;
        stars_draw();
        update_score_display();
        redraw_energy_bar();
        draw_enemy_list();
        level_scroll();
        update_all();
        frame_update();
        update_shots();
        scroll_events();
        if (xenon2.key_code != 0) {
            uint8_t key = xenon2.key_code;

            xenon2.key_code = 0;
            if (key_command(key))
                return;
        }
        camera_step();
        swap_screens_192();
        apply_palette();
        if (xenon2.player_died) {
            xenon2.player_died = 0;
            xenon2.autofire_level = 1;
            xenon2.player_energy = 0x27;
            xenon2.energy_dirty = 0xff;
            xenon2.player_lives--;
            draw_status_digit();
            if (xenon2.player_lives == 0) {
                clear_palette();
                hiscore_enter();
                clear_palette();
                xenon2.continues_left--;
                if (xenon2.continues_left == 0 || continue_screen() == 0) {
                    title_message(xenon2.game_over_text);
                    xenon2.player_active[xenon2.cur_player] = 0;
                    if (xenon2.player_active[xenon2.cur_player ^ 1] == 0)
                        return;
                } else {
                    xenon2.player_lives = 3;
                    draw_status_digit();
                    xenon2.player_score = 0;
                    for (int i = 0; i < 7; i++)
                        xenon2.score_digits[i] = 0;
                    xenon2.score_shown = 0;
                    panel_clear_score();
                }
            }
            switch_player();
        }
        if (xenon2.level_end_items != 0)
            continue;
        if (xenon2.level_over == 0) {
            if (xenon2.boss_dead == 0)
                continue;
        } else {
            uint16_t other;
            uint32_t money = 0;

            music_silence();
            if ((int16_t)xenon2.scroll_pos <= 0x1e0) {
                money = xenon2.shop_money_early;
                xenon2.shop_price_cap = (uint16_t)money;
                money = (uint16_t)(money + 0x190);
                if ((int32_t)money > (int32_t)xenon2.player_money) {
                    uint16_t bonus = (uint16_t)((random_next() & 3) * 0x32);

                    money = (uint16_t)(money + bonus);
                    xenon2.player_money = money;
                }
            } else {
                money = xenon2.shop_money_late;
                xenon2.shop_price_cap = (uint16_t)money;
                if ((int32_t)money > (int32_t)xenon2.player_money) {
                    uint16_t bonus = (uint16_t)((random_next() & 3) * 0x32);

                    money = (uint16_t)(money + bonus);
                    xenon2.player_money = money;
                }
            }
            xenon2.protection = 0;
            xenon2.shop_closing = 0;
            other = xenon2.cur_player ^ 1;
            if ((int16_t)xenon2.scroll_pos > 0x3c0 || xenon2.level_number != 5 ||
                (xenon2.shop_closing = 0xff, xenon2.players != 2) || xenon2.player_active[other] == 0 || xenon2.player_active[other + 2] != 0) {
                shop_run();
                draw_status_digit();
            }
            xenon2.level_over = 0;
            if (xenon2.nashwan_timer)
                init_weapons(0);
            if (xenon2.boss_dead == 0) {
                if (xenon2.music_switch)
                    music_restart_if_enabled();
                continue;
            }
        }
        /* $7C62: the player's ship was lost or the level ended */
        xenon2.boss_dead = 0;
        if (xenon2.level_number == 5) {
            for (;;) {
                Object *obj = xenon2.obj_list[1].prev;

                if (obj->type == 0)
                    break;
                free_with_slot(obj);
            }
            refill_slot0(0, 0);
            latch_slots(0);
            xenon2.continues_left++;
        }
        {
            uint16_t other = xenon2.cur_player ^ 1;

            if (xenon2.player_active[other] != 0 && xenon2.player_active[other + 2] == 0) {
                xenon2.player_active[xenon2.cur_player + 2] = 0xff;
                clear_level_objects();
                free_object_chain(xenon2.obj_list[2].prev);
                free_object_chain(xenon2.obj_list[4].prev);
                free_object_chain(xenon2.obj_list[3].prev);
                switch_player_after_clear();
                music_restart_if_enabled();
                continue;
            }
        }
        clear_palette();
        clear_level_objects();
        free_object_chain(xenon2.obj_list[2].prev);
        free_object_chain(xenon2.obj_list[4].prev);
        free_object_chain(xenon2.obj_list[3].prev);
        xenon2.level_number++;
        if ((int16_t)xenon2.level_number >= 6) {
            xenon2.level_number = 1;
            xenon2.difficulty++;
            xenon2.cycle_flag_a = 1;
            xenon2.cycle_flag_b = 0;
        }
        xenon2.loading_level_digit = (uint8_t)(xenon2.level_number + 0x30);
        level_load();
        create_queued_objects();
        if (xenon2.nashwan_timer)
            init_weapons(0);
        level_start();
        xenon2.player_active[2] = 0;
        xenon2.player_active[3] = 0;
        switch_player();
        wait_vbl_and_show();
        music_restart_if_enabled();
    }
}

/* orig $007800: (re)start of a game: palette faded out, level text and "GET READY" shown, then the first frame */
static void game_session_start(void)
{
    clear_palette();
    clear_screens_large();
    xenon2.key_code = 0;
    xenon2.joy_stick = 0;
    xenon2.key_pressed = 0;
    xenon2.sfx_cmd = -1;
    xenon2.sfx_cmd_aux = -1;
    xenon2.sfx_cmd2 = -1;
    xenon2.sfx_cmd2_aux = -1;
    xenon2.last_flip_vbl = xenon2.vbl_count;
    title_credits();
    menu();
    clear_palette();
    xenon2.cycle_flag_a = 1;
    xenon2.cycle_flag_b = 0;
    xenon2.level_number = 1;
    xenon2.loading_level_digit = 0x31;
    level_load();
    init_level_state();
    level_start();
    xenon2.ready_text[18] = 0x31;
    if (xenon2.player_active[0] == 0)
        xenon2.ready_text[18] = 0x32;
    xenon2.text_wait_key = 0xff;
    title_message(xenon2.ready_text);
    xenon2.text_wait_key = 0;
    clear_palette();
    music_restart_if_enabled();
    wait_vbl_and_show();
    xenon2.joy_stick = 0;
    xenon2.key_pressed = 0;
}

/* orig $0076A0-$0077C4: interrupt vectors, CIAs, display (4 lowres bitplanes, 320x192, copper list), DMA */
static void hardware_init(void)
{
    uint8_t sink;

    xenon2.irq_vectors[0] = irq_disk_block;
    xenon2.irq_vectors[1] = irq_keyboard;
    xenon2.irq_vectors[2] = irq_vblank;
    CIAB_PRB = 0xff;
    CIAB_PRB = 0x87;
    CIAB_PRB = 0xff;
    custom->bplcon0 = 0x4000;
    custom->bplcon1 = 0;
    custom->ddfstrt = 0x38;
    custom->ddfstop = 0xd0;
    custom->diwstrt = 0x3a81;
    custom->diwstop = 0x02c1;
    custom->bpl1mod = 0x78;
    custom->bpl2mod = 0x78;
    custom->potgo = 0x0c00;
    SETCLR_WRITE(intreq, 0x3fff);
    SETCLR_WRITE(intena, 0x3fd5);
    SETCLR_WRITE(intena, 0xc02a);
    SETCLR_WRITE(dmacon, 0x002f);
    SETCLR_WRITE(dmacon, 0x87d0);
    CIAA_ICR = 0x1f;
    CIAB_ICR = 0x1f;
    CIAA_ICR = 0x88;
    sink = CIAA_ICR;
    sink = CIAB_ICR;
    (void)sink;
    SETCLR_WRITE(adkcon, 0x00ff);
    for (int i = 0; i < 8; i++) {
        custom->spr[i][2] = 0;
        custom->spr[i][3] = 0;
    }
    custom->cop1lc = ADDR(xenon2.copper_list_b);
    xenon2.copper_cur = xenon2.copper_list_b;
    custom->copjmp1 = 0;
}

/* orig $000400, $007032, $0070F6, $00763A: the program's entry (the SDL port calls this instead of `jmp $400`; `bra $7032` is
 * also the target of every unused exception vector).  The original starts with SR = $2700, stack = $435FC, sets the (junk) trace
 * vector $24 and branches around; here: vectors, screens, hardware, then the game never returns.
 *
 *  - the vertical-blank interrupt (level 3, irq_vblank at $20C2) is enabled by hardware_init and runs once the original lowers
 *    the interrupt mask (SR = $2000) at the end of hardware_init: it counts frames (vbl_count), reads the joystick and sends sound
 *    commands to the music player;  level 2 ($FAC, keyboard) and level 1 ($95E4, disk) are set there too;
 *  - game_session_start ($7800) shows the loading text and the menu text (via the title/menu code of menus/title), then
 *    main_loop runs the game frame by frame; the level code of disk 2 is called through $54E24.
 *  - the menu itself is inside title_credits/menu (not C yet). */
void game_entry(void)
{
    uint16_t *rows = (uint16_t *)xenon2.row_offset;
    uint16_t off;

    for (int i = 0; i < 23; i++)
        xenon2.exc_vectors_lo[i] = game_entry;
    for (int i = 0; i < 7; i++)
        xenon2.irq_vectors[i] = game_entry;
    for (int i = 0; i < 224; i++)
        xenon2.exc_vectors_hi[i] = game_entry;
    clear_palette();
    xenon2.last_flip_vbl = 0;
    xenon2.draw_screen = (uint8_t *)xenon2.screen_b;
    xenon2.show_screen = (uint8_t *)xenon2.screen_a;
    xenon2.sfx_cmd = -1;
    xenon2.sfx_cmd_aux = -1;
    xenon2.sfx_cmd2 = -1;
    xenon2.sfx_cmd2_aux = -1;
    xenon2.min_vbl_per_frame = 4;
    xenon2.music_paused = 0xff;
    if (!(CIAA_CRA & 0x80)) {
        xenon2.min_vbl_per_frame = 5;
        xenon2.music_paused = 0;
    }
    hardware_init();
    /* SR = $2000: interrupts on */
    xenon2.music_switch = 0xff;
    xenon2.crush_reload = 1;
    xenon2.music_aux_a = 0;
    xenon2.music_aux_b = 0;
    xenon2.clip_bottom = 0xbf;
    off = 0;
    for (int i = 0; i < 200; i++) {
        rows[i] = off;
        off += 0xa0;
    }
    stars_init_dots();
    xenon2.players = 1;
    for (;;) {
        game_session_start();
        main_loop();
    }
}
