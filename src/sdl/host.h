/* The SDL port's machine: what the Amiga's chips, the disks and the operating environment do for the game's C. */
#ifndef HOST_H
#define HOST_H

#include <stdint.h>
#include <stdio.h>

/* guest.c: the game's memory is struct Xenon (game_state.h); the chips' registers are objects of their own */
extern volatile uint8_t host_custom_raw[0x1000];   /* struct Custom (amiga/hw.h) */
extern volatile uint8_t host_cia_raw[0x2000];      /* the CIAs, $BFD000 on */
void *guest_ptr(uint32_t address);                  /* an address of the original's memory, or an address of the struct, as a pointer */
uint32_t guest_address(const void *p);              /* the reverse */
void guest_load_range(uint32_t address, const uint8_t *bytes, uint32_t length);   /* bytes of the original's memory (big endian) into the members that cover them */
void host_irq_dispatch(int level);                  /* call the handler in the vector table for interrupt level 1..7 (nothing if there is none) */
unsigned host_vbl_count(void);                      /* the game's own frame counter ($41C) */
int host_export_ram(const char *path);              /* the memory in the original's byte order, for tools/compare_state.py */
void host_game_status(void);                        /* print a few variables of the game (tests) */
void host_game_start(void);                         /* call the game's entry (nothing if the C has none yet) */
int guest_load_game_image(void);                    /* disk 1, tracks 82-159, from $70C00: the game's program and data, as the loader of disk 1 leaves them at $400 */

/* files.c: the disks (ADF images) */
int files_open(const char *folder);
int files_read_sectors(int disk, uint32_t first_sector, uint32_t count, void *dest);   /* raw sectors of 512 bytes; 0 = ok */
int files_current_disk(void);                       /* the disk in the drive: 1 or 2 */
void files_insert(int disk);

/* display.c: the picture from the registers and the copper list the game leaves */
#define DISPLAY_W 320
#define DISPLAY_H 256
void display_render(uint32_t *rgb);
void display_status(void);                          /* print what the picture is made from */                 /* DISPLAY_W x DISPLAY_H pixels, 0x00RRGGBB */

/* sound.c: Paula from the channel registers */
void sound_init(void);
void sound_dma_changed(uint16_t dmacon_before, uint16_t dmacon_after);
void sound_mix(int16_t *stereo, int frames);        /* 44100 Hz, interleaved left/right */

/* input.c / main.c */
void host_input(int fire, int up, int down, int left, int right);
void host_quit(void);
void host_unimplemented(uint32_t original_address);   /* a call of original code that has no C (it never ran when the C was made) */

#endif
