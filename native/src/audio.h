#ifndef AUDIO_H
#define AUDIO_H
#include <stdint.h>

/* The four voices of the sound hardware.  A voice plays 8-bit samples (the data, its length in words of two bytes) at 3546895 Hz / period, with a volume of 0 to 64.  Started, it
 * plays from the data and length it has then, and at the end of them again from the ones it has by then (so a loop is the data and length set after the start).  Voices 0 and 3
 * are the left channel, 1 and 2 the right one. */
void audio_data(int voice, const void *samples);
void audio_length(int voice, uint16_t words);
void audio_period(int voice, uint16_t period);
void audio_volume(int voice, uint16_t volume);
void audio_start(int voice);        /* a voice that is playing goes on */
void audio_stop(int voice);
void audio_start_all(void);
void audio_stop_all(void);
#endif
