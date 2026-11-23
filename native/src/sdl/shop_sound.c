/* The shop's sound ($6477C-$64D4A of the blob): a player for the four audio channels, driven by the vertical-blank interrupt ($2090 = shop_irq_tick) and a command
 * ($2094 = shop_sound_cmd, cmd = (channel << 8) | effect; effect bit 7 = a sampled effect, else a synthesised one).  Its state is shop_data.snd (ShopSound): the channels
 * (chan), what the music left in each channel (mirror: lc, len, per, vol, then had: it had one, has: the effect has it), the tables of the sampled and synthesised
 * effects, the envelopes (offsets from snd, env_off).  It drives the voices of audio.h. */
#include "shop.h"
#include "host.h"
#include "game_all.h"

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
    struct ShopSound *snd = &xenon2.shop_data.snd;
    const struct BankSample *bank = &xenon2.shop_data.bank_head_0;
    const uint8_t *src;
    struct ShopWave *wave = &snd->wave;
    uint32_t size;

    if (snd->built1 != 0)
        return;
    size = FIXED_16_16(bank->size_hi, bank->size_lo);
    src = (const uint8_t *)(bank + 1);
    wave->lc = src;
    src += size;
    size >>= 1;
    wave->len = (uint16_t)size;
    wave->period = (uint16_t)divu(SND_PERIOD_CLOCK, bank->rate);
    snd->silence = src;
    src += SOUND_SILENCE_BYTES;
    snd->waves = src;
    snd->built1 = 0xff;
}

/* shop $64972: the descriptors of the 4 sampled effects (the last part of the sample bank) are built once */
void shop_sound_tables2(void)
{
    struct ShopSound *snd = &xenon2.shop_data.snd;
    const struct BankSample *bank = &xenon2.shop_data.bank_head_1;
    struct ShopSample *sample = snd->samples;
    int16_t n;

    if (snd->built2 != 0)
        return;
    for (n = 3; n >= 0; n--) {
        uint32_t steps, size;

        steps = divu((uint32_t)bank->size_lo * 0x32, bank->rate);
        sample->steps = (uint8_t)(steps + 1);
        size = FIXED_16_16(bank->size_hi, bank->size_lo);
        sample->lc = (const uint8_t *)(bank + 1);
        sample->len = (uint16_t)(size >> 1);
        sample->period = (uint16_t)divu(SND_PERIOD_CLOCK, bank->rate);
        bank = (const struct BankSample *)((const uint8_t *)(bank + 1) + size);
        sample++;
    }
    snd->built2 = 0xff;
}

/* shop $647CE: all the channels are stopped and the audio hardware silenced */
void shop_sound_stop(void)
{
    struct ShopSound *snd = &xenon2.shop_data.snd;
    int i;

    snd->running = 0;
    for (i = 0; i < 4; i++) {
        snd->chan[i].active = 0;
        snd->mirror[i].had = 0;
        snd->mirror[i].has = 0;
    }
    audio_stop_all();
    for (i = 0; i < 4; i++)
        audio_volume(i, 0);
}

/* shop $64828: every channel has one frame left */
void shop_sound_reset(void)
{
    int i;

    for (i = 0; i < 4; i++)
        xenon2.shop_data.snd.chan[i].head.timer = 1;
}

/* the effect on the channel stops; the channel goes back to the music if it had one */
static void channel_stop(int voice, struct ShopChan *ch, struct ShopMirror *mir)
{
    ch->active = 0;
    audio_stop(voice);
    mir->has = 0;
    if (mir->had == 0)
        return;
    audio_period(voice, 0x7c);
    audio_stop(voice);
    shop_sound_delay();
    audio_data(voice, mir->lc);
    audio_length(voice, mir->len);
    audio_period(voice, mir->per);
    audio_volume(voice, mir->vol);
    audio_start(voice);
}

/* shop $64C1E: one frame of a channel (voice = its voice, ch = its record, mir = its mirror) */
static void channel_step(int voice, struct ShopChan *ch, struct ShopMirror *mir)
{
    uint16_t id, period, slide;
    uint32_t pair;

    if (ch->head.timer != 0) {
        ch->head.timer--;
        if (ch->head.timer == 0) {
            channel_stop(voice, ch, mir);
            return;
        }
    }
    id = ch->head.sample_id;
    if (id != 0) {
        const struct ShopSample *sample = &xenon2.shop_data.snd.samples[id & 0x7f];

        period = ch->slide_period;
        slide = sample->slide;
        if (slide == 0) {
            audio_period(voice, ch->slide_period);
            return;
        }
        period = (uint16_t)(period + slide);
        if ((int16_t)period < 0x7c) {
            channel_stop(voice, ch, mir);
            return;
        }
        ch->slide_period = period;
        audio_period(voice, period);
        return;
    }
    if (ch->head.reload_f != 0) {
        ch->count_f--;
        if (ch->count_f == 0) {
            uint8_t b = ch->head.rot_a;
            int carry = b & 1;

            ch->count_f = ch->head.reload_f;
            pair = ch->head.pair_a;
            b = (uint8_t)((b >> 1) | (carry << 7));
            if (!carry)
                pair = (pair << 16) | (pair >> 16);
            ch->head.rot_a = b;
            ch->head.period = (uint16_t)(ch->head.period + (uint16_t)pair);
        }
    }
    ch->head.period = (uint16_t)(ch->head.period + ch->head.step);
    if (ch->head.reload_e != 0) {
        ch->count_e--;
        if (ch->count_e == 0) {
            ch->count_e = ch->head.reload_e;
            ch->head.period = ch->head.period_reset;
        }
    }
    ch->env_count--;
    if (ch->env_count == 0) {
        const uint8_t *env;
        uint8_t v;

        ch->env_count = ch->head.env_speed;
        env = ch->env_ptr;
        v = *env++;
        if (v == 0) {
            channel_stop(voice, ch, mir);
            return;
        }
        if (!(v & 0x80)) {
            ch->env_ptr = env;
            audio_volume(voice, v);
        }
    }
    period = ch->head.period;
    if ((int16_t)period < 0x7c)
        period = 0x7c;
    audio_period(voice, period);
    pair = ch->head.pair_b;
    {
        uint8_t b = ch->head.rot_b;
        int carry = b & 1;

        b = (uint8_t)((b >> 1) | (carry << 7));
        if (!carry)
            pair = (pair << 16) | (pair >> 16);
        ch->head.rot_b = b;
    }
    audio_data(voice, xenon2.shop_data.snd.waves + (int16_t)pair);
    audio_start(voice);
}

/* shop $64BBC: the vertical blank: every active channel goes on one frame */
void shop_sound_tick(void)
{
    struct ShopSound *snd = &xenon2.shop_data.snd;
    int i;

    for (i = 0; i < 4; i++)
        if (snd->chan[i].active != 0)
            channel_step(i, &snd->chan[i], &snd->mirror[i]);
}

/* orig $6477C: the interrupt's call of the player (the handler keeps the registers) */
void shop_irq_tick(void)
{
    shop_sound_tick();
}

/* shop $64AEE: a sampled effect (the id has bit 7) on the channel of the command */
static void start_sampled(uint16_t id)
{
    struct ShopSound *snd = &xenon2.shop_data.snd;
    struct ShopChan *ch;
    const struct ShopSample *sample;
    uint16_t voice;
    uint32_t off;

    shop_sound_tables2();
    voice = (uint8_t)(snd->cmd >> 8);
    ch = &snd->chan[voice];
    ch->head.sample_id = (uint8_t)id;
    audio_stop(voice);
    audio_period(voice, 0x7c);
    audio_length(voice, 1);
    shop_sound_delay();
    sample = &snd->samples[ch->head.sample_id & 0x7f];
    ch->head.timer = sample->steps;
    if (sample->once != 0)
        ch->head.timer = 0;
    audio_length(voice, sample->len);
    audio_data(voice, sample->lc);
    audio_period(voice, sample->period);
    ch->cur_period = sample->period;
    ch->slide_period = sample->period;
    audio_volume(voice, 0x40);
    audio_start(voice);
    ch->active = 0xff;
    shop_sound_delay();
    off = sample->loop_off;
    if ((int32_t)off < 0) {
        audio_data(voice, snd->silence);
        audio_length(voice, 0x20);
        return;
    }
    audio_length(voice, (uint16_t)(sample->len - (uint16_t)(off >> 1)));
    audio_data(voice, sample->lc + off);
}

/* orig $649C6, $647A6: start the effect of the command (cmd = (channel << 8) | effect) on the channel; the music's hold on the channel is marked */
void shop_sound_cmd(uint16_t cmd)
{
    struct ShopSound *snd = &xenon2.shop_data.snd;
    struct ShopChan *ch;
    uint8_t chan;

    CIAA_PRA |= 2;
    cmd &= 0x3ff;
    snd->cmd = cmd;
    chan = (uint8_t)(cmd >> 8);
    if (chan <= 3)
        snd->chan[chan].active = 0;
    shop_sound_tables();
    snd->mirror[chan].has = 0xff;
    if ((int8_t)cmd < 0) {
        start_sampled(cmd);
        return;
    }
    ch = &snd->chan[chan];
    audio_length(chan, 1);
    audio_period(chan, 0x7c);
    shop_sound_delay();
    audio_stop(chan);
    shop_sound_delay();
    ch->head = snd->synth[(uint8_t)cmd];
    audio_length(chan, ch->head.length);
    ch->count_e = ch->head.reload_e;
    ch->count_f = ch->head.reload_f;
    ch->env_count = 1;
    ch->env_ptr = (const uint8_t *)snd + (int16_t)snd->env_off[(int8_t)ch->head.env_index];
    shop_sound_delay();
    ch->active = 0xff;
}
