#ifndef WEAPONS_H
#define WEAPONS_H
#include <stdint.h>
#include "types.h"

/* a weapon slot at $C66 (7 of them) */
typedef struct Slot Slot;
struct Slot {
    uint16_t type;
    struct Object *obj;
};

#include "game.h"

/* the types of the weapons (the type of a weapon slot and of its object) */
#define WEAPON_LASER 0x20
#define WEAPON_BOMB 0x28
#define WEAPON_DRONE 0x2c
#define WEAPON_CANNON 0x34
#define WEAPON_MINE_1 0x38
#define WEAPON_MINE_2 0x3c
#define WEAPON_REAR_SHOT 0x44
#define WEAPON_HOMING_MISSILE 0x48
#define WEAPON_ELECTRO_BALL 0x4c
#define WEAPON_MISSILE_LAUNCHER 0x90
#define WEAPON_FORWARD_SHOT 0x94
#define WEAPON_SIDE_SHOT 0x98
#define WEAPON_FLAMER 0xb0
#define WEAPON_DOUBLE_SHOT 0xc0


/* a hit routine (an object's fn_hit): the shooter's object, the object that was hit, the damage */
typedef void (*hit_fn)(Object *shooter, Object *target, uint16_t damage);

void weapon_shield_draw(Object *obj);
void area_damage(Object *shooter, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t dmg);
void spinner_update(Object *obj); void drill_update(Object *obj); void double_shot_update(Object *obj); void flamer_update(Object *obj);
void pod_decoration_update(Object *obj); void drone_update(Object *obj); void electro_ball_update(Object *obj); void laser_update(Object *obj);
void beam_update(Object *obj); void beam_draw(Object *obj); void cannon_update(Object *obj); void pod_trail_update(Object *obj);
void missile_launcher_update(Object *obj); void shell_update(Object *obj); void mine_update(Object *obj);
void forward_shot_update(Object *obj); void side_shot_update(Object *obj); void rear_shot_update(Object *obj);
void bomb_update(Object *obj); void homing_missile_update(Object *obj);

/* the object lists: $AD8, $B3A and $B9C are sentinels in obj_list */
#define LIST_A ((Object *)&xenon2.obj_list[1])
#define LIST_C ((Object *)&xenon2.obj_list[2])
#define LIST_B ((Object *)&xenon2.obj_list[3])
#define PLAYER ((Object *)&xenon2.player)
#define SLOT(i) (&xenon2.weapon_slots[i])

Object *weapon_select(uint16_t type, Object *caller, Object *last);               /* orig $004822 */
Object *refill_slot0(Object *caller, Object *last);                                /* orig $005540 */
Object *give_forward_shot(Object *caller, Object *last);                                 /* orig $004AFC */
void init_weapons(Object *caller);                                             /* orig $004A0A */
void pickup_update(Object *obj);                                            /* orig $004634 */

Object *give_double_shot(Object *caller, Object *last);                                 /* orig $004BA8 */
Object *give_side_shot(Object *caller, Object *last);                                 /* orig $004C64 */
Object *give_bomb(Object *caller, Object *last);                                 /* orig $004D28 */
Object *give_homing_missile(Object *caller, Object *last);                                 /* orig $004DC6 */
Object *give_flamer(Object *caller, Object *last);                                 /* orig $004E74 */
Object *give_drone(Object *caller, Object *last);                                 /* orig $004F66 */
Object *give_electro_ball(Object *caller, Object *last);                                 /* orig $005014 */
Object *give_mine_1(Object *caller, Object *last);                                 /* orig $0050AC */
Object *give_mine_2(Object *caller, Object *last);                                 /* orig $005158 */
Object *give_rear_shot(Object *caller, Object *last);                                 /* orig $005206 */
Object *give_missile_launcher(Object *caller, Object *last);                                 /* orig $0052E0 */
Object *give_laser(Object *caller, Object *last);                                 /* orig $00538C */
Object *give_cannon(Object *caller, Object *last);                                 /* orig $00543A */
void pickup_shield_small(Object *obj); void pickup_shield_big(Object *obj); void pickup_grow_weakest(Object *obj);
void pickup_speed(Object *obj); void pickup_move(Object *obj); void pickup_flag_d83(Object *obj); void pickup_ca0(Object *obj);
void pickup_side_shot(Object *obj); void pickup_nothing(Object *obj); void pickup_bomb(Object *obj); void pickup_spinner(Object *obj);
void shot_fixed_update(Object *obj); void shot_update(Object *obj);
void kill_with_slot(Object *obj); void free_with_slot(Object *obj);
void player_dies(Object *ship); void player_damage(Object *ship, uint16_t damage); void player_move(Object *ship); void player_update(Object *ship);

/* level 2's blob (level2.c) and the entries that choose by the level in level_number */
Object *l2_collision_hook(int16_t box_x0, int16_t box_y0, int16_t box_x1, int16_t box_y1);
void l2_block_hit(Object *attacker, Object *target, uint16_t damage);
void l2_event(uint16_t event, const struct SpawnRec *rec); void l2_hook(uint16_t hook, const struct SpawnRec *rec, uint16_t rec_scroll);
void l2_scroll(void); void l2_swap_hook(void); void l2_init(void); void l2_debris_walls(void);
void l2_turret_update(Object *turret); void l2_front_update(Object *marker); void l2_boss_rest(Object *boss) __attribute__((noinline)); void l2_roamer_update(Object *roamer); void l2_roamer_shot_update(Object *bolt); void l2_marker_f8_update(Object *marker); void l2_marker_fc_update(Object *marker); void l2_flyer_update(Object *enemy); void l2_front_piece_update(Object *part); void l2_launcher_update(Object *launcher); void l2_piece_update(Object *part); void l2_gate_update(Object *gate);
void l2_gate_flash(Object *gate); void l2_boss_update(Object *boss); void l2_boss_draw(Object *boss); void l2_boss_flash(Object *boss);
void l2_hit_a(Object *attacker, Object *target, uint16_t damage); void l2_hit_b(Object *attacker, Object *target, uint16_t damage); void l2_hit_c(Object *attacker, Object *target, uint16_t damage);
void l2_gate_hit(Object *attacker, Object *gate, uint16_t damage); void l2_boss_hit(Object *attacker, Object *boss, uint16_t damage);
/* level 3's blob (level3.c) */
void l3_event(uint16_t event, const struct SpawnRec *rec); void l3_hook(uint16_t hook, const struct SpawnRec *rec, uint16_t scroll_value);
void l3_scroll(void); void l3_init(void); void l3_end_draw(Object *obj);
void l3_enemy_a(Object *obj); void l3_enemy_b(Object *obj); void l3_enemy_c(Object *obj);
void l3_snake_head(Object *obj); void l3_snake_tail(Object *obj); void l3_snake_body(Object *obj);
void l3_spawn_parts(void); void l3_part_core(Object *obj); void l3_part_follow(Object *obj); void l3_part_gun(Object *obj);
void l3_part_hit(Object *shot, Object *part, uint16_t damage); void l3_part_arm(Object *obj); void l3_part_arm_piece(Object *obj); void l3_part_arm_tip(Object *obj);
void l3_enemy_enter(Object *obj);
void l3_worm_head(Object *obj); void l3_worm_a(Object *obj); void l3_worm_b(Object *obj); void l3_worm_c(Object *obj);
void l3_gun_update(Object *obj); void l3_boss_update(Object *obj); void l3_boss_hit(Object *shot, Object *boss, uint16_t damage); void l3_boss_flash(Object *obj);
void l3_boss2_update(Object *obj); void l3_boss2_hit(Object *shot, Object *boss, uint16_t damage); void l3_boss2_flash(Object *obj); void l3_walker_update(Object *obj);
void l3_diver_update(Object *obj); void l3_diver_shot(Object *obj);
Object *level_collision(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Object *miss);
void level_event(uint16_t event, const struct SpawnRec *record); void level_hook(uint16_t hook, const struct SpawnRec *record, uint16_t slot_value);
void level_scroll(void); void level_swap_hook(void); void level_init(void);
void l4_event(uint16_t which, const struct SpawnRec *record); void l4_hook(uint16_t which, const struct SpawnRec *record, uint16_t param);
void l4_scroll(void); void l4_init(void); void l4_spawn_tower(const struct SpawnRec *record);
void l4_enemy_a(Object *obj); void l4_shot_a(Object *obj); void l4_enemy_f8(Object *obj); void l4_enemy_b(Object *obj);
void l4_b1_core(Object *obj); void l4_b1_gun(Object *obj); void l4_b1_head(Object *obj); void l4_b1_body(Object *obj); void l4_b1_tail(Object *obj);
void l4_b1_core_hit(Object *shot, Object *enemy, uint16_t damage); void l4_b1_gun_hit(Object *shot, Object *enemy, uint16_t damage); void l4_gun_dead(Object *obj);
void l4_b1_core_draw(Object *obj); void l4_b1_core_flash(Object *obj); void l4_b1_gun_draw(Object *obj); void l4_b1_gun_flash(Object *obj); void l4_b1_piece_draw(Object *obj);
void l4_b2_head(Object *obj); void l4_b2_follow(Object *obj); void l4_b2_gun(Object *obj); void l4_b2_barrel(Object *obj); void l4_b2_wave(Object *obj);
void l4_b2_link(Object *obj); void l4_b2_ring(Object *obj); void l4_b2_eye(Object *obj);
void l4_b2_gun_hit(Object *shot, Object *enemy, uint16_t damage); void l4_b2_barrel_hit(Object *shot, Object *enemy, uint16_t damage);
void l4_b2_ring_hit(Object *shot, Object *enemy, uint16_t damage); void l4_b2_eye_hit(Object *shot, Object *enemy, uint16_t damage);
void l4_bar_update(Object *obj); void l4_bar_draw(Object *obj);
void l4_walker_update(Object *obj); void l4_walker_hit(Object *shot, Object *enemy, uint16_t damage); void l4_walker_flash(Object *obj); void l4_walker_draw(Object *obj);
void l5_event(uint16_t event, const struct SpawnRec *record); void l5_hook(uint16_t hook, const struct SpawnRec *record, uint16_t slot_value); void l5_init(void); void l5_swap_hook(void);
#endif
