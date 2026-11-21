/* level: the entries the engine calls, by the level in v_0e04: level 1's code is in weapons.c and the others, level 2's in level2.c, level 3's
 * in level3.c, level 4's in level4.c, level 5's in level5.c */
#include "weapons.h"
#include "shop.h"
#include "game_all.h"

Object *level_collision(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Object *miss)
{
    if (v_0e04 == 2)
        return l2_collision_hook(x0, y0, x1, y1);
    if (v_0e04 == 1)
        return level1_collision_hook() ? miss : 0;
    return 0;   /* (level 3: $56FEA, no block) */
}

void level_event(uint16_t d0, const uint8_t *a1)
{
    if (v_0e04 == 2)
        l2_event(d0, a1);
    else if (v_0e04 == 3)
        l3_event(d0, a1);
    else if (v_0e04 == 4)
        l4_event(d0, a1);
    else if (v_0e04 == 5)
        l5_event(d0, a1);
    else if (v_0e04 == 1)
        level1_event(d0, a1);
}

void level_hook(uint16_t d0, const uint8_t *a1, uint16_t d2)
{
    if (v_0e04 == 2)
        l2_hook(d0, a1, d2);
    else if (v_0e04 == 3)
        l3_hook(d0, a1, d2);
    else if (v_0e04 == 4)
        l4_hook(d0, a1, d2);
    else if (v_0e04 == 5)
        l5_hook(d0, a1, d2);
    else if (v_0e04 == 1)
        level1_hook(d0, a1, d2);
}

void level_scroll(void)
{
    if (v_0e04 == 2)
        l2_scroll();
    else if (v_0e04 == 3)
        l3_scroll();
    else if (v_0e04 == 4)
        l4_scroll();
    else if (v_0e04 == 1)
        level1_scroll();
}

void level_swap_hook(void)
{
    if (v_0e04 == 2)
        l2_swap_hook();
    else if (v_0e04 == 5)
        l5_swap_hook();
    else if (v_0e04 == 1)
        level1_swap_hook();
}

void level_init(void)
{
    if (v_0e04 == 2)
        l2_init();
    else if (v_0e04 == 3)
        l3_init();
    else if (v_0e04 == 4)
        l4_init();
    else if (v_0e04 == 5)
        l5_init();
    else if (v_0e04 == 1)
        level1_init();
}
