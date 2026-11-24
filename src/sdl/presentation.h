#ifndef XENON2_PRESENTATION_H
#define XENON2_PRESENTATION_H

#include <stdint.h>

enum PresentationDraw {
    PRESENT_BOB, PRESENT_BEHIND_MAP, PRESENT_TILES, PRESENT_BEAM, PRESENT_SHOT, PRESENT_OVERLAY
};

/* The SDL host and game share one thread. Capture the original draws into a
 * separate buffer, then publish it only when its screen becomes visible. */
extern int presentation_capturing;
void presentation_enable(int on);
void presentation_begin(void);
void presentation_background(void);
void presentation_seal(void);
void presentation_flip(const void *screen, uint16_t tick, uint64_t now);
void presentation_reset(void);
void presentation_discard_history(void);

void presentation_owner(const void *record);
void presentation_reuse(const void *record);
void presentation_draw(enum PresentationDraw kind, int x, int y);
void presentation_pin(int fixed);
void presentation_span(int x, int y, uint16_t mask, const uint16_t planes[4]);
void presentation_word(uint16_t *destination, uint16_t mask, uint16_t bits);
void presentation_star(int index, int x, int y, uint16_t mask);

const uint16_t *presentation_picture(uint64_t now);
const uint8_t *presentation_screen(void);

#endif
