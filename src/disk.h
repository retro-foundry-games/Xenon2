#ifndef DISK_H
#define DISK_H
#include "game.h"
int game_disk_present(void);                                /* orig $8F9A: 1 = Z set = the game disk is in */
HLE_WEAK int disk_read_sectors(uint16_t first_sector, uint16_t count, void *dest);   /* orig $921A */
void disk_motor_on(void);                                   /* orig $92C8 */
void disk_motor_off(void);                                  /* orig $92EE */
void disk_recalibrate(void);                                /* orig $9308 */
void disk_seek(uint16_t track);                             /* orig $932E */
void disk_step_out(uint16_t steps);                         /* orig $93AA */
void disk_step_in(uint16_t steps);                          /* orig $93B6 */
void disk_delay(void);                                      /* orig $93EE */
int disk_read_track(void);                                  /* orig $93F8 */
int32_t mfm_align(const uint16_t *src);                     /* orig $951E */
uint16_t *mfm_decode(uint16_t *dest, uint16_t *src, uint16_t bytes);   /* orig $957E: returns the end of the source */
void irq_disk_block(void);                                  /* orig $95E4 */
#endif
