#ifndef TYPES_OBJECTS_H
#define TYPES_OBJECTS_H
#include <stdint.h>

/* the definition of one object of a formation (24 bytes); a definition whose kind is 0 ends the table */
struct SpawnDef {
    uint16_t kind;              /* 0 */
    uint16_t score;             /* 2: the score of the object, and its wait behind the previous one */
    void (*update)(void);       /* 4 */
    void (*draw)(void);         /* 8 */
    void (*hit)(void);          /* c */
    const uint8_t *anim;        /* 10: animation header: picture (long), delay, script */
    uint16_t pad14;
    uint8_t to_big;          /* 16: becomes byte $35 of the object */
    uint8_t to_is_part;          /* 17: becomes byte $34 of the object */
};

/* the kinds of pieces an object breaks into (6 bytes) */
struct SpawnKind {
    uint16_t kind;
    const uint16_t *anim;       /* animation header */
};
#endif
