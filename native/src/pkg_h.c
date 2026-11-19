/* The music and sound-effect player, $19F9E-$1B6CE (its data sits in the same area).  It writes the Paula registers directly
 * (custom->aud[n].lc/len/per/vol, SETCLR_WRITE(dmacon / adkcon)): the SDL mixer reads the channel registers and the dmacon state.
 *
 * Calls (via the pointers at $2090/$2094 of the interrupt handler): music_tick every vertical blank; sfx_play(request) with
 * request = (channel << 8) | effect, effect bit 7 = a sampled effect (sfx_samples), else a synthesised one (sfx_defs);
 * music_start(song) starts a song; music_silence, music_resume, music_fade_out, sfx_stop_all.
 *
 * Offsets: the player addresses its data with words relative to music_base ($19F9E); at(off) makes the address.
 *
 * Sample bank (sample_bank, $1BB80, loaded with the game image; to $2BAA0): 20 instruments, each { uint32 length in bytes, uint16 rate,
 *   8-bit samples }, then 0x40 bytes of silence (silence_ptr, sfx_wave_base = silence + 0x40); the first 20 follow at $1BB80 and a
 *   second part at sample_bank + 0xFF1C holds 18 sampled effects in the same form.  sound_init_instruments / sound_init_sfx_samples
 *   build the tables instruments ($1A738) and sfx_samples ($1B352) from them, once.
 * Song (song_headers $1A828): { tempo, rate, word list offset for each of the 4 channels }.  A list (words, offsets of
 *   sequences, 0 ends and restarts) is in music_data.  A sequence is bytes: 0..0x7F note (index into music_periods), the bytes
 *   0x80-0x8B control (80 next sequence of the list, 81 slide, 82 rest, 83 hold, 84 stop all, 85 transpose, 86 vibrato on,
 *   87 vibrato off, 88 channel transpose, 89 set list, 8A tempo, 8B fade), 0x90-0x9F pitch envelope
 *   (music_env_offsets), 0xA0-0xAF volume envelope (music_vol_env_offsets), 0xB0-0xDF instrument, 0xE0-0xFF note length
 *   ((byte - 0xDF) * music_tempo_mul ticks).
 * Channel state: music_channels[4] (MusicChannel, $1A596) and hw_mirror[4] (what was last written to each hardware channel; the
 *   sound effects use it to give a channel back); sound effects: sfx_channels[4] (SfxChannel, $1B6D0), sfx_request.
 *   Global: music_playing, music_enabled, music_paused, music_volume (word: music_flag_1a584 : music_volume_lo), music_rate,
 *   music_transpose, music_fade_speed, music_tempo_mul. */
#include "pkg_h.h"
#include "game_all.h"
#include "amiga/hw.h"

static uint8_t *at(int32_t off)
{
    return (uint8_t *)((uintptr_t)music_base + (uint32_t)off);
}

static uint16_t word_at(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t long_at(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint16_t get_volume(void)
{
    return (uint16_t)((music_flag_1a584 << 8) | music_volume_lo);
}

static void set_volume(uint16_t v)
{
    music_flag_1a584 = (uint8_t)(v >> 8);
    music_volume_lo = (uint8_t)v;
}

/* divu.w: 0x369E99 / d3; on overflow the destination keeps its low word 0x9E99 */
static uint16_t rate_divide(uint16_t d3)
{
    uint32_t q = 0x369e99u / d3;

    return q > 0xffff ? 0x9e99 : (uint16_t)q;
}

/* orig $1B536: a delay between audio register writes (HLE: audio) */
void audio_delay(void)
{
    volatile int16_t n;

    for (n = 0x200; n >= 0; n--)
        ;
}

/* orig $19FBA, $1A09A: stop the music and the sound effects, audio DMA and modulation off (HLE: audio) */
void music_silence(void)
{
    uint16_t i;

    music_playing = 0;
    for (i = 0; i < 4; i++) {
        sfx_channels[i].active = 0;
        hw_mirror[i].music_on = 0;
        hw_mirror[i].sfx_on = 0;
    }
    SETCLR_WRITE(dmacon, 0x000f);
    SETCLR_WRITE(adkcon, 0x00ff);
    custom->aud[0].vol = 0;
    custom->aud[1].vol = 0;
    custom->aud[2].vol = 0;
    custom->aud[3].vol = 0;
}

/* orig $19FE4, $1A0F4: every sound effect channel ends at its next tick */
void sfx_stop_all(void)
{
    sfx_channels[0].timer = 1;
    sfx_channels[1].timer = 1;
    sfx_channels[2].timer = 1;
    sfx_channels[3].timer = 1;
}

/* orig $19FC8, $1A112: audio DMA on for the four channels (HLE: audio) */
void music_resume(void)
{
    if (music_enabled) {
        SETCLR_WRITE(adkcon, 0x00ff);
        SETCLR_WRITE(dmacon, 0x820f);
        music_playing = 0xff;
    }
}

/* orig $19FF2, $1A13C: start fading the music out */
void music_fade_out(void)
{
    music_fade_speed = 3;
    music_fade_count = 3;
}

/* orig $1B1B6: build the instrument table from the sample bank (once) */
void sound_init_instruments(void)
{
    const uint8_t *p;
    uint16_t i;
    uint32_t len;
    uint16_t rate;

    if (instruments_ready)
        return;
    p = sample_bank;
    for (i = 0; i < 20; i++) {
        len = long_at(p);
        rate = word_at(p + 4);
        p += 6;
        instruments[i].sample = (uint8_t *)p;
        p += len;
        instruments[i].length = (uint16_t)(len >> 1);
        instruments[i].rate = rate_divide(rate);
    }
    silence_ptr = (uint8_t *)p;
    sfx_wave_base = (uint8_t *)p + 0x40;
    instruments_ready = 0xff;
    instruments[5].rate -= 8;
    instruments[6].rate -= 8;
    instruments[7].rate -= 2;
    instruments[11].rate -= 2;
}

/* orig $1B216: build the table of sampled sound effects from the second part of the sample bank (once) */
void sound_init_sfx_samples(void)
{
    const uint8_t *p;
    uint16_t i;
    uint32_t len, d2;
    uint16_t rate, q;

    if (sfx_samples_ready)
        return;
    p = sample_bank + 0xff1c;
    for (i = 0; i < 18; i++) {
        d2 = (uint32_t)word_at(p + 2) * 0x32;
        q = (uint16_t)(d2 / word_at(p + 4));
        if (d2 / word_at(p + 4) > 0xffff)
            q = (uint16_t)d2;
        sfx_samples[i].duration = (uint8_t)(q + 1);
        len = long_at(p);
        rate = word_at(p + 4);
        p += 6;
        sfx_samples[i].sample = (uint8_t *)p;
        p += len;
        sfx_samples[i].length = (uint16_t)(len >> 1);
        sfx_samples[i].period = rate_divide(rate);
    }
    sfx_samples_ready = 0xff;
}

/* orig $19F9E, $19FFC: start song `song` (HLE: audio) */
void music_start(uint16_t song)
{
    const SongHeader *h;
    uint16_t i;

    CIAA_PRA |= 2;
    music_silence();
    sound_init_instruments();
    music_transpose = 0;
    h = (const SongHeader *)((uintptr_t)song_headers + (uint16_t)((uint16_t)(int16_t)(int8_t)song * 10));
    music_tempo_mul = (uint16_t)(int16_t)(int8_t)h->tempo;
    music_rate = h->rate;
    music_rate_copy = music_rate;
    for (i = 0; i < 4; i++) {
        MusicChannel *c = &music_channels[i];

        c->dur = 1;
        c->flags = 0;
        c->vib = 0;
        c->transpose = 0;
        c->loaded = 0xff;
        c->env_start = music_env_data;
        c->env_ptr = music_env_data;
        c->list_off = h->list[i];
        c->list_idx = 2;
        c->stream = at((int16_t)word_at(at((int16_t)h->list[i])));
    }
    set_volume(0x40);
    music_fade_speed = 0;
    music_playing = 0xff;
    music_enabled = 0xff;
}

/* the end of the music: everything off (orig $1A4D8) */
static void music_stop(void)
{
    music_enabled = 0;
    music_playing = 0;
    sfx_channels[0].active = 0;
    sfx_channels[1].active = 0;
    sfx_channels[2].active = 0;
    sfx_channels[3].active = 0;
    hw_mirror[0].music_on = hw_mirror[0].sfx_on = 0;
    hw_mirror[1].music_on = hw_mirror[1].sfx_on = 0;
    hw_mirror[2].music_on = hw_mirror[2].sfx_on = 0;
    hw_mirror[3].music_on = hw_mirror[3].sfx_on = 0;
    SETCLR_WRITE(dmacon, 0x000f);
    SETCLR_WRITE(adkcon, 0x00ff);
}

/* orig $19FAC, $1A154: one tick of the music (every vertical blank), then of the sound effects (HLE: audio) */
void music_tick(void)
{
    uint16_t ch;

    if (!music_playing) {
        sfx_tick();
        return;
    }
    if (music_paused) {
        music_pause_count--;
        if (music_pause_count == 0) {
            music_pause_count = 6;
            return;
        }
    }
    {
        uint16_t sum = (uint16_t)music_accum + music_rate;

        music_accum = (uint8_t)sum;
        if (sum > 0xff) {
            sfx_tick();
            return;
        }
    }
    if (music_fade_speed) {
        if (get_volume() == 0) {
            music_stop();
            return;
        }
        music_fade_count--;
        if (music_fade_count == 0) {
            set_volume(get_volume() - 1);
            if (get_volume() == 0) {
                music_stop();
                return;
            }
            music_fade_count = music_fade_speed;
        }
    }
    for (ch = 0; ch < 4; ch++) {
        HwMirror *m = &hw_mirror[ch];
        MusicChannel *c = &music_channels[ch];
        volatile struct AudioChannel *hw = &custom->aud[ch];
        Instrument *ins = c->instr;
        const uint8_t *a1 = c->stream;
        uint32_t d0;
        uint16_t d1;

        m->music_on = 0xff;
        if (c->loaded == 0) {
            c->loaded = 0xff;
            if (ins->loop_off >= 0) {
                uint8_t *a2 = ins->sample + ins->loop_off;

                d1 = (uint16_t)(ins->length - (uint16_t)((uint32_t)ins->loop_off >> 1));
                m->lc = a2;
                m->len = d1;
                if (!m->sfx_on) {
                    hw->lc = ADDR(a2);
                    hw->len = d1;
                }
            } else {
                m->lc = silence_ptr;
                m->len = 0x20;
                if (!m->sfx_on) {
                    hw->lc = ADDR(silence_ptr);
                    hw->len = 0x20;
                }
            }
        }
        c->dur--;
        if (c->dur == 0) {
            c->flags = 0;
            goto fetch;
        }
        if (c->dur == 1) {
            if (!m->sfx_on && *a1 != 0x83)
                SETCLR_WRITE(dmacon, (uint16_t)(1 << ch));
            continue;
        }
        goto effects;

    fetch:
        d0 = *a1++;
        if (!(d0 & 0x80)) {
            /* a note */
            const uint8_t *a2;
            int16_t v;

            c->note = (uint8_t)d0;
            d0 = (uint8_t)(d0 + music_transpose + c->transpose);
            a2 = c->vol_start;
            v = (int8_t)*a2++;
            c->vol_ptr = a2;
            c->vol_count = c->vol_speed;
            m->lc = ins->sample;
            m->len = ins->length;
            d1 = (uint16_t)(((uint32_t)(uint16_t)v * get_volume()) & 0xffff) >> 6;
            m->vol = d1;
            if (!m->sfx_on) {
                hw->lc = ADDR(ins->sample);
                hw->len = ins->length;
                hw->vol = d1;
            }
            d0 = ((uint32_t)music_periods[d0 & 0xff] * ins->rate) >> 10;
            m->per = (uint16_t)d0;
            if (!m->sfx_on)
                hw->per = (uint16_t)d0;
            c->loaded = 0;
            goto start;
        }
        /* a control byte */
        if (d0 >= 0xe0) {
            c->dur_reload = (uint16_t)(((uint8_t)(d0 - 0xdf)) * music_tempo_mul);
            goto fetch;
        }
        if (d0 >= 0xb0) {
            ins = &instruments[d0 - 0xb0];
            c->instr = ins;
            goto fetch;
        }
        if (d0 >= 0xa0) {
            const uint8_t *a2 = at((int16_t)music_vol_env_offsets[d0 - 0xa0]);

            c->vol_start = a2;
            c->vol_speed = a2[-1];
            goto fetch;
        }
        if (d0 >= 0x90) {
            const uint8_t *a2 = at((int16_t)music_env_offsets[d0 - 0x90]);

            c->env_start = a2;
            c->env_ptr = a2;
            goto fetch;
        }
        switch (d0) {
        case 0x80: {
            uint16_t idx = c->list_idx;
            int16_t off = (int16_t)(c->list_off + idx);

            idx += 2;
            if (word_at(at(off)) == 0) {
                off = (int16_t)c->list_off;
                idx = 2;
            }
            a1 = at((int16_t)word_at(at(off)));
            c->list_idx = idx;
            goto fetch;
        }
        case 0x81:
            c->slide = 0;
            c->slide_step = (int8_t)*a1++;
            c->slide_delay = *a1++;
            c->flags |= 2;
            goto fetch;
        case 0x82:
            c->dur = c->dur_reload;
            c->stream = a1;
            m->lc = silence_ptr;
            m->len = 0x20;
            if (!m->sfx_on) {
                hw->lc = ADDR(silence_ptr);
                hw->len = 0x20;
            }
            continue;
        case 0x83:
            goto start;
        case 0x84:
            music_stop();
            return;
        case 0x85:
            music_transpose = *a1++;
            goto fetch;
        case 0x86:
            c->vib = 0xff;
            c->vib_step = *a1++;
            c->vib_limit = *a1++;
            c->vib_pos = 0;
            goto fetch;
        case 0x87:
            c->vib = 0;
            goto fetch;
        case 0x88:
            c->transpose = *a1++;
            goto fetch;
        case 0x89:
            c->list_off = (uint16_t)(a1[0] << 8 | a1[1]);
            a1 += 2;
            c->list_idx = 0;
            goto fetch;
        case 0x8a:
            music_tempo_mul = (uint16_t)(int16_t)(int8_t)*a1++;
            goto fetch;
        case 0x8b:
            music_fade_speed = *a1++;
            music_fade_count = music_fade_speed;
            goto fetch;
        default:
            return;   /* the original jumps through the data after the table */
        }

    start:
        c->stream = a1;
        c->dur = c->dur_reload;
        SETCLR_WRITE(dmacon, (uint16_t)(0x8200 | (1 << ch)));
        continue;

    effects: {
            uint8_t d1b;
            const uint8_t *a2;
            int16_t w;

            d0 = (uint8_t)(c->note + music_transpose + c->transpose);
            a1 = c->env_ptr;
            d1b = *a1++;
            if (d1b & 0x80)
                a1 = c->env_start;
            d1b &= 0x7f;
            c->env_ptr = a1;
            d0 = (uint8_t)(d0 + d1b);
            d0 = ((uint32_t)music_periods[d0] * ins->rate) >> 10;
            if (c->flags & 2) {
                if (c->slide_delay) {
                    c->slide_delay--;
                } else {
                    c->slide = (int16_t)(c->slide + c->slide_step);
                    d0 = (uint16_t)(d0 - c->slide);
                }
            }
            if (c->vib) {
                uint8_t pos;

                if (c->vib & 0x80) {
                    pos = (uint8_t)(c->vib_pos + c->vib_step);
                    c->vib_pos = pos;
                    if (pos == c->vib_limit)
                        c->vib ^= 0x80;
                } else {
                    pos = (uint8_t)(c->vib_pos - c->vib_step);
                    c->vib_pos = pos;
                    if (pos == 0)
                        c->vib ^= 0x80;
                }
                if (c->vib_pos == 0)
                    c->vib ^= 1;
                w = (int8_t)pos;
                if (c->vib & 1)
                    d0 = (uint16_t)(d0 + w);
                else
                    d0 = (uint16_t)(d0 - w);
            }
            m->per = (uint16_t)d0;
            if (!m->sfx_on)
                hw->per = (uint16_t)d0;
            if (c->vol_count-- == 0) {
                int8_t b;

                c->vol_count = c->vol_speed;
                a2 = c->vol_ptr;
                b = (int8_t)*a2++;
                if (b >= 0)
                    c->vol_ptr = a2;
                d1 = (uint16_t)(((uint32_t)(b & 0x7f) * get_volume()) & 0xffff) >> 6;
                m->vol = d1;
                if (!m->sfx_on)
                    hw->vol = d1;
            }
        }
    }
    sfx_tick();
}

/* orig $19FD6, $1B26A: start sound effect `request` = (channel << 8) | effect (HLE: audio) */
void sfx_play(uint16_t request)
{
    uint16_t ch, d2;
    int8_t id;
    SfxChannel *r;
    volatile struct AudioChannel *hw;

    CIAA_PRA |= 2;
    SETCLR_WRITE(adkcon, 0x00ff);
    request &= 0x3ff;
    sfx_request = request;
    ch = sfx_request >> 8;
    if (ch < 4)
        sfx_channels[ch].active = 0;
    sound_init_instruments();
    hw_mirror[ch].sfx_on = 0xff;
    r = &sfx_channels[ch];
    hw = &custom->aud[ch];
    d2 = (uint16_t)(1 << ch);
    id = (int8_t)request;

    if (id >= 0) {
        const SfxDef *def = &sfx_defs[(uint8_t)id];
        int8_t e;

        hw->len = 1;
        hw->per = 0x7c;
        audio_delay();
        SETCLR_WRITE(dmacon, d2);
        audio_delay();
        r->step = def->step;
        r->period_reset = def->period_reset;
        r->pair_a = def->pair_a;
        r->period = def->period;
        r->pair_b = def->pair_b;
        r->reload_e = def->reload_e;
        r->reload_f = def->reload_f;
        r->rot_a = def->b10;
        r->rot_b = def->b11;
        r->timer = def->b12;
        r->env_speed = def->b13;
        r->env_index = def->env_index;
        r->sample_id = def->b15;
        r->length = def->length;
        hw->len = r->length;
        r->count_e = r->reload_e;
        r->count_f = r->reload_f;
        r->env_count = 1;
        e = (int8_t)r->env_index;
        r->env_ptr = at((int16_t)sfx_env_offsets[e]);
        audio_delay();
        r->active = 0xff;
        return;
    } else {
        const SfxSample *s;
        int32_t d0;

        sound_init_sfx_samples();
        r->sample_id = (uint8_t)id;
        SETCLR_WRITE(dmacon, d2);
        hw->per = 0x7c;
        hw->len = 1;
        audio_delay();
        s = &sfx_samples[r->sample_id & 0x7f];
        r->timer = s->duration;
        if (s->endless)
            r->timer = 0;
        hw->len = s->length;
        hw->lc = ADDR(s->sample);
        hw->per = s->period;
        r->cur_period = s->period;
        r->slide_period = s->period;
        hw->vol = 0x40;
        SETCLR_WRITE(dmacon, (uint16_t)(d2 | 0x8200));
        r->active = 0xff;
        audio_delay();
        d0 = s->loop_off;
        if (d0 >= 0) {
            hw->len = (uint16_t)(s->length - (uint16_t)((uint32_t)d0 >> 1));
            hw->lc = ADDR(s->sample + d0);
        } else {
            hw->lc = ADDR(silence_ptr);
            hw->len = 0x20;
        }
    }
}

/* orig $1B5A2: one tick of a sound effect channel (HLE: audio) */
static void sfx_channel_tick(SfxChannel *r, volatile struct AudioChannel *hw, uint16_t d2, HwMirror *m)
{
    uint16_t d0, d1;

    if (r->timer != 0) {
        r->timer--;
        if (r->timer == 0)
            goto stop;
    }
    if (r->sample_id != 0) {
        const SfxSample *s = &sfx_samples[r->sample_id & 0x7f];

        d0 = r->slide_period;
        d1 = (uint16_t)s->slide;
        if (d1 != 0) {
            d0 = (uint16_t)(d0 + d1);
            if ((int16_t)d0 < 0x7c)
                goto stop;
            r->slide_period = d0;
            hw->per = d0;
            return;
        }
        hw->per = r->slide_period;
        return;
    }
    if (r->reload_f != 0) {
        r->count_f--;
        if (r->count_f == 0) {
            uint32_t d4 = r->pair_a;
            uint8_t b;

            r->count_f = r->reload_f;
            b = r->rot_a;
            if (b & 1)
                b = (uint8_t)((b >> 1) | 0x80);
            else {
                b = (uint8_t)(b >> 1);
                d4 = (d4 << 16) | (d4 >> 16);
            }
            r->rot_a = b;
            r->period = (uint16_t)(r->period + (uint16_t)d4);
        }
    }
    r->period = (uint16_t)(r->period + (uint16_t)r->step);
    if (r->reload_e != 0) {
        r->count_e--;
        if (r->count_e == 0) {
            r->count_e = r->reload_e;
            r->period = r->period_reset;
        }
    }
    r->env_count--;
    if (r->env_count == 0) {
        int8_t b;
        const uint8_t *a2 = r->env_ptr;

        r->env_count = r->env_speed;
        b = (int8_t)*a2++;
        if (b == 0)
            goto stop;
        if (b > 0) {
            r->env_ptr = a2;
            hw->vol = (uint16_t)(int16_t)b;
        }
    }
    {
        int16_t per = (int16_t)r->period;
        uint32_t d1l = r->pair_b;
        uint8_t b = r->rot_b;

        if (per < 0x7c)
            per = 0x7c;
        hw->per = (uint16_t)per;
        if (b & 1)
            b = (uint8_t)((b >> 1) | 0x80);
        else {
            b = (uint8_t)(b >> 1);
            d1l = (d1l << 16) | (d1l >> 16);
        }
        r->rot_b = b;
        hw->lc = ADDR(sfx_wave_base + (int16_t)d1l);
        SETCLR_WRITE(dmacon, (uint16_t)(d2 | 0x8200));
    }
    return;

stop:
    r->active = 0;
    SETCLR_WRITE(dmacon, d2);
    m->sfx_on = 0;
    if (m->music_on) {
        hw->per = 0x7c;
        SETCLR_WRITE(dmacon, d2);
        audio_delay();
        hw->lc = ADDR(m->lc);
        hw->len = m->len;
        hw->per = m->per;
        hw->vol = m->vol;
        SETCLR_WRITE(dmacon, (uint16_t)(d2 | 0x8200));
    }
}

/* orig $1B540, $1A14E: tick every active sound effect channel (HLE: audio) */
void sfx_tick(void)
{
    uint16_t i;

    for (i = 0; i < 4; i++)
        if (sfx_channels[i].active)
            sfx_channel_tick(&sfx_channels[i], &custom->aud[i], (uint16_t)(1 << i), &hw_mirror[i]);
}
