/* The words of an Object at $28..$30 and $3E..$40 mean something
 * else for each kind of object; a kind names them here (o->launcher.step), the offset names (v28 ...) stay for the words no kind has named. */
#ifndef TYPES_KINDS_H
#define TYPES_KINDS_H
#include <stdint.h>

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
#endif
