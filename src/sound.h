#ifndef SOUND_H
#define SOUND_H
#include "game.h"

/* where the sampled effects start in sample_bank */
#define SAMPLE_EFFECTS_OFFSET 0xff1c

void music_start(uint16_t song);        /* orig $19F9E, $19FFC */
void music_tick(void);                  /* orig $19FAC, $1A154 */
void music_silence(void);               /* orig $19FBA, $1A09A */
void music_resume(void);                /* orig $19FC8, $1A112 */
void sfx_play(uint16_t request);        /* orig $19FD6, $1B26A */
void sfx_stop_all(void);                /* orig $19FE4, $1A0F4 */
void music_fade_out(void);              /* orig $19FF2, $1A13C */
void sound_init_instruments(void);      /* orig $1B1B6 */
void sound_init_sfx_samples(void);      /* orig $1B216 */
void audio_delay(void);                 /* orig $1B536 */
void sfx_tick(void);                    /* orig $1B540 */
#endif
