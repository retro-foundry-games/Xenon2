#ifndef TYPES_PKG_D_H
#define TYPES_PKG_D_H
#include <stdint.h>

typedef struct Object Object;
typedef void (*obj_fn)(Object *);

/* an object of the lists at $A76 and of the pool at $3F07E (0x62 bytes); the fields at $2A..$2E are a word, a long or a pointer by kind */
struct Object {
    uint16_t type;              /* 0: 0 = free, 4 = dying */
    obj_fn fn_update;           /* 2 */
    obj_fn fn_draw;             /* 6 */
    void *fn_hit;               /* a: the routine that takes a shot's damage, or the weapon slot that owns the object */
    Object *next;               /* e */
    Object *prev;               /* 12 */
    const void *gfx;            /* 16 */
    int16_t anim_delay;         /* 1a */
    const uint16_t *anim;       /* 1c: the animation script */
    int16_t x;                  /* 20 */
    uint16_t x_frac;            /* 22 */
    int16_t y;                  /* 24 */
    uint16_t y_frac;            /* 26 */
    int16_t v28;
    int16_t v2a;                /* 2a: the high word of a long (v2a:v2c) */
    int16_t v2c;
    int16_t v2e;
    int16_t v30;
    int16_t energy;             /* 32 */
    int8_t flag34;
    int8_t flag35;
    int16_t hit[4];             /* 36: the box x0 y0 x1 y1 */
    int16_t v3e;
    int16_t v40;
    int16_t v42;                /* 42: score of the object */
    int16_t v44;                /* 44: kind */
    uint16_t *v46;
    uint16_t pad4a[5];          /* 4a: a pointer (pkg_c); 4e: a long and 52: a word in the level 1 boss */
    int16_t v54;
    Object *target;             /* 56 */
    Object *chain;              /* 5a */
    int16_t v5e;
    int8_t flag60;
    int8_t flag61;
};

/* a weapon slot at $C66 (7 of them) */
typedef struct Slot Slot;
struct Slot {
    uint16_t type;
    Object *obj;
};

#endif
