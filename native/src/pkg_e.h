#ifndef PKG_E_H
#define PKG_E_H
#include "game.h"

/* callees without C yet (arguments in the registers named in the comment; the results as noted) */
extern void fn_054e2c(void);                       /* in the level's blob */
extern Object *owner_chain_next(Object *a1);                    /* draws an object; returns a2, the next one */
extern void fn_00635e(uint16_t d0, uint16_t d1, uint16_t d2, uint16_t d3);

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
