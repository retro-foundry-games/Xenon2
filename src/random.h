#ifndef RANDOM_H
#define RANDOM_H
#include "game.h"

uint32_t random_next(void);            /* $1F50 */
uint16_t random_below(uint16_t n);     /* $1FF6 */
#endif
