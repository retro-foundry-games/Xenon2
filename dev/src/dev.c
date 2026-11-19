/* The dev build of the port: the game runs on a stack of its own (a coroutine that yields at every video frame, in host_idle), and a client drives it over a
 * control socket: steps it frame by frame, runs it to a function of the game by the address the function has in the original (as ami.hook does in ami9000),
 * reads the memory in the original's byte order and the picture, sends it keys and the stick.  The lockstep with the original (host/lockstep.lua) is a client.
 *
 *   xenon2-sdl-dev --control PATH [port options...]     serve a unix socket at PATH (one client at a time, then the next);  --control -  serves stdin/stdout (a peer
 *                                                       the emulator starts).  The options are the port's (--disks, ...); --headless unless --window is given.
 *   dev/ctl.py PATH COMMAND...                          a client
 *
 * Commands, one a line (numbers: decimal or 0x hex); the reply is a line, then bytes where it says so:
 *   joy UP DOWN LEFT RIGHT FIRE   the stick (0/1 each)                     -> ok
 *   key RAW DOWN                  an Amiga raw key code, DOWN 0/1            -> ok
 *   step                          one video frame                           -> frame TICKS 320 256 NAME -, then 320*256*3 bytes RGB
 *   run N                         N frames                                  -> ok FRAME
 *   image                         the picture as it is now                  -> as step
 *   sync ADDR [N] [MAX]           run until the function with the original address ADDR has been entered N times (default 1), the game standing inside it;
 *                                 at most MAX frames (default 20000)        -> synced FRAME COUNT, or unsynced FRAME
 *   ram                           the memory                                -> ram 524288, then the bytes (the original's byte order: named members
 *                                                                              converted, raw buffers as they are)
 *   peek8|peek16|peek32 ADDR      from the same                             -> value N (decimal)
 *   loadram LEN                   LEN (524288) bytes follow: the original's memory, put into the port before it starts   -> loaded
 *   picture_hash                  FNV-1a of the picture                     -> hash HEX
 *   trace N                       print (on stderr) the original address of the next N functions the game enters   -> ok
 *   disk N                        put disk 1 or 2 in the drive              -> ok
 *   frame                         the frames so far                         -> frame N
 *   quit                                                                  -> ok, and the process ends (a client that only disconnects leaves it serving)
 * Stopping inside a function needs the game's C built with -finstrument-functions (dev/Makefile does). */
#define _GNU_SOURCE
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ucontext.h>
#include <unistd.h>

#include "host.h"

void host_idle(void);   /* the game calls it (hw.h HOST_IDLE) */
extern unsigned guest_code_entries(const void *function, uint32_t *out, unsigned max);
extern void *guest_code(uint32_t address);

static ucontext_t ctrl_ctx, game_ctx;
static int game_started, in_game;
static uint8_t cache[0x80000];
static int cache_valid;

static void *stop_function;
static long stop_count, stop_entries;
static int stopped_at_hook, suspended_at_hook;   /* the game stands inside the function, not at the end of a frame */

/* ---- the game's context --------------------------------------------------------------------------------------------------------------------------------------- */
static void game_main(void)
{
    host_game_start();
    for (;;)
        host_idle();
}

/* the game waits for the next vertical blank: back to the script (the frame's end is done there), and the interrupt comes when it is resumed */
void host_idle(void)
{
    suspended_at_hook = 0;
    swapcontext(&game_ctx, &ctrl_ctx);
    cache_valid = 0;
    host_vblank();
}

/* run the game until it yields (host_idle or a stop at a hook) */
static void resume(void)
{
    cache_valid = 0;
    in_game = 1;
    swapcontext(&ctrl_ctx, &game_ctx);
    in_game = 0;
}

static void start_game(void)
{
    static char stack[1 << 20];

    getcontext(&game_ctx);
    game_ctx.uc_stack.ss_sp = stack;
    game_ctx.uc_stack.ss_size = sizeof stack;
    game_ctx.uc_link = 0;
    makecontext(&game_ctx, game_main, 0);
    game_started = 1;
    resume();   /* to the first host_idle */
}

/* ---- -finstrument-functions calls this on entry to every function of the game ------------------------------------------------------------------------------------ */
void __cyg_profile_func_enter(void *function, void *site) __attribute__((no_instrument_function));
void __cyg_profile_func_exit(void *function, void *site) __attribute__((no_instrument_function));

static long trace_left;

void __cyg_profile_func_enter(void *function, void *site)
{
    (void)site;
    if (trace_left > 0) {
        uint32_t a[4];

        trace_left--;
        if (guest_code_entries(function, a, 4))
            fprintf(stderr, "call $%06x\n", a[0]);
        else
            fprintf(stderr, "call %p (no original address)\n", function);
    }
    if (stop_function == function && in_game && ++stop_entries >= stop_count) {
        stopped_at_hook = 1;
        suspended_at_hook = 1;
        swapcontext(&game_ctx, &ctrl_ctx);   /* the client sees the game standing inside the function */
        cache_valid = 0;
    }
}

void __cyg_profile_func_exit(void *function, void *site)
{
    (void)function;
    (void)site;
}

/* ---- the control connection ---------------------------------------------------------------------------------------------------------------------------------------- */
static int fd_in, fd_out;
static uint8_t inbuf[1 << 16];
static size_t in_pos, in_len;

static int fill(void)
{
    ssize_t n;

    do n = read(fd_in, inbuf, sizeof inbuf); while (n < 0 && errno == EINTR);
    if (n <= 0)
        return 0;
    in_pos = 0;
    in_len = (size_t)n;
    return 1;
}

static int read_line(char *line, size_t max)
{
    size_t n = 0;

    for (;;) {
        if (in_pos == in_len && !fill())
            return 0;
        char c = (char)inbuf[in_pos++];
        if (c == '\n') break;
        if (n + 1 < max) line[n++] = c;
    }
    line[n] = 0;
    return 1;
}

static int read_exact(uint8_t *out, size_t n)
{
    while (n) {
        if (in_pos == in_len && !fill())
            return 0;
        size_t c = in_len - in_pos < n ? in_len - in_pos : n;
        memcpy(out, inbuf + in_pos, c);
        in_pos += c;
        out += c;
        n -= c;
    }
    return 1;
}

static void write_all(const void *data, size_t n)
{
    const uint8_t *p = data;

    while (n) {
        ssize_t w = write(fd_out, p, n);
        if (w < 0 && errno == EINTR) continue;
        if (w <= 0) return;
        p += w;
        n -= (size_t)w;
    }
}

static void say(const char *format, ...)
{
    char line[256];
    va_list ap;

    va_start(ap, format);
    vsnprintf(line, sizeof line, format, ap);
    va_end(ap);
    write_all(line, strlen(line));
}

/* ---- the commands --------------------------------------------------------------------------------------------------------------------------------------------------- */
static int one_step(void)
{
    stopped_at_hook = 0;
    if (!game_started) {
        start_game();
        return stopped_at_hook;
    }
    if (!suspended_at_hook)
        host_frame_end();   /* (a game that stands inside a function is in the middle of a frame: no frame end) */
    suspended_at_hook = 0;
    resume();
    return stopped_at_hook;
}

static void fill_cache(void)
{
    if (!cache_valid) {
        host_export_ram_buf(cache);
        cache_valid = 1;
    }
}

static void reply_image(void)
{
    const uint32_t *p = host_picture();
    static uint8_t rgb[DISPLAY_W * DISPLAY_H * 3];

    for (int i = 0; i < DISPLAY_W * DISPLAY_H; i++) {
        rgb[3 * i] = (uint8_t)(p[i] >> 16);
        rgb[3 * i + 1] = (uint8_t)(p[i] >> 8);
        rgb[3 * i + 2] = (uint8_t)p[i];
    }
    say("frame %d %d %d port -\n", host_frame_count(), DISPLAY_W, DISPLAY_H);
    write_all(rgb, sizeof rgb);
}

static void reply_peek(uint32_t a, int size)
{
    uint32_t v = 0;

    if ((uint64_t)a + size > sizeof cache) {
        say("error address $%x is outside the 512 KB\n", a);
        return;
    }
    fill_cache();
    for (int i = 0; i < size; i++)
        v = (v << 8) | cache[a + i];
    say("value %u\n", v);
}

static void reply_sync(uint32_t address, long n, long max)
{
    void *function = guest_code(address);
    long i;

    if (!function) {
        say("error no C function has the original address $%x\n", address);
        return;
    }
    stop_function = function;
    stop_count = n;
    stop_entries = 0;
    for (i = 0; i < max; i++)
        if (one_step())
            break;
    stop_function = 0;
    if (i < max)
        say("synced %d %ld\n", host_frame_count(), stop_entries);
    else
        say("unsynced %d\n", host_frame_count());
}

/* returns 0 to end the session */
static int command(char *line)
{
    char *save, *cmd = strtok_r(line, " \t\r", &save);
    unsigned long arg[5] = { 0 };
    int na = 0;

    if (!cmd)
        return 1;
    for (char *t; na < 5 && (t = strtok_r(0, " \t\r", &save)); na++)
        arg[na] = strtoul(t, 0, 0);

    if (!strcmp(cmd, "joy") && na == 5) { host_input((int)arg[4], (int)arg[0], (int)arg[1], (int)arg[2], (int)arg[3]); say("ok\n"); }
    else if (!strcmp(cmd, "key") && na == 2) { host_key_event((int)arg[0], (int)arg[1]); say("ok\n"); }
    else if (!strcmp(cmd, "step")) { one_step(); reply_image(); }
    else if (!strcmp(cmd, "run") && na == 1) { for (unsigned long i = 0; i < arg[0]; i++) one_step(); say("ok %d\n", host_frame_count()); }
    else if (!strcmp(cmd, "image")) reply_image();
    else if (!strcmp(cmd, "sync") && na >= 1) reply_sync((uint32_t)arg[0], na >= 2 && arg[1] ? (long)arg[1] : 1, na >= 3 ? (long)arg[2] : 20000);
    else if (!strcmp(cmd, "ram")) { fill_cache(); say("ram %zu\n", sizeof cache); write_all(cache, sizeof cache); }
    else if (!strcmp(cmd, "peek8") && na == 1) reply_peek((uint32_t)arg[0], 1);
    else if (!strcmp(cmd, "peek16") && na == 1) reply_peek((uint32_t)arg[0], 2);
    else if (!strcmp(cmd, "peek32") && na == 1) reply_peek((uint32_t)arg[0], 4);
    else if (!strcmp(cmd, "loadram") && na == 1) {
        static uint8_t ram[0x80000];

        if (arg[0] != sizeof ram) { say("error loadram wants %zu bytes\n", sizeof ram); return 0; }
        if (!read_exact(ram, sizeof ram)) return 0;
        host_import_ram_buf(ram);
        cache_valid = 0;
        say("loaded\n");
    }
    else if (!strcmp(cmd, "picture_hash")) {
        const uint32_t *p = host_picture();
        uint32_t h = 2166136261u;

        for (int i = 0; i < DISPLAY_W * DISPLAY_H; i++)
            h = (h ^ p[i]) * 16777619u;
        say("hash %08x\n", h);
    }
    else if (!strcmp(cmd, "trace") && na == 1) { trace_left = (long)arg[0]; say("ok\n"); }
    else if (!strcmp(cmd, "disk") && na == 1) { files_insert((int)arg[0]); say("ok\n"); }
    else if (!strcmp(cmd, "frame")) say("frame %d\n", host_frame_count());
    else if (!strcmp(cmd, "quit")) { say("ok\n"); exit(0); }
    else say("error unknown command or wrong arguments: %s\n", cmd);
    return 1;
}

static void serve(void)
{
    char line[256];

    in_pos = in_len = 0;
    while (read_line(line, sizeof line))
        if (!command(line))
            break;
}

int main(int argc, char **argv)
{
    const char *control = 0;
    char *forwarded[256];
    int n = 0, window = 0, r;

    forwarded[n++] = argv[0];
    for (int i = 1; i < argc && n < 250; i++) {
        if (!strcmp(argv[i], "--control") && i + 1 < argc) control = argv[++i];
        else if (!strcmp(argv[i], "--window")) window = 1;
        else forwarded[n++] = argv[i];
    }
    if (!window)
        forwarded[n++] = "--headless";
    if (!control) {
        fprintf(stderr, "usage: xenon2-sdl-dev --control PATH|- [--window] [port options]\n");
        return 2;
    }
    r = host_init(n, forwarded);
    if (r)
        return r;
    if (!strcmp(control, "-")) {
        fd_in = 0;
        fd_out = 1;
        serve();
        return 0;
    }
    {
        struct sockaddr_un addr = { .sun_family = AF_UNIX };
        int listener = socket(AF_UNIX, SOCK_STREAM, 0);

        if (strlen(control) >= sizeof addr.sun_path) { fprintf(stderr, "dev: socket path too long\n"); return 2; }
        strcpy(addr.sun_path, control);
        unlink(control);
        if (listener < 0 || bind(listener, (struct sockaddr *)&addr, sizeof addr) || listen(listener, 1)) {
            perror("dev: control socket");
            return 1;
        }
        fprintf(stderr, "dev: control socket %s\n", control);
        for (;;) {
            int c = accept(listener, 0, 0);

            if (c < 0) {
                if (errno == EINTR) continue;
                perror("dev: accept");
                return 1;
            }
            fd_in = fd_out = c;
            serve();
            close(c);
        }
    }
}
