#ifndef PKG_E_H
#define PKG_E_H
#include "game.h"

/* callees without C yet (arguments in the registers named in the comment; the results as noted) */
extern Obj *fn_002418(void);                       /* takes an object from the free list: a0 */
extern void fn_003130(void);                       /* an object's idle routine (stored as a pointer) */
extern void fn_005efe(void);                       /* the player's routine (stored as a pointer) */
extern void fn_00128e(void);                       /* an object's draw routine (stored as a pointer) */
extern void fn_005850(void);                       /* routine of a shot object (stored as a pointer) */
extern void fn_004afc(void);
extern void fn_00352e(void);
extern void fn_00176e(void);
extern void fn_0016be(void);
extern void fn_00842a(const uint8_t *a0);          /* prints the string at a0 */
extern void fn_004a0a(void);
extern void fn_019fe4(void);
extern void fn_054e20(void);                       /* in the level's blob */
extern void fn_054e2c(void);                       /* in the level's blob */
extern int fn_0030de(uint16_t d0, uint16_t d1);    /* result in the Z flag: returns 1 when Z is set */
extern void fn_005924(uint16_t d0);
extern void fn_002c5a(Obj *a0);                    /* steps an object's animation */
extern Obj *fn_003286(Obj *a1);                    /* draws an object; returns a2, the next one */
extern void fn_005d8c(Obj *a1, uint16_t d7);       /* scrolls and frees the objects of a list */
extern void fn_005c9e(Obj *a1);                    /* frees the object a1 */
extern Obj *fn_004822(uint16_t d1);                /* makes an object of type d1: a1 */
extern void fn_00635e(uint16_t d0, uint16_t d1, uint16_t d2, uint16_t d3);

void clear_screens_large(void);
void clear_screens_small(void);
void init_level_state(void);
void panel_clear_score(void);
void panel_init(void);
void update_score_display(void);
void spawn_shot_object(Obj *a0, uint16_t d0, uint16_t d1, uint16_t d2, uint16_t d3, void *const *a2);
void frame_update(void);
void free_object_chain(Obj *a1);
void reset_extra_objects(void);
void option_update(Obj *a0);
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
