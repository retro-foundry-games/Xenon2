#ifndef PLAY_H
#define PLAY_H
#include "game.h"

void draw_gauge(uint16_t x, uint16_t y, uint16_t fill, uint16_t max);
void switch_player_after_clear(void);

void clear_screens_large(void);
void clear_screens_small(void);
void init_level_state(void);
void panel_clear_score(void);
void panel_init(void);
void update_score_display(void);
void spawn_shot_object(Object *a0, uint16_t d0, uint16_t d1, uint16_t d2, uint16_t d3, void *const *a2);
void frame_update(void);
void free_object_chain(Object *a1);
void reset_extra_objects(void);
void option_update(Object *a0);
void draw_enemy_list(void);
void init_objects(void);
void update_shots(void);
void level_start(void);
void level_reset_scroll(void);
void clear_palette(void);
void clear_level_objects(void);
void create_queued_objects(void);
void respawn_player(void);
void switch_player(void);
void swap_player_state(void);
#endif

uint8_t autofire_stick(void);                                   /* orig $006426 */
