/* the declarations game.h carries after the memory struct (tools/gen_symbols.py pastes this file in): the game's start-up and frame flow */
/* start-up flow, vertical-blank interrupts, starfield, screen flip */
void game_entry(void);
void irq_vblank(void);
void irq_vblank_hooked(void);
void swap_screens(void);
uint32_t swap_screens_200(void);
uint32_t swap_screens_192(void);
void scroll_pos_update(uint16_t speed);
void stars_scroll(uint16_t speed);
void stars_draw(void);
uint16_t box_overlap(int16_t a_left, int16_t a_top, int16_t a_right, int16_t a_bottom, int16_t b_left, int16_t b_top, int16_t b_right, int16_t b_bottom);
Object *obj_free_pop(void);

void irq_keyboard(void);

/* the player, the objects lists, the status panel, the level start: */
void draw_gauge(uint16_t x, uint16_t y, uint16_t fill, uint16_t max);
void switch_player_after_clear(void);

void clear_screens_large(void);
void clear_screens_small(void);
void init_level_state(void);
void panel_clear_score(void);
void panel_init(void);
void update_score_display(void);
void spawn_shot_object(Object *owner, uint16_t dx, uint16_t dy, uint16_t off_x, uint16_t off_y, void *const *gfx_slot);
void frame_update(void);
void free_object_chain(Object *obj);
void reset_extra_objects(void);
void option_update(Object *option);
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

uint8_t autofire_stick(void);                                   /* orig $006426 */
void update_all(void);
