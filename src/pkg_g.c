#include "pkg_g.h"
#include "game_all.h"
#include "pkg_f.h"
#include "pkg_a.h"

#define SCREEN_A_WORDS(off) ((uint16_t *)&screen_a[off])

/* ---- title lines, fade, high-score table ---- */

/* the line of 20 characters squeezes in (a title page over the starfield) */
static void title_in(const uint8_t *s)
{
    title_stars_start();
    show_text_sequence(0xc, s, wipe_in);
}

/* the line squeezes out, the palette and the dot stars come back */
static void title_out(const uint8_t *s)
{
    show_text_sequence(0xc, s, wipe_out);
    title_restore();
}

/* orig $8C58: the HIGH SCORE line comes in */
void title_in_hiscore(void) { title_in(title_hiscore); }
/* orig $8C60: the ENTERING SHOP line comes in */
void title_in_shop(void) { title_in(shop_text); }
/* orig $8C68: the INSERT GAME DISK line comes in */
void title_in_insert_disk(void) { title_in(insert_disk_text); }
/* orig $8C70: the LOADING LEVEL line comes in */
void title_in_loading(void) { title_in(loading_text); }
/* orig $8C88: the HIGH SCORE line goes out */
void title_out_hiscore(void) { title_out(title_hiscore); }
/* orig $8C90: the ENTERING SHOP line goes out */
void title_out_shop(void) { title_out(shop_text); }
/* orig $8C98: the INSERT GAME DISK line goes out */
void title_out_insert_disk(void) { title_out(insert_disk_text); }
/* orig $8CA0: the LOADING LEVEL line goes out */
void title_out_loading(void) { title_out(loading_text); }

/* orig $885E: the 16 colours of the palette each lose 1 in every component, 8 times, waiting until vbl_count passes wait before
 * each step */
void palette_fade_out(int16_t wait)
{
    int16_t n;

    for (n = 7; n >= 0; n--) {
        int i;

        vbl_count = 0;
        while ((int16_t)wait >= (int16_t)vbl_count)
            HOST_IDLE();
        for (i = 0; i < 16; i++) {
            uint16_t c = (palette_shadow[i] & 0x777) - 0x111;

            if (c & 0x8)
                c += 1;
            if (c & 0x80)
                c += 0x10;
            if (c & 0x800)
                c += 0x100;
            palette_shadow[i] = c;
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

        vbl_count = 0;
        while ((int16_t)vbl_count < 2)
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
            palette_shadow[i] = c;
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
    const uint16_t *font = (const uint16_t *)font_gfx;
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
        idx = (int16_t)find_glyph(ch, text_chars);   /* inlined: $7EF8 */
        idx = (int16_t)((uint16_t)idx << 7);
        g = (const uint16_t *)((const uint8_t *)font + idx);
        for (row = 0; row < 16; row++) {
            wr16(&dest[0], rd16(g)); g++;
            wr16(&dest[20], rd16(g)); g++;
            wr16(&dest[40], rd16(g)); g++;
            wr16(&dest[60], rd16(g)); g++;
            dest += 80;
        }
        dest -= 0x4ff;
    }
}

/* orig $8B20: one frame of the high-score table: the heading and the ten lines, drawn with the big font */
void hiscore_draw(void)
{
    const uint8_t *line = hiscore_lines;
    uint16_t *dest = (uint16_t *)(draw_screen + 0x17c4);
    int i;

    clear_screen();
    title_scale = 0x10;
    title_y = 0xc;
    draw_text_squeezed(title_hiscore);
    for (i = 9; i >= 0; i--) {
        draw_big_text(line, dest);
        line += 15;
        dest += 0x500;
    }
    stars_update();
    swap_screens_200();
}

/* orig $89DC: the high-score table comes in, stays 85 frames or until fire, and goes out */
void hiscore_show(void)
{
    int16_t n = 0x55;

    show_text_sequence(0xc, title_hiscore, wipe_in);
    for (;;) {
        hiscore_draw();
        if (v_0a01)
            break;
        if (--n == 0) {
            show_text_sequence(0xc, title_hiscore, wipe_out);
            break;
        }
    }
}

/* orig $8A16: a score above the tenth is put into the table in its place; the player picks the three letters of the name with the
 * stick (up/down), fire takes a letter */
void hiscore_enter(void)
{
    int32_t d0 = (int32_t)v_0c92;
    uint32_t *a0;
    uint8_t *a1, *a2;
    const uint8_t *a3;
    const uint32_t *p;
    uint16_t idx;
    uint8_t d1, d2;
    int16_t d7;
    int8_t n;

    if (d0 <= (int32_t)hiscore_values[9])
        return;
    title_in_hiscore();
    d0 = (int32_t)v_0c92;
    a0 = &hiscore_values[9];
    a1 = &hiscore_lines[9 * 15];
    for (;;) {
        int k;

        if (d0 <= (int32_t)a0[-1])
            break;
        a0[0] = a0[-1];
        a0--;
        for (k = 10; k >= 0; k--) {
            a1--;
            a1[15] = a1[0];
        }
        a1 -= 4;
        if (!(a0 > hiscore_values))
            break;
    }
    *a0 = (uint32_t)d0;
    a1[12] = 0x3a;
    a1[13] = 0x3a;
    a1[14] = 0x3a;
    a2 = a1 + 4;
    for (p = decimal_powers; *p != 0; p++) {
        int32_t unit = (int32_t)*p;

        *a2 = 0x30;
        for (;;) {
            d0 -= unit;
            if (d0 < 0)
                break;
            (*a2)++;
        }
        d0 += unit;
        a2++;
    }
    v_0a01 = 0;
    a2 = a1 + 12;
    a3 = text_chars;
    idx = 0;
next_letter:
    *a2 = a3[(int16_t)idx];
wait_stick:
    hiscore_draw();
    if (v_0a01)
        goto letter_done;
    d1 = v_0a00 & 0xc;
    d2 = 0xff;
    if (d1 & 4)
        goto step;
    d2 = 1;
    if (!(d1 & 8))
        goto wait_stick;
step:
    d7 = 3;
step_again:
    idx = (uint16_t)(idx + (int8_t)d2);
    if ((int16_t)idx < 0)
        idx = 0x25;
    if ((int16_t)idx >= 0x26)
        idx = 0;
repeat:
    *a2 = a3[(int16_t)idx];
    hiscore_draw();
    if (v_0a01)
        goto letter_done;
    if ((uint8_t)(v_0a00 & 0xc) != d1)
        goto wait_stick;
    if (d7-- != 0)
        goto repeat;
    goto step_again;
letter_done:
    v_0a01 = 0;
    a2++;
    if (*a2 == 0x3a)
        goto next_letter;
    for (n = 0x11; n != 0; n--)
        hiscore_draw();
    title_out_hiscore();
}

/* ---- the menu ---- */

/* orig $8F56: CREDITS n at the bottom of the screen */
void credits_draw(void)
{
    credits_text[8] = (uint8_t)(v_0cf0 + 0x30);
    draw_text(credits_text, draw_screen + 0x7316);
}

/* orig $8CB6: the menu: 1 player, 2 players, music on/off; up/down moves the marker, fire takes it; leaves players = 1 or 2 */
void menu(void)
{
    int16_t sel = 0;
    const uint8_t *a1;

    v_0cf0 = 3;
    title_stars_start();
    show_text_sequence(0xc, menu_text, wipe_in);
    v_0a01 = 0;
    for (;;) {
        clear_screen();
        title_y = 0xc;
        draw_text_row(menu_text);
        title_y = 0x3c;
        a1 = &menu_text[20];
        if (sel == 0)
            a1 = &menu_text[40];
        draw_text_row(a1);
        title_y = 0x54;
        a1 = &menu_text[60];
        if (sel == 1)
            a1 = &menu_text[80];
        draw_text_row(a1);
        title_y = 0x6c;
        a1 = &menu_text[100];
        if (!v_09fc)
            a1 = &menu_text[140];
        if (sel == 2)
            a1 += 0x14;
        draw_text_row(a1);
        credits_draw();
        stars_update();
        swap_screens_200();
        if (v_0a00 & 1) {
            v_0a00 = 0;
            v_0414 = 0xc;
            sel--;
            if (sel < 0)
                sel = 2;
            continue;
        }
        if (v_0a00 & 2) {
            v_0a00 = 0;
            v_0414 = 0xc;
            sel++;
            if (sel >= 3)
                sel = 0;
            continue;
        }
        if (!v_0a01)
            continue;
        v_0a01 = 0;
        v_0414 = 0xd;
        if (sel == 2) {
            v_09fc = (uint8_t)~v_09fc;
            continue;
        }
        break;
    }
    players = (uint16_t)(sel + 1);
    show_text_sequence(0xc, menu_text, wipe_out);
    title_restore();
}

/* orig $8E88: CONTINUE GAME with a countdown from 9 (about 80 frames); fire says yes: returns 1, the countdown running out 0 */
uint16_t continue_screen(void)
{
    int16_t n = 0x4f;

    title_stars_start();
    show_text_sequence(0x50, continue_text, wipe_in);
    v_0a01 = 0;
    for (;;) {
        clear_screen();
        title_y = 0x50;
        draw_text_row(continue_text);
        continue_text[29] = (uint8_t)(((uint32_t)(int32_t)n / 8) + 0x30);
        title_y = 0x78;
        draw_text_row(&continue_text[20]);
        credits_draw();
        stars_update();
        swap_screens_200();
        if (v_0a01) {
            show_text_sequence(0x50, continue_text, wipe_out);
            title_restore();
            v_0a01 = 0;
            return 1;
        }
        if (--n == 0) {
            show_text_sequence(0x50, continue_text, wipe_out);
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

    if (draw_screen != screen_a) {
        uint32_t *dst = (uint32_t *)screen_b;
        const uint32_t *src = (const uint32_t *)screen_a;
        int i;

        for (i = 0; i < 0x1f40; i++)
            *dst++ = *src++;
        swap_screens_200();
    }
    irq_vectors[2] = irq_vblank_hooked;
    disk_read_sectors(0, 1, &screen_a[0x5600]);
    irq_vectors[2] = irq_vblank;
    ok = rd32(&screen_a[0x5602]) == 0x58324220;
    return ok;
}

/* orig $917E: loads the code of the level in v_0e04 unless it is loaded; asks for the game disk while it is not in */
void level_load(void)
{
    uint16_t level = v_0e04;
    uint16_t i;

    if (level == loaded_level)
        return;
    loaded_level = level;
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
    irq_vectors[2] = irq_vblank_hooked;
    i = (uint16_t)((uint16_t)(v_0e04 - 1) * 2);
    disk_read_sectors(level_sectors[i], level_sectors[i + 1], &map_image);
    irq_vectors[2] = irq_vblank;
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
    saved_level_init = level_init_src;
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
    irq_vectors[2] = irq_vblank_hooked;
    disk_read_sectors(level_sectors[10], level_sectors[11], &map_image);
    irq_vectors[2] = irq_vblank;
    title_out_shop();
    clear_palette();
    fn_054e00();
    if (v_0a07 != 0) {
        uint16_t other = cur_player ^ 1;

        if (v_0a03[other] == 0)
            return;
        if (v_0a03[2 + other] != 0)
            return;
    } else {
        loading_text[1] = 0x52;
        loading_text[2] = 0x45;
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
        loading_text[1] = 0x52;
        loading_text[2] = 0x45;
    }
    irq_vectors[2] = irq_vblank_hooked;
    i = (uint16_t)((uint16_t)(v_0e04 - 1) * 2);
    {
        int16_t count = (int16_t)level_sectors[i + 1];

        if (count >= 0x12c)
            count -= 0x18;
        disk_read_sectors(level_sectors[i], (uint16_t)count, &map_image);
    }
    level_init_src = saved_level_init;
    irq_vectors[2] = irq_vblank;
    title_out_loading();
    loading_text[1] = 0x20;
    loading_text[2] = 0x20;
    clear_palette();
    if (v_0a07 != 0)
        return;
    for (i = 0; i < 8; i++) {
        uint32_t v = rd32(&level_palette[i]);

        if (v_0c90 != 0)
            v >>= 1;
        level_palette_copy[2 * i] = (uint16_t)(v >> 16);
        level_palette_copy[2 * i + 1] = (uint16_t)v;
    }
    draw_background();
    stars_draw();
    swap_screens_200();
    palette_fade_in(level_palette_copy);
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
    disk_track = 0;
    disk_want = 0;
}

/* orig $932E: the head to track (bit 0 is the side); the percentage of the sectors read is drawn at the bottom of the shown screen
 * (HLE: disk) */
HLE_WEAK void disk_seek(uint16_t track)
{
    int16_t d0;

    if (disk_track < 0)
        disk_recalibrate();
    disk_want = track;
    CIAB_PRB &= ~4;
    if (!(track & 1))
        CIAB_PRB |= 4;
    d0 = (int16_t)((track & ~1) - (uint16_t)disk_track);
    if (d0 == 0)
        return;
    {
        uint16_t digit = 0;

        if (disk_count != 0)
            digit = (uint16_t)((uint32_t)((uint16_t)(disk_count - 1) * 10u) / disk_total);
        progress_text[0] = (uint8_t)(digit + 0x30);
        draw_text_row_at(show_screen + 0x3854, progress_text, 0);
    }
    disk_track = (int16_t)(disk_track + d0);
    d0 >>= 1;
    if (d0 >= 0)
        disk_step_in((uint16_t)d0);
    else
        disk_step_out((uint16_t)-d0);
}

/* orig $957E: bytes of MFM data (odd bits first, then the even bits) decoded to dest; returns the end of the source */
uint8_t *mfm_decode(uint8_t *dest, uint8_t *src, uint16_t bytes)
{
    uint8_t *odd = src;
    uint8_t *even = src + bytes;
    uint16_t n = (uint16_t)((bytes >> 2) - 1);

    do {
        uint32_t o = rd32(odd) & 0x55555555u;
        uint32_t e = rd32(even) & 0x55555555u;

        odd += 4;
        even += 4;
        wr32(dest, (o + o) | e);
        dest += 4;
    } while (n-- != 0);
    return even;
}

/* orig $951E: looks in the raw track from src on for the sector sync (a run of $5555 or $AAAA words followed by one of the
 * patterns of the table), then shifts the raw words 0 to 15 bits into place at the start of the track buffer; returns the shift,
 * -1 when nothing is found within 0xABC bytes */
int32_t mfm_align(const uint16_t *src)
{
    const uint16_t *a0 = src;
    const uint16_t *limit = (const uint16_t *)((const uint8_t *)src + 0xabc);
    const uint32_t *a2;
    uint16_t d2;
    int16_t d0;
    uint16_t d1;
    uint16_t *out;
    int i;

    for (;;) {
        for (;;) {
            d2 = rd16(a0++);
            if (d2 == 0x5555) {
                a2 = mfm_sync_a;
                d0 = 0xf;
                break;
            }
            if (d2 == 0xaaaa) {
                a2 = mfm_sync_b;
                d0 = 0xe;
                break;
            }
            if (!(a0 < limit))
                return -1;
        }
        while (d2 == rd16(a0++))
            ;
        a0--;
        {
            uint32_t pat = ((uint32_t)rd16(&a0[0]) << 16) | rd16(&a0[1]);

            for (;;) {
                if (pat == *a2++)
                    goto found;
                d0 -= 2;
                if (d0 < 0)
                    break;
            }
        }
    }
found:
    out = SCREEN_A_WORDS(0x1600);
    d1 = (uint16_t)(0x10 - d0);
    for (i = 0; i < 0x2000; i++) {
        uint16_t hi = (uint16_t)((uint32_t)rd16(&a0[-1]) << d1);
        uint16_t lo = (uint16_t)(rd16(a0++) >> d0);

        wr16(out++, (uint16_t)(lo | hi));
    }
    return d0;
}

/* orig $93F8: reads the track the head is on into the track buffer by DMA and decodes its eleven sectors into screen_a (sector n
 * at 512 * n); up to 4 tries, the head recalibrated between them; returns 0, or -1 when it fails (HLE: disk) */
HLE_WEAK int disk_read_track(void)
{
    uint8_t *a1;
    int n;

    disk_retries = 3;
retry:
    disk_done = 0;
    disk_error = 0;
    for (n = 0; n < 11; n++)
        disk_seen[n] = 0;
    custom->dsklen = 0x4000;
    custom->dskpt = ADDR(&screen_a[0x1600]);
    SETCLR_WRITE(intreq, 0x1002);
    SETCLR_WRITE(adkcon, 0x6600);
    SETCLR_WRITE(adkcon, 0x9100);
    custom->dsklen = 0xa000;
    custom->dsklen = 0xa000;
    while (!disk_done)
        HOST_IDLE();
    custom->dsklen = 0x4000;
    mfm_align(SCREEN_A_WORDS(0x1604));
    if (disk_error)
        goto fail;
    a1 = &screen_a[0x1602];
    while (rd16(a1) == 0x4489)
        a1 += 2;
    for (n = 10; n >= 0; n--) {
        uint16_t sector;

        a1 = mfm_decode(disk_header, a1, 4);
        a1 += 0x30;
        sector = disk_header[2];
        if (sector > 10)
            goto fail;
        if (disk_seen[sector] & 0x80)
            goto fail;
        disk_seen[sector] |= 0x80;
        a1 = mfm_decode(&screen_a[(uint16_t)(sector << 9)], a1, 0x200);
        if (disk_header[3] == 1) {
            mfm_align((const uint16_t *)a1);
            a1 = &screen_a[0x1602];
            while (rd16(a1) == 0x4489)
                a1 += 2;
        } else {
            a1 += 8;
        }
        if (disk_error)
            goto fail;
    }
    return 0;
fail:
    if ((int16_t)--disk_retries < 0)
        return -1;
    {
        uint16_t want = disk_want;

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

    disk_dest = dest;
    disk_sector = (uint16_t)(first_sector % 11);
    disk_count = count;
    disk_total = count;
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
        to = (uint32_t *)disk_dest;
        from = (const uint32_t *)&screen_a[(uint16_t)(disk_sector * 0x200u)];
        n = (int16_t)(11 - disk_sector);
        if (!(n <= (int16_t)disk_count))
            n = (int16_t)disk_count;
        disk_count = (uint16_t)(disk_count - n);
        m = (uint16_t)((uint16_t)n * 0x80u - 1);
        do {
            *to++ = *from++;
        } while (m-- != 0);
        disk_dest = (uint8_t *)to;
        disk_seek((uint16_t)(disk_want + 1));
        disk_sector = 0;
        if (disk_count == 0)
            break;
    }
    disk_motor_off();
    return 0;
}

/* orig $95E4: level 1 interrupt, the disk block is read: acknowledged, the flag set (HLE: disk) */
void irq_disk_block(void)
{
    SETCLR_WRITE(intreq, 2);
    disk_done = 0xff;
}

/* ---- the work area ---- */

/* orig $95F4: the first user copies the copy at $53F00 (1280 bytes) into the low RAM */
void work_area_save(void)
{
    if (work_area_users == 0) {
        const uint8_t *saved = panel_b;
        int i;

        for (i = 0; i < 1280; i++)
            work_area[i] = saved[i];
    }
    work_area_users++;
}

/* orig $9618: the last user puts the low RAM's copy back into the level's work area and into screen_a at $7800 */
void work_area_restore(void)
{
    if (--work_area_users == 0) {
        uint8_t *c = panel_a;
        uint8_t *saved = panel_b;
        int i;

        for (i = 0; i < 1280; i++) {
            saved[i] = work_area[i];
            *c++ = work_area[i];
        }
    }
}

/* ---- enemy path script ---- */

/* orig $F38, $9740: an object's path: steps along the angle (position += sin/cos, the angle changes by turn) for steps calls; when
 * the current command has run out the script at cursor gives the next one: 0 end (the object dies), 2 new heading and turn and
 * duration, 4 wait, 6 jump to one of 8 random places, 8 jump, 10 skip */
#define L32(hi, lo) ((((uint32_t)(hi)) << 16) | (uint16_t)(lo))
#define S32(hi, lo, v) do { (hi) = (uint16_t)((uint32_t)(v) >> 16); (lo) = (uint16_t)(v); } while (0)

void obj_script_step(Object *obj)
{
    ObjMove *o = (ObjMove *)obj;
    const int8_t *sine = sine_table;
    uint16_t d7;
    uint32_t d0, d1, d3, d4;
    int32_t d5;

    d7 = o->steps;
    if (d7 == 0)
        return;
    if (o->wait == 0)
        goto next_command;
    if (o->wait < 0) {
        d7 = (uint16_t)(d7 + o->wait);
        if ((int16_t)d7 > 0)
            goto next_command;
        o->wait = (int16_t)d7;
        return;
    }
move:
    d0 = L32(o->x_hi, o->x_lo);
    d1 = L32(o->y_hi, o->y_lo);
    d3 = L32(o->angle_hi, o->angle_lo);
    d4 = L32(o->turn_hi, o->turn_lo);
    d5 = o->turn_step;
    for (;;) {
        d7--;
        if ((int16_t)d7 < 0) {
            S32(o->x_hi, o->x_lo, d0);
            S32(o->y_hi, o->y_lo, d1);
            S32(o->angle_hi, o->angle_lo, d3);
            S32(o->turn_hi, o->turn_lo, d4);
            return;
        }
        o->wait--;
        if (o->wait < 0) {
            d7++;
            S32(o->x_hi, o->x_lo, d0);
            S32(o->y_hi, o->y_lo, d1);
            S32(o->angle_hi, o->angle_lo, d3);
            S32(o->turn_hi, o->turn_lo, d4);
            goto next_command;
        }
        d1 += (uint32_t)((int32_t)((uint32_t)(int16_t)sine[(int16_t)d3] << 16) >> 6);
        d0 += (uint32_t)((int32_t)((uint32_t)(int16_t)sine[0x40 + (int16_t)d3] << 16) >> 6);
        d3 = (d3 << 16) | (d3 >> 16);
        d3 += d4;
        d3 = (d3 << 16) | (d3 >> 16);
        d3 = (d3 & 0xffff0000u) | (d3 & 0xff);
        d4 += (uint32_t)d5;
    }
next_command:
    for (;;) {
        const uint8_t *cmd = o->cursor;

        switch (rd16(cmd)) {
        case 0: {
            int i;
            ObjMove *e;

            if (o->id != 0) {
                for (i = 0; i < 8; i++) {
                    if (v_043c[2 * i] == o->id) {
                        v_043c[2 * i] = 0;
                        v_043c[2 * i + 1] = 0;
                        break;
                    }
                }
            }
            if (o->flag34 == 0) {
                o->type = 4;
                o->fn_update = (obj_fn)free_object;
                o->fn_draw = (obj_fn)obj_nop;
                o->fn_hit = (obj_fn)obj_nop;
                o->flag34 = 0;
                o->flag60 = 0;
                return;
            }
            e = o->target;
            for (;;) {
                ObjMove *next = e->chain;

                e->type = 4;
                e->fn_update = (obj_fn)free_object;
                e->fn_draw = (obj_fn)obj_nop;
                e->fn_hit = (obj_fn)obj_nop;
                e->flag34 = 0;
                e->flag60 = 0;
                if (next == 0)
                    return;
                e = next;
            }
        }
        case 2:
            o->angle_hi = 0;
            o->angle_lo = rd16(cmd + 2);
            S32(o->turn_hi, o->turn_lo, (uint32_t)((int32_t)((uint32_t)rd16(cmd + 4) << 16) >> 8));
            o->turn_step = (int16_t)rd16(cmd + 6);
            o->wait = (int16_t)rd16(cmd + 8);
            o->cursor = cmd + 10;
            goto move;
        case 4: {
            uint16_t t = rd16(cmd + 2);

            o->cursor = cmd + 4;
            d7 = (uint16_t)(d7 - t);
            if ((int16_t)d7 > 0)
                continue;
            o->wait = (int16_t)d7;
            return;
        }
        case 6: {
            int16_t t;

            do {
                t = (int16_t)rd16(cmd + 2 + (random_next() & 0xe));
            } while (t < 0);
            o->cursor = o->script + t;
            continue;
        }
        case 8:
            o->cursor = o->script + (int16_t)rd16(cmd + 2);
            continue;
        case 10:
            o->cursor = cmd + 6;
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
    ObjMove *o = (ObjMove *)obj;
    uint8_t rate;
    uint16_t sum, dir;

    anim_step(obj);
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
        dir = direction8((int16_t)(player.x - (int16_t)o->x_hi), (int16_t)(player.y - (int16_t)o->y_hi));
    }
    spawn_enemy_shot(o->x_hi, o->y_hi, dir, 6, enemy_shot_gfx);
}

/* orig $EEA, $9670: the random state is kept for enemy_update_seeded, then enemy_update */
void enemy_update_keep_seed(Object *o)
{
    rand_saved[0] = random_a;
    rand_saved[1] = random_b;
    enemy_update(o);
}

/* orig $EF0, $9682: enemy_update with the random state of enemy_update_keep_seed; the caller's state is put back */
void enemy_update_seeded(Object *o)
{
    rand_saved[2] = random_a;
    rand_saved[3] = random_b;
    random_a = rand_saved[0];
    random_b = rand_saved[1];
    enemy_update(o);
    random_a = rand_saved[2];
    random_b = rand_saved[3];
}
