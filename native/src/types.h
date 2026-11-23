#ifndef TYPES_H
#define TYPES_H
#include <stdint.h>

/* a cell of the tile map */
typedef uint16_t Cell;

/* the steps of an animation script: a picture and a delay; the picture 0 and the code to call (it takes the position after it); the header of a script
 * is its first step; the script "jump" has the script to go on with.  The loader makes the longs pointers (sdl/guest.c). */
struct Object;
struct AnimStep { const uint16_t *gfx; uint16_t delay; };
struct AnimCall { const void *zero; void (*code)(struct Object *, const uint16_t *); };
struct AnimJump { const uint16_t *script; };

/* a row of a tile mask (16 of them, 160 bytes): the words of the row; the map's masks are read by the first one or two */
struct MaskRow { uint16_t w[5]; };
struct Cell4 { uint16_t plane[4]; };

/* the header of a picture (what Object.gfx points at; the pixels follow): the hot spot and the size */
struct Pic { uint16_t hot_x, hot_y, width, height; };

/* a command of a path script: the first word is what it is, the others its arguments by the command */
struct PathCmd {
    uint16_t op;                                      /* 0 ends (the object dies), 2 moves, 4 waits, 6 jumps to one of eight, 8 jumps, 10 sets the position */
    union {
        struct { uint16_t angle, turn, turn_step, wait; } move;   /* 2: a new heading and turn for a number of steps */
        struct { uint16_t steps; } pause;                         /* 4: waits that many steps */
        uint16_t pick[8];                                         /* 6: offsets from the script's start; one of them (not negative) at random */
        struct { uint16_t offset; } jump;                         /* 8: offset from the script's start */
        struct { uint16_t x, y; } pos;                            /* 10: the position (the script's start) */
    } arg;
};

/* the four words before a picture: its hit box (offset and size) */
struct PicBox { uint16_t dx, dy, w, h; };

/* the cells of a row of the tile map */
#define MAP_COLS 20

/* the screens: a row is the four planes one after the other, each of SCREEN_PLANE_WORDS words (a uint16_t pointer to a word of the first plane;
 * the same word of plane p is SCREEN_PLANE_WORDS * p on, the next row SCREEN_ROW_WORDS on) */
#define SCREEN_PLANE_WORDS 20
#define SCREEN_ROW_WORDS (4 * SCREEN_PLANE_WORDS)
#define SCREEN_PLANE_BYTES (2 * SCREEN_PLANE_WORDS)
#define SCREEN_ROW_BYTES (2 * SCREEN_ROW_WORDS)
#define SCREEN_ROWS 192   /* the rows of a screen; the panel (8 rows) follows */
#define PANEL_ROWS 8
#define SOUND_SILENCE_BYTES 0x40   /* the bytes of silence after the samples of a bank */
#define MAP_BLIT_WORDS (2 * SCREEN_PLANE_WORDS)   /* the blitter copies a row of the map image (two planes of a screen row) */
#define MAP_BLIT_BYTES (2 * MAP_BLIT_WORDS)
#define DIGIT_GLYPH_BYTES 32   /* 8 rows of 4 planes */
#define MAP_ROW_BYTES (2 * MAP_COLS)
#define MAP_ROW(row) ((row) * MAP_ROW_BYTES)   /* the byte offset of a row of the map */
#define MAP_CELL(row, col) (MAP_ROW(row) + 2 * (col))
/* the disk's work area in screen_a: the 11 sectors of a track decoded, the raw track as it comes from the drive, the boot sector read to find the game disk */
struct DiskSector { uint16_t word[256]; };
struct DiskScratch { struct DiskSector sector[11]; uint16_t track[0x2000]; uint16_t boot[256]; };

/* a 16.16 value from its whole and fraction words, the high word first */
#define FIXED_16_16(whole, fraction) (((uint32_t)(uint16_t)(whole) << 16) | (uint16_t)(fraction))

/* an interrupt handler, as the vector table at $64..$7C holds them (a pointer: 4 bytes in both builds) */
typedef void (*irq_fn)(void);

typedef struct Shot Shot;

/* a shot dot at $42D5C: type < 0 = free */
struct Shot {
    int16_t x, y, dx, dy;
};

#endif
