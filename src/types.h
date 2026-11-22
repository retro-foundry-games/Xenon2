#ifndef TYPES_H
#define TYPES_H
#include <stdint.h>

/* a cell of the tile map: two raw big-endian bytes (read and written with rd16/wr16 on .b) */
typedef struct Cell { uint8_t b[2]; } Cell;

/* the cells of a row of the tile map */
#define MAP_COLS 20

/* a 16.16 value from its whole and fraction words, the high word first */
#define FIXED_16_16(whole, fraction) (((uint32_t)(uint16_t)(whole) << 16) | (uint16_t)(fraction))

/* an interrupt handler, as the vector table at $64..$7C holds them (a pointer: 4 bytes in both builds) */
typedef void (*irq_fn)(void);

#endif
