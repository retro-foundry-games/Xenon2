/* The dev build of the port: the game runs on a stack of its own (a coroutine that yields at every video frame, in host_idle), and a Lua script drives it:
 * steps it frame by frame, hooks functions of the game by the addresses they have in the original (as ami.hook does in ami9000), stops at them, reads the
 * memory in the original's byte order and the picture, sends it keys and the stick.  The lockstep with the original (dev/lua/peer.lua, host/lockstep.lua)
 * is written in Lua on top of this.
 *
 *   xenon2-sdl-dev --lua SCRIPT [--lua-args TEXT] [port options...]   (TEXT is the Lua global `args`)          the options of xenon2-sdl (--disks, ...); --headless unless --window is given
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
 *   port.set_a6a(BYTE)           the byte the next update_all copies into fire_press (the original's)
 *   port.set_random(A, B)        the random generator's two longs (the lockstep gives the original's after a disk load, whose time the port cannot match)
 *   port.edge_defer(0|1)         the fire press the next interrupt sees is put into key_pressed at the first poke_stick with the fire bit (1) or at the interrupt (0)
 *   port.poke_stick(BYTE)        sets joy_stick (the stick byte the game decodes), as the original's interrupt does mid-frame
 *   port.peek8/16/32(address)    from the same (cached until the game runs again)
 *   port.picture()               RGB, 3 bytes a pixel, 320 x 256;  port.picture_hash()
 *   port.load_high(str)          the original's memory from $80000 to $FFFFF (524288 bytes), read by the map masks of bobs above the screen
 *   port.load_ram(str)           the memory of the original (0x80000 bytes, its byte order) into the port, before the game has started
 *   port.save_state(PATH)        the whole state of the port (memory block, chips, sound, disk, input, the game's stack and context); port.load_state(PATH) puts it back,
 *                                also into a new process of the same build (the game stands where it stood: at a frame's end, or inside the function of a stop)
 *   port.run_function(ADDR)      the game starts again at the C function with the original address ADDR, on the memory as it is (the old stack is dropped)
 *   port.disk_gate(on)           a disk load waits inside the loader (port.disk_waiting()) until port.disk_release(): the lockstep gives it the original's memory at the load's end
 *   port.listen(PATH)            a unix socket server (fd);  port.accept(fd, ms)  a client (fd) or nil on timeout (ms nil: wait)
 *   port.readline(fd, ms)        a line without its newline; nil, "timeout" or "eof";  port.read(fd, n): n bytes (nil at eof);  port.write(fd, str);  port.close(fd)
 *                                (the dev build only gives Lua the sockets: dev/lua/control.lua serves the control protocol)
 * Hooks need the game's C built with -finstrument-functions (dev/Makefile does). */
#define _GNU_SOURCE
#include <errno.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ucontext.h>
#include <unistd.h>

#include "host.h"

void host_idle(void);   /* the game calls it (hw.h HOST_IDLE) */
extern unsigned guest_code_entries(const void *function, uint32_t *out, unsigned max);
extern void *guest_code(uint32_t address);
extern void guest_set_stick(uint8_t value);
extern void guest_set_stale(uint32_t value);   /* sdl/guest.c: d0 as the original's boss code finds it */
extern void guest_load_high(const uint8_t *bytes);   /* sdl/guest.c: the original's memory from $80000 to $FFFFF */   /* sdl/guest.c: joy_stick, the stick byte the game decodes */

static lua_State *L;
static ucontext_t ctrl_ctx, game_ctx;
static int game_started, in_game;
static uint8_t cache[0x80000];
static int cache_valid;

struct hook { void *function; uint32_t address; int ref; };
static struct hook hooks[1024];
static int hook_count;
static void *stop_function;
static uint32_t stop_address, stopped_address;
static int stopped_at_hook, suspended_at_hook;   /* the game stands inside a hooked function, not at the end of a frame */

/* ---- the game's context --------------------------------------------------------------------------------------------------------------------------------------- */
static void (*entry_function)(void);   /* port.run_function: the game runs this instead of its own entry */

void shop_resume(void);

static void game_main(void)
{
    if (entry_function) {
        entry_function();
        shop_resume();
    } else
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

static char game_stack[1 << 20];

static void start_game(void)
{
    getcontext(&game_ctx);
    game_ctx.uc_stack.ss_sp = game_stack;
    game_ctx.uc_stack.ss_size = sizeof game_stack;
    game_ctx.uc_link = 0;
    makecontext(&game_ctx, game_main, 0);
    game_started = 1;
    resume();   /* to the first host_idle */
}

/* ---- savestates --------------------------------------------------------------------------------------------------------------------------------------------------- */
/* A state is the machine (host_state: the memory block, the chips, the sound, the disk, the input, the frame count) and the game: the part of its stack that is in use and the
 * context it is suspended in (at a host_idle, or inside the function a stop is in).  It holds addresses of this executable (the stack's frames point into the stack and the
 * memory block), so the dev build is not a PIE, has no stack protector, and a state is refused by another build (a hash of the code). */
#define STATE_MAGIC "X2DEVST2"

struct state_flags { int game_started, suspended_at_hook; };

static uint32_t build_hash(void)
{
    extern char __executable_start, etext;
    uint32_t h = 2166136261u;

    for (const unsigned char *p = (const unsigned char *)&__executable_start; p < (const unsigned char *)&etext; p++)
        h = (h ^ *p) * 16777619u;
    return h;
}

struct state_io { FILE *f; int saving, bad; };

static void state_chunk(void *data, size_t size, void *arg)
{
    struct state_io *io = arg;
    uint32_t n = (uint32_t)size, got;

    if (io->bad)
        return;
    if (io->saving) {
        io->bad = fwrite(&n, 4, 1, io->f) != 1 || (size && fwrite(data, size, 1, io->f) != 1);
    } else {
        io->bad = fread(&got, 4, 1, io->f) != 1 || got != n || (size && fread(data, size, 1, io->f) != 1);
        if (io->bad)
            fprintf(stderr, "dev: the state does not fit (an object of %u bytes where %u are wanted)\n", (unsigned)got, n);
    }
}

/* the part of the game's stack that is in use: from the saved stack pointer (a little below, for the red zone) to the top */
static void stack_range(char **from, size_t *size)
{
    char *top = game_stack + sizeof game_stack;
    char *sp = (char *)(uintptr_t)game_ctx.uc_mcontext.gregs[REG_ESP];

    if (sp < game_stack + 256 || sp > top)
        sp = game_stack + 256;
    *from = sp - 256;
    *size = (size_t)(top - *from);
}

static const char *save_state(const char *path)
{
    struct state_io io = { fopen(path, "wb"), 1, 0 };
    struct state_flags flags = { game_started, suspended_at_hook };
    uint32_t hash = build_hash();

    if (!io.f)
        return strerror(errno);
    fwrite(STATE_MAGIC, 8, 1, io.f);
    fwrite(&hash, 4, 1, io.f);
    state_chunk(&flags, sizeof flags, &io);
    host_state(state_chunk, &io);
    if (game_started) {
        char *from;
        size_t size;

        stack_range(&from, &size);
        state_chunk(&game_ctx, sizeof game_ctx, &io);
        state_chunk(from, size, &io);
    }
    if (fclose(io.f) || io.bad)
        return "write error";
    return 0;
}

static const char *load_state(const char *path)
{
    struct state_io io = { fopen(path, "rb"), 0, 0 };
    struct state_flags flags;
    char magic[8];
    uint32_t hash;

    if (!io.f)
        return strerror(errno);
    if (fread(magic, 8, 1, io.f) != 1 || memcmp(magic, STATE_MAGIC, 8) || fread(&hash, 4, 1, io.f) != 1) {
        fclose(io.f);
        return "not a state of the dev build";
    }
    if (hash != build_hash()) {
        fclose(io.f);
        return "the state is of another build";
    }
    state_chunk(&flags, sizeof flags, &io);
    host_state(state_chunk, &io);
    if (!io.bad && flags.game_started) {
        /* the context first: it says how much of the stack the file holds */
        uint32_t n;
        ucontext_t ctx;

        io.bad = fread(&n, 4, 1, io.f) != 1 || n != sizeof ctx || fread(&ctx, sizeof ctx, 1, io.f) != 1;
        if (!io.bad) {
            char *top = game_stack + sizeof game_stack, *from;
            size_t size;
            ucontext_t saved = game_ctx;

            game_ctx = ctx;
            stack_range(&from, &size);
            game_ctx = saved;
            io.bad = fread(&n, 4, 1, io.f) != 1 || n != size || fread(from, size, 1, io.f) != 1 || from + size != top;
            if (!io.bad)
                game_ctx = ctx;
        }
    }
    fclose(io.f);
    if (io.bad)
        return "the state is damaged or does not fit";
    game_started = flags.game_started;
    suspended_at_hook = flags.suspended_at_hook;
    stopped_at_hook = 0;
    cache_valid = 0;
    return 0;
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
        if (hook_count >= 1024)
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

void guest_edge_defer(int on);
void guest_set_a6a(uint8_t value);

void guest_set_shield(uint16_t frames);
void guest_set_random(uint32_t a, uint32_t b);
void guest_set_level(uint16_t level);
void guest_respawn_at(uint16_t scroll);

static int l_set_shield(lua_State *s)
{
    guest_set_shield((uint16_t)luaL_checkinteger(s, 1));
    cache_valid = 0;
    return 0;
}

static int l_set_level(lua_State *s)
{
    guest_set_level((uint16_t)luaL_checkinteger(s, 1));
    cache_valid = 0;
    return 0;
}

static int l_respawn_at(lua_State *s)
{
    guest_respawn_at((uint16_t)luaL_checkinteger(s, 1));
    cache_valid = 0;
    return 0;
}

static int l_set_random(lua_State *s)
{
    guest_set_random((uint32_t)luaL_checkinteger(s, 1), (uint32_t)luaL_checkinteger(s, 2));
    cache_valid = 0;
    return 0;
}

static int l_set_a6a(lua_State *s)
{
    guest_set_a6a((uint8_t)luaL_checkinteger(s, 1));
    return 0;
}

static int l_edge_defer(lua_State *s)
{
    guest_edge_defer((int)luaL_checkinteger(s, 1));
    return 0;
}

static int l_poke_stale(lua_State *s)
{
    guest_set_stale((uint32_t)luaL_checkinteger(s, 1));
    return 0;
}

static int l_poke_stick(lua_State *s)
{
    guest_set_stick((uint8_t)luaL_checkinteger(s, 1));
    cache_valid = 0;
    return 0;
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

static int l_load_high(lua_State *s)
{
    size_t n;
    const char *bytes = luaL_checklstring(s, 1, &n);

    if (n != 0x80000)
        return luaL_error(s, "load_high wants 524288 bytes");
    guest_load_high((const uint8_t *)bytes);
    return 0;
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

static int l_save_state(lua_State *s)
{
    const char *error;

    if (in_game)
        return luaL_error(s, "save_state from a hook: the game is running");
    error = save_state(luaL_checkstring(s, 1));
    if (error)
        return luaL_error(s, "save_state: %s", error);
    return 0;
}

static int l_load_state(lua_State *s)
{
    const char *error;

    if (in_game)
        return luaL_error(s, "load_state from a hook: the game is running");
    error = load_state(luaL_checkstring(s, 1));
    if (error)
        return luaL_error(s, "load_state: %s", error);
    return 0;
}

/* port.run_function(ADDR): the game starts again on a stack of its own at the C function that has the original address ADDR (shop_run, $8FF2: the original's state is in
 * the memory, the port runs the function the original was running), and goes on to the first frame's end; the old stack is dropped */
static int l_run_function(lua_State *s)
{
    uint32_t address = (uint32_t)luaL_checkinteger(s, 1);
    void *function = guest_code(address);

    if (!function)
        return luaL_error(s, "no C function has the original address $%x", address);
    if (in_game)
        return luaL_error(s, "run_function from a hook: the game is running");
    entry_function = (void (*)(void))function;
    stopped_at_hook = 0;
    suspended_at_hook = 0;
    cache_valid = 0;
    start_game();
    return 0;
}

extern int host_disk_gate, host_disk_waiting;
static int l_disk_gate(lua_State *s) { host_disk_gate = lua_toboolean(s, 1); return 0; }
static int l_disk_waiting(lua_State *s) { lua_pushboolean(s, host_disk_waiting); return 1; }
static int l_disk_release(lua_State *s) { (void)s; host_disk_waiting = 0; return 0; }
static int l_quit(lua_State *s) { (void)s; exit(0); }

/* ---- sockets for the scripts (the protocol is Lua's: dev/lua/control.lua) ------------------------------------------------------------------------------------------ */
#define MAX_FD 32
static struct { int fd; size_t pos, len; uint8_t buf[1 << 16]; } conn[MAX_FD];

static int slot(int fd, int create)
{
    static int initialised;

    if (!initialised) {
        for (int i = 0; i < MAX_FD; i++)
            conn[i].fd = -1;
        initialised = 1;
    }
    for (int i = 0; i < MAX_FD; i++)
        if (conn[i].fd == fd)
            return i;
    if (!create)
        return -1;
    for (int i = 0; i < MAX_FD; i++)
        if (conn[i].fd < 0) {
            conn[i].fd = fd;
            conn[i].pos = conn[i].len = 0;
            return i;
        }
    return -1;
}

static int l_listen(lua_State *s)
{
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    const char *path = luaL_checkstring(s, 1);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (strlen(path) >= sizeof addr.sun_path)
        return luaL_error(s, "socket path too long");
    strcpy(addr.sun_path, path);
    unlink(path);
    if (fd < 0 || bind(fd, (struct sockaddr *)&addr, sizeof addr) || listen(fd, 2))
        return luaL_error(s, "cannot serve %s: %s", path, strerror(errno));
    lua_pushinteger(s, fd);
    return 1;
}

static int wait_readable(int fd, int ms)
{
    struct pollfd p = { .fd = fd, .events = POLLIN };

    return poll(&p, 1, ms) > 0;
}

static int l_accept(lua_State *s)
{
    int fd = (int)luaL_checkinteger(s, 1), ms = lua_isnoneornil(s, 2) ? -1 : (int)lua_tointeger(s, 2), c;

    if (!wait_readable(fd, ms))
        return 0;
    c = accept(fd, 0, 0);
    if (c < 0 || slot(c, 1) < 0)
        return luaL_error(s, "accept: %s", strerror(errno));
    lua_pushinteger(s, c);
    return 1;
}

/* more bytes into the buffer of the connection: 1, 0 at the end of the stream, -1 on timeout */
static int refill(int i, int ms)
{
    ssize_t n;

    if (!wait_readable(conn[i].fd, ms))
        return -1;
    do n = read(conn[i].fd, conn[i].buf, sizeof conn[i].buf); while (n < 0 && errno == EINTR);
    if (n <= 0)
        return 0;
    conn[i].pos = 0;
    conn[i].len = (size_t)n;
    return 1;
}

static int l_readline(lua_State *s)
{
    int fd = (int)luaL_checkinteger(s, 1), ms = lua_isnoneornil(s, 2) ? -1 : (int)lua_tointeger(s, 2), i = slot(fd, 1);
    luaL_Buffer b;

    if (i < 0)
        return luaL_error(s, "no such connection");
    luaL_buffinit(s, &b);
    for (;;) {
        if (conn[i].pos == conn[i].len) {
            int r = refill(i, ms);

            if (r <= 0) {
                lua_pushnil(s);
                lua_pushstring(s, r < 0 ? "timeout" : "eof");
                return 2;
            }
        }
        char c = (char)conn[i].buf[conn[i].pos++];
        if (c == '\n') {
            luaL_pushresult(&b);
            return 1;
        }
        luaL_addchar(&b, c);
    }
}

static int l_read(lua_State *s)
{
    int fd = (int)luaL_checkinteger(s, 1), i = slot(fd, 1);
    size_t n = (size_t)luaL_checkinteger(s, 2);
    luaL_Buffer b;

    if (i < 0)
        return luaL_error(s, "no such connection");
    luaL_buffinit(s, &b);
    while (n) {
        size_t c;

        if (conn[i].pos == conn[i].len && refill(i, -1) <= 0) {
            lua_pushnil(s);
            return 1;
        }
        c = conn[i].len - conn[i].pos < n ? conn[i].len - conn[i].pos : n;
        luaL_addlstring(&b, (const char *)conn[i].buf + conn[i].pos, c);
        conn[i].pos += c;
        n -= c;
    }
    luaL_pushresult(&b);
    return 1;
}

static int l_write(lua_State *s)
{
    int fd = (int)luaL_checkinteger(s, 1);
    size_t n;
    const char *p = luaL_checklstring(s, 2, &n);

    while (n) {
        ssize_t w = write(fd, p, n);

        if (w < 0 && errno == EINTR) continue;
        if (w <= 0) { lua_pushboolean(s, 0); return 1; }
        p += w;
        n -= (size_t)w;
    }
    lua_pushboolean(s, 1);
    return 1;
}

static int l_close(lua_State *s)
{
    int fd = (int)luaL_checkinteger(s, 1), i = slot(fd, 0);

    if (i >= 0)
        conn[i].fd = -1;
    close(fd);
    return 0;
}

static const luaL_Reg port_functions[] = {
    { "step", l_step }, { "run", l_run }, { "run_until", l_run_until }, { "hook", l_hook }, { "unhook", l_unhook }, { "stop_on", l_stop_on },
    { "frame", l_frame }, { "trace", l_trace }, { "joy", l_joy }, { "key", l_key }, { "ram", l_ram }, { "poke_stick", l_poke_stick }, { "poke_stale", l_poke_stale }, { "edge_defer", l_edge_defer }, { "set_a6a", l_set_a6a }, { "set_shield", l_set_shield }, { "set_random", l_set_random }, { "set_level", l_set_level }, { "respawn_at", l_respawn_at }, { "peek8", l_peek8 }, { "peek16", l_peek16 }, { "peek32", l_peek32 },
    { "load_ram", l_load_ram }, { "load_high", l_load_high }, { "picture", l_picture }, { "picture_hash", l_picture_hash }, { "disk_gate", l_disk_gate }, { "disk_waiting", l_disk_waiting }, { "disk_release", l_disk_release }, { "run_function", l_run_function }, { "save_state", l_save_state }, { "load_state", l_load_state }, { "quit", l_quit }, { "listen", l_listen }, { "accept", l_accept }, { "readline", l_readline }, { "read", l_read }, { "write", l_write }, { "close", l_close }, { 0, 0 }
};

int main(int argc, char **argv)
{
    const char *script = 0, *script_args = "";
    char *forwarded[256];
    int n = 0, window = 0, start_level = 0, start_scroll = -1;
    int r;

    forwarded[n++] = argv[0];
    for (int i = 1; i < argc && n < 250; i++) {
        if (!strcmp(argv[i], "--lua") && i + 1 < argc) script = argv[++i];
        else if (!strcmp(argv[i], "--lua-args") && i + 1 < argc) script_args = argv[++i];
        else if (!strcmp(argv[i], "--window")) window = 1;
        else if (!strcmp(argv[i], "--level") && i + 1 < argc) start_level = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--scroll") && i + 1 < argc) {
            char *end;
            long value = strtol(argv[++i], &end, 0);   /* decimal, or hex with 0x */

            if (*end || value < 0 || value > 0xffff) {
                fprintf(stderr, "dev: --scroll takes a number from 0 to 65535 (0x for hex)\n");
                return 2;
            }
            start_scroll = (int)value;
        }
        else forwarded[n++] = argv[i];
    }
    host_set_headless(!window);
    if (!script) {
        fprintf(stderr, "usage: xenon2-sdl-dev --lua SCRIPT [--window] [--level N] [--scroll N] [port options]\n");
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
    lua_pushstring(L, script_args);
    lua_setglobal(L, "args");
    if (start_level < 0 || start_level > 5) {
        fprintf(stderr, "dev: --level takes 1 to 5\n");
        return 2;
    }
    if (start_level || start_scroll >= 0) {
        /* a new game loads level N (the hook runs when the game loads the level's code), and with --scroll the first frame
         * after the level's first scroll events (the first record sets scroll_y itself) is a respawn at that scroll_y */
        lua_pushinteger(L, start_level);
        lua_setglobal(L, "start_level");
        lua_pushinteger(L, start_scroll);
        lua_setglobal(L, "start_scroll");
        if (luaL_dostring(L,
            "local loaded, started, done, calls = false, false, false, 0\n"
            "if start_level > 0 then port.hook(0x917e, function() if not loaded then loaded = true; port.set_level(start_level) end end) end\n"
            "port.hook(0x6f94, function() started = true end)\n"
            "port.hook(0x3554, function() if not started or done or start_scroll < 0 then return end; calls = calls + 1; if calls == 3 then done = true; port.respawn_at(start_scroll) end end)\n")) {
            fprintf(stderr, "dev: %s\n", lua_tostring(L, -1));
            return 1;
        }
    }
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
