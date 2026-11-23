/* Paula, from the channel registers: four DMA channels that play 8-bit samples at a rate of 3546895 Hz / period, with a volume 0..64.  The game's player
 * writes the registers (custom->aud[n]: lc, len in words, per, vol) and SETCLR_WRITE(dmacon, ...); no register of the sound is emulated beyond what the
 * mixer needs: when DMA is switched on the channel latches lc and len, plays them and at the end starts again from the (then current) lc and len. */
#include <stdint.h>
#include <string.h>

#include "amiga/hw.h"
#include "host.h"

#define PAULA_CLOCK 3546895.0
#define OUTPUT_RATE 44100

struct channel {
    int on;
    const int8_t *start;
    uint32_t length;         /* in bytes */
    double position;         /* in bytes */
};

static struct channel channel[4];

void sound_state(host_state_visit visit, void *arg)
{
    visit(channel, sizeof channel, arg);
}

void sound_init(void)
{
    memset(channel, 0, sizeof channel);
}

/* DMA of channel n (and master DMAEN) went from off to on or the other way */
void sound_dma_changed(uint16_t before, uint16_t after)
{
    for (int n = 0; n < 4; n++) {
        int was = (before & 0x200) && (before & (1 << n));
        int is = (after & 0x200) && (after & (1 << n));

        if (is && !was) {
            volatile struct AudioChannel *a = &custom->aud[n];

            channel[n].start = (const int8_t *)guest_ptr(a->lc);
            channel[n].length = (a->len ? a->len : 0x10000u) * 2;
            channel[n].position = 0;
            channel[n].on = channel[n].start != 0;
        } else if (!is) {
            channel[n].on = 0;
        }
    }
}

void sound_mix(int16_t *out, int frames)
{
    for (int i = 0; i < frames; i++) {
        int left = 0, right = 0;

        for (int n = 0; n < 4; n++) {
            struct channel *ch = &channel[n];
            volatile struct AudioChannel *a = &custom->aud[n];
            int period, volume;
            int sample;

            if (!ch->on)
                continue;
            period = a->per < 124 ? 124 : a->per;
            volume = a->vol & 0x7f;
            if (volume > 64)
                volume = 64;
            sample = ch->start[(uint32_t)ch->position] * volume;
            if (n == 0 || n == 3)
                left += sample;
            else
                right += sample;
            ch->position += PAULA_CLOCK / period / OUTPUT_RATE;
            if (ch->position >= ch->length) {            /* the end: again from the registers as they are now */
                ch->start = (const int8_t *)guest_ptr(a->lc);
                ch->length = (a->len ? a->len : 0x10000u) * 2;
                ch->position = 0;
                if (!ch->start)
                    ch->on = 0;
            }
        }
        out[2 * i] = (int16_t)(left * 8 > 32767 ? 32767 : left * 8 < -32768 ? -32768 : left * 8);
        out[2 * i + 1] = (int16_t)(right * 8 > 32767 ? 32767 : right * 8 < -32768 ? -32768 : right * 8);
    }
}
