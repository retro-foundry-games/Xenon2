#ifndef PKG_D_H
#define PKG_D_H
#include "game.h"

/* a hit routine (an object's fn_hit): a0 = the shooter's object, a1 = the object that was hit, d0 = the damage */
typedef void (*hit_fn)(Object *a0, Object *a1, uint16_t d0);

extern uint32_t random_next(void);                                         /* $1F50 */
void weapon_shield_draw(Object *a0);
void area_damage(Object *a0, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t dmg);
void debris_burst(Object *a0, int16_t d7, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *a2);
void spawn_bonuses(int16_t d7);
void spinner_update(Object *a0); void drill_update(Object *a0); void weaponc0_update(Object *a0); void weaponb0_update(Object *a0);
void pod_decoration_update(Object *a0); void weapon2c_update(Object *a0); void weapon4c_update(Object *a0); void weapon20_update(Object *a0);
void beam_update(Object *a0); void beam_draw(Object *a0); void weapon34_update(Object *a0); void pod_trail_update(Object *a0);
void weapon90_update(Object *a0); void shell_update(Object *a0); void weapon38_update(Object *a0);
void level1_spawn_units(void); void level1_spawn_unit(uint16_t d6); void level1_head_update(Object *a0);
void level1_hit_free(Object *a0, Object *a1, uint16_t d0); void level1_segment_update(Object *a0);
void level1_hit_piece_a(Object *a0, Object *a1, uint16_t d0); void level1_hit_piece_b(Object *a0, Object *a1, uint16_t d0);
void level1_debris_update(Object *a0); void level1_boss_flash(Object *a0);
void level1_boss_hit(Object *a0, Object *a1, uint16_t d0); void level1_boss_part_update(Object *a0);
void level1_swap_hook(void);
void level1_turret_a_update(Object *a0); void level1_turret_b_update(Object *a0); void level1_turret_c_update(Object *a0);
void level1_launcher_update(Object *a0);
void weapon94_update(Object *a0); void weapon98_update(Object *a0); void weapon44_update(Object *a0);
void weapon9c_update(Object *a0); void weaponA0_update(Object *a0);
void enemy_killed(Object *a0, Object *a1);
void level1_hook(uint16_t d0, const uint8_t *a1, uint16_t d2);

/* the object lists: $AD8, $B3A and $B9C are sentinels in obj_list */
#define LIST_A ((Object *)&obj_list[1])
#define LIST_C ((Object *)&obj_list[2])
#define LIST_B ((Object *)&obj_list[3])
#define PLAYER ((Object *)&player)
#define SLOT(i) (&v_0c66[i])

Object *weapon_select(uint16_t d1, Object *a0, Object *a1);               /* orig $004822 */
Object *refill_slot0(Object *a0, Object *a1);                                /* orig $005540 */
Object *weapon_94(Object *a0, Object *a1);                                 /* orig $004AFC */
void init_weapons(Object *a0);                                             /* orig $004A0A */
void pickup_update(Object *a0);                                            /* orig $004634 */
int player_hits(Object *a0);                                               /* orig $005550 */

void apply_damage(Object *a0, Object *a1, uint16_t d0);                    /* orig $005924 */
void apply_damage_max(Object *a0, Object *a1);                             /* orig $005920 */
Object *weapon_c0(Object *a0, Object *a1);                                 /* orig $004BA8 */
Object *weapon_98(Object *a0, Object *a1);                                 /* orig $004C64 */
Object *weapon_28(Object *a0, Object *a1);                                 /* orig $004D28 */
Object *weapon_48(Object *a0, Object *a1);                                 /* orig $004DC6 */
Object *weapon_b0(Object *a0, Object *a1);                                 /* orig $004E74 */
Object *weapon_2c(Object *a0, Object *a1);                                 /* orig $004F66 */
Object *weapon_4c(Object *a0, Object *a1);                                 /* orig $005014 */
Object *weapon_38(Object *a0, Object *a1);                                 /* orig $0050AC */
Object *weapon_3c(Object *a0, Object *a1);                                 /* orig $005158 */
Object *weapon_44(Object *a0, Object *a1);                                 /* orig $005206 */
Object *weapon_90(Object *a0, Object *a1);                                 /* orig $0052E0 */
Object *weapon_20(Object *a0, Object *a1);                                 /* orig $00538C */
Object *weapon_34(Object *a0, Object *a1);                                 /* orig $00543A */
void pickup_shield_small(Object *a0); void pickup_shield_big(Object *a0); void pickup_grow_weakest(Object *a0);
void pickup_speed(Object *a0); void pickup_move(Object *a0); void pickup_flag_d83(Object *a0); void pickup_ca0(Object *a0);
void pickup_weapon_98(Object *a0); void pickup_nothing(Object *a0); void pickup_bomb(Object *a0); void pickup_spinner(Object *a0);
int point_in_player_box(int16_t d0, int16_t d1);
void animate_only(Object *a0); void follow_target(Object *a0);
void shot_fixed_update(Object *a0); void shot_update(Object *a0);
void hit_part(Object *a0, Object *a1, uint16_t d0); void hit_flash(Object *a0, Object *a1, uint16_t d0);
void hit_kill_shooter(Object *a0, Object *a1, uint16_t d0);
void hit_flash_kill_shooter(Object *a0, Object *a1, uint16_t d0); void hit_enemy(Object *a0, Object *a1, uint16_t d0);
void hit_explode(Object *a0, Object *a1, uint16_t d0); void hit_tile(Object *a0, Object *a1, uint16_t d0);
void tile_draw(Object *a0);
void kill_with_slot(Object *a1); void free_with_slot(Object *a1); void free_chain(Object *a1, int16_t d7);
void player_dies(Object *a0); void player_damage(Object *a0, uint16_t d0); void player_move(Object *a0); void player_update(Object *a0);
int level1_collision_hook(void); void level1_event(uint16_t d0, const uint8_t *a1); void level1_scroll(void);
void level1_boss_update(Object *a0); void level1_boss_draw(Object *a0); void level1_init(void);

/* level 2's blob (pkg_l2.c) and the entries that choose by the level in v_0e04 */
Object *l2_collision_hook(int16_t d0, int16_t d1, int16_t d2, int16_t d3);
void l2_block_hit(Object *a0, Object *a1, uint16_t d0);
void l2_event(uint16_t d0, const uint8_t *a1); void l2_hook(uint16_t d0, const uint8_t *a1, uint16_t d2);
void l2_scroll(void); void l2_swap_hook(void); void l2_init(void); void l2_debris_walls(void);
void l2_turret_update(Object *a0); void l2_front_update(Object *a0); void l2_roamer_update(Object *a0); void l2_roamer_shot_update(Object *a0); void l2_marker_f8_update(Object *a0); void l2_marker_fc_update(Object *a0); void l2_flyer_update(Object *a0); void l2_front_piece_update(Object *a0); void l2_launcher_update(Object *a0); void l2_piece_update(Object *a0); void l2_gate_update(Object *a0);
void l2_gate_flash(Object *a0); void l2_boss_update(Object *a0); void l2_boss_draw(Object *a0); void l2_boss_flash(Object *a0);
void l2_hit_a(Object *a0, Object *a1, uint16_t d0); void l2_hit_b(Object *a0, Object *a1, uint16_t d0); void l2_hit_c(Object *a0, Object *a1, uint16_t d0);
void l2_gate_hit(Object *a0, Object *a1, uint16_t d0); void l2_boss_hit(Object *a0, Object *a1, uint16_t d0);
Object *level_collision(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Object *miss);
void level_event(uint16_t d0, const uint8_t *a1); void level_hook(uint16_t d0, const uint8_t *a1, uint16_t d2);
void level_scroll(void); void level_swap_hook(void); void level_init(void);
#endif
