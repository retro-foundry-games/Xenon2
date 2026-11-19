/* Xenon 2, SDL3 port: the machine around the game's C.
 *
 * The game runs on the main thread, as the program it is.  Where the original waits for an interrupt the C calls host_idle() (hw.h: HOST_IDLE): that is
 * the end of a video frame of emulated time.  Then the picture is made from the registers and the copper list, the sound of the frame is mixed, the
 * keys are read, the clock is waited for, and the vertical blank interrupt is raised: if the game enabled it, its handler (the C function in the
 * vector table) is called right here.
 *
 *   xenon2-sdl [--disks FOLDER] [--scale N] [--headless --frames N [--hash] [--dump-every N --dump FOLDER]]
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
static int headless, max_frames = -1, scale = 3, want_hash, dump_every, status_at = -1;
static const char *dump_folder, *export_path;
static unsigned export_vbl;
static long frames;
static uint32_t picture[DISPLAY_W * DISPLAY_H];
static int quitting;
static uint16_t dmacon_state, intena_state, intreq_state;
static int joy_up, joy_down, joy_left, joy_right, joy_fire;
static const char *joy_script;   /* --joy F:dirs,F:dirs (dirs joined by +, or none): the stick at frame F (tests) */

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

static void key(SDL_Scancode s, int down)
{
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
static uint32_t hash_of(const uint32_t *p, int n)
{
    uint32_t h = 2166136261u;

    for (int i = 0; i < n; i++)
        h = (h ^ p[i]) * 16777619u;
    return h;
}

static void dump_picture(long n)
{
    char path[1024];
    FILE *f;

    snprintf(path, sizeof path, "%s/xenon2-%06ld.ppm", dump_folder, n);
    f = fopen(path, "wb");
    if (!f)
        return;
    fprintf(f, "P6\n%d %d\n255\n", DISPLAY_W, DISPLAY_H);
    for (int i = 0; i < DISPLAY_W * DISPLAY_H; i++) {
        uint8_t px[3] = { (uint8_t)(picture[i] >> 16), (uint8_t)(picture[i] >> 8), (uint8_t)picture[i] };

        fwrite(px, 1, 3, f);
    }
    fclose(f);
}

static void end_of_frame(void)
{
    static Uint64 next;
    int16_t samples[2 * 882];

    display_render(picture);
    sound_mix(samples, 882);
    if (export_path && host_vbl_count() >= export_vbl) {
        host_export_ram(export_path);
        fprintf(stderr, "xenon2-sdl: memory exported at the game's frame count %u\n", host_vbl_count());
        export_path = 0;
        quitting = 1;
    }
    if (frames == status_at) {
        display_status();
        host_game_status();
    }
    if (want_hash && frames % 50 == 0)
        printf("frame %ld picture %08x\n", frames, hash_of(picture, DISPLAY_W * DISPLAY_H));
    if (dump_every && dump_folder && frames % dump_every == 0)
        dump_picture(frames);
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
    if (joy_script) {
        const char *p = joy_script;

        while (*p) {
            long f = strtol(p, (char **)&p, 10);
            char dirs[64] = "";
            int n = 0;

            if (*p == ':')
                p++;
            while (*p && *p != ',' && n < 63)
                dirs[n++] = *p++;
            dirs[n] = 0;
            if (*p == ',')
                p++;
            if (f == frames)
                host_input(strstr(dirs, "fire") != 0, strstr(dirs, "up") != 0, strstr(dirs, "down") != 0, strstr(dirs, "left") != 0, strstr(dirs, "right") != 0);
        }
    }
    joystick_registers();
    frames++;
    if (max_frames >= 0 && frames >= max_frames)
        quitting = 1;
    if (quitting) {
        if (!headless) {
            SDL_Quit();
        }
        exit(0);
    }
}

void host_idle(void)
{
    volatile struct Custom *c = custom;

    end_of_frame();
    /* the vertical blank: level 3, bit 5 of INTREQ; the handler is the C function in the vector table */
    intreq_state |= 0x20;
    c->intreqr = intreq_state;
    if ((intena_state & 0x4000) && (intena_state & 0x20))
        host_irq_dispatch(3);
}

/* ---- start ------------------------------------------------------------------------------------------------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    const char *disks = ".";

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--disks") && i + 1 < argc) disks = argv[++i];
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--headless")) headless = 1;
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) max_frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--hash")) want_hash = 1;
        else if (!strcmp(argv[i], "--export-ram") && i + 1 < argc) { export_path = argv[++i]; export_vbl = (unsigned)strtoul(strchr(export_path, ':') ? strchr(export_path, ':') + 1 : "0", 0, 0); if (strchr(export_path, ':')) *strchr(export_path, ':') = 0; }
        else if (!strcmp(argv[i], "--joy") && i + 1 < argc) joy_script = argv[++i];
        else if (!strcmp(argv[i], "--status") && i + 1 < argc) status_at = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--dump-every") && i + 1 < argc) dump_every = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--dump") && i + 1 < argc) dump_folder = argv[++i];
        else {
            fprintf(stderr, "usage: xenon2-sdl [--disks FOLDER] [--scale N] [--headless --frames N [--hash] [--status F] [--joy F:dirs,...] [--dump-every N --dump FOLDER]]\n");
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
        renderer = window ? SDL_CreateRenderer(window, 0) : 0;
        if (!renderer) {
            fprintf(stderr, "xenon2-sdl: window: %s\n", SDL_GetError());
            return 1;
        }
        SDL_SetRenderVSync(renderer, 1);
        SDL_SetRenderLogicalPresentation(renderer, DISPLAY_W, DISPLAY_H, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, DISPLAY_W, DISPLAY_H);
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, 0, 0);
        if (audio)
            SDL_ResumeAudioStreamDevice(audio);
    }
    host_game_start();
    for (;;)
        host_idle();   /* the program came back: keep the machine running */
}

