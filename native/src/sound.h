#ifndef SOUND_H
#define SOUND_H
#include <stdint.h>
#include "types.h"

typedef struct Instrument Instrument;
typedef struct SongHeader SongHeader;
typedef struct MusicChannel MusicChannel;
typedef struct HwMirror HwMirror;
typedef struct SfxSample SfxSample;
typedef struct SfxDef SfxDef;
typedef struct SfxChannel SfxChannel;

/* the header of a sample in the sample bank: the 8-bit samples follow, then the next header */
struct SampleHeader {
    uint32_t length;        /* 0: bytes of samples */
    uint16_t rate;          /* 4 */
};

/* an entry of the sample bank: a header and its samples; the next entry follows */
struct SampleEntry {
    struct SampleHeader header;
    uint8_t samples[];
};

/* an instrument ($1A738, 20 of them, built by sound_init_instruments from the sample bank) */
struct Instrument {
    uint8_t *sample;        /* 0: first byte of the 8-bit sample */
    int32_t loop_off;       /* 4: byte offset of the loop inside the sample, negative = no loop (plays the silence) */
    uint16_t length;        /* 8: length in words */
    uint16_t rate;          /* a: 0x369E99 / rate of the sample; times the note's period table value >> 10 = the period */
};

/* a song ($1A828): the four channels' lists */
struct SongHeader {
    uint8_t tempo;          /* 0: -> music_tempo_mul after ext.w ($1A582 word) */
    uint8_t rate;           /* 1: -> music_rate */
    uint16_t list[4];       /* 2: offset from MUSIC_OFFSET_ZERO of each channel's list of sequences (words, 0 ends) */
};

/* a music channel ($1A596, 4 x 0x30) */
struct MusicChannel {
    uint8_t flags;          /* 0: bit 1 = pitch slide on */
    uint8_t vib;            /* 1: vibrato: 0 off, bit 7 = direction, bit 0 = sign */
    uint8_t note;           /* 2: current note */
    uint8_t transpose;      /* 3 */
    const uint8_t *stream;  /* 4: sequence being played */
    uint16_t list_off;      /* 8: offset (from MUSIC_OFFSET_ZERO) of the channel's list of sequences */
    uint16_t list_idx;      /* a: byte index into the list */
    const uint8_t *env_start; /* c: pitch envelope start (the loop target) */
    const uint8_t *env_ptr; /* 10: pitch envelope position, bit 7 of a byte = go to env_start */
    int8_t slide_step;      /* 14 */
    uint8_t slide_delay;    /* 15 */
    uint8_t loaded;         /* 16: 0 = a new note was started, 0xFF = instrument set up */
    uint8_t pad17;
    Instrument *instr;      /* 18 */
    uint16_t dur_reload;    /* 1c: note length in ticks */
    uint16_t dur;           /* 1e: ticks left */
    int16_t slide;          /* 20: accumulated slide */
    const uint8_t *vol_start; /* 22: volume envelope start (byte before it = speed) */
    const uint8_t *vol_ptr; /* 26: volume envelope position */
    uint8_t vol_speed;      /* 2a */
    uint8_t vol_count;      /* 2b */
    uint8_t vib_step;       /* 2c */
    uint8_t vib_pos;        /* 2d */
    uint8_t vib_limit;      /* 2e */
    uint8_t pad2f;
};

/* what the music last wrote to a hardware channel ($1B762, 4 x 0xC); the sound effects restore it */
struct HwMirror {
    const uint8_t *lc;      /* 0 */
    uint16_t len;           /* 4 */
    uint16_t per;           /* 6 */
    uint16_t vol;           /* 8 */
    uint8_t music_on;       /* a: the music uses the channel */
    uint8_t sfx_on;         /* b: a sound effect has the channel: the music does not write it */
};

/* a sampled sound effect ($1B352, 18 of them, built by sound_init_sfx_samples) */
struct SfxSample {
    uint8_t *sample;        /* 0 */
    int32_t loop_off;       /* 4: negative = no loop */
    uint16_t length;        /* 8: words */
    uint16_t period;        /* a */
    uint8_t duration;       /* c: ticks */
    uint8_t endless;        /* d: non-zero = plays until stopped */
    int16_t slide;          /* e: period change per tick */
};

/* a synthesised sound effect's definition ($1B792, 23 x 0x18): copied over the start of a SfxChannel */
struct SfxDef {
    int16_t step;           /* 0 */
    uint16_t period_reset;  /* 2 */
    uint32_t pair_a;        /* 4: two words, used alternately */
    uint16_t period;        /* 8 */
    uint32_t pair_b;        /* a: two word offsets into sfx_wave_base, used alternately */
    uint8_t reload_e;       /* e */
    uint8_t reload_f;       /* f */
    uint8_t rot_a;          /* 10 */
    uint8_t rot_b;          /* 11 */
    uint8_t timer;          /* 12 */
    uint8_t env_speed;      /* 13 */
    uint8_t env_index;      /* 14: index into sfx_env_offsets */
    uint8_t sample_id;      /* 15 */
    uint16_t length;        /* 16: words */
};

/* a sound effect channel ($1B6D0, 4 x 0x24) */
struct SfxChannel {
    int16_t step;           /* 0 */
    uint16_t period_reset;  /* 2 */
    uint32_t pair_a;        /* 4 */
    uint16_t period;        /* 8: current period */
    uint32_t pair_b;        /* a */
    uint8_t reload_e;       /* e */
    uint8_t reload_f;       /* f */
    uint8_t rot_a;          /* 10 */
    uint8_t rot_b;          /* 11 */
    uint8_t timer;          /* 12: ticks until the sample effect stops, 0 = never */
    uint8_t env_speed;      /* 13 */
    uint8_t env_index;      /* 14 */
    uint8_t sample_id;      /* 15: sampled effect number (bit 7 set), 0 = synthesised */
    uint16_t length;        /* 16 */
    uint8_t count_e;        /* 18 */
    uint8_t count_f;        /* 19 */
    uint8_t active;         /* 1a: 0xFF while playing */
    uint8_t env_count;      /* 1b */
    const uint8_t *env_ptr; /* 1c: volume envelope */
    uint16_t cur_period;    /* 20 */
    uint16_t slide_period;  /* 22 */
};

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
