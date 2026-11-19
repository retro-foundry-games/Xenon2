/* The dev build of the port: the game runs on a stack of its own (a coroutine that yields at every video frame, in host_idle), and a Lua script drives it:
 * steps it frame by frame, hooks functions of the game by the addresses they have in the original (as ami.hook does in ami9000), stops at them, reads the
 * memory in the original's byte order and the picture, sends it keys and the stick.  The lockstep with the original (dev/lua/peer.lua, host/lockstep.lua)
 * is written in Lua on top of this.
 *
 *   xenon2-sdl-dev --lua SCRIPT [port options...]          the options of xenon2-sdl (--disks, ...); --headless unless --window is given
 *
 * The Lua global `port`:
 *   port.step()                  one video frame (the end of the frame, then the game until its next host_idle); returns "frame", or "hook", ADDRESS when it stopped at a function
 *                                of port.stop_on()
 *   port.run(n)                  n frames (stops early at a hook: returns the same as step)
 *   port.run_until(ADDR, max)    steps until the function that has the original address ADDR is entered (the game stands inside it); true, or false after max frames
 *   port.hook(ADDR, fn)          fn(ADDR) is called whenever that function is entered; port.unhook(ADDR)
 *   port.stop_on(ADDR | nil)     step/run return when that function is entered
 *   port.trace(n)                prints the original address of the next n functions the game enters (on stderr)
 *   port.frame()                 the frames so far;  port.joy{up=,down=,left=,right=,fire=};  port.key(amiga_raw_code, down)
 *   port.ram()                   the memory as a string of 0x80000 bytes in the original's byte order (named members converted, raw buffers as they are)
 *   port.peek8/16/32(address)    from the same (cached until the game runs again)
 *   port.picture()               RGB, 3 bytes a pixel, 320 x 256;  port.picture_hash()
 *   port.load_ram(str)           the memory of the original (0x80000 bytes, its byte order) into the port, before the game has started
 *   port.disk(n)                 put disk 1 or 2 in the drive;  port.quit()
 * Hooks need the game's C built with -finstrument-functions (dev/Makefile does). */
#define _GNU_SOURCE
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>

#include "host.h"

void host_idle(void);   /* the game calls it (hw.h HOST_IDLE) */
extern unsigned guest_code_entries(const void *function, uint32_t *out, unsigned max);
extern void *guest_code(uint32_t address);

static lua_State *L;
static ucontext_t ctrl_ctx, game_ctx;
static int game_started, in_game;
static uint8_t cache[0x80000];
static int cache_valid;

struct hook { void *function; uint32_t address; int ref; };
static struct hook hooks[64];
static int hook_count;
static void *stop_function;
static uint32_t stop_address, stopped_address;
static int stopped_at_hook, suspended_at_hook;   /* the game stands inside a hooked function, not at the end of a frame */

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

/* ---- hooks: -finstrument-functions calls this on entry to every function of the game ------------------------------------------------------------------------------ */
void __cyg_profile_func_enter(void *function, void *site) __attribute__((no_instrument_function));
void __cyg_profile_func_exit(void *function, void *site) __attribute__((no_instrument_function));

static long trace_left;   /* port.trace(n): print the original address of the next n functions entered */

void __cyg_profile_func_enter(void *function, void *site)
{
    (void)site;
    if (trace_left > 0) {
        uint32_t a[4];
        unsigned c = guest_code_entries(function, a, 4);

        trace_left--;
        if (c)
            fprintf(stderr, "call $%06x\n", a[0]);
        else
            fprintf(stderr, "call %p (no original address)\n", function);
    }
    for (int i = 0; i < hook_count; i++) {
        if (hooks[i].function != function)
            continue;
        if (hooks[i].ref != LUA_NOREF && L) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, hooks[i].ref);
            lua_pushinteger(L, hooks[i].address);
            if (lua_pcall(L, 1, 0, 0) != 0) {
                fprintf(stderr, "dev: hook %06x: %s\n", hooks[i].address, lua_tostring(L, -1));
                lua_pop(L, 1);
            }
        }
        if (stop_function == function && in_game) {
            stopped_at_hook = 1;
            suspended_at_hook = 1;
            stopped_address = hooks[i].address;
            swapcontext(&game_ctx, &ctrl_ctx);   /* the script sees the game standing inside the function */
            cache_valid = 0;
        }
        break;
    }
    if (stop_function == function && in_game && !stopped_at_hook) {
        /* stop_on without a hook of its own */
        uint32_t a[4];
        unsigned n = guest_code_entries(function, a, 4);

        stopped_at_hook = 1;
        suspended_at_hook = 1;
        stopped_address = n ? a[0] : stop_address;
        swapcontext(&game_ctx, &ctrl_ctx);
        cache_valid = 0;
    }
}

void __cyg_profile_func_exit(void *function, void *site)
{
    (void)function;
    (void)site;
}

/* ---- Lua ------------------------------------------------------------------------------------------------------------------------------------------------------------------ */
static int one_step(void)
{
    stopped_at_hook = 0;
    if (!game_started) {
        start_game();
        return stopped_at_hook ? 1 : 0;
    }
    if (!suspended_at_hook)
        host_frame_end();   /* (a game that stands inside a function is in the middle of a frame: no frame end) */
    suspended_at_hook = 0;
    resume();
    return stopped_at_hook;
}

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

static void fill_cache(void)
{
    if (!cache_valid) {
        host_export_ram_buf(cache);
        cache_valid = 1;
    }
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

static const luaL_Reg port_functions[] = {
    { "step", l_step }, { "run", l_run }, { "run_until", l_run_until }, { "hook", l_hook }, { "unhook", l_unhook }, { "stop_on", l_stop_on },
    { "frame", l_frame }, { "trace", l_trace }, { "joy", l_joy }, { "key", l_key }, { "ram", l_ram }, { "peek8", l_peek8 }, { "peek16", l_peek16 }, { "peek32", l_peek32 },
    { "load_ram", l_load_ram }, { "picture", l_picture }, { "picture_hash", l_picture_hash }, { "disk", l_disk }, { "quit", l_quit }, { 0, 0 }
};

int main(int argc, char **argv)
{
    const char *script = 0;
    char *forwarded[256];
    int n = 0, window = 0;
    int r;

    forwarded[n++] = argv[0];
    for (int i = 1; i < argc && n < 250; i++) {
        if (!strcmp(argv[i], "--lua") && i + 1 < argc) script = argv[++i];
        else if (!strcmp(argv[i], "--window")) window = 1;
        else forwarded[n++] = argv[i];
    }
    if (!window)
        forwarded[n++] = "--headless";
    if (!script) {
        fprintf(stderr, "usage: xenon2-sdl-dev --lua SCRIPT [--window] [port options]\n");
        return 2;
    }
    r = host_init(n, forwarded);
    if (r)
        return r;
    for (int i = 0; i < hook_count; i++)
        hooks[i].ref = LUA_NOREF;
    L = luaL_newstate();
    luaL_openlibs(L);
    luaL_newlib(L, port_functions);
    lua_setglobal(L, "port");
    lua_newtable(L);   /* arg: the script's path and the options that were not the port's */
    lua_pushstring(L, script);
    lua_rawseti(L, -2, 0);
    lua_setglobal(L, "arg");
    if (luaL_dofile(L, script) != 0) {
        fprintf(stderr, "dev: %s\n", lua_tostring(L, -1));
        return 1;
    }
    lua_close(L);
    return 0;
}
