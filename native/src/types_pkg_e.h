#ifndef TYPES_PKG_E_H
#define TYPES_PKG_E_H
#include <stdint.h>

typedef struct Obj Obj;

/* an object of the lists at $A76 and of the pool at $3F07E (0x62 bytes) */
struct Obj {
    uint16_t type;              /* 0: 0 = free */
    void (*fn_a)(void);         /* 2 */
    void (*fn_b)(void);         /* 6 */
    void (*fn_c)(void);         /* a */
    struct Obj *next;           /* e: the free list also links here */
    struct Obj *prev;           /* 12 */
    void *gfx;                  /* 16 */
    uint16_t w1a;               /* 1a */
    const uint16_t *data;       /* 1c */
    uint16_t w20;               /* 20: x */
    uint16_t w22;
    uint16_t w24;               /* 24: y */
    uint16_t w26;
    uint16_t w28;
    uint16_t w2a;
    uint16_t w2c;
    uint16_t w2e;
    uint16_t w30;               /* 30: the byte at 31 is the low byte */
    uint16_t pad32[7];
    uint16_t w40;
    uint16_t w42;
    uint16_t pad44[15];
};

typedef struct Shot Shot;

/* a shot dot at $42D5C: type < 0 = free */
struct Shot {
    int16_t x, y, dx, dy;
};

#endif
