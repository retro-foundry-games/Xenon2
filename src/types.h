#ifndef TYPES_H
#define TYPES_H
#include <stdint.h>

/* an interrupt handler, as the vector table at $64..$7C holds them (a pointer: 4 bytes in both builds) */
/* a 16.16 value from its whole and fraction words, the high word first */
#define FIXED_16_16(whole, fraction) (((uint32_t)(uint16_t)(whole) << 16) | (uint16_t)(fraction))

typedef void (*irq_fn)(void);

#endif
