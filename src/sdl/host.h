/* The SDL port's machine: what the Amiga's chips, the disks and the operating environment do for the game's C. */
#ifndef HOST_H
#define HOST_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* guest.c: the game's memory is struct Xenon (game.h); the chips' registers are objects of their own */
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
void host_game_start(void);                         /* call the game's entry (nothing if the C has none yet) */
int guest_load_game_image(void);                    /* disk 1, tracks 82-159, from $70C00: the game's program and data, as the loader of disk 1 leaves them at $400 */

/* The machine's state for a savestate (the dev build): every module hands its objects to a visitor in a fixed order; the visitor writes them to a file or reads them back.
 * Objects hold addresses of this executable (the game's memory block, the samples of a sound channel): a state is for the build that made it (no PIE). */
typedef void (*host_state_visit)(void *data, size_t size, void *arg);
void host_state(host_state_visit visit, void *arg);   /* main.c: the memory, the chips, the disks, the sound, the input, the frame count */
void guest_state(host_state_visit visit, void *arg);  /* guest.c */
void sound_state(host_state_visit visit, void *arg);  /* sound.c */

/* files.c: the disks (ADF images) */
int files_open(const char *folder);
int files_read_sectors(int disk, uint32_t first_sector, uint32_t count, void *dest);   /* raw sectors of 512 bytes; 0 = ok */

/* display.c: the picture from the registers and the copper list the game leaves */
#define DISPLAY_W 320
#define DISPLAY_H 256
void display_render(uint32_t *rgb);

/* sound.c: Paula from the channel registers */
void sound_init(void);
void sound_dma_changed(uint16_t dmacon_before, uint16_t dmacon_after);
void sound_mix(int16_t *stereo, int frames);        /* 44100 Hz, interleaved left/right */

/* input.c / main.c */
void host_input(int fire, int up, int down, int left, int right);
void host_quit(void);
int host_init(int argc, char **argv);               /* the options, the machine, the window: 0 = ready to start the game */
void host_frame_end(void);                          /* the end of a video frame: picture, sound, clock, keys */
void host_vblank(void);                             /* raise the vertical blank interrupt (calls the game's handler if it enabled it) */
const uint32_t *host_picture(void);                 /* the picture of the registers as they are now: DISPLAY_W x DISPLAY_H pixels, 0xAARRGGBB */
long host_frame_count(void);
void host_key_event(int amiga_raw_code, int down);  /* a key as the keyboard would send it */
void host_unimplemented(uint32_t original_address);   /* a call of original code that has no C (it never ran when the C was made) */

#endif
