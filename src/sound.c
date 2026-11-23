/* The music and sound-effect player, $19F9E-$1B6CE (its data sits in the same area).  It drives the four voices of the sound hardware with the audio_ functions
 * (audio.h: the sample, its length, the period and the volume; start and stop).
 *
 * Calls (via the pointers at $2090/$2094 of the interrupt handler): music_tick every vertical blank; sfx_play(request) with
 * request = (channel << 8) | effect, effect bit 7 = a sampled effect (sfx_samples), else a synthesised one (sfx_defs);
 * music_start(song) starts a song; music_silence, music_resume, music_fade_out, sfx_stop_all.
 *
 * Offsets: the player addresses its data with words relative to the start of its code ($19F9E, MUSIC_OFFSET_ZERO); each table has a function that turns an offset into a pointer into it.
 *
 * Sample bank (sample_bank, $1BB80, loaded with the game image): 20 instruments, each { uint32 length in bytes, uint16 rate,
 *   8-bit samples }, then 0x40 bytes of silence (silence_ptr, sfx_wave_base = silence + 0x40); the first 20 follow at $1BB80 and a
 *   second part at sample_bank + SAMPLE_EFFECTS_OFFSET holds 18 sampled effects in the same form.  sound_init_instruments / sound_init_sfx_samples
 *   build the tables instruments ($1A738) and sfx_samples ($1B352) from them, once.
 * Song (song_headers $1A828): { tempo, rate, word list offset for each of the 4 channels }.  A list (words, offsets of
 *   sequences, 0 ends and restarts) is in music_data.  A sequence is bytes: 0..0x7F note (index into music_periods), the bytes
 *   0x80-0x8B control (80 next sequence of the list, 81 slide, 82 rest, 83 hold, 84 stop all, 85 transpose, 86 vibrato on,
 *   87 vibrato off, 88 channel transpose, 89 set list, 8A tempo, 8B fade), 0x90-0x9F pitch envelope
 *   (music_env_offsets), 0xA0-0xAF volume envelope (music_vol_env_offsets), 0xB0-0xDF instrument, 0xE0-0xFF note length
 *   ((byte - 0xDF) * music_tempo_mul ticks).
 * Channel state: music_channels[4] (MusicChannel, $1A596) and hw_mirror[4] (what was last written to each hardware channel; the
 *   sound effects use it to give a channel back); sound effects: sfx_channels[4] (SfxChannel, $1B6D0), sfx_request.
 *   Global: music_playing, music_enabled, music_paused, music_volume (word: music_volume_hi : music_volume_lo), music_rate,
 *   music_transpose, music_fade_speed, music_tempo_mul. */
#include "sound.h"
#include "game_all.h"
#include "audio.h"
#include "amiga/hw.h"

/* the music file's offsets (words, signed) count from the start of the player's code */
#define MUSIC_OFFSET_ZERO 0x19f9e

/* the bytes of TABLE (a member of struct Xenon2, at ADDRESS in the original) that the offset OFF of the music file names */
static const uint8_t *in_table(const uint8_t *table, uint32_t address, int16_t off)
{
    return table + (MUSIC_OFFSET_ZERO + off - address);
}

static const uint8_t *pitch_envelope(int16_t off)
{
    return in_table(xenon2.music_env_data, offsetof(struct Xenon2, music_env_data), off);
}

static const uint8_t *volume_envelope(int16_t off)
{
    return in_table(xenon2.music_vol_env_data, offsetof(struct Xenon2, music_vol_env_data), off);
}

static const uint8_t *effect_envelope(int16_t off)
{
    return in_table(xenon2.sfx_env_data, offsetof(struct Xenon2, sfx_env_data), off);
}

static const uint8_t *sequence(int16_t off)
{
    return in_table(xenon2.music_data, offsetof(struct Xenon2, music_data), off);
}

/* the word of a channel's list at OFF */
static uint16_t list_word(int16_t off)
{
    return xenon2.music_lists[(uint16_t)(MUSIC_OFFSET_ZERO + off - offsetof(struct Xenon2, music_lists)) / 2];
}

static uint16_t get_volume(void)
{
    return (uint16_t)((xenon2.music_volume_hi << 8) | xenon2.music_volume_lo);
}

static void set_volume(uint16_t v)
{
    xenon2.music_volume_hi = (uint8_t)(v >> 8);
    xenon2.music_volume_lo = (uint8_t)v;
}

/* divu.w: 0x369E99 / rate; on overflow the destination keeps its low word 0x9E99 */
static uint16_t rate_divide(uint16_t rate)
{
    uint32_t quot = 0x369e99u / rate;

    return quot > 0xffff ? 0x9e99 : (uint16_t)quot;
}

/* orig $1B536: a delay between audio register writes */
void audio_delay(void)
{
    volatile int16_t n;

    for (n = 0x200; n >= 0; n--)
        ;
}

/* orig $19FBA, $1A09A: stop the music and the sound effects, audio DMA and modulation off */
void music_silence(void)
{
    uint16_t i;

    xenon2.music_playing = 0;
    for (i = 0; i < 4; i++) {
        xenon2.sfx_channels[i].active = 0;
        xenon2.hw_mirror[i].music_on = 0;
        xenon2.hw_mirror[i].sfx_on = 0;
    }
    audio_stop_all();
    for (i = 0; i < 4; i++)
        audio_volume(i, 0);
}

/* orig $19FE4, $1A0F4: every sound effect channel ends at its next tick */
void sfx_stop_all(void)
{
    xenon2.sfx_channels[0].timer = 1;
    xenon2.sfx_channels[1].timer = 1;
    xenon2.sfx_channels[2].timer = 1;
    xenon2.sfx_channels[3].timer = 1;
}

/* orig $19FC8, $1A112: audio DMA on for the four channels */
void music_resume(void)
{
    if (xenon2.music_enabled) {
        audio_start_all();
        xenon2.music_playing = 0xff;
    }
}

/* orig $19FF2, $1A13C: start fading the music out */
void music_fade_out(void)
{
    xenon2.music_fade_speed = 3;
    xenon2.music_fade_count = 3;
}

/* orig $1B1B6: build the instrument table from the sample bank (once) */
void sound_init_instruments(void)
{
    const struct SampleEntry *entry;
    uint16_t i;

    if (xenon2.instruments_ready)
        return;
    entry = (const struct SampleEntry *)xenon2.sample_bank;
    for (i = 0; i < 20; i++) {
        xenon2.instruments[i].sample = (uint8_t *)entry->samples;
        xenon2.instruments[i].length = (uint16_t)(entry->header.length >> 1);
        xenon2.instruments[i].rate = rate_divide(entry->header.rate);
        entry = (const struct SampleEntry *)(entry->samples + entry->header.length);
    }
    xenon2.silence_ptr = (uint8_t *)entry;   /* (the silence follows the last entry) */
    xenon2.sfx_wave_base = (uint8_t *)entry + 0x40;
    xenon2.instruments_ready = 0xff;
    xenon2.instruments[5].rate -= 8;
    xenon2.instruments[6].rate -= 8;
    xenon2.instruments[7].rate -= 2;
    xenon2.instruments[11].rate -= 2;
}

/* orig $1B216: build the table of sampled sound effects from the second part of the sample bank (once) */
void sound_init_sfx_samples(void)
{
    const struct SampleEntry *entry;
    uint16_t i;
    uint32_t scaled;
    uint16_t quot;

    if (xenon2.sfx_samples_ready)
        return;
    entry = (const struct SampleEntry *)(xenon2.sample_bank + SAMPLE_EFFECTS_OFFSET);
    for (i = 0; i < 18; i++) {
        scaled = (uint32_t)(uint16_t)entry->header.length * 0x32;   /* (the low word of the length) */
        quot = (uint16_t)(scaled / entry->header.rate);
        if (scaled / entry->header.rate > 0xffff)
            quot = (uint16_t)scaled;
        xenon2.sfx_samples[i].duration = (uint8_t)(quot + 1);
        xenon2.sfx_samples[i].sample = (uint8_t *)entry->samples;
        xenon2.sfx_samples[i].length = (uint16_t)(entry->header.length >> 1);
        xenon2.sfx_samples[i].period = rate_divide(entry->header.rate);
        entry = (const struct SampleEntry *)(entry->samples + entry->header.length);
    }
    xenon2.sfx_samples_ready = 0xff;
}

/* orig $19F9E, $19FFC: start song `song` */
void music_start(uint16_t song)
{
    const SongHeader *header;
    uint16_t i;

    CIAA_PRA |= 2;
    music_silence();
    sound_init_instruments();
    xenon2.music_transpose = 0;
    header = &xenon2.song_headers[(uint16_t)(int16_t)(int8_t)song];
    xenon2.music_tempo_mul = (uint16_t)(int16_t)(int8_t)header->tempo;
    xenon2.music_rate = header->rate;
    xenon2.music_rate_copy = xenon2.music_rate;
    for (i = 0; i < 4; i++) {
        MusicChannel *chan = &xenon2.music_channels[i];

        chan->dur = 1;
        chan->flags = 0;
        chan->vib = 0;
        chan->transpose = 0;
        chan->loaded = 0xff;
        chan->env_start = xenon2.music_env_data;
        chan->env_ptr = xenon2.music_env_data;
        chan->list_off = header->list[i];
        chan->list_idx = 2;
        chan->stream = sequence((int16_t)list_word((int16_t)header->list[i]));
    }
    set_volume(0x40);
    xenon2.music_fade_speed = 0;
    xenon2.music_playing = 0xff;
    xenon2.music_enabled = 0xff;
}

/* the end of the music: everything off (orig $1A4D8) */
static void music_stop(void)
{
    xenon2.music_enabled = 0;
    xenon2.music_playing = 0;
    xenon2.sfx_channels[0].active = 0;
    xenon2.sfx_channels[1].active = 0;
    xenon2.sfx_channels[2].active = 0;
    xenon2.sfx_channels[3].active = 0;
    xenon2.hw_mirror[0].music_on = xenon2.hw_mirror[0].sfx_on = 0;
    xenon2.hw_mirror[1].music_on = xenon2.hw_mirror[1].sfx_on = 0;
    xenon2.hw_mirror[2].music_on = xenon2.hw_mirror[2].sfx_on = 0;
    xenon2.hw_mirror[3].music_on = xenon2.hw_mirror[3].sfx_on = 0;
    audio_stop_all();
}

/* orig $19FAC, $1A154: one tick of the music (every vertical blank), then of the sound effects */
void music_tick(void)
{
    uint16_t ch;

    if (!xenon2.music_playing) {
        sfx_tick();
        return;
    }
    if (xenon2.music_paused) {
        xenon2.music_pause_count--;
        if (xenon2.music_pause_count == 0) {
            xenon2.music_pause_count = 6;
            return;
        }
    }
    {
        uint16_t sum = (uint16_t)xenon2.music_accum + xenon2.music_rate;

        xenon2.music_accum = (uint8_t)sum;
        if (sum > 0xff) {
            sfx_tick();
            return;
        }
    }
    if (xenon2.music_fade_speed) {
        if (get_volume() == 0) {
            music_stop();
            return;
        }
        xenon2.music_fade_count--;
        if (xenon2.music_fade_count == 0) {
            set_volume(get_volume() - 1);
            if (get_volume() == 0) {
                music_stop();
                return;
            }
            xenon2.music_fade_count = xenon2.music_fade_speed;
        }
    }
    for (ch = 0; ch < 4; ch++) {
        HwMirror *mirror = &xenon2.hw_mirror[ch];
        MusicChannel *chan = &xenon2.music_channels[ch];
        Instrument *ins = chan->instr;
        const uint8_t *stream = chan->stream;
        uint32_t code;
        uint16_t vol;

        mirror->music_on = 0xff;
        if (chan->loaded == 0) {
            chan->loaded = 0xff;
            if (ins->loop_off >= 0) {
                uint8_t *loop = ins->sample + ins->loop_off;
                uint16_t loop_len;

                loop_len = (uint16_t)(ins->length - (uint16_t)((uint32_t)ins->loop_off >> 1));
                mirror->lc = loop;
                mirror->len = loop_len;
                if (!mirror->sfx_on) {
                    audio_data(ch, loop);
                    audio_length(ch, loop_len);
                }
            } else {
                mirror->lc = xenon2.silence_ptr;
                mirror->len = 0x20;
                if (!mirror->sfx_on) {
                    audio_data(ch, xenon2.silence_ptr);
                    audio_length(ch, 0x20);
                }
            }
        }
        chan->dur--;
        if (chan->dur == 0) {
            chan->flags = 0;
            goto fetch;
        }
        if (chan->dur == 1) {
            if (!mirror->sfx_on && *stream != 0x83)
                audio_stop(ch);
            continue;
        }
        goto effects;

    fetch:
        code = *stream++;
        if (!(code & 0x80)) {
            /* a note */
            const uint8_t *vol_data;
            int16_t v;

            chan->note = (uint8_t)code;
            code = (uint8_t)(code + xenon2.music_transpose + chan->transpose);
            vol_data = chan->vol_start;
            v = (int8_t)*vol_data++;
            chan->vol_ptr = vol_data;
            chan->vol_count = chan->vol_speed;
            mirror->lc = ins->sample;
            mirror->len = ins->length;
            vol = (uint16_t)(((uint32_t)(uint16_t)v * get_volume()) & 0xffff) >> 6;
            mirror->vol = vol;
            if (!mirror->sfx_on) {
                audio_data(ch, ins->sample);
                audio_length(ch, ins->length);
                audio_volume(ch, vol);
            }
            code = ((uint32_t)xenon2.music_periods[code & 0xff] * ins->rate) >> 10;
            mirror->per = (uint16_t)code;
            if (!mirror->sfx_on)
                audio_period(ch, (uint16_t)code);
            chan->loaded = 0;
            goto start;
        }
        /* a control byte */
        if (code >= 0xe0) {
            chan->dur_reload = (uint16_t)(((uint8_t)(code - 0xdf)) * xenon2.music_tempo_mul);
            goto fetch;
        }
        if (code >= 0xb0) {
            ins = &xenon2.instruments[code - 0xb0];
            chan->instr = ins;
            goto fetch;
        }
        if (code >= 0xa0) {
            const uint8_t *vol_env = volume_envelope((int16_t)xenon2.music_vol_env_offsets[code - 0xa0]);

            chan->vol_start = vol_env;
            chan->vol_speed = vol_env[-1];
            goto fetch;
        }
        if (code >= 0x90) {
            const uint8_t *env = pitch_envelope((int16_t)xenon2.music_env_offsets[code - 0x90]);

            chan->env_start = env;
            chan->env_ptr = env;
            goto fetch;
        }
        switch (code) {
        case 0x80: {
            uint16_t idx = chan->list_idx;
            int16_t off = (int16_t)(chan->list_off + idx);

            idx += 2;
            if (list_word(off) == 0) {
                off = (int16_t)chan->list_off;
                idx = 2;
            }
            stream = sequence((int16_t)list_word(off));
            chan->list_idx = idx;
            goto fetch;
        }
        case 0x81:
            chan->slide = 0;
            chan->slide_step = (int8_t)*stream++;
            chan->slide_delay = *stream++;
            chan->flags |= 2;
            goto fetch;
        case 0x82:
            chan->dur = chan->dur_reload;
            chan->stream = stream;
            mirror->lc = xenon2.silence_ptr;
            mirror->len = 0x20;
            if (!mirror->sfx_on) {
                audio_data(ch, xenon2.silence_ptr);
                audio_length(ch, 0x20);
            }
            continue;
        case 0x83:
            goto start;
        case 0x84:
            music_stop();
            return;
        case 0x85:
            xenon2.music_transpose = *stream++;
            goto fetch;
        case 0x86:
            chan->vib = 0xff;
            chan->vib_step = *stream++;
            chan->vib_limit = *stream++;
            chan->vib_pos = 0;
            goto fetch;
        case 0x87:
            chan->vib = 0;
            goto fetch;
        case 0x88:
            chan->transpose = *stream++;
            goto fetch;
        case 0x89:
            chan->list_off = (uint16_t)(stream[0] << 8 | stream[1]);
            stream += 2;
            chan->list_idx = 0;
            goto fetch;
        case 0x8a:
            xenon2.music_tempo_mul = (uint16_t)(int16_t)(int8_t)*stream++;
            goto fetch;
        case 0x8b:
            xenon2.music_fade_speed = *stream++;
            xenon2.music_fade_count = xenon2.music_fade_speed;
            goto fetch;
        default:
            return;   /* the original jumps through the data after the table */
        }

    start:
        chan->stream = stream;
        chan->dur = chan->dur_reload;
        audio_start(ch);
        continue;

    effects: {
            uint32_t period;
            uint8_t env_byte;
            const uint8_t *vol_data;
            int16_t w;

            period = (uint8_t)(chan->note + xenon2.music_transpose + chan->transpose);
            stream = chan->env_ptr;
            env_byte = *stream++;
            if (env_byte & 0x80)
                stream = chan->env_start;
            env_byte &= 0x7f;
            chan->env_ptr = stream;
            period = (uint8_t)(period + env_byte);
            period = ((uint32_t)xenon2.music_periods[period] * ins->rate) >> 10;
            if (chan->flags & 2) {
                if (chan->slide_delay) {
                    chan->slide_delay--;
                } else {
                    chan->slide = (int16_t)(chan->slide + chan->slide_step);
                    period = (uint16_t)(period - chan->slide);
                }
            }
            if (chan->vib) {
                uint8_t pos;

                if (chan->vib & 0x80) {
                    pos = (uint8_t)(chan->vib_pos + chan->vib_step);
                    chan->vib_pos = pos;
                    if (pos == chan->vib_limit)
                        chan->vib ^= 0x80;
                } else {
                    pos = (uint8_t)(chan->vib_pos - chan->vib_step);
                    chan->vib_pos = pos;
                    if (pos == 0)
                        chan->vib ^= 0x80;
                }
                if (chan->vib_pos == 0)
                    chan->vib ^= 1;
                w = (int8_t)pos;
                if (chan->vib & 1)
                    period = (uint16_t)(period + w);
                else
                    period = (uint16_t)(period - w);
            }
            mirror->per = (uint16_t)period;
            if (!mirror->sfx_on)
                audio_period(ch, (uint16_t)period);
            if (chan->vol_count-- == 0) {
                int8_t b;

                chan->vol_count = chan->vol_speed;
                vol_data = chan->vol_ptr;
                b = (int8_t)*vol_data++;
                if (b >= 0)
                    chan->vol_ptr = vol_data;
                vol = (uint16_t)(((uint32_t)(b & 0x7f) * get_volume()) & 0xffff) >> 6;
                mirror->vol = vol;
                if (!mirror->sfx_on)
                    audio_volume(ch, vol);
            }
        }
    }
    sfx_tick();
}

/* orig $19FD6, $1B26A: start sound effect `request` = (channel << 8) | effect */
void sfx_play(uint16_t request)
{
    uint16_t ch;
    int8_t id;
    SfxChannel *sfx;

    CIAA_PRA |= 2;
    request &= 0x3ff;
    xenon2.sfx_request = request;
    ch = xenon2.sfx_request >> 8;
    if (ch < 4)
        xenon2.sfx_channels[ch].active = 0;
    sound_init_instruments();
    xenon2.hw_mirror[ch].sfx_on = 0xff;
    sfx = &xenon2.sfx_channels[ch];
    id = (int8_t)request;

    if (id >= 0) {
        const SfxDef *def = &xenon2.sfx_defs[(uint8_t)id];
        int8_t e;

        audio_length(ch, 1);
        audio_period(ch, 0x7c);
        audio_delay();
        audio_stop(ch);
        audio_delay();
        sfx->step = def->step;
        sfx->period_reset = def->period_reset;
        sfx->pair_a = def->pair_a;
        sfx->period = def->period;
        sfx->pair_b = def->pair_b;
        sfx->reload_e = def->reload_e;
        sfx->reload_f = def->reload_f;
        sfx->rot_a = def->rot_a;
        sfx->rot_b = def->rot_b;
        sfx->timer = def->timer;
        sfx->env_speed = def->env_speed;
        sfx->env_index = def->env_index;
        sfx->sample_id = def->sample_id;
        sfx->length = def->length;
        audio_length(ch, sfx->length);
        sfx->count_e = sfx->reload_e;
        sfx->count_f = sfx->reload_f;
        sfx->env_count = 1;
        e = (int8_t)sfx->env_index;
        sfx->env_ptr = effect_envelope((int16_t)xenon2.sfx_env_offsets[e]);
        audio_delay();
        sfx->active = 0xff;
        return;
    } else {
        const SfxSample *sample;
        int32_t loop_off;

        sound_init_sfx_samples();
        sfx->sample_id = (uint8_t)id;
        audio_stop(ch);
        audio_period(ch, 0x7c);
        audio_length(ch, 1);
        audio_delay();
        sample = &xenon2.sfx_samples[sfx->sample_id & 0x7f];
        sfx->timer = sample->duration;
        if (sample->endless)
            sfx->timer = 0;
        audio_length(ch, sample->length);
        audio_data(ch, sample->sample);
        audio_period(ch, sample->period);
        sfx->cur_period = sample->period;
        sfx->slide_period = sample->period;
        audio_volume(ch, 0x40);
        audio_start(ch);
        sfx->active = 0xff;
        audio_delay();
        loop_off = sample->loop_off;
        if (loop_off >= 0) {
            audio_length(ch, (uint16_t)(sample->length - (uint16_t)((uint32_t)loop_off >> 1)));
            audio_data(ch, sample->sample + loop_off);
        } else {
            audio_data(ch, xenon2.silence_ptr);
            audio_length(ch, 0x20);
        }
    }
}

/* orig $1B5A2: one tick of a sound effect channel */
static void sfx_channel_tick(SfxChannel *sfx, int ch, HwMirror *mirror)
{
    uint16_t period, slide_val;

    if (sfx->timer != 0) {
        sfx->timer--;
        if (sfx->timer == 0)
            goto stop;
    }
    if (sfx->sample_id != 0) {
        const SfxSample *s = &xenon2.sfx_samples[sfx->sample_id & 0x7f];

        period = sfx->slide_period;
        slide_val = (uint16_t)s->slide;
        if (slide_val != 0) {
            period = (uint16_t)(period + slide_val);
            if ((int16_t)period < 0x7c)
                goto stop;
            sfx->slide_period = period;
            audio_period(ch, period);
            return;
        }
        audio_period(ch, sfx->slide_period);
        return;
    }
    if (sfx->reload_f != 0) {
        sfx->count_f--;
        if (sfx->count_f == 0) {
            uint32_t pair = sfx->pair_a;
            uint8_t b;

            sfx->count_f = sfx->reload_f;
            b = sfx->rot_a;
            if (b & 1)
                b = (uint8_t)((b >> 1) | 0x80);
            else {
                b = (uint8_t)(b >> 1);
                pair = (pair << 16) | (pair >> 16);
            }
            sfx->rot_a = b;
            sfx->period = (uint16_t)(sfx->period + (uint16_t)pair);
        }
    }
    sfx->period = (uint16_t)(sfx->period + (uint16_t)sfx->step);
    if (sfx->reload_e != 0) {
        sfx->count_e--;
        if (sfx->count_e == 0) {
            sfx->count_e = sfx->reload_e;
            sfx->period = sfx->period_reset;
        }
    }
    sfx->env_count--;
    if (sfx->env_count == 0) {
        int8_t b;
        const uint8_t *env = sfx->env_ptr;

        sfx->env_count = sfx->env_speed;
        b = (int8_t)*env++;
        if (b == 0)
            goto stop;
        if (b > 0) {
            sfx->env_ptr = env;
            audio_volume(ch, (uint16_t)(int16_t)b);
        }
    }
    {
        int16_t per = (int16_t)sfx->period;
        uint32_t pair = sfx->pair_b;
        uint8_t b = sfx->rot_b;

        if (per < 0x7c)
            per = 0x7c;
        audio_period(ch, (uint16_t)per);
        if (b & 1)
            b = (uint8_t)((b >> 1) | 0x80);
        else {
            b = (uint8_t)(b >> 1);
            pair = (pair << 16) | (pair >> 16);
        }
        sfx->rot_b = b;
        audio_data(ch, xenon2.sfx_wave_base + (int16_t)pair);
        audio_start(ch);
    }
    return;

stop:
    sfx->active = 0;
    audio_stop(ch);
    mirror->sfx_on = 0;
    if (mirror->music_on) {
        audio_period(ch, 0x7c);
        audio_stop(ch);
        audio_delay();
        audio_data(ch, mirror->lc);
        audio_length(ch, mirror->len);
        audio_period(ch, mirror->per);
        audio_volume(ch, mirror->vol);
        audio_start(ch);
    }
}

/* orig $1B540, $1A14E: tick every active sound effect channel */
void sfx_tick(void)
{
    uint16_t i;

    for (i = 0; i < 4; i++)
        if (xenon2.sfx_channels[i].active)
            sfx_channel_tick(&xenon2.sfx_channels[i], i, &xenon2.hw_mirror[i]);
}
