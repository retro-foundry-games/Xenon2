#ifndef OBJECTS_H
#define OBJECTS_H
#include <stdint.h>
#include "types.h"

/* The words of an Object at $28..$30 and $3E..$40 mean something
 * else for each kind of object; a kind names them here (o->launcher.step), the offset names (v28 ...) stay for the words no kind has named. */

struct KindA_aim {
    uint8_t pad28[2];
    uint32_t dir;
    uint8_t pad_end[4];
};
struct KindA_arm {
    uint8_t pad28[2];
    uint32_t angle;
    int16_t swing_step;
    uint8_t pad_end[2];
};
struct KindB_arm {
    int16_t swing;
    uint8_t pad_end[2];
};
struct KindA_b1_gun {
    uint8_t pad28[2];
    uint32_t dir;
    uint8_t pad_end[4];
};
struct KindA_bar {
    int16_t phase;
    uint32_t step;
    uint8_t pad_end[4];
};
struct KindA_beam {
    union {
        int16_t growth;
        int16_t length;
    };
    uint32_t dy;
    uint8_t pad_end[4];
};
struct KindA_bolt {
    int16_t target;
    uint32_t dir;
    uint8_t pad_end[4];
};
struct KindA_boss {
    int16_t phase;
    uint32_t step;
    uint8_t pad_end[4];
};
struct KindA_bridge {
    int16_t cell;
    uint8_t pad_end[8];
};
struct KindA_cannon {
    uint8_t pad28[2];
    uint32_t bob;
    uint8_t pad_end[4];
};
struct KindA_core {
    int16_t tick;
    uint8_t pad_end[8];
};
struct KindA_debris {
    uint8_t pad8[8];
    int16_t kind;
};
struct KindA_diver {
    int16_t state;
    uint32_t side;
    uint8_t pad_end[4];
};
struct KindA_drill {
    int16_t state;
    uint32_t dir;
    uint8_t pad2e[2];
    int16_t target_hi;
};
struct KindA_enemy_shot {
    uint8_t pad28[2];
    uint32_t dir;
    uint8_t pad_end[4];
};
struct KindA_flyer {
    int16_t dive;
    uint8_t pad_end[8];
};
struct KindA_front {
    union {
        int16_t step;
        int16_t tick;
    };
    uint32_t dir;
    uint8_t pad_end[4];
};
struct KindA_gate {
    int16_t step;
    uint8_t pad8[6];
    int16_t number;
};
struct KindA_gun {
    uint8_t pad8[8];
    int16_t side;
};
struct KindA_gunner {
    uint8_t pad28[2];
    uint32_t phase;
    uint8_t pad_end[4];
};
struct KindA_head {
    int16_t bend;
    uint32_t angle;
    int16_t dart;
    uint8_t pad_end[2];
};
struct KindA_l1 {
    int16_t wait;
    uint32_t angle;
    uint8_t pad_end[4];
};
struct KindA_l1_boss {
    int16_t phase;
    uint8_t pad6[4];
    int16_t move;
    int16_t wait;
};
struct KindA_l1_turret {
    int16_t step;
    uint8_t pad_end[8];
};
struct KindA_l2_boss {
    int16_t wait;
    uint32_t step;
    int16_t frames;
    int16_t cell;
};
struct KindB_l2_boss {
    int16_t flash;
    uint8_t pad_end[2];
};
struct KindA_l3_boss {
    int16_t cell;
    uint8_t pad_end[8];
};
struct KindA_l3_boss2 {
    int16_t cell;
    uint8_t pad_end[8];
};
struct KindA_l3_gun {
    int16_t open;
    uint8_t pad_end[8];
};
struct KindA_l3_picture {
    uint8_t pad28[2];
    uint32_t phase;
    uint8_t pad_end[4];
};
struct KindA_l4 {
    int16_t size;
    uint32_t angle;
    int16_t extra;
    uint8_t pad_end[2];
};
struct KindA_l4_gun {
    int16_t tick;
    uint8_t pad_end[8];
};
struct KindA_l5 {
    int16_t cell;
    uint8_t pad_end[8];
};
struct KindA_l5_arm {
    int16_t stroke;
    uint8_t pad_end[8];
};
struct KindA_l5_turret {
    uint8_t pad28[2];
    uint32_t dir;
    uint8_t pad_end[4];
};
struct KindA_launcher {
    int16_t step;
    uint32_t range;
    uint8_t pad2e[2];
    int16_t pos;
};
struct KindA_pod_trail {
    int16_t tick;
    uint8_t pad_end[8];
};
typedef struct Object Object;
struct KindA_link {
    uint8_t pad28[2];
    Object *ptr;
    uint8_t pad_end[4];
};
struct KindA_marker {
    int16_t step;
    uint8_t pad2[6];
    int16_t launches;
};
struct KindA_marker_f8 {
    int16_t blink;
    uint8_t pad_end[8];
};
struct KindA_marker_fc {
    int16_t step;
    uint8_t pad8[6];
    int16_t launches;
};
struct KindA_missile {
    int16_t tick;
    uint32_t dir;
    uint8_t pad_end[4];
};
struct KindA_mover {
    union {
        int16_t cell;
        int16_t tick;
    };
    uint32_t step;
    uint8_t pad_end[4];
};
struct KindA_option {
    uint8_t pad8[8];
    int16_t index;
};
struct KindA_part {
    uint8_t pad8[8];
    int16_t slot;
};
struct KindB_part {
    int16_t dx;
    int16_t dy;
};
struct KindA_path {
    int16_t wait;
    uint32_t angle;
    uint8_t pad_end[4];
};
struct KindA_pellet {
    int16_t tick;
    uint32_t dir;
    uint8_t pad_end[4];
};
struct KindA_pickup {
    int16_t mode;
    uint32_t dir;
    uint8_t pad2e[2];
    int16_t kind;
};
struct KindA_piece {
    int16_t wait;
    uint32_t dir;
    int16_t var;
    int16_t wall;
};
struct KindA_roamer {
    int16_t state;
    uint32_t dir;
    uint8_t pad_end[4];
};
struct KindA_shell {
    int16_t phase;
    uint8_t pad_end[8];
};
struct KindA_shot {
    int16_t phase;
    uint32_t speed;
    int16_t dy;
    int16_t hits;
};
struct KindA_snake {
    int16_t swing;
    uint32_t step;
    int16_t x;
    int16_t limit;
};
struct KindB_tile {
    int16_t cols;
    int16_t rows;
};
struct KindA_tower {
    int16_t phase;
    uint8_t pad_end[8];
};
struct KindA_turret {
    int16_t step;
    uint8_t pad_end[8];
};
struct KindA_unit {
    union {
        int16_t tick;
        int16_t wait;
    };
    uint32_t dir;
    int16_t counter;
    union {
        int16_t group;
        int16_t id;
    };
};
struct KindA_w20 {
    int16_t tick;
    uint8_t pad_end[8];
};
struct KindA_w2c {
    int16_t tick;
    uint8_t pad_end[8];
};
struct KindA_walker {
    int16_t state;
    uint32_t dir;
    int16_t dy;
    int16_t side;
};
struct KindB_walker {
    int16_t home_x;
    int16_t home_y;
};
struct KindA_wb0 {
    int16_t tick;
    uint8_t pad_end[8];
};
struct KindB_weapon {
    int16_t dx;
    int16_t dy;
};
struct KindA_wheel {
    int16_t angle;
    uint8_t pad_end[8];
};
struct KindA_worm {
    int16_t delay;
    uint32_t phase;
    uint8_t pad2e[2];
    int16_t side;
};

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
    uint16_t x_frac;            /* 22: not cleared when the slot is reused; only some kinds set it */
    int16_t y;                  /* 24 */
    uint16_t y_frac;            /* 26: as x_frac */
    union {
        struct {
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
        };
        struct {            /* the drill keeps its target in the words 30 and 32 */
            int16_t state;
            uint32_t dir;
            uint8_t pad2e[2];
            struct Object *target;
        } drill_target;
    };
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
        uint8_t *script;        /* 46: the path the object follows */
        Cell *map;              /* 46: the cell of the tile map the object draws into (MAP_COLS to a row) */
    };
    const struct PathCmd *cursor;      /* 4a: the next command of a path script */
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

/* the kinds of pieces an object breaks into (6 bytes) */
struct SpawnKind {
    uint16_t kind;
    const uint16_t *anim;       /* animation header */
};

#include "game.h"

/* Object routines take the object as their parameter (the original passes it in a register). */
typedef void (*ObjFn)(Object *);
typedef void (*AnimOp)(Object *, const uint16_t *);

/* The lists of objects at $A76: the head pointer of list i is the forward link ($12, `prev` of struct Object) of obj_list[i]; the
 * links of the objects: forward $12 (`prev`), backward $E (`next`). */
#define OBJ_FWD(o) ((o)->prev)
#define OBJ_BACK(o) ((o)->next)
#define LIST_SENTINEL(i) (&xenon2.obj_list[i])
#define LIST_HEAD(i) (LIST_SENTINEL(i)->prev)
enum { LIST_PLAYER = 0, LIST_MISC = 1, LIST_ENEMY = 2, LIST_EFFECT = 3, LIST_FRONT = 4 };

/* other modules, no C yet */
/* the level blob's entries ($54E0C: a shot or a point meets nothing else: a flag; $54E10/$54E18: a scroll event fires) */

void anim_step(Object *o);
void anim_op_jump(Object *o, const uint16_t *script);
void anim_op_free(Object *o, const uint16_t *script);
void map_tile_mask(uint16_t x, uint16_t y, uint16_t rows);
void map_tile_mask2(uint16_t x, uint16_t y, uint16_t rows);
uint16_t player_tile_hit(Object *o);
Object *collide_enemies(Object *o);
Object *scan_boxes(Object *obj, int16_t x0, int16_t y0, int16_t x1, int16_t y1);
Object *point_in_enemy(int16_t x, int16_t y);
uint16_t direction8(int16_t dx, int16_t dy);
void obj_nop(Object *o);
void obj_nop_thunk(Object *o);
Object *owner_chain_next(Object *obj);
Object *spawn_formation(Object *obj, const struct SpawnRec *rec, const struct SpawnDef *formation);
void bbox_from_frame(Object *o);
void spawn_effect(int16_t x, int16_t y, const uint16_t *anim, ObjFn draw);
void note_spawn_record(const struct SpawnRec *rec, uint16_t score);
void latch_slots(uint16_t score);
void scroll_events(void);
void run_updates(Object *obj);
void run_draws(Object *obj);
void spawn_debris(Object *shooter_unused, Object *target, uint16_t damage_unused);
void group_box_update(Object *end);
Object *spawn_wave_326c(Object *obj, const struct SpawnRec *rec);
Object *spawn_marker_enemy(uint16_t type, const struct SpawnRec *rec, void (*update)(void));
Object *spawn_marker_front(uint16_t type, const struct SpawnRec *rec, void (*update)(void));

void enemy_shot_update(Object *shot);
Object *spawn_enemy_shot(uint16_t x, uint16_t y, uint16_t angle, uint16_t steps, const void *gfx);

/* from the weapons module */
void kill_object(Object *o);
Object *alloc_a(Object *caller);
void free_object(Object *obj);
void debris_burst(Object *obj, int16_t count, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *anim);
void spawn_bonuses(int16_t count);
void enemy_killed(Object *shooter, Object *enemy);
int player_hits(Object *obj);                                               /* orig $005550 */
void apply_damage(Object *shooter, Object *victim, uint16_t damage);                    /* orig $005924 */
void apply_damage_max(Object *shooter, Object *victim);                             /* orig $005920 */
int point_in_player_box(int16_t x, int16_t y);
void animate_only(Object *obj);
void follow_target(Object *obj);
void hit_part(Object *shooter, Object *enemy, uint16_t damage);
void hit_flash(Object *shooter, Object *victim, uint16_t damage);
void hit_kill_shooter(Object *shooter, Object *victim, uint16_t damage);
void hit_flash_kill_shooter(Object *shooter, Object *victim, uint16_t damage);
void hit_enemy(Object *shooter, Object *enemy, uint16_t damage);
void hit_explode(Object *shooter, Object *enemy, uint16_t damage);
void hit_tile(Object *shooter, Object *tile, uint16_t damage);
void tile_draw(Object *obj);
void free_chain(Object *obj, int16_t dy);
#endif
