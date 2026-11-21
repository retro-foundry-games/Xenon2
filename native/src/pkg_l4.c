/* pkg_l4: level 4's own code (the blob at $54E00 while level 4 is loaded), written like pkg_l3.c; a function has `level4 $ADDR` for its address. */
#include "pkg_d.h"
#include "pkg_i.h"
#include "game_all.h"

#define FN(f) ((obj_fn)(f))
#define W(a) sh_r16(a)
#define B(a) sh_r8(a)

static Object *alloc_in(Object *list)
{
    Object *o = obj_free_pop(), *tail = list->prev;

    list->prev = o;
    o->prev = tail;
    o->next = list;
    tail->next = o;
    return o;
}

static void kill_thunk(Object *o)
{
    o->type = 4;
    o->fn_update = (obj_fn)free_object;
    o->fn_draw = (obj_fn)obj_nop_thunk;
    o->fn_hit = (void *)obj_nop_thunk;
    o->flag34 = 0;
    o->flag60 = 0;
}

static void copy4(uint8_t *dst, const uint8_t *src)
{
    int i;

    for (i = 0; i < 4; i++)
        dst[i] = src[i];
}

/* the two longs of a table entry go to the long at the cell and the one a row below it */
static void put_cells(uint8_t *cell, uint32_t table)
{
    copy4(cell, (const uint8_t *)GPTR(table));
    copy4(cell + 0x28, (const uint8_t *)GPTR(table + 4));
}

void l4_tower_update(Object *a0);
void l4_tower_hit(Object *a0, Object *a1, uint16_t d0);
void l4_tower_draw(Object *a0);

/* level4 $054E10, $057272: level events by d0 (a1 = the record that fired): 0 the wave of $326C, 1 to 6 formations */
void l4_event(uint16_t d0, const uint8_t *a1)
{
    static const uint32_t tables[7] = { 0, 0x54e8c, 0x54ed8, 0x54ef2, 0x54ebe, 0x54f0c, 0x54f26 };

    if (d0 == 0)
        spawn_wave_326c(0, a1);   /* $E5A */
    else if (d0 < 7)
        spawn_formation(0, a1, (const uint8_t *)GPTR(tables[d0]));
}

/* level4 $054E18, $05585E: the hooks by d0 */
void l4_hook(uint16_t d0, const uint8_t *a1, uint16_t d2)
{
    switch (d0) {
    case 0:
        note_spawn_record(a1, d2);   /* $EA2 */
        break;
    case 1:
        l4_spawn_tower(a1);
        break;
    }
}

/* level4 $054E20, $0572D8: the data is copied once (backwards: the copy lies above its source) */
void l4_init(void)
{
    int i;

    if (level_init_src == (uint8_t *)GPTR(0x7d080)) {
        for (i = 11999; i >= 0; i--)
            ((uint8_t *)GPTR(0x7d120))[i] = ((const uint8_t *)GPTR(0x7d080))[i];
        level_init_src = (uint8_t *)GPTR(0x7d120);
    }
    v_0ce8 = 0x880;
    v_0cea = 0x11ff;
    sh_w8(0xd97, 0);
    sh_w8(0xd98, 0);
    sh_w16(0x558a2, W(0x54e6e));
    sh_w16(0x558b4, W(0x54e6e));
    sh_w16(0x55890, W(0x54e70));
    sh_w8(0x5628c, B(0x54e77));
    sh_w8(0x562a0, B(0x54e77));
    sh_w8(0x562b4, B(0x54e77));
    sh_w8(0x562c8, B(0x54e77));
    sh_w8(0x56278, B(0x54e79));
    sh_w8(0x5619c, B(0x54e7b));
    v_0424 = (uint16_t)(v_0e02 * 6);
    v_0426 = (uint16_t)(v_0e02 * 8);
}

/* level4 $057064: the tower of tiles: it fires four ways by its phase v28 */
void l4_tower_update(Object *a0)
{
    uint32_t cells;
    uint16_t d0;

    if ((int16_t)(v_0cea + 0xd0) < a0->y) {
        kill_thunk(a0);
        return;
    }
    a0->hit[0] = (int16_t)(a0->x + 2);
    a0->hit[2] = (int16_t)(a0->x + 0x1e);
    d0 = (uint16_t)(a0->y - v_0cd0 + 2);
    a0->hit[1] = (int16_t)d0;
    a0->hit[3] = (int16_t)(d0 + 0x1c);
    if (a0->v28 == 0) {
        uint16_t sum = (uint16_t)(a0->v5e_b[0] + B(0x54e69));

        a0->v5e_b[0] = (uint8_t)sum;
        if (sum > 0xff) {
            a0->v5e_b[0] = (uint8_t)(random_next() & 0x3f);
            a0->v28 = 1;
        }
        return;
    }
    cells = (a0->type == 0xd0 ? 0x57160 : 0x57190) + ((uint16_t)a0->v28 << 3);
    put_cells(a0->v46, cells);
    a0->v28++;
    if (a0->v28 == 4) {
        int16_t x = (int16_t)(a0->x + 0x12), y = (int16_t)(a0->y - v_0cd0 + 0xe);
        uint16_t angle = 2;
        uint32_t table = sh_r32(0x77b70 + 4 * (uint32_t)W(0x54e6a));

        if (a0->type != 0xd4) {
            angle = 6;
            x = (int16_t)(x - 8);
        }
        spawn_enemy_shot((uint16_t)x, (uint16_t)y, angle, W(0x54e6c), GPTR(sh_r32(table)));
        return;
    }
    if (a0->v28 == 6)
        a0->v28 = 0;
}

/* level4 $0571C0: the hit of a tower: it flashes; when it is shot to pieces it bursts and the cells are the broken ones */
void l4_tower_hit(Object *a0, Object *a1, uint16_t d0)
{
    uint16_t old = (uint16_t)a1->energy;

    a1->energy = (int16_t)(old - d0);
    if (old > d0) {
        a1->fn_draw = FN(l4_tower_draw);
        return;
    }
    spawn_effect((int16_t)(a1->x + 0x10), (int16_t)(a1->y - v_0cd0 + 0x10), v_0f64, draw_bob_masked);
    put_cells(a1->v46, a1->type == 0xd0 ? 0x57236 : 0x5723e);
    v_0c92 += 0x190;
    kill_thunk(a1);
}

/* level4 $057246: the flash of a tower, one frame */
void l4_tower_draw(Object *a0)
{
    a0->fn_draw = (obj_fn)obj_nop_thunk;
    draw_tile_grid_flash((const uint8_t *)GPTR(a0->type == 0xd0 ? 0x57160 : 0x57190), 1, 1, (int16_t)(a0->y - v_0cd0), (uint16_t)((uint16_t)a0->x >> 1));
}

/* level4 $05700A: a marker makes a tower; the record's word at +$C says which of the two */
void l4_spawn_tower(const uint8_t *a1)
{
    int flag = rd16(a1 + 0xc) != 0;
    Object *a0 = spawn_marker_enemy(flag ? 0xd0 : 0xd4, a1, (void (*)(void))l4_tower_update);

    a0->fn_draw = (obj_fn)obj_nop_thunk;
    a0->fn_hit = (void *)l4_tower_hit;
    a0->energy = (int16_t)W(0x54e66);
    a0->hit[0] = 0x3e8;
    a0->hit[2] = 0x3e8;
    put_cells(a0->v46, flag ? 0x57160 : 0x57190);
}

/* level4 $055090: the scroll: the floor of the scroll ends at $DF0 and between $AE0 and $E50 an enemy comes every 16 pixels */
void l4_scroll(void)
{
    if ((int16_t)v_0cea >= 0xdf0 && (int16_t)v_0cd0 <= 0xdf0)
        v_0cea = 0xdf0;
    if ((v_0cdc & 0xf) != 0 || (int16_t)v_0cd0 > 0xe50 || (int16_t)v_0cd0 < 0xae0)
        return;
    {
        Object *a0 = alloc_in(LIST_C);
        uint16_t d0 = (random_next() & 1) ? 0xc : 0;
        uint32_t a2;

        if (sh_r8(0xcdd) & 0x20)
            d0 += 0x18;
        a2 = 0x5515a + d0;
        a0->type = (int16_t)W(a2);
        a0->x = (int16_t)W(a2 + 2);
        a0->fn_update = (obj_fn)GFN(sh_r32(a2 + 4));
        a0->fn_hit = (void *)hit_enemy;
        a0->fn_draw = draw_bob_masked;
        anim_header_set(a0, (const uint8_t *)GPTR(sh_r32(a2 + 8)));
        a0->v5e_b[0] = (uint8_t)random_next();
        a0->y = -0x20;
        a0->v28 = 0;
        a0->energy = (int16_t)W(0x54e42);
        a0->flag35 = 0xff;
        a0->v44 = 0;
        a0->v42 = 0x12c;
        bbox_from_frame(a0);
    }
}
