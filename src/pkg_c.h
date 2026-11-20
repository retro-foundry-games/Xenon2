#ifndef PKG_C_H
#define PKG_C_H
#include "game.h"

/* Object routines take the object as their parameter (the original passes it in a0). */
typedef void (*ObjFn)(Object *);
typedef void (*AnimOp)(Object *, const uint8_t *);

/* The lists of objects at $A76: the head pointer of list i is the forward link ($12, `prev` of struct Object) of obj_list[i]; the
 * links of the objects: forward $12 (`prev`), backward $E (`next`). */
#define OBJ_FWD(o) ((o)->prev)
#define OBJ_BACK(o) ((o)->next)
#define LIST_SENTINEL(i) (&obj_list[i])
#define LIST_HEAD(i) (LIST_SENTINEL(i)->prev)
enum { LIST_PLAYER = 0, LIST_MISC = 1, LIST_ENEMY = 2, LIST_EFFECT = 3, LIST_FRONT = 4 };

/* other modules, no C yet */
extern uint32_t random_next(void);                    /* $1F50 */
/* the level blob's entries ($54E0C: a shot or a point meets nothing else: a flag; $54E10/$54E18: a scroll event fires) */

void anim_step(Object *o);
void anim_op_jump(Object *o, const uint8_t *a1);
void anim_op_free(Object *o, const uint8_t *a1);
void map_tile_mask(uint16_t x, uint16_t y, uint16_t rows);
void map_tile_mask2(uint16_t x, uint16_t y, uint16_t rows);
extern uint16_t tile_mask2_words;
uint16_t player_tile_hit(Object *o);
Object *collide_enemies(Object *o);
Object *scan_boxes(Object *a1, int16_t x0, int16_t y0, int16_t x1, int16_t y1);
Object *point_in_enemy(int16_t x, int16_t y);
uint16_t direction8(int16_t dx, int16_t dy);
void obj_nop(Object *o);
void obj_nop_thunk(Object *o);
Object *owner_chain_next(Object *a1);
Object *spawn_formation(Object *a0, const uint8_t *a1, const uint8_t *a2);
void bbox_from_frame(Object *o);
void spawn_effect(int16_t x, int16_t y, const uint8_t *a2, ObjFn a3);
void note_spawn_record(const uint8_t *a1, uint16_t d2);
void latch_slots(uint16_t d2);
void scroll_events(void);
void run_updates(Object *a0);
void run_draws(Object *a0);
void update_all(void);
void spawn_debris(Object *a0, Object *a1, uint16_t d0);
void group_box_update(Object *a0);
Object *spawn_wave_326c(Object *a0, const uint8_t *a1);
Object *spawn_marker_enemy(uint16_t d0, const uint8_t *a1, void (*a2)(void));
Object *spawn_marker_front(uint16_t d0, const uint8_t *a1, void (*a2)(void));

void enemy_shot_update(Object *a0);
Object *spawn_enemy_shot(uint16_t x, uint16_t y, uint16_t angle, uint16_t steps, const void *gfx);

#endif
