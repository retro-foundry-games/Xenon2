/* Xenon 2, SDL3 port: the machine around the game's C.
 *
 * The game runs on the main thread, as the program it is.  Where the original waits for an interrupt the C calls host_idle() (hw.h: HOST_IDLE): that is
 * the end of a video frame of emulated time.  Then the picture is made from the registers and the copper list, the sound of the frame is mixed, the
 * keys are read, the clock is waited for, and the vertical blank interrupt is raised: if the game enabled it, its handler (the C function in the
 * vector table) is called right here.
 *
 *   xenon2-sdl [--disks FOLDER] [--scale N] [--renderer NAME]
 */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "amiga/hw.h"   /* (this file is built without -fpack-struct: SDL's headers need that; the game's memory is in guest.c) */
#include "host.h"


static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static SDL_AudioStream *audio;
static const char *renderer_name;   /* --renderer NAME: software, opengl, vulkan, ... (default: SDL chooses) */
static int headless, scale = 3;
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
    case 0x96: {
        uint16_t before = dmacon_state;

        dmacon_state = apply(dmacon_state, value) & 0x07ff;
        c->dmaconr = dmacon_state;   /* (the blitter is never busy: its draws are done when the function returns) */
        sound_dma_changed(before, dmacon_state);
        break;
    }
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

/* ---- the frame ------------------------------------------------------------------------------------------------------------------------------------------------------ */
/* the end of a video frame of emulated time: the picture, the sound, the keys, the clock (the dev build calls this and host_vblank itself) */
void host_frame_end(void)
{
    static Uint64 next;
    int16_t samples[2 * 882];

    display_render(picture);
    for (int i = 0; i < DISPLAY_W * DISPLAY_H; i++)
        picture[i] |= 0xff000000u;   /* opaque */
    sound_mix(samples, 882);
    if (!headless) {
        poll_events();
        SDL_UpdateTexture(texture, 0, picture, DISPLAY_W * 4);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, 0, 0);
        SDL_RenderPresent(renderer);
        if (audio)
            SDL_PutAudioStreamData(audio, samples, sizeof samples);
        Uint64 now = SDL_GetTicksNS();

        if (next == 0)
            next = now;
        next += 20000000;
        if (next > now)
            SDL_DelayNS(next - now);
        else if (now - next > 200000000)
            next = now;
    }
    joystick_registers();
    deliver_keys();
    frames++;
    if (quitting) {
        if (!headless) {
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

/* the options, the machine and the window; 0 when the game can start, else the exit code */
int host_init(int argc, char **argv)
{
    const char *disks = ".";

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--disks") && i + 1 < argc) disks = argv[++i];
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--renderer") && i + 1 < argc) renderer_name = argv[++i];
        else {
            fprintf(stderr, "usage: xenon2-sdl [--disks FOLDER] [--scale N] [--renderer NAME]\n");
            return 2;
        }
    }
    if (files_open(disks) || guest_load_game_image())
        return 1;
    sound_init();
    joystick_registers();
    if (!headless) {
        SDL_AudioSpec spec = { SDL_AUDIO_S16, 2, 44100 };

        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
            fprintf(stderr, "xenon2-sdl: SDL_Init: %s\n", SDL_GetError());
            return 1;
        }
        window = SDL_CreateWindow("Xenon 2", DISPLAY_W * scale, DISPLAY_H * scale, 0);
        renderer = window ? SDL_CreateRenderer(window, renderer_name) : 0;
        if (renderer)
            fprintf(stderr, "xenon2-sdl: renderer %s, video driver %s\n", SDL_GetRendererName(renderer), SDL_GetCurrentVideoDriver());
        if (!renderer) {
            fprintf(stderr, "xenon2-sdl: window: %s\n", SDL_GetError());
            return 1;
        }
        SDL_SetRenderVSync(renderer, 1);
        SDL_SetRenderLogicalPresentation(renderer, DISPLAY_W, DISPLAY_H, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, DISPLAY_W, DISPLAY_H);
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
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

