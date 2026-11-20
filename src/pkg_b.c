#include "game.h"
#include "game_all.h"
#include "amiga/hw.h"
#include "pkg_b.h"

#define CIAA_CRA CIA_BYTE(0xbfe001, 0xe)    /* bit 7: the mains frequency (50 Hz) */
#define CIAA_ICR CIA_BYTE(0xbfe001, 0xd)
#define CIAA_SDR CIA_BYTE(0xbfe001, 0xc)    /* the serial data register: the keyboard's key code */
#define CIAB_ICR CIA_BYTE(0xbfd000, 0xd)

/* The starfield of the game keeps in each record of `stars` (struct Star of pkg_f): x = offset in the screen, y = bit mask, z = fractional line. */
#define STAR_OFF(s) ((s)->x)
#define STAR_MASK(s) ((s)->y)
#define STAR_POS(s) ((s)->z)

/* orig $ED8, $001F22: the background's scroll position (v_0436, high word) moves back by half the scroll speed; d0.w = scroll speed */
void scroll_pos_update(uint16_t d0)
{
    uint16_t d2 = d0;
    uint16_t d1 = v_0cdc & 1;

    d0 = (uint16_t)((int16_t)d0 >> 1);
    d1 &= d2;
    d0 += d1;
    if (d0 != 0) {
        d0 = (uint16_t)(v_0436 >> 16) - d0;
        if ((int16_t)d0 < 0)
            d0 += 0xc0;
        if ((int16_t)d0 >= 0xc0)
            d0 -= 0xc0;
        v_0436 = (v_0436 & 0xffffu) | ((uint32_t)d0 << 16);
    }
}

/* orig $ED2, $00200A: do two boxes overlap?  The original returns it in the Z flag: Z clear = overlap (returned here as 1).
 * d0,d1 = left, top of the first, d2,d3 = right, bottom; d4,d5 = left, top of the second, d6,d7 = right, bottom. */
uint16_t box_overlap(int16_t d0, int16_t d1, int16_t d2, int16_t d3, int16_t d4, int16_t d5, int16_t d6, int16_t d7)
{
    if (d6 < d0 || d4 > d2 || d7 < d1 || d5 > d3)
        return 0;
    return 1;
}

/* orig $002034: wait until the minimum number of vertical blanks has passed since the last flip, wait for the beam below line 258,
 * swap the draw and shown screens and start the copper list of the other one.  Inside $2026 and $202E. */
void swap_screens(void)
{
    uint8_t *t;
    uint32_t list;

    if ((uint16_t)(vbl_count - v_0404) >= v_0410)
        HOST_IDLE();   /* (the original's frame then takes real time, vertical blanks the interrupt uses to read the stick; the port's takes none) */
    while ((uint16_t)(vbl_count - v_0404) < v_0410)
        HOST_IDLE();
    v_0404 = vbl_count;
    while (!(custom->vposr & 1))
        HOST_IDLE();
    while ((custom->vhposr >> 8) < 2)
        HOST_IDLE();
    t = draw_screen;
    draw_screen = show_screen;
    show_screen = t;
    list = ADDR(copper_list_a) + ADDR(copper_list_b) - ADDR(copper_cur);   /* the lists alternate */
    copper_cur = (uint16_t *)list;
    custom->cop1lc = list;
    custom->copjmp1 = (uint16_t)list;   /* HLE: copper */
}

/* orig $002026: swap_screens for a 200-line screen; returns d7 = size of the screen in bytes per plane group (0x7D00) */
uint32_t swap_screens_200(void)
{
    swap_screens();
    return 0x7d00;
}

/* orig $00202E: swap_screens for a 192-line screen; returns d7 = 0x7800 */
uint32_t swap_screens_192(void)
{
    swap_screens();
    return 0x7800;
}

/* orig $EDE, $0022DA: scroll the starfield: each of the 48 stars moves by a step that grows with its index (d0.w = scroll speed);
 * the fractional line (z) wraps at 0x180 and moves the offset by one line (0xA0 bytes), the offset wraps at 0x7800. */
void stars_scroll(uint16_t d0)
{
    uint16_t d5 = *(const uint16_t *)((const uint8_t *)(star_speed + 16) + (int16_t)(uint16_t)(d0 * 2));
    uint16_t step = (uint16_t)(d0 << 3);

    for (int i = 0; i < 48; i++) {
        struct Star *s = &stars[i];
        uint16_t d6 = STAR_POS(s) + d5;

        while ((int16_t)d6 < 0) {
            STAR_OFF(s) -= 0xa0;
            if ((int16_t)STAR_OFF(s) < 0)
                STAR_OFF(s) += 0x7800;
            d6 += 0x180;
        }
        while ((int16_t)d6 >= 0x180) {
            STAR_OFF(s) += 0xa0;
            if ((int16_t)STAR_OFF(s) >= 0x7800)
                STAR_OFF(s) -= 0x7800;
            d6 -= 0x180;
        }
        STAR_POS(s) = d6;
        d5 += step;
    }
}

/* orig $00233A: draw the starfield into the draw screen: a star is drawn only where its four planes are empty under the mask;
 * four groups of 12 stars in four colours (planes 2; 0+2; 1+2; 0+1+2). */
void stars_draw(void)
{
    uint8_t *screen = draw_screen;

    stars_scroll(v_0cd8);
    for (int g = 0; g < 4; g++) {
        for (int i = 0; i < 12; i++) {
            struct Star *s = &stars[g * 12 + i];
            uint8_t *a2 = screen + (int16_t)STAR_OFF(s);
            uint16_t mask = STAR_MASK(s);
            uint16_t d4 = rd16(a2) | rd16(a2 + 0x28) | rd16(a2 + 0x50) | rd16(a2 + 0x78);

            if (d4 & mask)
                continue;
            if (g == 1 || g == 3)
                wr16(a2, rd16(a2) | mask);
            if (g == 2 || g == 3)
                wr16(a2 + 0x28, rd16(a2 + 0x28) | mask);
            wr16(a2 + 0x50, rd16(a2 + 0x50) | mask);
        }
    }
}

/* orig $E54, $002418: take an object off the free list (free_obj) and clear its flag bytes; the result is in a0 (0 here when the list is
 * empty: the original then searches the lists from $2436, goes on at $2436). */
Object *obj_free_pop(void)
{
    Object *a0 = free_obj;
    Object *next, *prev;

    if (a0 != 0) {
        free_obj = a0->next;
        goto clear;
    }
    /* $2436: the pool is empty: take an object away from the lists, the effects first (obj_list[3], obj_list[4] is the front list, obj_list[2] the enemies) */
    a0 = &obj_list[3];
    for (;;) {
        a0 = a0->prev;
        if (a0->type == 4 || a0->type == 0xc || a0->type == 0x10)
            goto steal;
        if (a0->type == 0)
            break;
    }
    a0 = obj_list[3].prev;
    if (a0->type != 0)
        goto steal;
    a0 = obj_list[4].prev;
    if (a0->type != 0)
        goto steal;
    a0 = obj_list[2].prev;   /* $247E: from the enemies' list head on, the first dead one or one far enough down */
    for (;;) {
        if ((int16_t)a0->type >= 0xc8 || a0->type == 4)
            goto steal;
        a0 = a0->prev;
        if (a0->type == 0)
            break;
    }
    a0 = obj_list[2].prev;
steal:
    next = a0->next;   /* $2464: take it out of its list */
    prev = a0->prev;
    next->prev = prev;
    prev->next = next;
clear:
    a0->flag34 = 0;
    a0->flag60 = 0;
    a0->flag61 = 0;
    return a0;
}

/* orig $0020C2: the level 3 interrupt (vertical blank): counts the frame, reads the joystick (port 2) and the fire button into
 * irq_joy / v_0a00, sends pending sound commands to the music player and calls its tick.  Ends with rte. */
void irq_vblank(void)
{
    if (custom->intreqr & 0x20) {
        uint16_t d0, d1;
        uint8_t joy = 0;

        vbl_count++;
        vbl_count++;
        d0 = custom->joy1dat;
        d1 = d0;
        if (d0 & 0x0002) joy |= 0x08;
        if (d0 & 0x0200) joy |= 0x04;
        d0 = (uint16_t)(d0 + d0) ^ d1;
        if (d0 & 0x0002) joy |= 0x02;
        if (d0 & 0x0200) joy |= 0x01;
        if (!(CIAA_PRA & 0x80))
            joy |= 0x80;
        irq_joy = joy;
        if (joy != irq_joy_prev) {
            if (v_0a01 == 0 && (joy & 0x80))
                v_0a01 = (v_0a00 & 0x80) ? 0 : 0xff;
            v_0a00 = joy;
            irq_joy_prev = joy;
        }
        if (v_0414 >= 0 || v_0418 >= 0) {
            void (*cmd)(uint16_t) = (void (*)(uint16_t))music_cmd_fn;   /* d0.w = command */

            if (v_0414 >= 0) {
                cmd((uint16_t)v_0414 | 0x200);
                v_0414 = -1;
            }
            if (v_0418 >= 0) {
                cmd((uint16_t)v_0418 | 0x100);
                v_0418 = -1;
            }
        }
        music_tick_fn();
    }
    SETCLR_WRITE(intreq, 0x70);
}

/* orig $000FAC: the level 2 interrupt (the keyboard through CIA A's serial port): when the port's flag is set the key code, inverted and
 * rotated right so that bit 7 is the release flag, is kept in key_code unless it is a release; the handshake is a pulse of 85 us
 * with the port in output mode.  Ends with rte. */
void irq_keyboard(void)
{
    if (CIAA_ICR & 0x08) {
        uint8_t d0 = (uint8_t)~CIAA_SDR;
        volatile uint16_t n = 0x36;

        d0 = (uint8_t)((d0 >> 1) | (d0 << 7));
        if (!(d0 & 0x80))
            key_code = d0;
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
    if ((int16_t)--irq_div < 0) {
        uint8_t *saved = draw_screen;

        irq_div = 2;
        draw_screen = show_screen;
        stars_erase();
        stars_update();
        draw_screen = saved;
    }
    irq_vblank();
}

/* The palette of the level (level_palette, 8 longwords of 12-bit colour pairs) goes to palette_shadow (halved per component when
 * v_0c90 is set) and, doubled, to the colour registers 0-15. */
static void apply_palette(void)
{
    const uint32_t *pal = level_palette;

    for (int i = 0; i < 8; i++) {
        uint32_t v = rd32(&pal[i]);

        if (v_0c90)
            v = (v >> 1) & 0x07770777u;
        palette_shadow[2 * i] = (uint16_t)(v >> 16);
        palette_shadow[2 * i + 1] = (uint16_t)v;
        v += v;
        custom->color[2 * i] = (uint16_t)(v >> 16);
        custom->color[2 * i + 1] = (uint16_t)v;
    }
}

static void music_restart_if_enabled(void)
{
    if (v_09fc) {
        music_start(0);
        music_flag_1a584 = 0x30;
    }
}

/* the camera / scroll speed step of the main loop ($79D6-$7A72) */
static void camera_step(void)
{
    uint16_t d7 = v_0cd0;
    uint16_t d0 = v_0cda;
    uint16_t d1, d2, d3;

    if (d0 != v_0ccc) {
        v_0ce0++;
        if ((int16_t)v_0ce0 >= 0x23) {
            d0 += d0;
            v_0cda = d0;
        }
    } else {
        v_0ce0 = 0;
    }
    d1 = v_0cd6;
    d2 = v_0cce;
    d3 = v_0cd0 - d0;
    if ((int16_t)d3 < (int16_t)v_0ce8) {
        d3 -= v_0ce8;
        d0 += d3;
        d3 = v_0ce8;
    }
    if ((int16_t)d3 > (int16_t)v_0cea) {
        d0 += d3;
        d0 -= v_0cea;
        d3 = v_0cea;
    }
    d1 -= d0;
    if ((int16_t)d1 < 0) {
        d1 += 0x10;
        d2 -= 0x28;
    } else if ((int16_t)d1 >= 0x10) {
        d1 -= 0x10;
        d2 += 0x28;
    }
    v_0cd8 = d0;
    v_0cd6 = d1;
    v_0cce = d2;
    v_0cd0 = d3;
    if (d7 != v_0cea || (v_0a00 & 2) == 0)
        d3 += v_0cee;
    if (!((int16_t)d3 > (int16_t)v_0cea))
        v_0cea = d3;
}

/* The target of a key in key_table: the addresses inside the original's main loop */
enum { KEY_PAUSE = 0x79ba, KEY_RESTART = 0x7800, KEY_FIRE = 0x797a };

/* orig $0079BA, $00797A: the keys of the main loop.  Returns 1 for the restart key (the main loop goes back to $7800). */
static int key_command(uint8_t key)
{
    uint32_t target = 0;

    for (int i = 0; i < 4; i++) {
        if ((key_table[i] >> 24) == key || i == 3) {   /* the original patches the last entry's code to the key */
            target = key_table[i] & 0xffffff;
            break;
        }
    }
    if (target == KEY_RESTART)
        return 1;
    if (target == KEY_PAUSE) {
        v_0a01 = 0;
        while (*(volatile uint8_t *)&v_0a01 == 0 && key_code == 0)
            HOST_IDLE();
        key_code = 0;
        v_0a01 = 0;
    } else if (target == KEY_FIRE) {
        if (v_0d84 == 0) {
            if (v_0430 == 0) {
                if (v_0ca0 != 0) {
                    v_0432 = 1;
                    v_0414 = 0x10;
                    v_0430 = 1;
                    v_0ca0--;
                    v_0434 = 0x88;
                }
            } else {
                v_0434 = 1;
            }
        }
    }
    return 0;
}

/* wait for the next vertical blank, flip, draw the starfield, show the level palette ($7D60 and $78A4 do the same) */
static void wait_vbl_and_show(void)
{
    uint16_t t = vbl_count;
    uint16_t colors[16];
    int i;

    while (t == vbl_count)
        HOST_IDLE();
    draw_background();
    stars_draw();
    swap_screens_192();
    for (i = 0; i < 16; i++)
        colors[i] = rd16((const uint16_t *)level_palette + i);
    palette_fade_in(colors);
}

/* orig $0078CE-$007D9A: the main loop, one pass for each frame.  Returns when the restart key was pressed or a player's game
 * ended with no one left (the original jumps back to $7800). */
static void main_loop(void)
{
    for (;;) {
        v_0cdc++;
        if ((v_0cdc & 3) == 0) {
            v_045c = v_0cec;
            v_045e = v_0cec;
            v_045e++;
            v_0cec += 2;
        }
        scroll_pos_update(v_0cd8);
        draw_background();
        v_0cda = 1;
        stars_draw();
        update_score_display();
        redraw_energy_bar();
        draw_enemy_list();
        level_scroll();
        update_all();
        frame_update();
        update_shots();
        scroll_events();
        if (key_code != 0) {
            uint8_t key = key_code;

            key_code = 0;
            if (key_command(key))
                return;
        }
        camera_step();
        swap_screens_192();
        apply_palette();
        if (v_0a02) {
            v_0a02 = 0;
            v_0c64 = 1;
            v_0cc6 = 0x27;
            v_0a6b = 0xff;
            v_0cca--;
            draw_status_digit();
            if (v_0cca == 0) {
                clear_palette();
                hiscore_enter();
                clear_palette();
                v_0cf0--;
                if (v_0cf0 == 0 || continue_screen()) {
                    v_0a03[cur_player] = 0;
                    if (v_0a03[cur_player ^ 1] == 0)
                        return;
                } else {
                    v_0cca = 3;
                    draw_status_digit();
                    v_0c92 = 0;
                    for (int i = 0; i < 7; i++)
                        v_0d7c[i] = 0;
                    v_0c96 = 0;
                    panel_clear_score();
                }
            }
            switch_player();
        }
        if (v_0ca4 != 0)
            continue;
        if (v_0a6c == 0) {
            if (v_0a07 == 0)
                continue;
        } else {
            uint16_t other;
            uint32_t d1 = 0;

            music_silence();
            if ((int16_t)v_0cd0 <= 0x1e0) {
                d1 = v_54e3c;
                v_09fa = (uint16_t)d1;
                d1 = (uint16_t)(d1 + 0x190);
                if ((int32_t)d1 > (int32_t)v_0c9a) {
                    uint16_t r = (uint16_t)((random_next() & 3) * 0x32);

                    d1 = (uint16_t)(d1 + r);
                    v_0c9a = d1;
                }
            } else {
                d1 = v_54e3e;
                v_09fa = (uint16_t)d1;
                if ((int32_t)d1 > (int32_t)v_0c9a) {
                    uint16_t r = (uint16_t)((random_next() & 3) * 0x32);

                    d1 = (uint16_t)(d1 + r);
                    v_0c9a = d1;
                }
            }
            v_0d83 = 0;
            v_8fec = 0;
            other = cur_player ^ 1;
            if ((int16_t)v_0cd0 > 0x3c0 || v_0e04 != 5 ||
                (v_8fec = 0xff, players != 2) || v_0a03[other] == 0 || v_0a03[other + 2] != 0) {
                shop_run();
                draw_status_digit();
            }
            v_0a6c = 0;
            if (v_0ca2)
                init_weapons(0);
            if (v_0a07 == 0) {
                if (v_09fc)
                    music_restart_if_enabled();
                continue;
            }
        }
        /* $7C62: the player's ship was lost or the level ended */
        v_0a07 = 0;
        if (v_0e04 == 5) {
            for (;;) {
                Object *a1 = obj_list[1].prev;

                if (a1->type == 0)
                    break;
                free_with_slot(a1);
            }
            refill_slot0(0, 0);
            latch_slots(0);
            v_0cf0++;
        }
        {
            uint16_t other = cur_player ^ 1;

            if (v_0a03[other] != 0 && v_0a03[other + 2] == 0) {
                v_0a03[cur_player + 2] = 0xff;
                clear_level_objects();
                free_object_chain(obj_list[2].prev);
                free_object_chain(obj_list[4].prev);
                free_object_chain(obj_list[3].prev);
                switch_player_after_clear();
                music_restart_if_enabled();
                continue;
            }
        }
        clear_palette();
        clear_level_objects();
        free_object_chain(obj_list[2].prev);
        free_object_chain(obj_list[4].prev);
        free_object_chain(obj_list[3].prev);
        v_0e04++;
        if ((int16_t)v_0e04 >= 6) {
            v_0e04 = 1;
            v_0e02++;
            v_8f94 = 1;
            v_8f96 = 0;
        }
        v_8c41 = (uint8_t)(v_0e04 + 0x30);
        level_load();
        create_queued_objects();
        if (v_0ca2)
            init_weapons(0);
        level_start();
        v_0a03[2] = 0;
        v_0a03[3] = 0;
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
    key_code = 0;
    v_0a00 = 0;
    v_0a01 = 0;
    v_0414 = -1;
    v_0416 = -1;
    v_0418 = -1;
    v_041a = -1;
    v_0404 = vbl_count;
    title_credits();
    menu();
    clear_palette();
    v_8f94 = 1;
    v_8f96 = 0;
    v_0e04 = 1;
    v_8c41 = 0x31;
    level_load();
    init_level_state();
    level_start();
    v_8836[18] = 0x31;
    if (v_0a03[0] == 0)
        v_8836[18] = 0x32;
    v_8428 = 0xff;
    title_message(v_8836);
    v_8428 = 0;
    clear_palette();
    music_restart_if_enabled();
    wait_vbl_and_show();
    v_0a00 = 0;
    v_0a01 = 0;
}

/* orig $0076A0-$0077C4: interrupt vectors, CIAs, display (4 lowres bitplanes, 320x192, copper list), DMA */
static void hardware_init(void)
{
    uint8_t sink;

    irq_vectors[0] = irq_disk_block;
    irq_vectors[1] = irq_keyboard;
    irq_vectors[2] = irq_vblank;
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
    custom->cop1lc = ADDR(copper_list_b);
    copper_cur = copper_list_b;
    custom->copjmp1 = 0;
}

/* orig $000400, $007032, $0070F6, $00763A: the program's entry (the SDL port calls this instead of `jmp $400`; `bra $7032` is
 * also the target of every unused exception vector).  The original starts with SR = $2700, stack = $435FC, sets the (junk) trace
 * vector $24 and branches around; here: vectors, screens, hardware, then the game never returns.
 *
 *  - the vertical-blank interrupt (level 3, irq_vblank at $20C2) is enabled by hardware_init and runs once the original lowers
 *    the interrupt mask (SR = $2000) at the end of hardware_init: it counts frames (vbl_count), reads the joystick and sends sound
 *    commands to the music player;  level 2 ($FAC, keyboard) and level 1 ($95E4, disk) are set there too;
 *  - game_session_start ($7800) shows the loading text and the menu text (via the title/menu code of pkg_g/pkg_f), then
 *    main_loop runs the game frame by frame; the level code of disk 2 is called through $54E24.
 *  - the menu itself is inside title_credits/menu (not C yet). */
void game_entry(void)
{
    uint16_t *rows = (uint16_t *)row_offset;
    uint16_t off;

    for (int i = 0; i < 23; i++)
        exc_vectors_lo[i] = game_entry;
    for (int i = 0; i < 7; i++)
        irq_vectors[i] = game_entry;
    for (int i = 0; i < 224; i++)
        exc_vectors_hi[i] = game_entry;
    clear_palette();
    v_0404 = 0;
    draw_screen = screen_b;
    show_screen = screen_a;
    v_0414 = -1;
    v_0416 = -1;
    v_0418 = -1;
    v_041a = -1;
    v_0410 = 4;
    music_paused = 0xff;
    if (!(CIAA_CRA & 0x80)) {
        v_0410 = 5;
        music_paused = 0;
    }
    hardware_init();
    /* SR = $2000: interrupts on */
    v_09fc = 0xff;
    v_041e = 1;
    v_09fd = 0;
    v_09fe = 0;
    clip_bottom = 0xbf;
    off = 0;
    for (int i = 0; i < 200; i++) {
        rows[i] = off;
        off += 0xa0;
    }
    stars_init_dots();
    players = 1;
    for (;;) {
        game_session_start();
        main_loop();
    }
}
