/* the declarations game.h carries after the memory struct (tools/gen_symbols.py pastes this file in): the game's start-up and frame flow */
/* start-up flow, vertical-blank interrupts, starfield, screen flip */
void game_entry(void);
void irq_vblank(void);
void irq_vblank_hooked(void);
void swap_screens(void);
uint32_t swap_screens_200(void);
uint32_t swap_screens_192(void);
void scroll_pos_update(uint16_t speed);

void irq_keyboard(void);

/* the player, the objects lists, the status panel, the level start: */
void switch_player_after_clear(void);

void clear_screens_large(void);
void clear_screens_small(void);
void init_level_state(void);
void frame_update(void);
void draw_enemy_list(void);
void level_start(void);
void level_reset_scroll(void);
void clear_palette(void);
void respawn_player(void);
void switch_player(void);
void swap_player_state(void);

uint8_t autofire_stick(void);                                   /* orig $006426 */
void update_all(void);
