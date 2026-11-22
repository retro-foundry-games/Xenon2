/* The shop's sound ($6477C-$64D4A of the blob): a player for the four audio channels, driven by the vertical-blank interrupt ($2090 = shop_irq_tick) and a command
 * ($2094 = shop_sound_cmd, cmd = (channel << 8) | effect; effect bit 7 = a sampled effect, else a synthesised one).  Its state is raw memory of the blob:
 *   SND_CH:     4 records of $24 bytes (the channels): 0 period slide, 2 period to restart at, 4 pair of slides, 8 period, $A pair of waveform offsets, $E and $F reload of
 *               the counters at $18 and $19, $10 and $11 the bits that choose the half of a pair, $12 frames left, $13 reload of the envelope's step counter ($1B),
 *               $14 envelope, $15 the sampled effect (bit 7), $1A active, $1C the envelope's next byte (0 ends, bit 7 holds the volume), $20 and $22 periods.
 *   SND_MIRROR: 4 records of $C bytes: what the music left in the channel (lc, len, per, vol, then $A: it had one, $B: the effect has it).
 * The hardware is written as the original does (HLE: audio). */
#include "shop.h"
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
    uint32_t src, desc;
    int16_t n;

    if (sh_r8(SND_BUILT1) != 0)
        return;
    src = SND_BANK;
    desc = SND_TAB1;
    for (n = 0; n >= 0; n--) {
        uint32_t size = sh_r32(src);
        uint16_t rate = sh_r16(src + 4);

        src += 6;
        sh_w32(desc, src);
        src += size;
        size >>= 1;
        sh_w16(desc + 8, (uint16_t)size);
        sh_w16(desc + 0xa, (uint16_t)divu(SND_PERIOD_CLOCK, rate));
        desc += 0xc;
    }
    sh_w32(SND_BASE + 0xec, src);
    src += 0x40;
    sh_w32(SND_WAVES, src);
    sh_w8(SND_BUILT1, 0xff);
}

/* shop $64972: the descriptors of the 4 sampled effects (the last part of the sample bank) are built once */
void shop_sound_tables2(void)
{
    uint32_t src, desc;
    int16_t n;

    if (sh_r8(SND_BUILT2) != 0)
        return;
    src = SND_BANK + 0xf42;
    desc = SND_SAMPLES;
    for (n = 3; n >= 0; n--) {
        uint32_t steps = (uint32_t)sh_r16(src + 2) * 0x32, size;
        uint16_t rate;

        steps = divu(steps, sh_r16(src + 4));
        sh_w8(desc + 0xc, (uint8_t)(steps + 1));
        size = sh_r32(src);
        rate = sh_r16(src + 4);
        src += 6;
        sh_w32(desc, src);
        src += size;
        size >>= 1;
        sh_w16(desc + 8, (uint16_t)size);
        sh_w16(desc + 0xa, (uint16_t)divu(SND_PERIOD_CLOCK, rate));
        desc += 0x10;
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
static void channel_stop(volatile struct AudioChannel *hw, uint32_t ch, uint32_t mir, uint16_t dma_bit)
{
    sh_w8(ch + 0x1a, 0);
    SETCLR_WRITE(dmacon, dma_bit);
    sh_w8(mir + 0xb, 0);
    if (sh_r8(mir + 0xa) == 0)
        return;
    hw->per = 0x7c;
    SETCLR_WRITE(dmacon, dma_bit);
    shop_sound_delay();
    hw->lc = ADDR(GPTR(sh_r32(mir)));
    hw->len = sh_r16(mir + 4);
    hw->per = sh_r16(mir + 6);
    hw->vol = sh_r16(mir + 8);
    SETCLR_WRITE(dmacon, (uint16_t)(dma_bit | 0x8200));
}

/* shop $64C1E: one frame of a channel (hw = its registers, ch = its record, mir = its mirror, dma_bit = its DMA bit) */
static void channel_step(volatile struct AudioChannel *hw, uint32_t ch, uint32_t mir, uint16_t dma_bit)
{
    uint16_t id, period, slide;
    uint32_t sample, env, pair;

    if (sh_r8(ch + 0x12) != 0) {
        sh_w8(ch + 0x12, (uint8_t)(sh_r8(ch + 0x12) - 1));
        if (sh_r8(ch + 0x12) == 0) {
            channel_stop(hw, ch, mir, dma_bit);
            return;
        }
    }
    id = sh_r8(ch + 0x15);
    if (id != 0) {
        sample = SND_SAMPLES + (uint32_t)(id & 0x7f) * 0x10;
        period = sh_r16(ch + 0x22);
        slide = sh_r16(sample + 0xe);
        if (slide == 0) {
            hw->per = sh_r16(ch + 0x22);
            return;
        }
        period = (uint16_t)(period + slide);
        if ((int16_t)period < 0x7c) {
            channel_stop(hw, ch, mir, dma_bit);
            return;
        }
        sh_w16(ch + 0x22, period);
        hw->per = period;
        return;
    }
    if (sh_r8(ch + 0xf) != 0) {
        sh_w8(ch + 0x19, (uint8_t)(sh_r8(ch + 0x19) - 1));
        if (sh_r8(ch + 0x19) == 0) {
            uint8_t b = sh_r8(ch + 0x10);
            int carry = b & 1;

            sh_w8(ch + 0x19, sh_r8(ch + 0xf));
            pair = sh_r32(ch + 4);
            b = (uint8_t)((b >> 1) | (carry << 7));
            if (!carry)
                pair = (pair << 16) | (pair >> 16);
            sh_w8(ch + 0x10, b);
            sh_w16(ch + 8, (uint16_t)(sh_r16(ch + 8) + (uint16_t)pair));
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
        env = sh_r32(ch + 0x1c);
        v = sh_r8(env++);
        if (v == 0) {
            channel_stop(hw, ch, mir, dma_bit);
            return;
        }
        if (!(v & 0x80)) {
            sh_w32(ch + 0x1c, env);
            hw->vol = v;
        }
    }
    period = sh_r16(ch + 8);
    if ((int16_t)period < 0x7c)
        period = 0x7c;
    hw->per = period;
    pair = sh_r32(ch + 0xa);
    {
        uint8_t b = sh_r8(ch + 0x11);
        int carry = b & 1;

        b = (uint8_t)((b >> 1) | (carry << 7));
        if (!carry)
            pair = (pair << 16) | (pair >> 16);
        sh_w8(ch + 0x11, b);
    }
    hw->lc = ADDR(GPTR(sh_r32(SND_WAVES) + (uint32_t)(int32_t)(int16_t)pair));
    SETCLR_WRITE(dmacon, (uint16_t)(dma_bit | 0x8200));
}

/* shop $64BBC: the vertical blank: every active channel goes on one frame */
void shop_sound_tick(void)
{
    int i;

    for (i = 0; i < 4; i++)
        if (sh_r8(SND_CH + 0x24u * i + 0x1a) != 0)
            channel_step(&custom->aud[i], SND_CH + 0x24u * i, SND_MIRROR + 0xcu * i, (uint16_t)(1 << i));
}

/* orig $6477C: the interrupt's call of the player (the handler keeps the registers) */
void shop_irq_tick(void)
{
    shop_sound_tick();
}

/* shop $64AEE: a sampled effect (the id has bit 7) on the channel of the command */
static void start_sampled(uint16_t id)
{
    uint32_t ch, sample, loop_addr;
    uint16_t dma_bit, chan;
    volatile struct AudioChannel *hw;

    shop_sound_tables2();
    chan = sh_r8(SND_CMD);
    ch = SND_CH + 0x24u * chan;
    sh_w8(ch + 0x15, (uint8_t)id);
    dma_bit = (uint16_t)(1u << chan);
    SETCLR_WRITE(dmacon, dma_bit);
    hw = &custom->aud[chan];
    hw->per = 0x7c;
    hw->len = 1;
    shop_sound_delay();
    sample = SND_SAMPLES + (uint32_t)(sh_r8(ch + 0x15) & 0x7f) * 0x10;
    sh_w8(ch + 0x12, sh_r8(sample + 0xc));
    if (sh_r8(sample + 0xd) != 0)
        sh_w8(ch + 0x12, 0);
    hw->len = sh_r16(sample + 8);
    hw->lc = ADDR(GPTR(sh_r32(sample)));
    hw->per = sh_r16(sample + 0xa);
    sh_w16(ch + 0x20, sh_r16(sample + 0xa));
    sh_w16(ch + 0x22, sh_r16(sample + 0xa));
    hw->vol = 0x40;
    dma_bit |= 0x8200;
    SETCLR_WRITE(dmacon, dma_bit);
    sh_w8(ch + 0x1a, 0xff);
    shop_sound_delay();
    id = 0;
    {
        uint32_t off = sh_r32(sample + 4);

        if ((int32_t)off < 0) {
            hw->lc = ADDR(GPTR(sh_r32(SND_SILENCE)));
            hw->len = 0x20;
            return;
        }
        loop_addr = sh_r32(sample) + off;
        off >>= 1;
        hw->len = (uint16_t)(sh_r16(sample + 8) - (uint16_t)off);
        hw->lc = ADDR(GPTR(loop_addr));
    }
}

/* orig $649C6, $647A6: start the effect of the command (cmd = (channel << 8) | effect) on the channel; the music's hold on the channel is marked */
void shop_sound_cmd(uint16_t cmd)
{
    uint32_t synth, env, rec, rec_alias;
    uint16_t dma_bit, offset;
    uint8_t ch;
    volatile struct AudioChannel *hw;
    int i;

    CIAA_PRA |= 2;
    SETCLR_WRITE(adkcon, 0x00ff);
    cmd &= 0x3ff;
    sh_w16(SND_CMD, cmd);
    ch = sh_r8(SND_CMD);
    if (ch <= 3)
        sh_w8(SND_CH + 0x24u * ch + 0x1a, 0);
    shop_sound_tables();
    sh_w8(SND_MIRROR + (uint16_t)(sh_r8(SND_CMD) * 0xc) + 0xb, 0xff);
    if ((int8_t)cmd < 0) {
        start_sampled(cmd);
        return;
    }
    offset = (uint16_t)((uint8_t)cmd * 0x18);
    ch = sh_r8(SND_CMD);
    rec = SND_CH + 0x24u * ch;
    dma_bit = (uint16_t)(1u << ch);
    hw = &custom->aud[ch];
    synth = SND_SYNTH + (int16_t)offset;
    rec_alias = rec;
    hw->len = 1;
    hw->per = 0x7c;
    shop_sound_delay();
    SETCLR_WRITE(dmacon, dma_bit);
    shop_sound_delay();
    for (i = 0; i < 12; i++)
        sh_w16(rec + 2u * i, sh_r16(synth + 2u * i));
    hw->len = sh_r16(rec_alias + 0x16);
    sh_w16(rec_alias + 0x18, sh_r16(rec_alias + 0xe));
    sh_w8(rec_alias + 0x1b, 1);
    offset = (uint16_t)(int16_t)(int8_t)sh_r8(rec_alias + 0x14);
    offset = (uint16_t)(offset + offset);
    env = SND_BASE + (uint32_t)(int32_t)(int16_t)sh_r16(SND_ENV + (int16_t)offset);
    sh_w32(rec_alias + 0x1c, env);
    shop_sound_delay();
    sh_w8(rec_alias + 0x1a, 0xff);
}
