#include "disk.h"
#include "game_all.h"
#include "amiga/hw.h"

#define DISK_SCRATCH ((struct DiskScratch *)xenon2.screen_a)

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
