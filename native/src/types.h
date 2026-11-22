#ifndef TYPES_H
#define TYPES_H
#include <stdint.h>

/* an interrupt handler, as the vector table at $64..$7C holds them (a pointer: 4 bytes in both builds) */
typedef void (*irq_fn)(void);

/* a long the original keeps as two words (high word first); the words are 16-bit members, so that the loader byte-swaps them one by one */
static inline uint32_t join32(uint16_t hi, uint16_t lo) { return ((uint32_t)hi << 16) | lo; }
static inline void split32(void *hi, void *lo, uint32_t v) { *(uint16_t *)hi = (uint16_t)(v >> 16); *(uint16_t *)lo = (uint16_t)v; }

#endif
