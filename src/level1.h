#ifndef LEVEL1_H
#define LEVEL1_H
#include "game.h"
void level1_spawn_units(void); void level1_spawn_unit(uint16_t d6); void level1_head_update(Object *a0);
void level1_hit_free(Object *a0, Object *a1, uint16_t d0); void level1_segment_update(Object *a0);
void level1_hit_piece_a(Object *a0, Object *a1, uint16_t d0); void level1_hit_piece_b(Object *a0, Object *a1, uint16_t d0);
void level1_debris_update(Object *a0); void level1_boss_flash(Object *a0);
void level1_boss_hit(Object *a0, Object *a1, uint16_t d0); void level1_boss_part_update(Object *a0);
void level1_swap_hook(void);
void level1_turret_a_update(Object *a0); void level1_turret_b_update(Object *a0); void level1_turret_c_update(Object *a0);
void level1_launcher_update(Object *a0);
void level1_hook(uint16_t d0, const uint8_t *a1, uint16_t d2);
int level1_collision_hook(void); void level1_event(uint16_t d0, const uint8_t *a1); void level1_scroll(void);
void level1_boss_update(Object *a0); void level1_boss_draw(Object *a0); void level1_init(void);
#endif
