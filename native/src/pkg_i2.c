/* The shop's sound ($6477C-$64D4A of the blob): a player for the four audio channels, driven by the vertical-blank interrupt ($2090 = shop_irq_tick) and a command
 * ($2094 = shop_sound_cmd, d0 = (channel << 8) | effect; effect bit 7 = a sampled effect, else a synthesised one).  Its state is raw memory of the blob:
 *   SND_CH:     4 records of $24 bytes (the channels): 0 period slide, 2 period to restart at, 4 pair of slides, 8 period, $A pair of waveform offsets, $E and $F reload of
 *               the counters at $18 and $19, $10 and $11 the bits that choose the half of a pair, $12 frames left, $13 reload of the envelope's step counter ($1B),
 *               $14 envelope, $15 the sampled effect (bit 7), $1A active, $1C the envelope's next byte (0 ends, bit 7 holds the volume), $20 and $22 periods.
 *   SND_MIRROR: 4 records of $C bytes: what the music left in the channel (lc, len, per, vol, then $A: it had one, $B: the effect has it).
 * The hardware is written as the original does (HLE: audio). */
#include "pkg_i.h"
#include "game_all.h"

#define SND_BASE   0x6476eu
#define SND_BUILT1 (SND_BASE + 0xe8)     /* byte: the tables of the synthesised effects are built */
#define SND_WAVES  (SND_BASE + 0x8c8)    /* long: the waveform area */
#define SND_BUILT2 (SND_BASE + 0x256)    /* byte: the table of the sampled effects is built */
#define SND_CMD    (SND_BASE + 0x66e)    /* word: the command */
#define SND_CH     0x64d4cu
#define SND_MIRROR 0x64ddeu
#define SND_SAMPLES 0x64aaeu             /* 4 records of 16 bytes: lc, loop offset, len in words, period, steps, 0 */
#define SND_SYNTH  0x64e0eu              /* the synthesised effects, $18 bytes each */
#define SND_TAB1   0x6491eu              /* 1 record of 12 bytes (and room for a second) */
#define SND_BANK   0x650dau              /* the sample bank: long length, word rate, samples */
#define SND_SILENCE 0x6485au             /* long: the address of the silent sample */
#define SND_ENV    (SND_BASE + 0x8cc)    /* words: the offsets (from SND_BASE) of the envelopes */
#define SND_PERIOD_CLOCK 0x369e99u

/* shop $64BB2: the wait the hardware needs after a write to a channel */
void shop_sound_delay(void)
{
    volatile int16_t n;

    for (n = 0x200; n >= 0; n--)
        ;
}

/* the ROM's divide: the 32-bit value by a 16-bit one, the quotient if it fits in 16 bits, else the value is left (divu overflow) */
static uint32_t divu(uint32_t value, uint16_t divisor)
{
    uint32_t q = value / divisor;

    return q > 0xffff ? value : (q & 0xffff) | ((value % divisor) << 16);
}

/* shop $6492A: the descriptors of the synthesised effects' waveforms are built once from the sample bank */
void shop_sound_tables(void)
{
    uint32_t a0, a5;
    int16_t n;

    if (sh_r8(SND_BUILT1) != 0)
        return;
    a0 = SND_BANK;
    a5 = SND_TAB1;
    for (n = 0; n >= 0; n--) {
        uint32_t d2 = sh_r32(a0);
        uint16_t d3 = sh_r16(a0 + 4);

        a0 += 6;
        sh_w32(a5, a0);
        a0 += d2;
        d2 >>= 1;
        sh_w16(a5 + 8, (uint16_t)d2);
        sh_w16(a5 + 0xa, (uint16_t)divu(SND_PERIOD_CLOCK, d3));
        a5 += 0xc;
    }
    sh_w32(SND_BASE + 0xec, a0);
    a0 += 0x40;
    sh_w32(SND_WAVES, a0);
    sh_w8(SND_BUILT1, 0xff);
}

/* shop $64972: the descriptors of the 4 sampled effects (the last part of the sample bank) are built once */
void shop_sound_tables2(void)
{
    uint32_t a0, a5;
    int16_t n;

    if (sh_r8(SND_BUILT2) != 0)
        return;
    a0 = SND_BANK + 0xf42;
    a5 = SND_SAMPLES;
    for (n = 3; n >= 0; n--) {
        uint32_t d2 = (uint32_t)sh_r16(a0 + 2) * 0x32;
        uint16_t d3;

        d2 = divu(d2, sh_r16(a0 + 4));
        sh_w8(a5 + 0xc, (uint8_t)(d2 + 1));
        d2 = sh_r32(a0);
        d3 = sh_r16(a0 + 4);
        a0 += 6;
        sh_w32(a5, a0);
        a0 += d2;
        d2 >>= 1;
        sh_w16(a5 + 8, (uint16_t)d2);
        sh_w16(a5 + 0xa, (uint16_t)divu(SND_PERIOD_CLOCK, d3));
        a5 += 0x10;
    }
    sh_w8(SND_BUILT2, 0xff);
}

/* shop $647CE: all the channels are stopped and the audio hardware silenced */
void shop_sound_stop(void)
{
    int i;

    sh_w8(SND_BASE + 0xe9, 0);
    for (i = 0; i < 4; i++) {
        sh_w8(SND_CH + 0x24u * i + 0x1a, 0);
        sh_w16(SND_MIRROR + 0xcu * i + 0xa, 0);
    }
    SETCLR_WRITE(dmacon, 0x000f);
    SETCLR_WRITE(adkcon, 0x00ff);
    for (i = 0; i < 4; i++)
        custom->aud[i].vol = 0;
}

/* shop $64828: every channel has one frame left */
void shop_sound_reset(void)
{
    int i;

    for (i = 0; i < 4; i++)
        sh_w8(SND_CH + 0x24u * i + 0x12, 1);
}

/* the effect on the channel stops; the channel goes back to the music if it had one */
static void channel_stop(volatile struct AudioChannel *hw, uint32_t ch, uint32_t mir, uint16_t d2)
{
    sh_w8(ch + 0x1a, 0);
    SETCLR_WRITE(dmacon, d2);
    sh_w8(mir + 0xb, 0);
    if (sh_r8(mir + 0xa) == 0)
        return;
    hw->per = 0x7c;
    SETCLR_WRITE(dmacon, d2);
    shop_sound_delay();
    hw->lc = ADDR(GPTR(sh_r32(mir)));
    hw->len = sh_r16(mir + 4);
    hw->per = sh_r16(mir + 6);
    hw->vol = sh_r16(mir + 8);
    SETCLR_WRITE(dmacon, (uint16_t)(d2 | 0x8200));
}

/* shop $64C1E: one frame of a channel (hw = its registers, ch = its record, mir = its mirror, d2 = its DMA bit) */
static void channel_step(volatile struct AudioChannel *hw, uint32_t ch, uint32_t mir, uint16_t d2)
{
    uint16_t d0, d1;
    uint32_t a2, l;

    if (sh_r8(ch + 0x12) != 0) {
        sh_w8(ch + 0x12, (uint8_t)(sh_r8(ch + 0x12) - 1));
        if (sh_r8(ch + 0x12) == 0) {
            channel_stop(hw, ch, mir, d2);
            return;
        }
    }
    d0 = sh_r8(ch + 0x15);
    if (d0 != 0) {
        a2 = SND_SAMPLES + (uint32_t)(d0 & 0x7f) * 0x10;
        d0 = sh_r16(ch + 0x22);
        d1 = sh_r16(a2 + 0xe);
        if (d1 == 0) {
            hw->per = sh_r16(ch + 0x22);
            return;
        }
        d0 = (uint16_t)(d0 + d1);
        if ((int16_t)d0 < 0x7c) {
            channel_stop(hw, ch, mir, d2);
            return;
        }
        sh_w16(ch + 0x22, d0);
        hw->per = d0;
        return;
    }
    if (sh_r8(ch + 0xf) != 0) {
        sh_w8(ch + 0x19, (uint8_t)(sh_r8(ch + 0x19) - 1));
        if (sh_r8(ch + 0x19) == 0) {
            uint8_t b = sh_r8(ch + 0x10);
            int carry = b & 1;

            sh_w8(ch + 0x19, sh_r8(ch + 0xf));
            l = sh_r32(ch + 4);
            b = (uint8_t)((b >> 1) | (carry << 7));
            if (!carry)
                l = (l << 16) | (l >> 16);
            sh_w8(ch + 0x10, b);
            sh_w16(ch + 8, (uint16_t)(sh_r16(ch + 8) + (uint16_t)l));
        }
    }
    sh_w16(ch + 8, (uint16_t)(sh_r16(ch + 8) + sh_r16(ch)));
    if (sh_r8(ch + 0xe) != 0) {
        sh_w8(ch + 0x18, (uint8_t)(sh_r8(ch + 0x18) - 1));
        if (sh_r8(ch + 0x18) == 0) {
            sh_w8(ch + 0x18, sh_r8(ch + 0xe));
            sh_w16(ch + 8, sh_r16(ch + 2));
        }
    }
    sh_w8(ch + 0x1b, (uint8_t)(sh_r8(ch + 0x1b) - 1));
    if (sh_r8(ch + 0x1b) == 0) {
        uint8_t v;

        sh_w8(ch + 0x1b, sh_r8(ch + 0x13));
        a2 = sh_r32(ch + 0x1c);
        v = sh_r8(a2++);
        if (v == 0) {
            channel_stop(hw, ch, mir, d2);
            return;
        }
        if (!(v & 0x80)) {
            sh_w32(ch + 0x1c, a2);
            hw->vol = v;
        }
    }
    d1 = sh_r16(ch + 8);
    if ((int16_t)d1 < 0x7c)
        d1 = 0x7c;
    hw->per = d1;
    l = sh_r32(ch + 0xa);
    {
        uint8_t b = sh_r8(ch + 0x11);
        int carry = b & 1;

        b = (uint8_t)((b >> 1) | (carry << 7));
        if (!carry)
            l = (l << 16) | (l >> 16);
        sh_w8(ch + 0x11, b);
    }
    hw->lc = ADDR(GPTR(sh_r32(SND_WAVES) + (uint32_t)(int32_t)(int16_t)l));
    SETCLR_WRITE(dmacon, (uint16_t)(d2 | 0x8200));
}

/* shop $64BBC: the vertical blank: every active channel goes on one frame */
void shop_sound_tick(void)
{
    int i;

    for (i = 0; i < 4; i++)
        if (sh_r8(SND_CH + 0x24u * i + 0x1a) != 0)
            channel_step(&custom->aud[i], SND_CH + 0x24u * i, SND_MIRROR + 0xcu * i, (uint16_t)(1 << i));
}

/* shop $6477C: the interrupt's call of the player (the handler keeps the registers) */
void shop_irq_tick(void)
{
    shop_sound_tick();
}

/* shop $64AEE: a sampled effect (id with bit 7 in d0) on the channel of the command */
static void start_sampled(uint16_t d0)
{
    uint32_t a1, a0, a2;
    uint16_t d2, d7;
    volatile struct AudioChannel *hw;

    shop_sound_tables2();
    d7 = sh_r8(SND_CMD);
    a1 = SND_CH + 0x24u * d7;
    sh_w8(a1 + 0x15, (uint8_t)d0);
    d2 = (uint16_t)(1u << d7);
    SETCLR_WRITE(dmacon, d2);
    hw = &custom->aud[d7];
    hw->per = 0x7c;
    hw->len = 1;
    shop_sound_delay();
    a0 = SND_SAMPLES + (uint32_t)(sh_r8(a1 + 0x15) & 0x7f) * 0x10;
    sh_w8(a1 + 0x12, sh_r8(a0 + 0xc));
    if (sh_r8(a0 + 0xd) != 0)
        sh_w8(a1 + 0x12, 0);
    hw->len = sh_r16(a0 + 8);
    hw->lc = ADDR(GPTR(sh_r32(a0)));
    hw->per = sh_r16(a0 + 0xa);
    sh_w16(a1 + 0x20, sh_r16(a0 + 0xa));
    sh_w16(a1 + 0x22, sh_r16(a0 + 0xa));
    hw->vol = 0x40;
    d2 |= 0x8200;
    SETCLR_WRITE(dmacon, d2);
    sh_w8(a1 + 0x1a, 0xff);
    shop_sound_delay();
    d0 = 0;
    {
        uint32_t off = sh_r32(a0 + 4);

        if ((int32_t)off < 0) {
            hw->lc = ADDR(GPTR(sh_r32(SND_SILENCE)));
            hw->len = 0x20;
            return;
        }
        a2 = sh_r32(a0) + off;
        off >>= 1;
        hw->len = (uint16_t)(sh_r16(a0 + 8) - (uint16_t)off);
        hw->lc = ADDR(GPTR(a2));
    }
}

/* shop $649C6 ($647A6): start the effect of the command (d0 = (channel << 8) | effect) on the channel; the music's hold on the channel is marked */
void shop_sound_cmd(uint16_t d0)
{
    uint32_t a0, a1, a2;
    uint16_t d2;
    uint8_t ch;
    volatile struct AudioChannel *hw;
    int i;

    CIAA_PRA |= 2;
    SETCLR_WRITE(adkcon, 0x00ff);
    d0 &= 0x3ff;
    sh_w16(SND_CMD, d0);
    ch = sh_r8(SND_CMD);
    if (ch <= 3)
        sh_w8(SND_CH + 0x24u * ch + 0x1a, 0);
    shop_sound_tables();
    sh_w8(SND_MIRROR + (uint16_t)(sh_r8(SND_CMD) * 0xc) + 0xb, 0xff);
    if ((int8_t)d0 < 0) {
        start_sampled(d0);
        return;
    }
    d0 = (uint16_t)((uint8_t)d0 * 0x18);
    ch = sh_r8(SND_CMD);
    a1 = SND_CH + 0x24u * ch;
    d2 = (uint16_t)(1u << ch);
    hw = &custom->aud[ch];
    a0 = SND_SYNTH + (int16_t)d0;
    a2 = a1;
    hw->len = 1;
    hw->per = 0x7c;
    shop_sound_delay();
    SETCLR_WRITE(dmacon, d2);
    shop_sound_delay();
    for (i = 0; i < 12; i++)
        sh_w16(a1 + 2u * i, sh_r16(a0 + 2u * i));
    hw->len = sh_r16(a2 + 0x16);
    sh_w16(a2 + 0x18, sh_r16(a2 + 0xe));
    sh_w8(a2 + 0x1b, 1);
    d0 = (uint16_t)(int16_t)(int8_t)sh_r8(a2 + 0x14);
    d0 = (uint16_t)(d0 + d0);
    a0 = SND_BASE + (uint32_t)(int32_t)(int16_t)sh_r16(SND_ENV + (int16_t)d0);
    sh_w32(a2 + 0x1c, a0);
    shop_sound_delay();
    sh_w8(a2 + 0x1a, 0xff);
}
