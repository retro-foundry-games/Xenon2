/* The dev build of the port: the game runs on a stack of its own (a coroutine that yields at every video frame, in host_idle), and a client drives it over a
 * control socket: steps it frame by frame, runs it to a function of the game by the address the function has in the original (as ami.hook does in ami9000),
 * reads the memory in the original's byte order and the picture, sends it keys and the stick.  The lockstep with the original (host/lockstep.lua) is a client.
 *
 *   xenon2-sdl-dev [--lua SCRIPT] [--control PATH|-] [port options...]
 *     --lua SCRIPT       runs a Lua script with the global `port` (below) to steer the game: step, run to a function, hook functions with callbacks, memory, picture.
 *                        With --control as well the script runs first and calls port.serve(PATH) when it wants the socket served.
 *     --control PATH     serve a unix socket at PATH (one client at a time, then the next);  --control -  serves stdin/stdout (a peer
 *                                                       the emulator starts).  The options are the port's (--disks, ...); --headless unless --window is given.
 *   dev/ctl.py PATH COMMAND...                          a client;  `lua CODE` runs Lua in the script's state (an expression, else statements)  -> ok RESULT
 *                                                       (not with --control -: print would corrupt the protocol)
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
 * The Lua global `port`:
 *   step() run(n) run_until(ADDR, max) hook(ADDR, fn) unhook(ADDR) stop_on(ADDR|nil) trace(n) frame() joy{up=,down=,left=,right=,fire=} key(raw, down)
 *   ram() peek8/16/32(a) picture() picture_hash() load_ram(str) disk(n) serve(PATH) quit();  step/run return "frame" or "hook", ADDR (stopped inside a stop_on function).
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

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include "host.h"

void host_idle(void);   /* the game calls it (hw.h HOST_IDLE) */
extern unsigned guest_code_entries(const void *function, uint32_t *out, unsigned max);
extern void *guest_code(uint32_t address);

static ucontext_t ctrl_ctx, game_ctx;
static int game_started, in_game;
static uint8_t cache[0x80000];
static int cache_valid;

static lua_State *L;
struct hook { void *function; uint32_t address; int ref; };
static struct hook hooks[64];
static int hook_count;
static void *stop_function;
static uint32_t stop_address, stopped_address;
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
    for (int i = 0; i < hook_count; i++) {
        if (hooks[i].function != function || hooks[i].ref == LUA_NOREF || !L)
            continue;
        lua_rawgeti(L, LUA_REGISTRYINDEX, hooks[i].ref);
        lua_pushinteger(L, hooks[i].address);
        if (lua_pcall(L, 1, 0, 0) != 0) {
            fprintf(stderr, "dev: hook %06x: %s\n", hooks[i].address, lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
    if (stop_function == function && in_game && ++stop_entries >= stop_count) {
        uint32_t a[4];

        stopped_address = guest_code_entries(function, a, 4) ? a[0] : stop_address;
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
static void lua_line(char *code);
static void listen_on(const char *control);

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
    char *save, *cmd;
    unsigned long arg[5] = { 0 };
    int na = 0;

    if (!strncmp(line, "lua ", 4)) {
        lua_line(line + 4);
        return 1;
    }
    cmd = strtok_r(line, " \t\r", &save);
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

/* ---- Lua ------------------------------------------------------------------------------------------------------------------------------------------------------------------ */
static int l_step(lua_State *s)
{
    if (one_step()) {
        lua_pushstring(s, "hook");
        lua_pushinteger(s, stopped_address);
        return 2;
    }
    lua_pushstring(s, "frame");
    return 1;
}

static int l_run(lua_State *s)
{
    long n = (long)luaL_checkinteger(s, 1);

    for (long i = 0; i < n; i++)
        if (one_step()) {
            lua_pushstring(s, "hook");
            lua_pushinteger(s, stopped_address);
            return 2;
        }
    lua_pushstring(s, "frame");
    return 1;
}

static int find_hook(uint32_t address)
{
    for (int i = 0; i < hook_count; i++)
        if (hooks[i].address == address)
            return i;
    return -1;
}

static int add_function(uint32_t address, void **function)
{
    *function = guest_code(address);
    return *function != 0;
}

static int l_hook(lua_State *s)
{
    uint32_t address = (uint32_t)luaL_checkinteger(s, 1);
    void *function;
    int i = find_hook(address);

    luaL_checktype(s, 2, LUA_TFUNCTION);
    if (!add_function(address, &function))
        return luaL_error(s, "no C function has the original address %d (decimal)", address);
    if (i < 0) {
        if (hook_count >= 64)
            return luaL_error(s, "too many hooks");
        i = hook_count++;
    } else if (hooks[i].ref != LUA_NOREF) {
        luaL_unref(s, LUA_REGISTRYINDEX, hooks[i].ref);
    }
    lua_pushvalue(s, 2);
    hooks[i].ref = luaL_ref(s, LUA_REGISTRYINDEX);
    hooks[i].function = function;
    hooks[i].address = address;
    return 0;
}

static int l_unhook(lua_State *s)
{
    int i = find_hook((uint32_t)luaL_checkinteger(s, 1));

    if (i >= 0) {
        luaL_unref(s, LUA_REGISTRYINDEX, hooks[i].ref);
        hooks[i] = hooks[--hook_count];
    }
    return 0;
}

static int l_stop_on(lua_State *s)
{
    void *function;

    if (lua_isnil(s, 1)) {
        stop_function = 0;
        return 0;
    }
    stop_address = (uint32_t)luaL_checkinteger(s, 1);
    if (!add_function(stop_address, &function))
        return luaL_error(s, "no C function has the original address %d (decimal)", stop_address);
    stop_function = function;
    stop_count = 1;
    if (find_hook(stop_address) < 0) {   /* a hook without a Lua function, to have the address named */
        hooks[hook_count].function = function;
        hooks[hook_count].address = stop_address;
        hooks[hook_count].ref = LUA_NOREF;
        hook_count++;
    }
    return 0;
}

static int l_run_until(lua_State *s)
{
    uint32_t address = (uint32_t)luaL_checkinteger(s, 1);
    long max = (long)luaL_optinteger(s, 2, 100000);
    void *function, *saved = stop_function;
    uint32_t saved_address = stop_address;

    if (!add_function(address, &function))
        return luaL_error(s, "no C function has the original address %d (decimal)", address);
    stop_function = function;
    stop_count = 1;
    stop_address = address;
    if (find_hook(address) < 0) {
        hooks[hook_count].function = function;
        hooks[hook_count].address = address;
        hooks[hook_count].ref = LUA_NOREF;
        hook_count++;
    }
    for (long i = 0; i < max; i++)
        if (one_step()) {
            stop_function = saved;
            stop_address = saved_address;
            lua_pushboolean(s, 1);
            return 1;
        }
    stop_function = saved;
    stop_address = saved_address;
    lua_pushboolean(s, 0);
    return 1;
}

static int l_trace(lua_State *s) { trace_left = (long)luaL_checkinteger(s, 1); return 0; }

static int l_frame(lua_State *s) { lua_pushinteger(s, host_frame_count()); return 1; }

static int l_joy(lua_State *s)
{
    int up = 0, down = 0, left = 0, right = 0, fire = 0;

    if (lua_istable(s, 1)) {
        lua_getfield(s, 1, "up"); up = lua_toboolean(s, -1);
        lua_getfield(s, 1, "down"); down = lua_toboolean(s, -1);
        lua_getfield(s, 1, "left"); left = lua_toboolean(s, -1);
        lua_getfield(s, 1, "right"); right = lua_toboolean(s, -1);
        lua_getfield(s, 1, "fire"); fire = lua_toboolean(s, -1);
    }
    host_input(fire, up, down, left, right);
    return 0;
}

static int l_key(lua_State *s)
{
    host_key_event((int)luaL_checkinteger(s, 1), lua_toboolean(s, 2));
    return 0;
}

static int l_ram(lua_State *s)
{
    fill_cache();
    lua_pushlstring(s, (const char *)cache, sizeof cache);
    return 1;
}

static int peek(lua_State *s, int size)
{
    uint32_t a = (uint32_t)luaL_checkinteger(s, 1), v = 0;

    if (a + size > sizeof cache)
        return luaL_error(s, "address $%x is outside the 512 KB", a);
    fill_cache();
    for (int i = 0; i < size; i++)
        v = (v << 8) | cache[a + i];
    lua_pushinteger(s, v);
    return 1;
}

static int l_peek8(lua_State *s) { return peek(s, 1); }
static int l_peek16(lua_State *s) { return peek(s, 2); }
static int l_peek32(lua_State *s) { return peek(s, 4); }

static int l_picture(lua_State *s)
{
    const uint32_t *p = host_picture();
    static uint8_t rgb[DISPLAY_W * DISPLAY_H * 3];

    for (int i = 0; i < DISPLAY_W * DISPLAY_H; i++) {
        rgb[3 * i] = (uint8_t)(p[i] >> 16);
        rgb[3 * i + 1] = (uint8_t)(p[i] >> 8);
        rgb[3 * i + 2] = (uint8_t)p[i];
    }
    lua_pushlstring(s, (const char *)rgb, sizeof rgb);
    return 1;
}

static int l_picture_hash(lua_State *s)
{
    const uint32_t *p = host_picture();
    uint32_t h = 2166136261u;

    for (int i = 0; i < DISPLAY_W * DISPLAY_H; i++)
        h = (h ^ p[i]) * 16777619u;
    lua_pushinteger(s, h);
    return 1;
}

static int l_load_ram(lua_State *s)
{
    size_t n;
    const char *ram = luaL_checklstring(s, 1, &n);

    if (n != 0x80000)
        return luaL_error(s, "load_ram wants 524288 bytes");
    host_import_ram_buf((const uint8_t *)ram);
    cache_valid = 0;
    return 0;
}

static int l_disk(lua_State *s) { files_insert((int)luaL_checkinteger(s, 1)); return 0; }
static int l_quit(lua_State *s) { (void)s; exit(0); }

static int l_serve(lua_State *s);
static const luaL_Reg port_functions[] = {
    { "step", l_step }, { "run", l_run }, { "run_until", l_run_until }, { "hook", l_hook }, { "unhook", l_unhook }, { "stop_on", l_stop_on },
    { "frame", l_frame }, { "trace", l_trace }, { "joy", l_joy }, { "key", l_key }, { "ram", l_ram }, { "peek8", l_peek8 }, { "peek16", l_peek16 }, { "peek32", l_peek32 },
    { "serve", l_serve }, { "load_ram", l_load_ram }, { "picture", l_picture }, { "picture_hash", l_picture_hash }, { "disk", l_disk }, { "quit", l_quit }, { 0, 0 }
};

static int l_serve(lua_State *s)
{
    listen_on(luaL_checkstring(s, 1));
    return 0;
}

/* `lua CODE` over the socket: the expression (or else the statements) in the script's state; the reply is the result or the error */
static void lua_line(char *code)
{
    char text[600];

    if (!L) {
        say("error no Lua state (start with --lua SCRIPT)\n");
        return;
    }
    snprintf(text, sizeof text, "return %s", code);
    if (luaL_loadstring(L, text) != 0) {
        lua_pop(L, 1);
        if (luaL_loadstring(L, code) != 0) {
            say("error %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
            return;
        }
    }
    if (lua_pcall(L, 0, 1, 0) != 0) {
        say("error %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return;
    }
    say("ok %s\n", lua_isnil(L, -1) ? "" : luaL_tolstring(L, -1, 0));
    lua_settop(L, 0);
}

static void listen_on(const char *control)
{
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    int listener = socket(AF_UNIX, SOCK_STREAM, 0);

    if (strlen(control) >= sizeof addr.sun_path) { fprintf(stderr, "dev: socket path too long\n"); exit(2); }
    strcpy(addr.sun_path, control);
    unlink(control);
    if (listener < 0 || bind(listener, (struct sockaddr *)&addr, sizeof addr) || listen(listener, 1)) {
        perror("dev: control socket");
        exit(1);
    }
    fprintf(stderr, "dev: control socket %s\n", control);
    for (;;) {
        int c = accept(listener, 0, 0);

        if (c < 0) {
            if (errno == EINTR) continue;
            perror("dev: accept");
            exit(1);
        }
        fd_in = fd_out = c;
        serve();
        close(c);
    }
}
int main(int argc, char **argv)
{
    const char *control = 0, *script = 0;
    char *forwarded[256];
    int n = 0, window = 0, r;

    forwarded[n++] = argv[0];
    for (int i = 1; i < argc && n < 250; i++) {
        if (!strcmp(argv[i], "--control") && i + 1 < argc) control = argv[++i];
        else if (!strcmp(argv[i], "--lua") && i + 1 < argc) script = argv[++i];
        else if (!strcmp(argv[i], "--window")) window = 1;
        else forwarded[n++] = argv[i];
    }
    if (!window)
        forwarded[n++] = "--headless";
    if (!control && !script) {
        fprintf(stderr, "usage: xenon2-sdl-dev [--control PATH|-] [--lua SCRIPT] [--window] [port options]\n");
        return 2;
    }
    r = host_init(n, forwarded);
    if (r)
        return r;
    for (int i = 0; i < hook_count; i++)
        hooks[i].ref = LUA_NOREF;
    if (script || control) {
        L = luaL_newstate();
        luaL_openlibs(L);
        luaL_newlib(L, port_functions);
        lua_setglobal(L, "port");
    }
    if (script && luaL_dofile(L, script) != 0) {
        fprintf(stderr, "dev: %s\n", lua_tostring(L, -1));
        return 1;
    }
    if (!control)
        return 0;
    if (!strcmp(control, "-")) {
        fd_in = 0;
        fd_out = 1;
        serve();
        return 0;
    }
    listen_on(control);
    return 0;
}
