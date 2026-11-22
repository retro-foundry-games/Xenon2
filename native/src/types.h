#ifndef TYPES_H
#define TYPES_H
#include <stdint.h>

/* an interrupt handler, as the vector table at $64..$7C holds them (a pointer: 4 bytes in both builds) */
typedef void (*irq_fn)(void);

#endif
