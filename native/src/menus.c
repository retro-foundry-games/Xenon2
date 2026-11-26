#include "shop.h"
#include "menus.h"
#include "game_all.h"
#include "title.h"
#include "draw.h"
#ifdef GAME_SDL
int host_skip_menu(void);   /* sdl/main.c */
#endif

/* orig $8F9A: sector 0 of the disk must hold "X2B ": returns 1 when it does (Z set) */
int game_disk_present(void)
{
    struct DiskScratch *scratch = (struct DiskScratch *)xenon2.screen_a;
    int ok;

    if (xenon2.draw_screen != (uint8_t *)xenon2.screen_a) {
        uint32_t *dst = (uint32_t *)xenon2.screen_b;
        const uint32_t *src = (const uint32_t *)xenon2.screen_a;
        int i;

        for (i = 0; i < 0x1f40; i++)
            *dst++ = *src++;
        swap_screens_200();
    }
    xenon2.irq_vectors[2] = irq_vblank_hooked;
    disk_read_sectors(0, 1, scratch->boot);
    xenon2.irq_vectors[2] = irq_vblank;
    ok = scratch->boot[1] == 0x5832 && scratch->boot[2] == 0x4220;
    return ok;
}

#define MENU_LINE 20   /* the characters of a line of menu_text */

/* ---- title lines, fade, high-score table ---- */

/* the line of 20 characters squeezes in (a title page over the starfield) */
static void title_in(const uint8_t *s)
{
    title_stars_start();
    show_text_sequence(0xc, s, xenon2.wipe_in);
}

/* the line squeezes out, the palette and the dot stars come back */
static void title_out(const uint8_t *s)
{
    show_text_sequence(0xc, s, xenon2.wipe_out);
    title_restore();
}

/* orig $8C58: the HIGH SCORE line comes in */
void title_in_hiscore(void) { title_in(xenon2.title_hiscore); }
/* orig $8C60: the ENTERING SHOP line comes in */
void title_in_shop(void) { title_in(xenon2.shop_text); }
/* orig $8C68: the INSERT GAME DISK line comes in */
void title_in_insert_disk(void) { title_in(xenon2.insert_disk_text); }
/* orig $8C70: the LOADING LEVEL line comes in */
void title_in_loading(void) { title_in(xenon2.loading_text); }
/* orig $8C88: the HIGH SCORE line goes out */
void title_out_hiscore(void) { title_out(xenon2.title_hiscore); }
/* orig $8C90: the ENTERING SHOP line goes out */
void title_out_shop(void) { title_out(xenon2.shop_text); }
/* orig $8C98: the INSERT GAME DISK line goes out */
void title_out_insert_disk(void) { title_out(xenon2.insert_disk_text); }
/* orig $8CA0: the LOADING LEVEL line goes out */
void title_out_loading(void) { title_out(xenon2.loading_text); }

/* orig $885E: the 16 colours of the palette each lose 1 in every component, 8 times, waiting until vbl_count passes wait before
 * each step */
void palette_fade_out(int16_t wait)
{
    int16_t n;

#ifdef GAME_SDL
    presentation_reset();
#endif
    for (n = 7; n >= 0; n--) {
        int i;

        xenon2.vbl_count = 0;
        while ((int16_t)wait >= (int16_t)xenon2.vbl_count)
            HOST_IDLE();
        for (i = 0; i < 16; i++) {
            uint16_t c = (xenon2.palette_shadow[i] & 0x777) - 0x111;

            if (c & 0x8)
                c += 1;
            if (c & 0x80)
                c += 0x10;
            if (c & 0x800)
                c += 0x100;
            xenon2.palette_shadow[i] = c;
            custom->color[i] = (uint16_t)(c << 1);
        }
    }
}

/* orig $88B6: the palette goes from black to the colours at target in 7 steps, one every 2 vertical blanks; the colour registers
 * get each colour doubled */
void palette_fade_in(const uint16_t *target)
{
    uint16_t level;

    for (level = 0x666;; ) {
        int i;

        xenon2.vbl_count = 0;
        while ((int16_t)xenon2.vbl_count < 2)
            HOST_IDLE();
        for (i = 0; i < 16; i++) {
            uint16_t c = (target[i] & 0x777) - level;

            while (c & 0x8)
                c++;
            while (c & 0x80)
                c += 0x10;
            while (c & 0x800)
                c += 0x100;
            c &= 0x777;
            xenon2.palette_shadow[i] = c;
            custom->color[i] = (uint16_t)(c << 1);
        }
        level -= 0x111;
        if ((int16_t)level < 0)
            return;
    }
}

/* orig $8B6A: 15 characters of the big font (16 rows of 4 plane words) drawn into the four planes at dest, a space leaves a gap;
 * dest steps one word to the right for each character */
void draw_big_text(const uint8_t *text, uint16_t *dest)
{
    const uint16_t *font = xenon2.font_gfx;
    int n;

    for (n = 14; n >= 0; n--) {
        uint8_t ch = *text++;
        int row;
        const uint16_t *g;
        int16_t idx;

        if (ch == 0x20) {
            dest += 1;
            continue;
        }
        idx = (int16_t)find_glyph(ch, xenon2.text_chars);   /* inlined: $7EF8 */
        idx = (int16_t)((uint16_t)idx << 6);
        g = font + idx;
        for (row = 0; row < 16; row++) {
            dest[0] = *g++;
            dest[SCREEN_PLANE_WORDS] = *g++;
            dest[2 * SCREEN_PLANE_WORDS] = *g++;
            dest[3 * SCREEN_PLANE_WORDS] = *g++;
            dest += SCREEN_ROW_WORDS;
        }
        dest -= 16 * SCREEN_ROW_WORDS - 1;
    }
}

/* orig $8B20: one frame of the high-score table: the heading and the ten lines, drawn with the big font */
void hiscore_draw(void)
{
    const uint8_t *line = xenon2.hiscore_lines;
    uint16_t *dest = (uint16_t *)SCREEN_POS(xenon2.draw_screen, 38, 4);
    int i;

    clear_screen();
    xenon2.title_scale = 0x10;
    xenon2.title_y = 0xc;
    draw_text_squeezed(xenon2.title_hiscore);
    for (i = 9; i >= 0; i--) {
        draw_big_text(line, dest);
        line += 15;
        dest += 16 * SCREEN_ROW_WORDS;
    }
    stars_update();
    swap_screens_200();
}

/* orig $89DC: the high-score table comes in, stays 85 frames or until fire, and goes out */
void hiscore_show(void)
{
    int16_t n = 0x55;

    show_text_sequence(0xc, xenon2.title_hiscore, xenon2.wipe_in);
    for (;;) {
        hiscore_draw();
        if (xenon2.key_pressed)
            break;
        if (--n == 0) {
            show_text_sequence(0xc, xenon2.title_hiscore, xenon2.wipe_out);
            break;
        }
    }
}

/* orig $8AB0: the first read of the stick in the name entry (the lockstep hooks it to give the port the byte the original read) */
uint8_t hiscore_stick_first(void)
{
    return xenon2.joy_stick;
}

/* orig $8AEE: the read of the stick while a letter repeats */
uint8_t hiscore_stick_again(void)
{
    return xenon2.joy_stick;
}

/* orig $8A16: a score above the tenth is put into the table in its place; the player picks the three letters of the name with the
 * stick (up/down), fire takes a letter */
void hiscore_enter(void)
{
    int32_t score = (int32_t)xenon2.player_score;
    uint32_t *slot;
    uint8_t *line, *cursor;
    const uint8_t *chars;
    const uint32_t *p;
    uint16_t idx;
    uint8_t stick, dir;
    int16_t repeats;
    int8_t wait_frames;

    if (score <= (int32_t)xenon2.hiscore_values[9])
        return;
    title_in_hiscore();
    score = (int32_t)xenon2.player_score;
    slot = &xenon2.hiscore_values[9];
    line = &xenon2.hiscore_lines[9 * 15];
    for (;;) {
        int k;

        if (score <= (int32_t)slot[-1])
            break;
        slot[0] = slot[-1];
        slot--;
        for (k = 10; k >= 0; k--) {
            line--;
            line[15] = line[0];
        }
        line -= 4;
        if (!(slot > xenon2.hiscore_values))
            break;
    }
    *slot = (uint32_t)score;
    line[12] = 0x3a;
    line[13] = 0x3a;
    line[14] = 0x3a;
    cursor = line + 4;
    for (p = xenon2.decimal_powers; *p != 0; p++) {
        int32_t unit = (int32_t)*p;

        *cursor = 0x30;
        for (;;) {
            score -= unit;
            if (score < 0)
                break;
            (*cursor)++;
        }
        score += unit;
        cursor++;
    }
    xenon2.key_pressed = 0;
    cursor = line + 12;
    chars = xenon2.text_chars;
    idx = 0;
next_letter:
    *cursor = chars[(int16_t)idx];
wait_stick:
    hiscore_draw();
    if (xenon2.key_pressed)
        goto letter_done;
    stick = hiscore_stick_first() & 0xc;
    dir = 0xff;
    if (stick & 4)
        goto step;
    dir = 1;
    if (!(stick & 8))
        goto wait_stick;
step:
    repeats = 3;
step_again:
    idx = (uint16_t)(idx + (int8_t)dir);
    if ((int16_t)idx < 0)
        idx = 0x25;
    if ((int16_t)idx >= 0x26)
        idx = 0;
repeat:
    *cursor = chars[(int16_t)idx];
    hiscore_draw();
    if (xenon2.key_pressed)
        goto letter_done;
    if ((uint8_t)(hiscore_stick_again() & 0xc) != stick)
        goto wait_stick;
    if (repeats-- != 0)
        goto repeat;
    goto step;
letter_done:
    xenon2.key_pressed = 0;
    cursor++;
    if (*cursor == 0x3a)
        goto next_letter;
    for (wait_frames = 0x11; wait_frames != 0; wait_frames--)
        hiscore_draw();
    title_out_hiscore();
}

/* ---- the menu ---- */

/* orig $8F56: CREDITS n at the bottom of the screen */
void credits_draw(void)
{
    xenon2.credits_text[8] = (uint8_t)(xenon2.continues_left + 0x30);
    draw_text(xenon2.credits_text, SCREEN_POS(xenon2.draw_screen, 184, 22));
}

/* orig $8CB6: the menu: 1 player, 2 players, music on/off; up/down moves the marker, fire takes it; leaves players = 1 or 2 */
void menu(void)
{
    int16_t sel = 0;
    const uint8_t *row;

    xenon2.continues_left = 3;
    title_stars_start();
    show_text_sequence(0xc, xenon2.menu_text, xenon2.wipe_in);
    xenon2.key_pressed = 0;
    for (;;) {
        clear_screen();
        xenon2.title_y = 0xc;
        draw_text_row(xenon2.menu_text);
        xenon2.title_y = 0x3c;
        row = &xenon2.menu_text[1 * MENU_LINE];
        if (sel == 0)
            row = &xenon2.menu_text[2 * MENU_LINE];
        draw_text_row(row);
        xenon2.title_y = 0x54;
        row = &xenon2.menu_text[3 * MENU_LINE];
        if (sel == 1)
            row = &xenon2.menu_text[4 * MENU_LINE];
        draw_text_row(row);
        xenon2.title_y = 0x6c;
        row = &xenon2.menu_text[5 * MENU_LINE];
        if (!xenon2.music_switch)
            row = &xenon2.menu_text[7 * MENU_LINE];
        if (sel == 2)
            row += MENU_LINE;
        draw_text_row(row);
        credits_draw();
        stars_update();
        swap_screens_200();
        if (xenon2.joy_stick & 1) {
            xenon2.joy_stick = 0;
            xenon2.sfx_cmd = 0xc;
            sel--;
            if (sel < 0)
                sel = 2;
            continue;
        }
        if (xenon2.joy_stick & 2) {
            xenon2.joy_stick = 0;
            xenon2.sfx_cmd = 0xc;
            sel++;
            if (sel >= 3)
                sel = 0;
            continue;
        }
        if (!xenon2.key_pressed)
            continue;
        xenon2.key_pressed = 0;
        xenon2.sfx_cmd = 0xd;
        if (sel == 2) {
            xenon2.music_switch = (uint8_t)~xenon2.music_switch;
            continue;
        }
        break;
    }
    xenon2.players = (uint16_t)(sel + 1);
    show_text_sequence(0xc, xenon2.menu_text, xenon2.wipe_out);
    title_restore();
}

/* orig $8E88: CONTINUE GAME with a countdown from 9 (about 80 frames); fire says yes: returns 1, the countdown running out 0 */
uint16_t continue_screen(void)
{
    int16_t n = 0x4f;

    title_stars_start();
    show_text_sequence(0x50, xenon2.continue_text, xenon2.wipe_in);
    xenon2.key_pressed = 0;
    for (;;) {
        clear_screen();
        xenon2.title_y = 0x50;
        draw_text_row(xenon2.continue_text);
        xenon2.continue_text[29] = (uint8_t)(((uint32_t)(int32_t)n / 8) + 0x30);
        xenon2.title_y = 0x78;
        draw_text_row(&xenon2.continue_text[20]);
        credits_draw();
        stars_update();
        swap_screens_200();
        if (xenon2.key_pressed) {
            show_text_sequence(0x50, xenon2.continue_text, xenon2.wipe_out);
            title_restore();
            xenon2.key_pressed = 0;
            return 1;
        }
        if (--n == 0) {
            show_text_sequence(0x50, xenon2.continue_text, xenon2.wipe_out);
            title_restore();
            return 0;
        }
    }
}

/* ---- the game disk and the level load ---- */

/* orig $917E: loads the code of the level in level_number unless it is loaded; asks for the game disk while it is not in */
void level_load(void)
{
    uint16_t level = xenon2.level_number;
    uint16_t i;

    int quiet = 0;   /* the dev build's start options: the first load shows no LOADING screen */

    if (level == xenon2.loaded_level)
        return;
    xenon2.loaded_level = level;
#ifdef GAME_SDL
    {
        static int first = 1;

        quiet = first && host_skip_menu();
        first = 0;
    }
#endif
    for (;;) {
        if (!quiet)
            title_in_loading();
        if (game_disk_present())
            break;
        title_out_loading();
        title_in_insert_disk();
        while (!game_disk_present())
            ;
        title_out_insert_disk();
    }
    xenon2.irq_vectors[2] = irq_vblank_hooked;
    i = (uint16_t)((uint16_t)(xenon2.level_number - 1) * 2);
    guest_blob_raw(level > 1 ? level : 0);   /* (level 1's members at $54E40 on do not describe the blob of another level) */
    disk_read_sectors(xenon2.level_sectors[i], xenon2.level_sectors[i + 1], &xenon2.map_image);
    xenon2.irq_vectors[2] = irq_vblank;
    if (!quiet)
        title_out_loading();
    clear_palette();
}

/* orig $8FF2: the shop: the screen fades, ENTERING SHOP, the shop's code is loaded from the game disk and run; then the level's
 * code is loaded again (RELOADING) and its palette fades in.  Parts of the flow the recorded run did not reach are written from
 * the listing. */
void shop_run(void)
{
    uint16_t i;

    palette_fade_out(2);
    sfx_stop_all();
    music_silence();
    xenon2.saved_level_init = xenon2.level_init_src;
    for (;;) {
        title_in_shop();
        if (game_disk_present())
            break;
        title_out_shop();
        title_in_insert_disk();
        while (!game_disk_present())
            ;
        title_out_insert_disk();
    }
    guest_blob_raw(1);   /* (the shop's blob overlays the level's) */
    xenon2.irq_vectors[2] = irq_vblank_hooked;
    disk_read_sectors(xenon2.level_sectors[10], xenon2.level_sectors[11], &xenon2.map_image);
    xenon2.irq_vectors[2] = irq_vblank;
    title_out_shop();
    clear_palette();
    shop_main();
    guest_blob_raw(xenon2.level_number > 1 ? xenon2.level_number : 0);
    if (xenon2.boss_dead != 0) {
        uint16_t other = xenon2.cur_player ^ 1;

        if (xenon2.player_active[other] == 0)
            return;
        if (xenon2.player_active[2 + other] != 0)
            return;
    } else {
        xenon2.loading_text[1] = 0x52;
        xenon2.loading_text[2] = 0x45;
    }
    for (;;) {
        title_in_loading();
        if (game_disk_present())
            break;
        title_out_loading();
        title_in_insert_disk();
        while (!game_disk_present())
            ;
        title_out_insert_disk();
        xenon2.loading_text[1] = 0x52;
        xenon2.loading_text[2] = 0x45;
    }
    xenon2.irq_vectors[2] = irq_vblank_hooked;
    i = (uint16_t)((uint16_t)(xenon2.level_number - 1) * 2);
    {
        int16_t count = (int16_t)xenon2.level_sectors[i + 1];

        if (count >= 0x12c)
            count -= 0x18;
        disk_read_sectors(xenon2.level_sectors[i], (uint16_t)count, &xenon2.map_image);
    }
    xenon2.level_init_src = xenon2.saved_level_init;
    xenon2.irq_vectors[2] = irq_vblank;
    title_out_loading();
    xenon2.loading_text[1] = 0x20;
    xenon2.loading_text[2] = 0x20;
    clear_palette();
    if (xenon2.boss_dead != 0)
        return;
    for (i = 0; i < 8; i++) {
        uint16_t hi = xenon2.level_palette[2 * i], lo = xenon2.level_palette[2 * i + 1];

        if (xenon2.shades_timer != 0) {
            lo = (uint16_t)((lo >> 1) | (hi << 15));   /* lsr.l #1 of the pair */
            hi >>= 1;
        }
        xenon2.level_palette_copy[2 * i] = hi;
        xenon2.level_palette_copy[2 * i + 1] = lo;
    }
    draw_background();
    stars_draw();
    swap_screens_200();
    palette_fade_in(xenon2.level_palette_copy);
}

/* ---- the disk (HLE: disk) ---- */

/* ---- the work area ---- */

/* orig $95F4: the first user copies the copy at $53F00 (1280 bytes) into the low RAM */
void work_area_save(void)
{
    if (xenon2.work_area_users == 0) {
        const uint16_t *saved = xenon2.panel_b;
        int i;

        for (i = 0; i < 640; i++)
            xenon2.work_area[i] = saved[i];
    }
    xenon2.work_area_users++;
}

/* orig $9618: the last user puts the low RAM's copy back into the level's work area and into screen_a at $7800 */
void work_area_restore(void)
{
    if (--xenon2.work_area_users == 0) {
        uint16_t *c = xenon2.panel_a;
        uint16_t *saved = xenon2.panel_b;
        int i;

        for (i = 0; i < 640; i++) {
            saved[i] = xenon2.work_area[i];
            *c++ = xenon2.work_area[i];
        }
    }
}

/* ---- enemy path script ---- */

/* orig $F38, $9740: an object's path: steps along the angle (position += sin/cos, the angle changes by turn) for steps calls; when
 * the current command has run out the script at cursor gives the next one: 0 end (the object dies), 2 new heading and turn and
 * duration, 4 wait, 6 jump to one of 8 random places, 8 jump, 10 skip */
