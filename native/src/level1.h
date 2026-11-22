#ifndef LEVEL1_H
#define LEVEL1_H
#include "game.h"
void level1_spawn_units(void); void level1_spawn_unit(uint16_t unit); void level1_head_update(Object *head);
void level1_hit_free(Object *shooter, Object *target, uint16_t damage); void level1_segment_update(Object *seg);
void level1_hit_piece_a(Object *shooter, Object *target, uint16_t damage); void level1_hit_piece_b(Object *shooter, Object *target, uint16_t damage);
void level1_debris_update(Object *debris); void level1_boss_flash(Object *boss);
void level1_boss_hit(Object *shooter, Object *boss, uint16_t damage); void level1_boss_part_update(Object *part);
void level1_swap_hook(void);
void level1_turret_a_update(Object *turret); void level1_turret_b_update(Object *turret); void level1_turret_c_update(Object *turret);
void level1_launcher_update(Object *launcher);
void level1_hook(uint16_t id, const uint8_t *rec, uint16_t scroll);
int level1_collision_hook(void); void level1_event(uint16_t id, const uint8_t *rec); void level1_scroll(void);
void level1_boss_update(Object *boss); void level1_boss_draw(Object *boss); void level1_init(void);
#endif
