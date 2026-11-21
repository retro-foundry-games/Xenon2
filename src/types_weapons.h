#ifndef TYPES_WEAPONS_H
#define TYPES_WEAPONS_H
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
    const uint8_t *anim;        /* 1c: the animation script (raw bytes) */
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
    int8_t is_part;
    int8_t big;
    int16_t hit[4];             /* 36: the box x0 y0 x1 y1 */
    int16_t v3e;
    int16_t v40;
    int16_t score;               /* 42: score the object gives; for a weapon its power */
    int16_t kind;               /* 44: the item or weapon kind (weapons, pickups, the shop's slots), 0 for most enemies */
    uint8_t *v46;
    const uint8_t *cursor;      /* 4a: the next command of a path script (ObjMove cursor) */
    uint16_t turn[3];           /* 4e: the angle change per step as two words (high, low), then the step (52): ObjMove turn_hi, turn_lo, turn_step */
    int16_t v54;
    Object *target;             /* 56 */
    Object *chain;              /* 5a */
    uint8_t v5e_b[2];           /* 5e: a word (high byte first), and two bytes in ObjMove (shot_count, shot_rate): kept as bytes, V5E/SET_V5E for the word */
    int8_t flag60;
    int8_t flag61;
};
#define V5E(o) ((int16_t)(((o)->v5e_b[0] << 8) | (o)->v5e_b[1]))
#define SET_V5E(o, v) ((o)->v5e_b[0] = (uint8_t)((uint16_t)(v) >> 8), (o)->v5e_b[1] = (uint8_t)(v))

/* a weapon slot at $C66 (7 of them) */
typedef struct Slot Slot;
struct Slot {
    uint16_t type;
    Object *obj;
};

/* level 1: a record of the table at $55D6C (12 bytes) */
struct LevelSpawn {
    int16_t x;                  /* 0 */
    int16_t y;                  /* 2: the map's y */
    int16_t index;              /* 4: the counter in level_vars it keeps alive */
    int16_t flag;               /* 6: becomes $2E of the pieces */
    uint8_t *path;              /* 8: the path script */
};

/* level 1: an entry of the table at $55CE8 (12 bytes) */
struct LevelSeg {
    uint16_t type;              /* 0 */
    void (*hit)(void);          /* 2 */
    const void *gfx;            /* 6 */
    uint16_t score;             /* a */
};

#endif
