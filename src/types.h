#ifndef TYPES_H
#define TYPES_H
#include <stdint.h>

/* a cell of the tile map: two raw big-endian bytes (read and written with rd16/wr16 on .b) */
typedef struct Cell { uint8_t b[2]; } Cell;

/* a word of the original's data that stays big endian (read with rd16 on .b) */
typedef struct RawWord { uint8_t b[2]; } RawWord;

/* a sample of the shop's sound bank: its size in bytes (a long; the words kept apart for the 16-bit use), its rate, then the samples */
struct BankSample { RawWord size_hi, size_lo, rate; };

/* the header of a picture (what Object.gfx points at; the pixels follow): the hot spot and the size */
struct Pic { RawWord hot_x, hot_y, width, height; };

/* a command of a path script (the words stay big endian): the first word is what it is, the others its arguments by the command */
struct PathCmd {
    RawWord op;                                       /* 0 ends (the object dies), 2 moves, 4 waits, 6 jumps to one of eight, 8 jumps, 10 sets the position */
    union {
        struct { RawWord angle, turn, turn_step, wait; } move;   /* 2: a new heading and turn for a number of steps */
        struct { RawWord steps; } pause;                         /* 4: waits that many steps */
        RawWord pick[8];                                         /* 6: offsets from the script's start; one of them (not negative) at random */
        struct { RawWord offset; } jump;                         /* 8: offset from the script's start */
        struct { RawWord x, y; } pos;                            /* 10: the position (the script's start) */
    } arg;
};

/* the four words before a picture: its hit box (offset and size) */
struct PicBox { RawWord dx, dy, w, h; };

/* the cells of a row of the tile map */
#define MAP_COLS 20

/* a 16.16 value from its whole and fraction words, the high word first */
#define FIXED_16_16(whole, fraction) (((uint32_t)(uint16_t)(whole) << 16) | (uint16_t)(fraction))

/* an interrupt handler, as the vector table at $64..$7C holds them (a pointer: 4 bytes in both builds) */
typedef void (*irq_fn)(void);

#endif
