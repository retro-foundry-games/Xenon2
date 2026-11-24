#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "draw.h"
#include "host.h"
#include "presentation.h"

enum {
    WIDTH = SCREEN_PLANE_WORDS * 16,
    STARS = 48,
    POOL_SIZE = sizeof xenon2.obj_pool / sizeof xenon2.obj_pool[0],
    SHOT_COUNT = sizeof xenon2.shots / sizeof xenon2.shots[0]
};

struct Span {
    int16_t x, y;
    uint16_t mask, bits[4];
    uint8_t planes;
};

struct Drawing {
    const void *owner;
    unsigned generation, part;
    enum PresentationDraw kind;
    int x, y;
    size_t first, count;
    int dx, dy;
};

struct Frame {
    const uint8_t *screen;
    uint16_t tick;
    int camera, parallax;
    uint16_t map_offset, map_fine;
    uint64_t flip_time;
    uint16_t strip[SCREEN_ROWS][MAP_BLIT_WORDS];
    uint16_t terrain[SCREEN_ROWS][SCREEN_ROW_WORDS];
    uint16_t solid[SCREEN_ROWS][SCREEN_PLANE_WORDS];
    uint16_t coverage[SCREEN_ROWS][SCREEN_PLANE_WORDS];
    struct { int x, y; uint16_t mask; } stars[STARS];
    int star_count;
    struct Drawing *draws;
    size_t draw_count, draw_capacity;
    struct Span *spans;
    size_t span_count, span_capacity;
};

static struct Frame frames[3];
static struct Frame *capture = &frames[0], *current = &frames[1], *previous = &frames[2];
static uint16_t composite[SCREEN_ROWS][SCREEN_ROW_WORDS];
static unsigned generations[POOL_SIZE + SHOT_COUNT];
static const void *owner;
static unsigned generation, parts[PRESENT_OVERLAY + 1];
static int enabled, sealed, pinned;
int presentation_capturing;

static void *grow(void *data, size_t *capacity, size_t item_size)
{
    size_t count = *capacity ? *capacity * 2 : 256;
    void *buffer = realloc(data, count * item_size);

    if (!buffer) {
        fprintf(stderr, "xenon2-sdl: could not allocate a gameplay presentation snapshot\n");
        exit(1);
    }
    *capacity = count;
    return buffer;
}

static unsigned *record_generation(const void *record)
{
    uintptr_t object = (uintptr_t)record - (uintptr_t)xenon2.obj_pool;
    uintptr_t shot = (uintptr_t)record - (uintptr_t)xenon2.shots;

    if (object < sizeof xenon2.obj_pool)
        return &generations[object / sizeof xenon2.obj_pool[0]];
    if (shot < sizeof xenon2.shots)
        return &generations[POOL_SIZE + shot / sizeof xenon2.shots[0]];
    return NULL;
}

void presentation_reuse(const void *record)
{
    if (enabled) {
        unsigned *id = record_generation(record);
        if (id)
            ++*id;
    }
}

void presentation_enable(int on)
{
    enabled = on;
    presentation_reset();
}

void presentation_reset(void)
{
    current->screen = previous->screen = NULL;
    presentation_capturing = sealed = 0;
}

void presentation_discard_history(void)
{
    previous->screen = NULL;
}

void presentation_begin(void)
{
    if (!enabled)
        return;
    capture->screen = xenon2.draw_screen;
    capture->camera = (int16_t)xenon2.scroll_pos;
    capture->parallax = xenon2.background_scroll >> 16;
    capture->map_offset = xenon2.map_row_offset;
    capture->map_fine = xenon2.scroll_fine;
    capture->draw_count = capture->span_count = 0;
    capture->star_count = 0;
    memset(capture->coverage, 0, sizeof capture->coverage);
    owner = NULL;
    generation = 0;
    memset(parts, 0, sizeof parts);
    pinned = sealed = 0;
    presentation_capturing = 1;
}

/* The strip from $17D6 wraps independently of the tile map. Keep the tile
 * mask separate from its colors so an opaque black tile stays opaque. */
void presentation_background(void)
{
    const Cell *cells = (const Cell *)(xenon2.level_init_src + (int16_t)xenon2.map_row_offset);
    int fine = xenon2.scroll_fine;

    memcpy(capture->strip, xenon2.map_image, sizeof capture->strip);
    memcpy(capture->terrain, capture->screen, sizeof capture->terrain);
    for (int y = -fine; y < SCREEN_ROWS; y += 16, cells += MAP_COLS) {
        for (int col = 0; col < MAP_COLS; col++) {
            const struct MaskRow *mask = tile_mask_ptr(cells[col]);
            for (int r = y < 0 ? -y : 0; r < 16 && y + r < SCREEN_ROWS; r++)
                capture->solid[y + r][col] = mask[r].w[0];
        }
    }
}

void presentation_owner(const void *record)
{
    unsigned *id = record ? record_generation(record) : NULL;

    owner = record;
    generation = id ? *id : 0;
    memset(parts, 0, sizeof parts);
}

void presentation_pin(int fixed)
{
    pinned = fixed;
}

void presentation_draw(enum PresentationDraw kind, int x, int y)
{
    if (pinned)
        kind = PRESENT_OVERLAY;
    if (capture->draw_count == capture->draw_capacity)
        capture->draws = grow(capture->draws, &capture->draw_capacity, sizeof *capture->draws);
    capture->draws[capture->draw_count++] = (struct Drawing){
        .owner = owner, .generation = generation, .part = parts[kind]++,
        .kind = kind, .x = x, .y = y, .first = capture->span_count
    };
}

static void append_span(int x, int y, uint16_t mask, const uint16_t bits[4], unsigned planes)
{
    if (capture->span_count == capture->span_capacity)
        capture->spans = grow(capture->spans, &capture->span_capacity, sizeof *capture->spans);
    capture->spans[capture->span_count++] = (struct Span){
        x, y, mask, {bits[0], bits[1], bits[2], bits[3]}, planes
    };
    capture->draws[capture->draw_count - 1].count++;
}

void presentation_span(int x, int y, uint16_t mask, const uint16_t planes[4])
{
    if (!mask || y < 0 || y >= SCREEN_ROWS || x <= -16 || x >= WIDTH)
        return;
    /* The background tile pass precedes all recorded sprites and bosses. */
    if (capture->draw_count == 0) {
        capture->coverage[y][x / 16] = mask;
        return;
    }
    append_span(x, y, mask, planes, 15);
}

void presentation_word(uint16_t *destination, uint16_t mask, uint16_t bits)
{
    uintptr_t offset = (uintptr_t)destination - (uintptr_t)capture->screen;
    uint16_t values[4] = {0};

    if (!mask || offset >= sizeof capture->terrain)
        return;
    int plane = offset % SCREEN_ROW_BYTES / SCREEN_PLANE_BYTES;
    values[plane] = bits;
    append_span((offset % SCREEN_PLANE_BYTES) * 8, offset / SCREEN_ROW_BYTES, mask, values, 1u << plane);
}

void presentation_star(int index, int x, int y, uint16_t mask)
{
    capture->stars[index].x = x;
    capture->stars[index].y = y;
    capture->stars[index].mask = mask;
    capture->star_count = index + 1;
}

void presentation_seal(void)
{
    /* Some level hooks jump to a boss area after the background was drawn.
     * Resume with a matched map and object pass on the next gameplay frame. */
    if (presentation_capturing && (capture->camera != (int16_t)xenon2.scroll_pos ||
        capture->map_offset != xenon2.map_row_offset || capture->map_fine != xenon2.scroll_fine)) {
        presentation_reset();
        return;
    }
    sealed = presentation_capturing;
    presentation_capturing = 0;
}

static int wrapped_delta(int difference)
{
    if (difference > SCREEN_ROWS / 2)
        return difference - SCREEN_ROWS;
    if (difference < -SCREEN_ROWS / 2)
        return difference + SCREEN_ROWS;
    return difference;
}

void presentation_flip(const void *screen, uint16_t tick, uint64_t now)
{
    if (!enabled)
        return;
    if (!sealed || capture->screen != screen) {
        presentation_reset();
        return;
    }
    struct Frame *spare = previous;
    previous = current;
    current = capture;
    capture = spare;
    current->tick = tick;
    current->flip_time = now;
    sealed = 0;

    int camera_shift = current->camera - previous->camera;
    for (size_t i = 0; i < current->draw_count; i++) {
        struct Drawing *d = &current->draws[i];
        if (d->kind == PRESENT_OVERLAY)
            continue;
        d->dy = camera_shift;
        for (size_t j = 0; d->owner && previous->screen && j < previous->draw_count; j++) {
            const struct Drawing *old = &previous->draws[j];
            if (old->owner != d->owner || old->generation != d->generation || old->part != d->part ||
                old->kind != d->kind)
                continue;
            int dx = old->x - d->x, dy = old->y - d->y;
            if (abs(dx) < WIDTH && abs(dy) < SCREEN_ROWS) {
                d->dx = dx;
                d->dy = dy;
            }
            break;
        }
    }
}

static int wrap_row(int row)
{
    return (row + SCREEN_ROWS) % SCREEN_ROWS;
}

/* Take the adjacent frame's tiles at the leading edge of the scroll. The
 * parallax strip is cyclic, so it never needs the adjacent frame's pixels. */
static void scenery(int shift, int parallax, uint16_t solid[SCREEN_ROWS][SCREEN_PLANE_WORDS])
{
    for (int y = 0; y < SCREEN_ROWS; y++) {
        const struct Frame *map = current;
        int row = y - shift;
        if (row < 0 || row >= SCREEN_ROWS) {
            map = previous;
            row += current->camera - previous->camera;
        }
        const uint16_t *strip = current->strip[wrap_row(y + parallax)];
        for (int col = 0; col < SCREEN_PLANE_WORDS; col++) {
            uint16_t coverage = map->coverage[row][col];
            solid[y][col] = map->solid[row][col];
            for (int p = 0; p < 4; p++) {
                int word = p * SCREEN_PLANE_WORDS + col;
                uint16_t distant = p < 2 ? strip[word] : 0;
                composite[y][word] = (map->terrain[row][word] & coverage) | (distant & ~coverage);
            }
        }
    }
}

/* $233A only draws stars into index-zero pixels. Test the moving terrain here
 * so a star entering a hole or passing behind a tile follows the same rule. */
static void stars(double lag)
{
    for (int i = 0; i < current->star_count; i++) {
        int x = current->stars[i].x, y = current->stars[i].y;

        uint16_t mask = current->stars[i].mask;
        if (i < previous->star_count && x == previous->stars[i].x && mask == previous->stars[i].mask)
            y = wrap_row(y + (int)lround(wrapped_delta(previous->stars[i].y - y) * lag));
        unsigned col = (unsigned)x / 16;
        if (col >= SCREEN_PLANE_WORDS || y < 0 || y >= SCREEN_ROWS)
            continue;
        uint16_t *row = composite[y] + col;
        if ((row[0] | row[SCREEN_PLANE_WORDS] | row[2 * SCREEN_PLANE_WORDS] | row[3 * SCREEN_PLANE_WORDS]) & mask)
            continue;
        int group = i / 12;
        if (group & 1) row[0] |= mask;
        if (group & 2) row[SCREEN_PLANE_WORDS] |= mask;
        row[2 * SCREEN_PLANE_WORDS] |= mask;
    }
}

static void draw_span(const struct Span *span, int x, int y, const uint16_t *solid)
{
    int shift = x & 15, col = (x - shift) / 16;
    uint32_t mask = (uint32_t)span->mask << (16 - shift);

    for (int word = 0; word < 2; word++, col++) {
        int half = 16 - word * 16;
        uint16_t covered = mask >> half;
        if (!covered || col < 0 || col >= SCREEN_PLANE_WORDS)
            continue;
        if (solid)
            covered &= ~solid[col];
        for (int p = 0; p < 4; p++) {
            if (!(span->planes & (1u << p)))
                continue;
            uint16_t bits = ((uint32_t)span->bits[p] << (16 - shift)) >> half;
            uint16_t *dst = &composite[y][p * SCREEN_PLANE_WORDS + col];
            *dst = (*dst & ~covered) | (bits & covered);
        }
    }
}

const uint16_t *presentation_picture(uint64_t now)
{
    unsigned ticks = (uint16_t)(current->tick - previous->tick);
    if (!previous->screen || !current->screen || !ticks || ticks > 10 ||
        now - current->flip_time >= (uint64_t)ticks * HOST_VBLANK_NS)
        return NULL;
    int delta = current->camera - previous->camera;
    if (abs(delta) >= SCREEN_ROWS)
        return NULL;

    double lag = 1.0 - (double)(now - current->flip_time) / ((uint64_t)ticks * HOST_VBLANK_NS);
    int parallax = wrap_row(current->parallax + (int)lround(wrapped_delta(previous->parallax - current->parallax) * lag));
    uint16_t solid[SCREEN_ROWS][SCREEN_PLANE_WORDS];
    scenery((int)lround(delta * lag), parallax, solid);
    stars(lag);
    for (size_t i = 0; i < current->draw_count; i++) {
        const struct Drawing *d = &current->draws[i];
        int dx = (int)lround(d->dx * lag), dy = (int)lround(d->dy * lag);
        for (size_t j = d->first; j < d->first + d->count; j++) {
            const struct Span *span = &current->spans[j];
            int y = span->y + dy;
            if (y >= 0 && y < SCREEN_ROWS)
                draw_span(span, span->x + dx, y, d->kind == PRESENT_BEHIND_MAP ? solid[y] : NULL);
        }
    }
    return composite[0];
}

const uint8_t *presentation_screen(void)
{
    return current->screen;
}
