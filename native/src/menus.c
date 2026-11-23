#include "shop.h"
#include "menus.h"
#include "game_all.h"
#include "title.h"
#include "draw.h"

#define MENU_LINE 20   /* the characters of a line of menu_text */
#define DISK_SCRATCH ((struct DiskScratch *)xenon2.screen_a)

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

/* orig $8F9A: sector 0 of the disk must hold "X2B ": returns 1 when it does (Z set) */
int game_disk_present(void)
{
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
    disk_read_sectors(0, 1, DISK_SCRATCH->boot);
    xenon2.irq_vectors[2] = irq_vblank;
    ok = DISK_SCRATCH->boot[1] == 0x5832 && DISK_SCRATCH->boot[2] == 0x4220;
    return ok;
}

/* orig $917E: loads the code of the level in level_number unless it is loaded; asks for the game disk while it is not in */
void level_load(void)
{
    uint16_t level = xenon2.level_number;
    uint16_t i;

    if (level == xenon2.loaded_level)
        return;
    xenon2.loaded_level = level;
    for (;;) {
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

/* busy waits of the original (dbra loops); the host does not wait */
static void spin(uint32_t n)
{
    volatile uint32_t i;

    for (i = n; i != 0; i--)
        ;
}

/* orig $92C8: drive 0 selected, motor on (HLE: disk) */
HLE_WEAK void disk_motor_on(void)
{
    CIAB_PRB = 0xff;
    CIAB_PRB = 0x7f;
    CIAB_PRB = 0x77;
    spin(4 * 65536);
}

/* orig $92EE: motor off, drive deselected (HLE: disk) */
HLE_WEAK void disk_motor_off(void)
{
    CIAB_PRB = 0xff;
    CIAB_PRB = 0x87;
    CIAB_PRB = 0xff;
}

/* orig $93EE: the wait after each head step (HLE: disk) */
HLE_WEAK void disk_delay(void)
{
    spin(0x5961);
}

/* the steps both directions share: up to 0x52 steps of the head, each a pulse on the step line followed by the delay (HLE: disk) */
static void disk_step(uint16_t steps, uint8_t dir_bits)
{
    uint8_t low, high;
    int16_t n;

    if ((int16_t)steps > 0x52)
        steps = 0x52;
    low = dir_bits & ~1;
    high = dir_bits | 1;
    n = (int16_t)(steps - 1);
    do {
        CIAB_PRB = low;
        CIAB_PRB = high;
        disk_delay();
    } while (n-- != 0);
}

/* orig $93AA: the head steps outwards, toward track 0 (HLE: disk) */
HLE_WEAK void disk_step_out(uint16_t steps)
{
    disk_step(steps, (uint8_t)(CIAB_PRB | 2));
}

/* orig $93B6: the head steps inwards (HLE: disk) */
HLE_WEAK void disk_step_in(uint16_t steps)
{
    disk_step(steps, (uint8_t)(CIAB_PRB & 0xfd));
}

/* orig $9308: the head goes out until track 0 shows (at most 256 steps); the track is 0 afterwards (HLE: disk) */
HLE_WEAK void disk_recalibrate(void)
{
    int16_t n;

    for (n = 0xff; n >= 0; n--) {
        if (!(CIAA_PRA & 0x10))
            break;
        disk_step_out(1);
    }
    xenon2.disk_track = 0;
    xenon2.disk_want = 0;
}

/* orig $932E: the head to track (bit 0 is the side); the percentage of the sectors read is drawn at the bottom of the shown screen
 * (HLE: disk) */
HLE_WEAK void disk_seek(uint16_t track)
{
    int16_t delta;

    if (xenon2.disk_track < 0)
        disk_recalibrate();
    xenon2.disk_want = track;
    CIAB_PRB &= ~4;
    if (!(track & 1))
        CIAB_PRB |= 4;
    delta = (int16_t)((track & ~1) - (uint16_t)xenon2.disk_track);
    if (delta == 0)
        return;
    {
        uint16_t digit = 0;

        if (xenon2.disk_count != 0)
            digit = (uint16_t)((uint32_t)((uint16_t)(xenon2.disk_count - 1) * 10u) / xenon2.disk_total);
        xenon2.progress_text[0] = (uint8_t)(digit + 0x30);
        draw_text_row_at(SCREEN_POS(xenon2.show_screen, 90, 20), xenon2.progress_text, 0);
    }
    xenon2.disk_track = (int16_t)(xenon2.disk_track + delta);
    delta >>= 1;
    if (delta >= 0)
        disk_step_in((uint16_t)delta);
    else
        disk_step_out((uint16_t)-delta);
}

/* orig $957E: bytes of MFM data (odd bits first, then the even bits) decoded to dest; returns the end of the source */
uint16_t *mfm_decode(uint16_t *dest, uint16_t *src, uint16_t bytes)
{
    uint16_t words = (uint16_t)(bytes >> 1);
    const uint16_t *odd = src;
    const uint16_t *even = src + words;

    for (uint16_t i = 0; i < words; i++)
        dest[i] = (uint16_t)(((odd[i] & 0x5555) << 1) | (even[i] & 0x5555));
    return src + 2 * words;
}

/* orig $951E: looks in the raw track from src on for the sector sync (a run of $5555 or $AAAA words followed by one of the
 * patterns of the table), then shifts the raw words 0 to 15 bits into place at the start of the track buffer; returns the shift,
 * -1 when nothing is found within 0xABC bytes */
int32_t mfm_align(const uint16_t *src)
{
    const uint16_t *p = src;
    const uint16_t *limit = (const uint16_t *)((const uint8_t *)src + 0xabc);
    const uint32_t *pats;
    uint16_t word;
    int16_t shift_right;
    uint16_t shift_left;
    uint16_t *out;
    int i;

    for (;;) {
        for (;;) {
            word = *p++;
            if (word == 0x5555) {
                pats = xenon2.mfm_sync_a;
                shift_right = 0xf;
                break;
            }
            if (word == 0xaaaa) {
                pats = xenon2.mfm_sync_b;
                shift_right = 0xe;
                break;
            }
            if (!(p < limit))
                return -1;
        }
        while (word == *p++)
            ;
        p--;
        {
            for (;;) {
                if (p[0] == (*pats >> 16) && p[1] == (uint16_t)*pats)
                    goto found;
                pats++;
                shift_right -= 2;
                if (shift_right < 0)
                    break;
            }
        }
    }
found:
    out = DISK_SCRATCH->track;
    shift_left = (uint16_t)(0x10 - shift_right);
    for (i = 0; i < 0x2000; i++) {
        uint16_t hi = (uint16_t)((uint32_t)p[-1] << shift_left);
        uint16_t lo = (uint16_t)(*p++ >> shift_right);

        *out++ = (uint16_t)(lo | hi);
    }
    return shift_right;
}

/* orig $93F8: reads the track the head is on into the track buffer by DMA and decodes its eleven sectors into screen_a (sector n
 * at 512 * n); up to 4 tries, the head recalibrated between them; returns 0, or -1 when it fails (HLE: disk) */
HLE_WEAK int disk_read_track(void)
{
    uint16_t *p;
    int n;

    xenon2.disk_retries = 3;
retry:
    xenon2.disk_done = 0;
    xenon2.disk_error = 0;
    for (n = 0; n < 11; n++)
        xenon2.disk_seen[n] = 0;
    custom->dsklen = 0x4000;
    custom->dskpt = (uintptr_t)(DISK_SCRATCH->track);
    SETCLR_WRITE(intreq, 0x1002);
    SETCLR_WRITE(adkcon, 0x6600);
    SETCLR_WRITE(adkcon, 0x9100);
    custom->dsklen = 0xa000;
    custom->dsklen = 0xa000;
    while (!xenon2.disk_done)
        HOST_IDLE();
    custom->dsklen = 0x4000;
    mfm_align(&DISK_SCRATCH->track[2]);
    if (xenon2.disk_error)
        goto fail;
    p = &DISK_SCRATCH->track[1];
    while (*p == 0x4489)
        p++;
    for (n = 10; n >= 0; n--) {
        uint16_t sector;

        p = mfm_decode(xenon2.disk_header, p, 4);
        p += 0x18;
        sector = (uint16_t)(xenon2.disk_header[1] >> 8);
        if (sector > 10)
            goto fail;
        if (xenon2.disk_seen[sector] & 0x80)
            goto fail;
        xenon2.disk_seen[sector] |= 0x80;
        p = mfm_decode(DISK_SCRATCH->sector[sector].word, p, 0x200);
        if ((xenon2.disk_header[1] & 0xff) == 1) {
            mfm_align(p);
            p = &DISK_SCRATCH->track[1];
            while (*p == 0x4489)
                p++;
        } else {
            p += 4;
        }
        if (xenon2.disk_error)
            goto fail;
    }
    return 0;
fail:
    if ((int16_t)--xenon2.disk_retries < 0)
        return -1;
    {
        uint16_t want = xenon2.disk_want;

        disk_recalibrate();
        disk_seek(want);
    }
    goto retry;
}

/* orig $921A (rabs): count sectors from sector first_sector of the disk (11 to a track, track = sector / 11) to dest; 0 = read, -1 =
 * failed (HLE: disk) */
HLE_WEAK int disk_read_sectors(uint16_t first_sector, uint16_t count, void *dest)
{
    uint16_t track = (uint16_t)(first_sector / 11);

    xenon2.disk_dest = dest;
    xenon2.disk_sector = (uint16_t)(first_sector % 11);
    xenon2.disk_count = count;
    xenon2.disk_total = count;
    disk_motor_on();
    disk_recalibrate();
    disk_seek(track);
    for (;;) {
        uint32_t *to;
        const uint32_t *from;
        int16_t n;
        uint16_t m;

        if (disk_read_track() < 0) {
            disk_motor_off();
            return -1;
        }
        to = (uint32_t *)xenon2.disk_dest;
        from = (const uint32_t *)DISK_SCRATCH->sector[xenon2.disk_sector].word;
        n = (int16_t)(11 - xenon2.disk_sector);
        if (!(n <= (int16_t)xenon2.disk_count))
            n = (int16_t)xenon2.disk_count;
        xenon2.disk_count = (uint16_t)(xenon2.disk_count - n);
        m = (uint16_t)((uint16_t)n * 0x80u - 1);
        do {
            *to++ = *from++;
        } while (m-- != 0);
        xenon2.disk_dest = (uint8_t *)to;
        disk_seek((uint16_t)(xenon2.disk_want + 1));
        xenon2.disk_sector = 0;
        if (xenon2.disk_count == 0)
            break;
    }
    disk_motor_off();
    return 0;
}

/* orig $95E4: level 1 interrupt, the disk block is read: acknowledged, the flag set (HLE: disk) */
void irq_disk_block(void)
{
    SETCLR_WRITE(intreq, 2);
    xenon2.disk_done = 0xff;
}

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

void obj_script_step(Object *obj)
{
    Object *o = obj;
    const int8_t *sine = xenon2.sine_table;
    uint16_t steps;
    uint32_t x_fixed, y_fixed, angle_fixed, turn;
    int32_t turn_delta;

    steps = o->steps;
    if (steps == 0)
        return;
    if (o->path.wait == 0)
        goto next_command;
    if (o->path.wait < 0) {
        steps = (uint16_t)(steps + o->path.wait);
        if ((int16_t)steps > 0)
            goto next_command;
        o->path.wait = (int16_t)steps;
        return;
    }
move:
    x_fixed = ((uint32_t)o->x << 16) | o->x_frac;
    y_fixed = ((uint32_t)o->y << 16) | o->y_frac;
    angle_fixed = o->path.angle;
    turn = o->turn;
    turn_delta = (int16_t)o->turn_step;
    for (;;) {
        steps--;
        if ((int16_t)steps < 0) {
            o->x = (uint16_t)(x_fixed >> 16);
            o->x_frac = (uint16_t)x_fixed;
            o->y = (uint16_t)(y_fixed >> 16);
            o->y_frac = (uint16_t)y_fixed;
            o->path.angle = angle_fixed;
            o->turn = turn;
            return;
        }
        o->path.wait--;
        if (o->path.wait < 0) {
            steps++;
            o->x = (uint16_t)(x_fixed >> 16);
            o->x_frac = (uint16_t)x_fixed;
            o->y = (uint16_t)(y_fixed >> 16);
            o->y_frac = (uint16_t)y_fixed;
            o->path.angle = angle_fixed;
            o->turn = turn;
            goto next_command;
        }
        y_fixed += (uint32_t)((int32_t)((uint32_t)(int16_t)sine[(int16_t)angle_fixed] << 16) >> 6);
        x_fixed += (uint32_t)((int32_t)((uint32_t)(int16_t)sine[0x40 + (int16_t)angle_fixed] << 16) >> 6);
        angle_fixed = (angle_fixed << 16) | (angle_fixed >> 16);
        angle_fixed += turn;
        angle_fixed = (angle_fixed << 16) | (angle_fixed >> 16);
        angle_fixed = (angle_fixed & 0xffff0000u) | (angle_fixed & 0xff);
        turn += (uint32_t)turn_delta;
    }
next_command:
    for (;;) {
        const struct PathCmd *cmd = o->cursor;

        switch (cmd->op) {
        case 0: {
            int i;
            Object *e;

            if (o->kind != 0) {
                for (i = 0; i < 8; i++) {
                    if (xenon2.pickup_groups[2 * i] == (uint16_t)o->kind) {
                        xenon2.pickup_groups[2 * i] = 0;
                        xenon2.pickup_groups[2 * i + 1] = 0;
                        break;
                    }
                }
            }
            if (o->is_part == 0) {
                o->type = 4;
                o->fn_update = (obj_fn)free_object;
                o->fn_draw = (obj_fn)obj_nop;
                o->fn_hit = (obj_fn)obj_nop;
                o->is_part = 0;
                o->persists = 0;
                return;
            }
            e = o->target;
            for (;;) {
                Object *next = e->chain;

                e->type = 4;
                e->fn_update = (obj_fn)free_object;
                e->fn_draw = (obj_fn)obj_nop;
                e->fn_hit = (obj_fn)obj_nop;
                e->is_part = 0;
                e->persists = 0;
                if (next == 0)
                    return;
                e = next;
            }
        }
        case 2:
            o->path.angle = cmd->arg.move.angle;
            o->turn = (uint32_t)((int32_t)((uint32_t)cmd->arg.move.turn << 16) >> 8);
            o->turn_step = (int16_t)cmd->arg.move.turn_step;
            o->path.wait = (int16_t)cmd->arg.move.wait;
            o->cursor = (const struct PathCmd *)(&cmd->arg.move + 1);
            goto move;
        case 4: {
            uint16_t t = cmd->arg.pause.steps;

            o->cursor = (const struct PathCmd *)(&cmd->arg.pause + 1);
            steps = (uint16_t)(steps - t);
            if ((int16_t)steps > 0)
                continue;
            o->path.wait = (int16_t)steps;
            return;
        }
        case 6: {
            int16_t t;

            do {
                t = (int16_t)cmd->arg.pick[(random_next() & 0xe) / 2];
            } while (t < 0);
            o->cursor = (const struct PathCmd *)(o->script + t);
            continue;
        }
        case 8:
            o->cursor = (const struct PathCmd *)(o->script + (int16_t)cmd->arg.jump.offset);
            continue;
        case 10:
            o->cursor = (const struct PathCmd *)(&cmd->arg.pos + 1);
            continue;
        default:
            return;
        }
    }
}

/* orig $EF6, $9734: the path script, then the box of the object unless it is dying */
void obj_script_run(Object *o)
{
    obj_script_step(o);
    if (o->type != 4)
        bbox_from_frame(o);
}

/* orig $E0C, $96B6: an enemy's update: the animation, the path, and now and then a shot (at the player when shot_rate is 100 or more,
 * else in a random direction) */
void enemy_update(Object *obj)
{
    anim_step(obj);
    enemy_update_no_anim(obj);
}

/* orig $E78, $96BA: enemy_update without the animation step */
void enemy_update_no_anim(Object *obj)
{
    Object *o = obj;
    uint8_t rate;
    uint16_t sum, dir;

    obj_script_run(obj);
    if (o->type == 4)
        return;
    if (o->shot_rate == 0)
        return;
    rate = o->shot_rate;
    while (rate >= 0x64)
        rate -= 0x64;
    sum = (uint16_t)(o->shot_count + rate);
    o->shot_count = (uint8_t)sum;
    if (!(sum & 0x100))
        return;
    o->shot_count = (uint8_t)(random_next() & 0x3f);
    if (o->shot_rate < 0x64) {
        dir = (uint16_t)(random_next() & 7);
    } else {
        dir = direction8((int16_t)(xenon2.player.x - (int16_t)o->x), (int16_t)(xenon2.player.y - (int16_t)o->y));
    }
    spawn_enemy_shot(o->x, o->y, dir, 6, xenon2.enemy_shot_gfx);
}

/* orig $EEA, $9670: the random state is kept for enemy_update_seeded, then enemy_update */
void enemy_update_keep_seed(Object *o)
{
    xenon2.rand_saved[0] = xenon2.random_state_a;
    xenon2.rand_saved[1] = xenon2.random_state_b;
    enemy_update(o);
}

/* orig $EF0, $9682: enemy_update with the random state of enemy_update_keep_seed; the caller's state is put back */
void enemy_update_seeded(Object *o)
{
    xenon2.rand_saved[2] = xenon2.random_state_a;
    xenon2.rand_saved[3] = xenon2.random_state_b;
    xenon2.random_state_a = xenon2.rand_saved[0];
    xenon2.random_state_b = xenon2.rand_saved[1];
    enemy_update(o);
    xenon2.random_state_a = xenon2.rand_saved[2];
    xenon2.random_state_b = xenon2.rand_saved[3];
}
