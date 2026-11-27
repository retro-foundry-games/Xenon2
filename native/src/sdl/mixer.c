/* The four voices of the sound hardware (the sound players of this directory set them), mixed for the SDL stream: sound_mix makes the stereo samples of one video frame from what the game has set. */
#include <stdint.h>
#include <string.h>

#include "host.h"

#define PAULA_CLOCK 3546895.0
#define OUTPUT_RATE 44100

struct voice {
    int playing;
    const int8_t *start;     /* what it plays now */
    uint32_t length;         /* in bytes */
    double position;         /* in bytes */
    const int8_t *data;      /* what it plays next, from the end of this */
    uint32_t words;
    uint16_t period, volume;
};

static struct voice voice[4];

void sound_state(host_state_visit visit, void *arg)
{
    visit(voice, sizeof voice, arg);
}

void sound_init(void)
{
    memset(voice, 0, sizeof voice);
}

void audio_data(int n, const void *samples)
{
    voice[n].data = samples;
}

void audio_length(int n, uint16_t words)
{
    voice[n].words = words ? words : 0x10000u;
}

void audio_period(int n, uint16_t period)
{
    voice[n].period = period;
}

void audio_volume(int n, uint16_t volume)
{
    voice[n].volume = volume;
}

void audio_start(int n)
{
    struct voice *v = &voice[n];

    if (v->playing)
        return;
    v->start = v->data;
    v->length = v->words * 2;
    v->position = 0;
    v->playing = v->start != 0;
}

void audio_stop(int n)
{
    voice[n].playing = 0;
}

void audio_start_all(void)
{
    for (int n = 0; n < 4; n++)
        audio_start(n);
}

void audio_stop_all(void)
{
    for (int n = 0; n < 4; n++)
        audio_stop(n);
}

static int16_t clip(int sample)
{
    return (int16_t)(sample > 32767 ? 32767 : sample < -32768 ? -32768 : sample);
}

void sound_mix(int16_t *out, int frames)
{
    for (int i = 0; i < frames; i++) {
        int left = 0, right = 0;

        for (int n = 0; n < 4; n++) {
            struct voice *v = &voice[n];
            int period, volume, sample;

            if (!v->playing)
                continue;
            period = v->period < 124 ? 124 : v->period;
            volume = v->volume & 0x7f;
            if (volume > 64)
                volume = 64;
            sample = v->start[(uint32_t)v->position] * volume;
            if (n == 0 || n == 3)
                left += sample;
            else
                right += sample;
            v->position += PAULA_CLOCK / period / OUTPUT_RATE;
            if (v->position >= v->length) {
                v->start = v->data;
                v->length = v->words * 2;
                v->position = 0;
                if (!v->start)
                    v->playing = 0;
            }
        }
        out[2 * i] = clip(left * 4);
        out[2 * i + 1] = clip(right * 4);
    }
}
