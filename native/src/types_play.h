#ifndef TYPES_PLAY_H
#define TYPES_PLAY_H
#include <stdint.h>

typedef struct Shot Shot;

/* a shot dot at $42D5C: type < 0 = free */
struct Shot {
    int16_t x, y, dx, dy;
};

#endif
