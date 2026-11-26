/* The SDL port's machine: what the Amiga's chips, the disks and the operating environment do for the game's C. */
#ifndef HOST_H
#define HOST_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

enum { HOST_VBLANK_NS = 20000000 };

/* guest.c: the game's memory is struct Xenon2 (game.h); the chips' registers are objects of their own */
extern volatile uint8_t host_custom_raw[0x1000];   /* struct Custom (amiga/hw.h) */
extern volatile uint8_t host_cia_raw[0x2000];      /* the CIAs, $BFD000 on */
void *guest_ptr(uint32_t address);                  /* an address of the original's memory, or an address of the struct, as a pointer */
uint32_t guest_address(const void *p);              /* the reverse */
void guest_load_range(uint32_t address, const uint8_t *bytes, uint32_t length);   /* bytes of the original's memory (big endian) into the members that cover them */
void guest_blob_raw(int on);                        /* the shop's blob overlays the level's at $54E00: the members there are not converted while it is */
void host_irq_dispatch(int level);                  /* call the handler in the vector table for interrupt level 1..7 (nothing if there is none) */
unsigned host_vbl_count(void);                      /* the game's own frame counter ($41C) */
void host_export_ram_buf(uint8_t *ram_0x80000);     /* the memory in the original's byte order, into a buffer of 0x80000 bytes */
void host_import_ram_buf(const uint8_t *ram_0x80000);   /* and back: the memory of the original into the block (before the game starts) */
void host_set_headless(int on);                   /* before host_init */
void host_set_fixed_window(int on);               /* a window that cannot be resized: a tiling compositor floats it; before host_init */
void host_game_start(void);                         /* call the game's entry (nothing if the C has none yet) */
int guest_load_game_image(void);                    /* the game image disk 1 leaves at $400 */

/* The machine's state for a savestate (the dev build): every module hands its objects to a visitor in a fixed order; the visitor writes them to a file or reads them back.
 * Objects hold addresses of this executable (the game's memory block, the samples of a sound channel): a state is for the build that made it (no PIE). */
typedef void (*host_state_visit)(void *data, size_t size, void *arg);
void host_state(host_state_visit visit, void *arg);   /* main.c: the memory, the chips, the disks, the sound, the input, the frame count */
void guest_state(host_state_visit visit, void *arg);  /* guest.c */
void sound_state(host_state_visit visit, void *arg);  /* mixer.c */

/* files.c: the disks (ADF images) */
int files_open(const char *folder);
int files_read_bs1_game(uint8_t *image, size_t size);
int files_read_sectors(int disk, uint32_t first_sector, uint32_t count, void *dest);   /* raw sectors of 512 bytes; 0 = ok */

/* display.c: the picture from the registers and the copper list the game leaves */
#define DISPLAY_W 320
#define DISPLAY_H 256
/* the game's window (DIWSTRT $3A81, DIWSTOP $02C1) is the 200 lines that start 14 lines into the picture; the window shows only those */
#define DISPLAY_TOP 14
#define DISPLAY_VISIBLE_H 200
void display_render(uint32_t *rgb);
void display_render_interpolated(uint32_t *rgb, uint64_t now);

/* mixer.c: the four voices of the sound hardware.  A voice plays 8-bit samples (the data, its length in words of two bytes) at 3546895 Hz / period, with a volume of 0 to 64.
 * Started, it plays from the data and length it has then, and at the end of them again from the ones it has by then (a loop is the data and length set after the start).
 * Voices 0 and 3 are the left channel, 1 and 2 the right one.  The players (sound.c, shop_sound.c) set them. */
void sound_init(void);
void sound_mix(int16_t *stereo, int frames);        /* 44100 Hz, interleaved left/right */
void audio_data(int voice, const void *samples);
void audio_length(int voice, uint16_t words);
void audio_period(int voice, uint16_t period);
void audio_volume(int voice, uint16_t volume);
void audio_start(int voice);        /* a voice that is playing goes on */
void audio_stop(int voice);
void audio_start_all(void);
void audio_stop_all(void);

/* input.c / main.c */
void host_input(int fire, int up, int down, int left, int right);
void host_quit(void);
int host_init(int argc, char **argv);               /* the options, the machine, the window: 0 = ready to start the game */
void host_frame_end(void);                          /* the end of a video frame: picture, sound, clock, keys */
void host_vblank(void);                             /* raise the vertical blank interrupt (calls the game's handler if it enabled it) */
void host_screen_flip(const void *screen, uint16_t tick);
const uint32_t *host_picture(void);                 /* the picture of the registers as they are now: DISPLAY_W x DISPLAY_H pixels, 0xAARRGGBB */
long host_frame_count(void);
void host_key_event(int amiga_raw_code, int down);  /* a key as the keyboard would send it */
void host_unimplemented(uint32_t original_address);   /* a call of original code that has no C (it never ran when the C was made) */

#endif
