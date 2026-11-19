#ifndef TYPES_PKG_F_H
#define TYPES_PKG_F_H
/* a star of the title starfield: x, y, depth; bit 15 of z marks a star that is hidden behind drawn pixels
 * (the dot-star variant at the same address uses the words as screen offset, bit mask, 0) */
struct Star {
    int16_t x, y, z;
};
#endif
