/* Xenon 2, SDL3 port: the machine around the game's C.
 *
 * The game runs on the main thread, as the program it is.  Where the original waits for an interrupt the C calls host_idle() (hw.h: HOST_IDLE): that is
 * the end of a video frame of emulated time.  Then the picture is made from the registers and the copper list, the sound of the frame is mixed, the
 * keys are read, the clock is waited for, and the vertical blank interrupt is raised: if the game enabled it, its handler (the C function in the
 * vector table) is called right here.
 *
 *   xenon2-sdl [--disks FOLDER] [--scale N] [--renderer NAME] [--interpolate]
 */
#include <SDL3/SDL.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "amiga/hw.h"   /* (this file is built without -fpack-struct: SDL's headers need that; the game's memory is in guest.c) */
#include "host.h"
#include "pixel_scale.h"
#include "presentation.h"


static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static SDL_AudioStream *audio;
static PixelScale pixel_scale;
static const char *renderer_name;   /* --renderer NAME: software, opengl, vulkan, ... (default: SDL chooses) */
static int headless, interpolate, scale = 3, start_level = 1;
static long frames;
static uint32_t picture[DISPLAY_W * DISPLAY_H];
static int quitting;
static uint16_t dmacon_state, intena_state, intreq_state;
static int joy_up, joy_down, joy_left, joy_right, joy_fire;

/* ---- the registers with set/clear semantics (hw.h SETCLR_WRITE) ----------------------------------------------------------------------------------------------- */
static uint16_t apply(uint16_t state, uint16_t value)
{
    return (value & 0x8000) ? (state | (value & 0x7fff)) : (state & ~value);
}

void host_setclr_write(unsigned offset, uint16_t value)
{
    volatile struct Custom *c = custom;

    switch (offset) {
    case 0x96:
        dmacon_state = apply(dmacon_state, value) & 0x07ff;
        c->dmaconr = dmacon_state;
        break;
    case 0x9a:
        intena_state = apply(intena_state, value) & 0x7fff;
        c->intenar = intena_state;
        break;
    case 0x9c:
        intreq_state = apply(intreq_state, value) & 0x7fff;
        c->intreqr = intreq_state;
        break;
    case 0x9e:
        c->adkconr = apply(c->adkconr, value) & 0x7fff;
        break;
    default:
        break;
    }
}

/* ---- input ------------------------------------------------------------------------------------------------------------------------------------------------------------ */
static void joystick_registers(void)
{
    volatile struct Custom *c = custom;
    uint16_t dat = 0;

    if (joy_right) dat |= 2;
    if (joy_left) dat |= 0x200;
    if ((dat & 2 ? 1 : 0) ^ joy_down) dat |= 1;
    if ((dat & 0x200 ? 1 : 0) ^ joy_up) dat |= 0x100;
    /* the beam: the game runs in the vertical blank period (below line 258); no beam is emulated */
    c->vposr = 0x8001;     /* long frame, line number bit 8 set */
    c->vhposr = 0x4000;    /* line 256 + 0x40 */
    c->joy0dat = dat;
    c->joy1dat = dat;
    CIAA_PRA = (uint8_t)(0xff & ~(joy_fire ? 0xc0 : 0));
}

/* ---- the keyboard: a key goes to the CIA A serial data register in the Amiga's encoding and raises the level 2 interrupt ($68) ----------------------------------------------------- */
static int amiga_raw(SDL_Scancode s)
{
    static const char rows[3][11] = { "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM" };
    static const int first[3] = { 0x10, 0x20, 0x31 };

    if (s >= SDL_SCANCODE_A && s <= SDL_SCANCODE_Z) {
        char letter = (char)('A' + (s - SDL_SCANCODE_A));

        for (int r = 0; r < 3; r++)
            for (int i = 0; rows[r][i]; i++)
                if (rows[r][i] == letter)
                    return first[r] + i;
    }
    if (s >= SDL_SCANCODE_1 && s <= SDL_SCANCODE_9) return 0x01 + (s - SDL_SCANCODE_1);
    if (s == SDL_SCANCODE_0) return 0x0a;
    if (s >= SDL_SCANCODE_F1 && s <= SDL_SCANCODE_F10) return 0x50 + (s - SDL_SCANCODE_F1);
    switch (s) {
    case SDL_SCANCODE_TAB: return 0x42;
    case SDL_SCANCODE_BACKSPACE: return 0x41;
    case SDL_SCANCODE_DELETE: return 0x46;
    case SDL_SCANCODE_P: return 0x19;
    default: return -1;   /* (the arrows, Ctrl, Space and Return are the joystick; Esc quits) */
    }
}

static int key_queue[32], key_queue_n;

void host_state(host_state_visit visit, void *arg)
{
    presentation_reset();
    guest_state(visit, arg);
    sound_state(visit, arg);
    visit(&frames, sizeof frames, arg);
    visit(&dmacon_state, sizeof dmacon_state, arg);
    visit(&intena_state, sizeof intena_state, arg);
    visit(&intreq_state, sizeof intreq_state, arg);
    visit(&joy_up, sizeof joy_up, arg);
    visit(&joy_down, sizeof joy_down, arg);
    visit(&joy_left, sizeof joy_left, arg);
    visit(&joy_right, sizeof joy_right, arg);
    visit(&joy_fire, sizeof joy_fire, arg);
    visit(key_queue, sizeof key_queue, arg);
    visit(&key_queue_n, sizeof key_queue_n, arg);
}

static void deliver_keys(void)
{
    for (int i = 0; i < key_queue_n; i++) {
        int code = key_queue[i];   /* bit 7: released */
        uint8_t sdr = (uint8_t)~((code << 1) | (code >> 7));

        CIAA_SDR = sdr;
        CIAA_ICR |= 0x08;   /* CIA A interrupt control: the serial port (a key) is the source */
        intreq_state |= 0x08;
        custom->intreqr = intreq_state;
        if ((intena_state & 0x4000) && (intena_state & 0x08))
            host_irq_dispatch(2);
    }
    key_queue_n = 0;
}

static void key(SDL_Scancode s, int down)
{
    int raw = amiga_raw(s);

    if (raw >= 0 && key_queue_n < 32)
        key_queue[key_queue_n++] = raw | (down ? 0 : 0x80);
    switch (s) {
    case SDL_SCANCODE_UP: joy_up = down; break;
    case SDL_SCANCODE_DOWN: joy_down = down; break;
    case SDL_SCANCODE_LEFT: joy_left = down; break;
    case SDL_SCANCODE_RIGHT: joy_right = down; break;
    case SDL_SCANCODE_SPACE:
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL:
    case SDL_SCANCODE_RETURN: joy_fire = down; break;
    case SDL_SCANCODE_ESCAPE:
    case SDL_SCANCODE_F10: if (down) quitting = 1; break;
    default: break;
    }
}

void host_input(int fire, int up, int down, int left, int right)
{
    joy_fire = fire; joy_up = up; joy_down = down; joy_left = left; joy_right = right;
}

void host_quit(void) { quitting = 1; }

void host_unimplemented(uint32_t address)
{
    static uint32_t seen[64];
    static int n;

    for (int i = 0; i < n; i++)
        if (seen[i] == address)
            return;
    if (n < 64)
        seen[n++] = address;
    fprintf(stderr, "xenon2-sdl: the original's code at $%06x has no C (it never ran when the C was made)\n", address);
}

static void poll_events(void)
{
    SDL_Event e;

    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT)
            quitting = 1;
        else if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP)
            key(e.key.scancode, e.type == SDL_EVENT_KEY_DOWN);
    }
}

#ifdef __EMSCRIPTEN__
/* The shell snaps the canvas to the physical pixel grid. SDL renders directly
 * into that many pixels, including when the browser is zoomed. */
EM_JS(int, browser_pixel_width, (void), {
    const visible = window.visualViewport;
    const density = window.devicePixelRatio * (visible ? visible.scale : 1);
    if (density !== Module['xenonCanvasDensity']) Module['xenonFitCanvas']();
    return Module['xenonCanvasWidth'];
});
EM_JS(int, browser_pixel_height, (void), { return Module['xenonCanvasHeight']; });

EM_ASYNC_JS(void, browser_refresh, (void), {
    await new Promise(resolve => requestAnimationFrame(resolve));
});

static bool resize_browser_window(void)
{
    int width = browser_pixel_width(), height = browser_pixel_height();
    int current_width, current_height;

    return SDL_GetWindowSize(window, &current_width, &current_height) &&
        ((width == current_width && height == current_height) || SDL_SetWindowSize(window, width, height));
}
#endif

static bool present(Uint64 now)
{
#ifdef __EMSCRIPTEN__
    if (!resize_browser_window())
        return false;
#endif
    if (interpolate)
        display_render_interpolated(picture, now);
    else
        display_render(picture);
    for (int i = 0; i < DISPLAY_W * DISPLAY_H; i++)
        picture[i] |= 0xff000000u;
    return SDL_UpdateTexture(texture, NULL, picture, DISPLAY_W * 4) &&
        pixel_scale_draw(&pixel_scale, texture) && SDL_RenderPresent(renderer);
}

static void present_or_exit(Uint64 now)
{
    if (!present(now)) {
        fprintf(stderr, "xenon2-sdl: could not present the frame: %s\n", SDL_GetError());
        exit(1);
    }
}

/* The clock advances in 20 ms steps. While the game waits for its next
 * interrupt, smooth mode uses the available presentation time. */
enum { STALL_NS = 200000000 };

static void wait_for_vblank(void)
{
    static Uint64 next;
    Uint64 now = SDL_GetTicksNS();
    if (!next || now > next + STALL_NS) {
        next = now;
        presentation_discard_history();
    }
    next += HOST_VBLANK_NS;
#ifndef __EMSCRIPTEN__
    if (!interpolate) {
        poll_events();
        present_or_exit(now);
        now = SDL_GetTicksNS();
        if (next > now)
            SDL_DelayNS(next - now);
        return;
    }
#endif
    while (now < next && !quitting) {
#ifdef __EMSCRIPTEN__
        browser_refresh();
        now = SDL_GetTicksNS();
        if (now > next + STALL_NS) {
            next = now;
            presentation_discard_history();
        }
#endif
        poll_events();
        present_or_exit(now);
        now = SDL_GetTicksNS();
    }
}

void host_screen_flip(const void *screen, uint16_t tick)
{
    if (interpolate)
        presentation_flip(screen, tick, SDL_GetTicksNS());
}

/* One machine frame. Audio, joystick registers and IRQs stay on this clock. */
void host_frame_end(void)
{
    int16_t samples[2 * 882];

    sound_mix(samples, 882);
    if (!headless) {
        if (audio && SDL_GetAudioStreamQueued(audio) < (int)(sizeof samples * 10))
            SDL_PutAudioStreamData(audio, samples, sizeof samples);
        wait_for_vblank();
    }
    joystick_registers();
    deliver_keys();
    frames++;
    if (quitting) {
        if (!headless) {
            pixel_scale_close(&pixel_scale);
            SDL_Quit();
        }
        exit(0);
    }
}

/* the vertical blank: level 3, bit 5 of INTREQ; the handler is the C function in the vector table */
void host_vblank(void)
{
    volatile struct Custom *c = custom;

    intreq_state |= 0x20;
    c->intreqr = intreq_state;
    if ((intena_state & 0x4000) && (intena_state & 0x20))
        host_irq_dispatch(3);
}

/* what the game's C calls where the original waits for an interrupt (hw.h HOST_IDLE).  Weak: the dev build replaces it. */
__attribute__((weak)) void host_idle(void)
{
    host_frame_end();
    host_vblank();
}

const uint32_t *host_picture(void)
{
    display_render(picture);
    for (int i = 0; i < DISPLAY_W * DISPLAY_H; i++)
        picture[i] |= 0xff000000u;
    return picture;
}

long host_frame_count(void) { return frames; }

void host_key_event(int raw, int down)
{
    if (key_queue_n < 32)
        key_queue[key_queue_n++] = raw | (down ? 0 : 0x80);
}

/* ---- start ------------------------------------------------------------------------------------------------------------------------------------------------------------ */
/* no window, no sound, no clock: for a host that drives the machine itself (the dev build); before host_init */
void host_set_headless(int on) { headless = on; }
int host_start_level(void) { return start_level; }

/* the options, the machine and the window; 0 when the game can start, else the exit code */
int host_init(int argc, char **argv)
{
    const char *disks = ".";

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--disks") && i + 1 < argc) disks = argv[++i];
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--renderer") && i + 1 < argc) renderer_name = argv[++i];
        else if (!strcmp(argv[i], "--level") && i + 1 < argc) start_level = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--interpolate")) interpolate = 1;
        else {
            fprintf(stderr, "usage: xenon2-sdl [--disks FOLDER] [--scale N] [--level N] [--renderer NAME] [--interpolate]\n");
            return 2;
        }
    }
    if (start_level < 1 || start_level > 5) {
        fprintf(stderr, "xenon2-sdl: --level takes 1 to 5\n");
        return 2;
    }
    if (files_open(disks) || guest_load_game_image())
        return 1;
    sound_init();
    joystick_registers();
    presentation_enable(interpolate && !headless);
    if (!headless) {
        SDL_AudioSpec spec = { SDL_AUDIO_S16, 2, 44100 };

        /* Wayland first, then X11, unless SDL_VIDEO_DRIVER says which */
        char failure[256] = "";
#ifdef __EMSCRIPTEN__
        const char *drivers[] = { 0, 0, 0 };   /* the browser's */
#else
        const char *drivers[] = { getenv("SDL_VIDEO_DRIVER") ? 0 : "wayland", getenv("SDL_VIDEO_DRIVER") ? 0 : "x11", 0 };
#endif

        for (int i = 0; !renderer && (i == 0 || drivers[i - 1]); i++) {
            if (drivers[i])
                SDL_SetHint(SDL_HINT_VIDEO_DRIVER, drivers[i]);
            if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
                snprintf(failure, sizeof failure, "SDL_Init: %s", SDL_GetError());
                continue;
            }
#ifdef __EMSCRIPTEN__
            window = SDL_CreateWindow("Xenon 2", browser_pixel_width(), browser_pixel_height(), 0);
#else
            window = SDL_CreateWindow("Xenon 2", DISPLAY_W * scale, DISPLAY_H * scale,
                                      SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
#endif
            renderer = window ? SDL_CreateRenderer(window, renderer_name) : 0;
            if (!renderer) {
                snprintf(failure, sizeof failure, "%s: %s", drivers[i] ? drivers[i] : "window", SDL_GetError());
                if (window)
                    SDL_DestroyWindow(window);
                window = 0;
                SDL_Quit();
            }
        }
        if (!renderer) {
            fprintf(stderr, "xenon2-sdl: %s\n", failure);
            return 1;
        }
#ifdef __EMSCRIPTEN__
        SDL_SetRenderVSync(renderer, 0);
#else
        SDL_SetRenderVSync(renderer, interpolate ? 0 : 1);
#endif
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, DISPLAY_W, DISPLAY_H);
        if (!texture || !SDL_SetRenderLogicalPresentation(renderer, DISPLAY_W, DISPLAY_H, SDL_LOGICAL_PRESENTATION_LETTERBOX) ||
            !SDL_UpdateTexture(texture, NULL, picture, DISPLAY_W * 4) ||
            !pixel_scale_open(&pixel_scale, renderer, texture, DISPLAY_W, DISPLAY_H)) {
            fprintf(stderr, "xenon2-sdl: pixel upscaling setup: %s\n", SDL_GetError());
            SDL_Quit();
            return 1;
        }
        audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, 0, 0);
        if (audio)
            SDL_ResumeAudioStreamDevice(audio);
    }
    return 0;
}

__attribute__((weak)) int main(int argc, char **argv)
{
    int r = host_init(argc, argv);

    if (r)
        return r == -1 ? 1 : r;
    host_game_start();
    for (;;)
        host_idle();   /* the program came back: keep the machine running */
}

