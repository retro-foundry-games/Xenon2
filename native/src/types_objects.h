#ifndef TYPES_OBJECTS_H
#define TYPES_OBJECTS_H
#include <stdint.h>

/* a spawn record of the level's event list (14 bytes): the scroll position at which it fires, and what to spawn */
struct SpawnRec {
    int16_t scroll;             /* 0: fires when the scroll passes it; negative ends the list */
    uint16_t w2;                /* 2: argument of the level's spawn routine */
    uint16_t count;             /* 4: how many copies */
    uint16_t index;             /* 6: 1-based index into the level's table of formation data */
    int16_t x;                  /* 8: 100 and above: x of a group; below: offset */
    int16_t y;                  /* a */
    uint16_t w0c;               /* c: becomes word $54 of the objects */
};

/* the definition of one object of a formation (24 bytes); a definition whose kind is 0 ends the table */
struct SpawnDef {
    uint16_t kind;              /* 0 */
    uint16_t w2;                /* 2: x offset behind the previous object */
    void (*update)(void);       /* 4 */
    void (*draw)(void);         /* 8 */
    void (*hit)(void);          /* c */
    const uint8_t *anim;        /* 10: animation header: picture (long), delay, script */
    uint16_t pad14;
    uint8_t to_flag35;          /* 16: becomes byte $35 of the object */
    uint8_t to_flag34;          /* 17: becomes byte $34 of the object */
};

/* the kinds of pieces an object breaks into (6 bytes) */
struct SpawnKind {
    uint16_t kind;
    const uint16_t *anim;       /* animation header */
};
#endif
