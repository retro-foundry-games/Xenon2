#ifndef TYPES_OBJECTS_H
#define TYPES_OBJECTS_H
#include <stdint.h>

/* the kinds of pieces an object breaks into (6 bytes) */
struct SpawnKind {
    uint16_t kind;
    const uint16_t *anim;       /* animation header */
};
#endif
