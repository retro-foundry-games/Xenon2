#ifndef DISK_H
#define DISK_H
#include "game.h"
int disk_read_sectors(uint16_t first_sector, uint16_t count, void *dest);   /* orig $921A */
void irq_disk_block(void);                                  /* orig $95E4 */
#endif
