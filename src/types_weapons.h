#ifndef TYPES_WEAPONS_H
#define TYPES_WEAPONS_H
#include <stdint.h>
#include "types.h"
#include "types_kinds.h"

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
    uint16_t x_frac;            /* 22: not cleared when the slot is reused; only some kinds set it */
    int16_t y;                  /* 24 */
    uint16_t y_frac;            /* 26: as x_frac */
    union {
        struct {            /* the layout the loader byte-swaps; the kinds below name the words, the long at 2a is the high word first */
            int16_t w28;
            uint32_t w2a;
            int16_t w2e, w30;
        } raw_a;
        struct KindA_aim aim;
        struct KindA_arm arm;
        struct KindA_b1_gun b1_gun;
        struct KindA_bar bar;
        struct KindA_beam beam;
        struct KindA_bolt bolt;
        struct KindA_boss boss;
        struct KindA_bridge bridge;
        struct KindA_cannon cannon;
        struct KindA_core core;
        struct KindA_debris debris;
        struct KindA_diver diver;
        struct KindA_drill drill;
        struct KindA_enemy_shot enemy_shot;
        struct KindA_flyer flyer;
        struct KindA_front front;
        struct KindA_gate gate;
        struct KindA_gun gun;
        struct KindA_gunner gunner;
        struct KindA_head head;
        struct KindA_l1 l1;
        struct KindA_l1_boss l1_boss;
        struct KindA_l1_turret l1_turret;
        struct KindA_l2_boss l2_boss;
        struct KindA_l3_boss l3_boss;
        struct KindA_l3_boss2 l3_boss2;
        struct KindA_l3_gun l3_gun;
        struct KindA_l3_picture l3_picture;
        struct KindA_l4 l4;
        struct KindA_l4_gun l4_gun;
        struct KindA_l5 l5;
        struct KindA_l5_arm l5_arm;
        struct KindA_l5_turret l5_turret;
        struct KindA_launcher launcher;
        struct KindA_link link;
        struct KindA_pod_trail pod_trail;
        struct KindA_marker marker;
        struct KindA_marker_f8 marker_f8;
        struct KindA_marker_fc marker_fc;
        struct KindA_missile missile;
        struct KindA_mover mover;
        struct KindA_option option;
        struct KindA_part part;
        struct KindA_path path;
        struct KindA_pellet pellet;
        struct KindA_pickup pickup;
        struct KindA_piece piece;
        struct KindA_roamer roamer;
        struct KindA_shell shell;
        struct KindA_shot shot;
        struct KindA_snake snake;
        struct KindA_tower tower;
        struct KindA_turret turret;
        struct KindA_unit unit;
        struct KindA_w20 w20;
        struct KindA_w2c w2c;
        struct KindA_walker walker;
        struct KindA_wb0 wb0;
        struct KindA_wheel wheel;
        struct KindA_worm worm;
    };
    int16_t energy;             /* 32 */
    int8_t is_part;
    int8_t big;
    int16_t hit[4];             /* 36: the box x0 y0 x1 y1 */
    union {
        int16_t raw_b[2];
        struct KindB_arm arm_b;
        struct KindB_l2_boss l2_boss_b;
        struct KindB_part part_b;
        struct KindB_tile tile;
        struct KindB_walker walker_b;
        struct KindB_weapon weapon;
    };
    int16_t score;               /* 42: score the object gives; for a weapon its power */
    int16_t kind;               /* 44: the item or weapon kind (weapons, pickups, the shop's slots), 0 for most enemies */
    union {
        uint8_t *script;        /* 46: the path the object follows (ObjMove script) */
        Cell *map;              /* 46: the cell of the tile map the object draws into (MAP_COLS to a row) */
    };
    const struct PathCmd *cursor;      /* 4a: the next command of a path script (ObjMove cursor) */
    uint32_t turn;              /* 4e: added to the angle with its words swapped: the high word steps the integer (low byte counts), the low word the fraction */
    uint16_t turn_step;         /* 52: added to the low word of turn each step */
    int16_t steps;
    Object *target;             /* 56 */
    Object *chain;              /* 5a */
    uint8_t shot_count;         /* 5e: the fire accumulator; the weapons keep a countdown in the word shot_count:shot_rate */
    uint8_t shot_rate;          /* 5f */
    int8_t persists;
    int8_t screen_fixed;
};

/* a weapon slot at $C66 (7 of them) */
typedef struct Slot Slot;
struct Slot {
    uint16_t type;
    Object *obj;
};

#endif
