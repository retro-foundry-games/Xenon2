#include <string.h>
#include "game.h"
#include "game_all.h"
#include "amiga/hw.h"
#ifdef GAME_SDL
uint8_t guest_a6a(uint8_t a01);   /* sdl/guest.c */
extern void guest_stick_restore(void);   /* sdl/guest.c: undoes what the lockstep put into joy_stick */
#endif


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
            pos += SCREEN_ROWS;
        if ((int16_t)pos >= SCREEN_ROWS)
            pos -= SCREEN_ROWS;
        xenon2.background_scroll = (xenon2.background_scroll & 0xffffu) | ((uint32_t)pos << 16);
    }
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
    list = (uintptr_t)(xenon2.copper_list_a) + (uintptr_t)(xenon2.copper_list_b) - (uintptr_t)(xenon2.copper_cur);   /* the lists alternate */
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
        row_off -= MAP_ROW_BYTES;
    } else if ((int16_t)fine >= 0x10) {
        fine -= 0x10;
        row_off += MAP_ROW_BYTES;
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
    custom->cop1lc = (uintptr_t)(xenon2.copper_list_b);
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
    xenon2.clip_bottom = SCREEN_ROWS - 1;
    off = 0;
    for (int i = 0; i < SCREEN_ROWS + PANEL_ROWS; i++) {
        rows[i] = off;
        off += SCREEN_ROW_BYTES;
    }
    stars_init_dots();
    xenon2.players = 1;
    for (;;) {
        game_session_start();
        main_loop();
    }
}

/* orig $006bc8 and $006bce: clears the first `rows` rows of both screens (the panel's rows follow the screen's: $6bc8 clears 200, $6bce 192) */
static void clear_screens(uint16_t rows)
{
    for (uint16_t row = 0; row < rows; row++) {
        uint16_t *a = row < SCREEN_ROWS ? &xenon2.screen_b[row * SCREEN_ROW_WORDS] : &xenon2.panel_b[(row - SCREEN_ROWS) * SCREEN_ROW_WORDS];
        uint16_t *b = row < SCREEN_ROWS ? &xenon2.screen_a[row * SCREEN_ROW_WORDS] : &xenon2.panel_a[(row - SCREEN_ROWS) * SCREEN_ROW_WORDS];

        memset(a, 0, SCREEN_ROW_BYTES);
        memset(b, 0, SCREEN_ROW_BYTES);
    }
}

/* orig $006bc8: clears the screens and the panels */
void clear_screens_large(void)
{
    clear_screens(SCREEN_ROWS + PANEL_ROWS);
}

/* orig $006bce: clears the screens */
void clear_screens_small(void)
{
    clear_screens(SCREEN_ROWS);
}

/* orig $006bea: sets up a level: flags, both screens' panel, the free list of objects, the shot list, the objects */
void init_level_state(void)
{
    uint16_t *dst_b, *dst_a;
    const uint16_t *words = xenon2.panel_words;
    uint16_t g, c, i;
    Object *o;

    xenon2.player_died = 0;
    xenon2.player_active[0] = 0xff;
    xenon2.respawn_shield = 8;
    xenon2.difficulty = 1;
    xenon2.cur_player = 0;
    xenon2.player_active[1] = (xenon2.players == 2) ? 0xff : 0;
    xenon2.player_active[2] = 0;
    xenon2.player_active[3] = 0;
    clear_screens_small();
    dst_b = xenon2.panel_b;   /* what $6bce leaves in dst_b and dst_a: the panels */
    dst_a = xenon2.panel_a;
    for (g = 0; g < 8; g++) {
        for (c = 0; c < 20; c++) {
            uint16_t word;

            word = *words++; dst_b[0] = word; dst_a[0] = word;
            word = *words++; dst_b[SCREEN_PLANE_WORDS] = word; dst_a[SCREEN_PLANE_WORDS] = word;
            word = *words++; dst_b[2 * SCREEN_PLANE_WORDS] = word; dst_a[2 * SCREEN_PLANE_WORDS] = word;
            word = *words++; dst_b[3 * SCREEN_PLANE_WORDS] = word; dst_a[3 * SCREEN_PLANE_WORDS] = word;
            dst_b++;
            dst_a++;
        }
        dst_b += 3 * SCREEN_PLANE_WORDS;
        dst_a += 3 * SCREEN_PLANE_WORDS;
    }
    xenon2.boss_dead = 0;
    o = xenon2.obj_pool;
    xenon2.free_obj = o;
    for (i = 0; i < 158; i++) {
        Object *n = o + 1;

        o->type = 0;
        o->next = n;
        o = n;
    }
    o->type = 0;
    o->next = 0;
    for (i = 0; i < 80; i++) {
        xenon2.shots[i].x = -1;
        xenon2.shots[i].y = -1;
        xenon2.shots[i].dx = -1;
        xenon2.shots[i].dy = -1;
    }
    panel_init();
    init_objects();
    if (xenon2.player_active[1]) {
        swap_player_state();
        init_objects();
        swap_player_state();
    }
}

/* orig $006426: the read of the stick for the autofire counter (the lockstep hooks it to give the port the byte the original read) */
uint8_t autofire_stick(void)
{
    return xenon2.joy_stick;
}

/* orig $006372: the per-frame counters: the panel's fade, the palette load, the two timers, the autofire counter */
void frame_update(void)
{
    if (xenon2.dive_phase != 0 || xenon2.player_dead != 0) {
        if (xenon2.respawn_shield != 0x10)
            xenon2.respawn_shield++;
    } else if (xenon2.respawn_shield != 0) {
        xenon2.respawn_shield--;
    }
    if (xenon2.shades_timer != 0) {
        xenon2.shades_timer--;
        if (xenon2.shades_timer == 0) {
            const uint16_t *palette = xenon2.level_palette;
            uint16_t i;

            for (i = 0; i < 16; i++) {
                xenon2.palette_shadow[i] = palette[i];
                custom->color[i] = (uint16_t)(palette[i] + palette[i]);
            }
        }
    }
    if (xenon2.dive_timer != 0) {
        xenon2.dive_timer--;
        draw_gauge(0x98, 8, xenon2.dive_timer, 0x88);
        if (xenon2.dive_timer == 0) {
            xenon2.dive_step = 0xffff;
            xenon2.sfx_cmd = 0x10;
        }
    }
    if (xenon2.nashwan_timer != 0) {
        xenon2.nashwan_timer--;
        draw_gauge(0x98, 0xa0, xenon2.nashwan_timer, 0xaa);
        if (xenon2.nashwan_timer == 0)
            reset_extra_objects();
    }
    if (autofire_stick() & 0x80) {
        int16_t a = (int16_t)xenon2.autofire_counter;
        int16_t b = (int16_t)xenon2.autofire_level;

        xenon2.autofire_counter = (uint16_t)(a - b);
        if (!(a > b)) {
            xenon2.autofire_counter += xenon2.autofire_period;
            xenon2.key_pressed = 0xff;
        }
    } else {
        xenon2.autofire_counter = xenon2.autofire_period;
    }
}

/* orig $00683c: draws the objects of list 2 and counts those that are not of type $50 or $54 */
void draw_enemy_list(void)
{
    Object *obj = xenon2.obj_list[2].prev;
    uint16_t count = 0;
    uint16_t type;

    while ((type = obj->type) != 0) {
        if (type != 0x50 && type != 0x54)
            count++;
        obj = owner_chain_next(obj);
    }
    xenon2.target_count = count;
}

/* orig $006e70: exchanges the state of the two players: the block at $A76, the player object, the level data */
void swap_player_state(void)
{
    uint32_t *a = xenon2.other_player_block;
    uint32_t *b = (uint32_t *)xenon2.obj_list;   /* the block runs on over the variables that follow the lists */
    uint8_t *pa = (uint8_t *)&xenon2.player;
    uint8_t *pb = xenon2.other_player_object;
    uint16_t *la = (uint16_t *)xenon2.level_init_src;
    uint16_t *lb = xenon2.other_player_level;
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
    for (i = 0; i < 6000; i++) {
        uint16_t t = la[i];

        la[i] = lb[i];
        lb[i] = t;
    }
    xenon2.cur_player ^= 1;
    level_swap_hook();
}

/* orig $006fe8: all 16 palette entries, the copy and the chip's, to black */
void clear_palette(void)
{
    uint16_t i;

    for (i = 0; i < 16; i++) {
        xenon2.palette_shadow[i] = 0;
        custom->color[i] = 0;
    }
}

/* orig $006d9c: puts the player back at the start of a level section (scroll positions, the shot trails, the "&xenon2.player n" text) */
void respawn_player(void)
{
    uint16_t pos, i;

    xenon2.player_dead = 0;
    xenon2.player.x = xenon2.player_x_saved;
    xenon2.player.y = 0xb0;
    pos = xenon2.scroll_y;
    xenon2.scroll_pos = pos;
    xenon2.scroll_max = pos;
    pos++;
    xenon2.next_event_a = pos;
    pos += SCREEN_ROWS - 1;
    xenon2.next_event_b = pos;
    xenon2.scroll_fine = xenon2.scroll_y & 0xf;
    xenon2.map_row_offset = (uint16_t)MAP_ROW(xenon2.scroll_y >> 4);
    create_queued_objects();
    xenon2.player_money = xenon2.player_money_saved;
    for (i = 0; i < 16; i++)
        xenon2.pickup_groups[i] = 0;
    xenon2.dive_phase = 0;
    xenon2.crush_timer = 0;
    xenon2.energy_dirty = 0xff;
    xenon2.player_vx = 0;
    for (i = 0; i < 4; i++) {
        xenon2.player_trail[i * 2] = xenon2.player.x;
        xenon2.player_trail[i * 2 + 1] = xenon2.player.y;
    }
    for (i = 0; i < 17; i++) {
        xenon2.player_history[i * 3] = xenon2.scroll_pos;
        xenon2.player_history[i * 3 + 1] = xenon2.player.x;
        xenon2.player_history[i * 3 + 2] = xenon2.player.y;
    }
    xenon2.ready_text[18] = (uint8_t)(xenon2.cur_player + 0x31);
    xenon2.text_wait_key = 0xff;
    title_message(xenon2.ready_text);
    xenon2.text_wait_key = 0;
    clear_palette();
    if (xenon2.nashwan_timer)
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

    who = xenon2.cur_player;
    if (xenon2.player_active[who]) {
        who ^= 1;
        if (xenon2.player_active[who]) {
            if (xenon2.player_active[2 + who] == 0)
                swap_player_state();
        }
    } else {
        swap_player_state();
        who = xenon2.cur_player;
        if (xenon2.player_active[2 + who]) {
            xenon2.boss_dead = 0xff;
            if (xenon2.level_number == 5)
                xenon2.level_over = 0xff;
        }
    }
    respawn_player();
    xenon2.respawn_shield = 8;
}

/* orig $006f1e: the scroll and timing variables at the start of a level */
void level_reset_scroll(void)
{
    xenon2.map_row_offset = MAP_ROW(288);
    xenon2.scroll_fine = 0;
    xenon2.scroll_pos = 0x1200;
    xenon2.next_event_a = 0x7fee;
    xenon2.next_event_b = 0x7fee;
    xenon2.scroll_margin = 0x10;
    xenon2.player_x_saved = xenon2.player.x;
    xenon2.player_y_saved = xenon2.player.y;
    xenon2.scroll_y = xenon2.scroll_pos;
    xenon2.player_money_saved = xenon2.player_money;
    xenon2.scroll_speed = 1;
    xenon2.scroll_step = 0;
    xenon2.player_vx = 0;
    level_init();
    xenon2.scroll_accel_frames = 0;
    xenon2.crush_timer = 0;
    xenon2.frame_tick = 0;
    xenon2.player_money = 0;
    xenon2.player_money_saved = 0;
    xenon2.level_end_items = 0;
    xenon2.pickup_cycle = 1;
    xenon2.advice_hints = 0;
}

/* orig $006f94: copies the level's initial data in and resets the start state of each player */
void level_start(void)
{
    const uint16_t *src = (const uint16_t *)xenon2.level_init_src;
    uint16_t i;

    for (i = 0; i < 6000; i++)
        xenon2.other_player_level[i] = src[i];
    xenon2.background_scroll = 0;
    xenon2.target_count = 0;
    xenon2.trail_count = 0;
    for (i = 0; i < 16; i++)
        xenon2.pickup_groups[i] = 0;
    xenon2.level_over = 0;
    xenon2.joy_stick = 0;
    xenon2.key_pressed = 0;
    level_reset_scroll();
    if (xenon2.players == 2) {
        swap_player_state();
        level_reset_scroll();
        swap_player_state();
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
