#ifndef TYPES_MENUS_H
#define TYPES_MENUS_H
#include <stdint.h>
#include "types_weapons.h"

/* an object as the path script (obj_script_step) sees it: 0x62 bytes, fields at the original offsets */
typedef struct ObjMove ObjMove;
struct ObjMove {
    uint16_t type;              /* 0: 4 = dying */
    void (*fn_update)(Object *); /* 2 */
    void (*fn_draw)(Object *); /* 6 */
    void *fn_hit;              /* a */
    uint8_t pad0e[0x12];        /* e */
    uint16_t x_hi, x_lo;        /* 20: 16.16 as a long: the high word (the x of Object) then the fraction (words, not a uint32_t: a long over words is wrong on a little-endian host) */
    uint16_t y_hi, y_lo;        /* 24 */
    int16_t wait;               /* 28: steps left of the running command (negative: a wait) */
    uint16_t angle_hi, angle_lo; /* 2a: the angle is the low word (at 2c), the high word a fraction */
    uint8_t pad2e[6];           /* 2e */
    uint8_t flag34;             /* 34 */
    uint8_t pad35[15];          /* 35 */
    uint16_t id;                /* 44 */
    const uint8_t *script;      /* 46: the script's start */
    const uint8_t *cursor;      /* 4a: the next command */
    uint16_t turn_hi, turn_lo;  /* 4e: angle change per step, 8.24 */
    int16_t turn_step;          /* 52 */
    uint16_t steps;             /* 54: steps per call */
    ObjMove *target;            /* 56 */
    ObjMove *chain;             /* 5a */
    uint8_t shot_count;         /* 5e */
    uint8_t shot_rate;          /* 5f */
    uint8_t flag60;             /* 60 */
    uint8_t pad61;
};

#endif
