/* The voices of audio.h on the Amiga: the channel registers of the chip and its DMA bits. */
#include "audio.h"
#include "hw.h"

void audio_data(int voice, const void *samples)
{
    custom->aud[voice].lc = (uintptr_t)samples;
}

void audio_length(int voice, uint16_t words)
{
    custom->aud[voice].len = words;
}

void audio_period(int voice, uint16_t period)
{
    custom->aud[voice].per = period;
}

void audio_volume(int voice, uint16_t volume)
{
    custom->aud[voice].vol = volume;
}

void audio_start(int voice)
{
    SETCLR_WRITE(dmacon, (uint16_t)(0x8200 | (1 << voice)));
}

void audio_stop(int voice)
{
    SETCLR_WRITE(dmacon, (uint16_t)(1 << voice));
}

void audio_start_all(void)
{
    SETCLR_WRITE(adkcon, 0x00ff);
    SETCLR_WRITE(dmacon, 0x820f);
}

void audio_stop_all(void)
{
    SETCLR_WRITE(dmacon, 0x000f);
    SETCLR_WRITE(adkcon, 0x00ff);
}
