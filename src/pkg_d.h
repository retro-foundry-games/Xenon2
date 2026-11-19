#ifndef PKG_D_H
#define PKG_D_H
#include "game.h"

/* a hit routine (an object's fn_hit): a0 = the shooter's object, a1 = the object that was hit, d0 = the damage */
typedef void (*hit_fn)(Object *a0, Object *a1, uint16_t d0);

/* callees without C yet (arguments in the registers named in the comment) */
extern uint32_t random_next(void);                                         /* $1F50 */
extern Object *fn_000e54(void);                                            /* the level's object allocator: a0 */
extern void fn_000e60(const uint16_t *a2);                                 /* a2 = a table of the level */
extern void fn_000e8a(const void *a1, uint16_t d0, uint16_t d1, uint16_t d2, uint16_t d3);
extern void fn_000e90(const void *a0, uint16_t d0, uint16_t d1);
extern void fn_000f56(uint16_t d0, const uint16_t *a2);
extern void fn_001438(Object *a0);
extern void fn_001bd0(uint16_t d0, uint16_t d1, uint16_t d2, uint16_t d3, const void *a1);
extern void fn_006912(Object *a0);
extern void fn_003868(Object *a0); extern void fn_0039a2(Object *a0); extern void fn_003b4a(Object *a0); extern void fn_003b94(Object *a0);
extern void fn_003c7a(Object *a0); extern void fn_003cdc(Object *a0); extern void fn_003e2c(Object *a0); extern void fn_003f3a(Object *a0);
extern void fn_004094(Object *a0); extern void fn_00415a(Object *a0); extern void fn_004340(Object *a0); extern void fn_0042a6(Object *a0);
extern void fn_055e2c(void);
extern void fn_000e5a(void); extern void fn_000ea2(void); extern void fn_000e12(Object *a0); extern void fn_000f2c(Object *a0);
extern void fn_056278(Object *a0); extern void fn_0561da(Object *a0);
void weapon94_update(Object *a0); void weapon98_update(Object *a0); void weapon44_update(Object *a0);
void weapon9c_update(Object *a0); void weaponA0_update(Object *a0);
void enemy_killed(Object *a0, Object *a1);
void level1_hook(uint16_t d0, const uint8_t *a1, uint16_t d2);

/* the object lists: $AD8, $B3A and $B9C are sentinels in obj_list */
#define LIST_A ((Object *)&obj_list[1])
#define LIST_C ((Object *)&obj_list[2])
#define LIST_B ((Object *)&obj_list[3])
#define PLAYER ((Object *)&player)
#define SLOT(i) ((Slot *)&v_0c66[3 * (i)])

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
#endif
