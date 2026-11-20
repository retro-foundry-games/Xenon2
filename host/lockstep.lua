-- Lockstep of the original (this emulator) and the dev build of the port (dev/build/xenon2-sdl-dev, driven through its interactive Lua shell, dev/lua/shell.lua, on stdin/stdout): both run the game from its start (the port from
-- the original's memory at that moment); at every entry of swap_screens (the game passes it once a frame) the named members of the memory and the picture are compared.
-- The first difference pauses the emulator and the port is not stepped again, so both can be looked at (the emulator's --control socket); continue=1 goes on and lists them all.
--
--   ami9000_host host/lockstep.lua --headless --script-args "[syncs=N] [tolerant=1] [continue=1] [mem=0] [pic=0] [ignore=NAME,NAME] [joy=SYNC:DIRS,...] [show=NAME,NAME] [strict=1] [report=FILE]"
--     state=NAME    start from the emulator's savestate local/save/NAME.state (frame-8880: level 1 running) instead of the start of the game: the port plays to the level by itself
--                   (the fire button on the schedule of tools/verify_port.sh) and takes the original's memory at the first sync point
--     play=1        play it: ami9000_host host/lockstep.lua --script-args "play=1 state=frame-8880 tolerant=1"  (a window, not --headless): arrows = stick, Ctrl / Space / Return = fire;
--                   the keys are applied to both sides at the sync points; the port's picture and the difference are shown beside the original; the first difference pauses both
--     auto=1        the autoplayer (host/autoplay.lua) plays, headless or in the window: finds differences in a run no one has to play
--     record=FILE   where the input of play=1 / auto=1 is written (default captures/input/play-latest.txt, overwritten each run)
--     snap=ADDR:NAME the first time the original reaches ADDR (hex) its state is saved as local/save/NAME.state and its memory as captures/NAME.ram
--     live=0|1      the port's panel live: the port is stepped one frame for each emulator frame and waits at a sync point for the original (default on with play=1 / auto=1)
--     hold=1        stop at the first difference and stay there, both sides standing: with `ami9000_host --control PATH` a client (ami9000/host/ctl.py PATH 'LS.port_call("print(port.peek16(0x41c))")')
--                   asks the port and the original: LS.port_call(LUA) runs a line in the port's Lua shell, LS.port_binary(LUA) a call that answers BIN, LS.snap the original's ram at the point (LS.snap.ram), LS.ram16(addr) the original's live memory
--     calls=1       also compare the number of calls of every function between two sync points (finds where a difference starts)
--     startfn=HEX   (with state=) the original's state is inside a function that has no sync point for a long time (the shop: state=shop-before-512k startfn=8ff2): the port is given the memory at once and runs that function
--     resync=N      after more than N frames between two sync points (a disk load) the port takes the original's memory and the baseline starts again (with startfn=: 120)
--     gate=0        (default on) a disk load of the port waits for the original's: the port takes the original's memory at the instant its load returns and goes on from there, the comparison starts again
--     health=full   (with state=) the player's energy is set to full in the state before the run
--     syncs=N       how many synchronisation points to compare (default 300)
--     tolerant=1    also leave out what depends on how many interrupts the original took while its (slow) code ran: the music player, the sound requests and the joystick bytes (the interrupt fills them), the hardware mirror, the random generator
--     continue=1    go on after a difference
--     mem=0, pic=0  leave the memory / the picture out
--     ignore=       more members to leave out (the default list: counters that depend on the timing of the interrupts)
--     joy=          the stick from sync point SYNC on, DIRS as in the oracle (up down left right fire joined by +, or none), on both sides
--     show=         print these members of both sides at the first sync points
--     strict=1      report every difference, not only the changes since the first comparison (the baseline)
--     addr=HEX,HEX  the original functions whose entries are the sync points (default 2026,202e: the thunks of swap_screens; $2034 loops to itself while it waits)
--     port=HEX      the function the port is stopped at for each point (default 2034: the C swap_screens)
--     report=FILE   the log is also written here
-- Needs dev/members.lua (tools/gen_symbols.py) and the dev build (make -C dev).
--
-- What is compared, and why this way: the port runs its code instantly, the original takes real cycles, so the two sides cannot be compared at the same instant;
-- they are compared at the same point of the program (an entry of swap_screens), where both have done the same work.  What the original's memory held before the game
-- started (junk the game overwrites later, pointer fields with random bits that cannot be held the same way in the port) is the baseline: only changes are reported.
-- The picture: the emulator's frame one frame after the sync point against the port's picture at the sync point (the flip takes effect at the next vertical blank);
-- where the port's 320 x 256 picture sits in the emulator's frame is found from the first point by the shift at which the lit pixels coincide.
local args = ami.args or ""
local SYNCS = tonumber(args:match("syncs=(%d+)") or (args:match("play=1") and "1000000000" or "300"))
local ADDRS = {}
for a in (args:match("addr=(%S+)") or "2026,202e"):gmatch("%x+") do ADDRS[#ADDRS + 1] = tonumber(a, 16) end
local PORT_ADDR = tonumber(args:match("%f[%w]port=(%x+)") or "2034", 16)
local COMPARE_PIC = args:match("pic=0") == nil
local COMPARE_MEM = args:match("mem=0") == nil
local INLAG = args:match("inlag=(%w+)") or "auto"   -- inlag=auto (default): at the sync points reached from the shop's code the port gets the stick one sync point later than the original (1: everywhere, 0: nowhere)
local held_joy
local shop_loaded = false
local TRACEMEM
do local a, l, f, t = (args:match("tracemem=(%S+)") or ""):match("^(%x+),(%d+),(%d+),(%d+)$"); if a then TRACEMEM = { tonumber(a, 16), tonumber(l), tonumber(f), tonumber(t) } end end   -- tracemem=ADDR,LEN,FROM,TO: both sides' bytes at those sync points
local NOSPR = args:match("nospr=1") ~= nil   -- debug: sprite DMA off in the original at every frame
local CONTINUE = args:match("continue=1") ~= nil
function set_continue(on) CONTINUE = on end   -- from the control shell: ctl.py SOCK 'set_continue(false)' stops the run at the next difference again
local STRICT = args:match("strict=1") ~= nil
local REPORT = args:match("report=(%S+)")
local PLAY = args:match("play=1") ~= nil   -- you play: arrows = stick, Ctrl / Space / Return = fire, in the emulator's window (without --headless); implies hold=1, no sync limit
local AUTO = args:match("auto=1") ~= nil   -- the autoplayer (host/autoplay.lua) plays: the stick from the original's memory at each sync point; the input is written to captures/input/play-latest.txt as when played
local LIVE = (PLAY or AUTO or args:match("live=1") ~= nil) and args:match("live=0") == nil   -- the port is stepped a frame at a time beside the emulator, its panel shows it live (it waits at each sync point for the original); off for headless regression runs
local HOLD = PLAY or args:match("hold=1") ~= nil   -- (a replay=FILE of a run is shown in the window with the three views too)   -- at the first difference stay paused (with --control PATH: LS.port_call(LUA) runs Lua in the port, LS.snap is the original's memory and frame at the point)
local CALLS = args:match("calls=1") ~= nil   -- also compare how often each function of the game was called between two sync points (the code table of the port)
local RESYNC = args:match("resync=(%d+)")   -- resync=N: after a gap of more than N emulator frames between two sync points (a disk load: the original takes its real time, the port none) the port takes the original's memory and the comparison starts again from there (default with startfn=: 120)
local STARTFN = args:match("startfn=(%x+)")   -- with state=: the port runs the function with this original address (8ff2: shop_run) on the original's memory as it is at the start, instead of taking it at the first sync point
local STATE = args:match("state=(%S+)")   -- the original starts from local/save/NAME.state (a level running); the port plays itself to the level and takes the original's memory at the first sync point
local ROOT = ami.script_dir .. "/.."

-- members whose values depend on when the interrupts fall relative to the code, not on what the code does
local IGNORE = { vbl_count = true, v_0404 = true, exc_vectors_lo = true, exc_vectors_hi = true }
if args:match("tolerant=1") then
    for _, name in ipairs{ "music_accum", "music_channels", "hw_mirror", "random_a", "random_b", "sfx_channels", "sfx_request", "sfx_samples", "sfx_samples_ready", "v_0414", "v_0418", "irq_joy", "irq_joy_prev", "v_0a00", "v_0a01", "tile_mask_buf" } do IGNORE[name] = true end
end
if args:match("play=1") or args:match("replay=") then IGNORE.key_table = true; IGNORE.key_code = true end   -- (the keys reach the original's keyboard when they are pressed, the port's at the next sync point)
for name in (args:match("ignore=(%S+)") or ""):gmatch("[%w_]+") do IGNORE[name] = true end
local SHOW = {}
for name in (args:match("show=(%S+)") or ""):gmatch("[%w_]+") do SHOW[#SHOW + 1] = name end

local RECORD = args:match("record=(%S+)") or "captures/input/play-latest.txt"   -- where the applied input of a played or auto-played run is written
local REPLAY = args:match("replay=(%S+)")   -- the input of a played run (captures/input/play-latest.txt), as it was applied: the same run again, e.g. headless
local replay_keys = {}   -- [sync] = { {raw, down}, ... }
local joys = {}
for at, dirs in (args:match("joy=(%S+)") or ""):gmatch("(%d+):([%a+]+)") do
    local st = { up = 0, down = 0, left = 0, right = 0, fire = 0 }
    for d in dirs:gmatch("%a+") do if st[d] then st[d] = 1 end end
    joys[tonumber(at)] = st
end

ami.machine{
    name = "Xenon2 lockstep", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    content = "../local/xenon2.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_joyport_order = "2143" },
    peers = { port = { ROOT .. "/dev/build/xenon2-sdl-dev", "--lua", ROOT .. "/dev/lua/shell.lua", "--lua-args", "-", "--disks", ROOT .. "/local" } },
}
ami.boot()
if REPLAY then
    local text = ami.file_read(REPLAY, 0, ami.file_size(REPLAY))
    for line in text:gmatch("[^\n]+") do
        local at, u, d, l, r, f = line:match("^(%d+) joy (%d) (%d) (%d) (%d) (%d)$")
        if at then joys[tonumber(at)] = { up = tonumber(u), down = tonumber(d), left = tonumber(l), right = tonumber(r), fire = tonumber(f) } end
        local kat, raw, down = line:match("^(%d+) key (%d+) (%d)$")
        if kat then
            replay_keys[tonumber(kat)] = replay_keys[tonumber(kat)] or {}
            table.insert(replay_keys[tonumber(kat)], { tonumber(raw), tonumber(down) })
        end
    end
end
local autoplay = AUTO and load(ami.file_read("host/autoplay.lua", 0, ami.file_size("host/autoplay.lua")))()(ami.peek16, ami.peek32) or nil
local started = false
if STATE then
    ami.wait(1)
    assert(ami.snapshot_load(STATE .. ".state"), "cannot load " .. STATE)
    if args:match("health=full") then ami.poke16(0xcc6, 0x27); ami.poke8(0xa6b, 0xff); ami.log(string.format("health=full: energy now $%x", ami.peek16(0xcc6))) end   -- the player's energy ($CC6, 39 bar cells) and the flag that redraws the bar; the port takes the original's memory at the first sync point
    started = true
end
local peer = ami.peer("port")

-- The port is driven through its interactive Lua shell (dev/lua/shell.lua on the peer's stdin/stdout): a line of Lua goes in, what it prints comes back up to the prompt,
-- which is set to a line this script can wait for.  Binary results (the memory, the picture) come as `BIN LEN` and LEN bytes.
local READY = "<<READY>>"
local function wait_ready()
    local out = {}
    while true do
        local line = peer:readline()
        assert(line, "the port ended")
        if line:sub(-#READY) == READY then
            local before = line:sub(1, -#READY - 1)
            if before ~= "" and before ~= "> " then out[#out + 1] = before end
            return table.concat(out, "\n")
        end
        out[#out + 1] = line
    end
end
local function port_call(code)
    peer:send(code .. "\n")
    return wait_ready()
end
local function port_binary(code)
    peer:send(code .. "\n")
    local header = peer:readline()
    local length = tonumber((header or ""):match("^BIN (%d+)$"))
    assert(length, "the port answered " .. tostring(header))
    local data = peer:read(length)
    assert(data and #data == length, "the port ended")
    wait_ready()
    return data
end
port_call('shell_prompt, shell_prompt2 = "' .. READY .. '\\n", "' .. READY .. '\\n"')
for _, line in ipairs{
    'function joy(u, d, l, r, f) port.joy{ up = u == 1, down = d == 1, left = l == 1, right = r == 1, fire = f == 1 } end',
    'function bin(s) io.write(("BIN %d\\n"):format(#s)); io.write(s) end',
    'function ram_out() bin(port.ram()) end',
    'function image_out() bin(port.picture()) end',
    'function key(raw, down) port.key(raw, down == 1) end',
    'function ram_in(n) port.load_ram(io.read(n)) end',
    'function high_in(n) port.load_high(io.read(n)) end',
    -- run until the function with the original address `addr` has been entered once: "synced FRAME COUNT", or "unsynced FRAME" after 20000 frames
    -- the entries of the sync function are counted by one hook; sync(addr) runs to the next one ("synced FRAME N"), live() one frame, or to the entry ("entry" / "frame")
    'entries = 0; function count_entries(addr) port.hook(addr, function() entries = entries + 1 end) end',
    'function sync(addr) local start = entries; port.stop_on(addr); local ok = false; for _ = 1, 20000 do if port.disk_waiting() then break end; if port.step() == "hook" and entries > start then ok = true; break end end; port.stop_on(nil); print(ok and ("synced %d %d"):format(port.frame(), entries - start) or (port.disk_waiting() and "gated" or ("unsynced %d"):format(port.frame()))) end',
    'function live(addr) local start = entries; port.stop_on(addr); local what = port.step(); port.stop_on(nil); print((what == "hook" and entries > start) and "entry" or "frame") end',
} do port_call(line) end
port_call(("count_entries(0x%x)"):format(PORT_ADDR))
local members = load(ami.file_read("dev/members.lua", 0, ami.file_size("dev/members.lua")))()

local log_lines = {}
local function say(...)
    local line = string.format(...)
    ami.log(line)
    log_lines[#log_lines + 1] = line
    if REPORT then ami.save_file(REPORT, table.concat(log_lines, "\n") .. "\n") end
end
local function hex(img, from, count)
    return (img:sub(from, from + count - 1):gsub(".", function(c) return string.format("%02x", c:byte()) end))
end

-- ---- the emulator's side: a snapshot at every sync point, from the first instruction of the game on ---------------------------------------------------------------------
local start_ram
local fire_until = 0
local GATE = args:match("gate=0") == nil
local RESYNC_GAP = (not GATE) and (tonumber(RESYNC) or (args:match("startfn=") and 120 or nil)) or nil
local port_entries = 0   -- the entries of the sync function the port has reached (live stepping parks it at one until the original compares it)
local last_snap_frame = nil
local pending, n = {}, 0
local FRAME_W, FRAME_H = 720, 287      -- the emulator's frame
local ring = {}                        -- ring[frame] = the emulator's whole frame at that frame (the last 16)

local input_log = ""
local stick, applied_stick = { up = 0, down = 0, left = 0, right = 0, fire = 0 }, { up = 0, down = 0, left = 0, right = 0, fire = 0 }
local KEY_STICK = { [273] = "up", [274] = "down", [275] = "right", [276] = "left", [305] = "fire", [306] = "fire", [32] = "fire", [13] = "fire" }   -- libretro key codes
local fire_keys = {}
local OPPOSITE = { up = "down", down = "up", left = "right", right = "left" }
-- libretro key code -> Amiga raw key code: the keys of the window go to the original's keyboard as they are, and to the port as the same raw codes (at the next sync point)
local RAW = { [273] = 0x4c, [274] = 0x4d, [275] = 0x4e, [276] = 0x4f, [305] = 0x63, [306] = 0x63, [32] = 0x40, [13] = 0x44, [27] = 0x45, [9] = 0x42, [8] = 0x41, [127] = 0x46, [303] = 0x61, [304] = 0x60 }
for i, c in ipairs{ "q", "w", "e", "r", "t", "y", "u", "i", "o", "p" } do RAW[c:byte()] = 0x10 + i - 1 end
for i, c in ipairs{ "a", "s", "d", "f", "g", "h", "j", "k", "l" } do RAW[c:byte()] = 0x20 + i - 1 end
for i, c in ipairs{ "z", "x", "c", "v", "b", "n", "m" } do RAW[c:byte()] = 0x31 + i - 1 end
for d = 1, 9 do RAW[48 + d] = d end
RAW[48] = 0x0a
for f = 1, 10 do RAW[281 + f] = 0x4f + f end
local LIBRETRO_OF = {}   -- Amiga raw code -> a libretro key code (for replaying keys into the original)
for libretro, raw in pairs(RAW) do
    if not LIBRETRO_OF[raw] or libretro < LIBRETRO_OF[raw] then LIBRETRO_OF[raw] = libretro end
end
local key_queue = {}   -- {raw, down} since the last sync point
local function read_keys()   -- what was pressed since the last call: the stick that is held now
    for _, e in ipairs(ami.input()) do
        if e[1] == "k" and RAW[e[2]] then key_queue[#key_queue + 1] = { RAW[e[2]], e[3] } end
        if e[1] == "k" and KEY_STICK[e[2]] then
            local what = KEY_STICK[e[2]]
            if what == "fire" then fire_keys[e[2]] = e[3] == 1 and true or nil; stick.fire = next(fire_keys) and 1 or 0
            else
                stick[what] = e[3]
                -- opposite directions exclude each other (a stick cannot have both; the emulator would drop one, the port would take both): the last one pressed wins
                if e[3] == 1 then stick[OPPOSITE[what]] = 0 end
            end
        end
    end
end
-- snap=HEXADDR:NAME: the first time the original reaches that address a savestate (local/save/NAME.state, one frame later) and its memory (captures/NAME.ram) are saved
local snap_request
do
    local addr, name = args:match("snap=(%x+):(%S+)")
    if addr then
        local done_once = false
        ami.hook(tonumber(addr, 16), function()
            if done_once then return end
            done_once = true
            ami.save_file("captures/" .. name .. ".ram", ami.read(0, 0x80000))
            snap_request = name
        end)
    end
end
local deferred_keys = {}   -- releases of keys that were pressed in the same interval: the keyboard takes one byte a frame, so they go one sync point later
local function split_keys(list)
    local out, pressed = {}, {}
    for _, k in ipairs(deferred_keys) do out[#out + 1] = k end
    deferred_keys = {}
    for _, k in ipairs(list) do
        if k[2] == 1 then pressed[k[1]] = true end
        if k[2] == 0 and pressed[k[1]] then deferred_keys[#deferred_keys + 1] = k else out[#out + 1] = k end
    end
    return out
end
local call_names, call_counts = {}, {}   -- the functions of the code table: original address -> name; the calls since the last sync point on the original's side
local last_sync_frame = 0
local function sync_point()
    if not started then return end
    last_sync_frame = ami.frame()
    n = n + 1
    if n > SYNCS then return end
    local calls = call_counts
    call_counts = {}
    pending[#pending + 1] = { n = n, frame = ami.frame(), calls = calls, ram = (COMPARE_MEM or (STATE and n == 1)) and ami.read(0, 0x80000) or nil }
    local st = joys[n]
    if AUTO then   -- the autoplayer decides from the original's memory as it is now
        local want = autoplay(n)
        for k, v in pairs(want) do stick[k] = v end
    end
    if PLAY or AUTO then   -- the stick as it is held now, on both sides from here on
        if PLAY then read_keys() end
        local changed = false
        for k, v in pairs(stick) do if applied_stick[k] ~= v then changed = true end end
        if changed then
            st = { up = stick.up, down = stick.down, left = stick.left, right = stick.right, fire = stick.fire }
            for k, v in pairs(stick) do applied_stick[k] = v end
        end
    end
    pending[#pending].shop = shop_loaded   -- the shop's overlay (sector 1476) is the last thing loaded at $54E00 (the level's code lives there too)
    do
        local list = PLAY and key_queue or replay_keys[n] or {}
        if PLAY then key_queue = {} end
        if #list > 0 or #deferred_keys > 0 then
            pending[#pending].keys = split_keys(list)
            if not PLAY then
                for _, k in ipairs(pending[#pending].keys) do   -- the original gets the keys as they were typed (the keyboard is what enters the shop); in play it has them from the window
                    local code = LIBRETRO_OF[k[1]]
                    if code then ami.key(code, k[2] == 1) end
                end
            end
        end
    end
    if PLAY or AUTO then   -- what is applied is written down: the run can be played again (replay=)
        local lines = {}
        for _, k in ipairs(pending[#pending].keys or {}) do lines[#lines + 1] = ("%d key %d %d\n"):format(n, k[1], k[2]) end
        if st then lines[#lines + 1] = ("%d joy %d %d %d %d %d\n"):format(n, st.up, st.down, st.left, st.right, st.fire) end
        if #lines > 0 then input_log = input_log .. table.concat(lines); ami.save_file(RECORD, input_log) end
    end
    if st then
        ami.joystick{ up = st.up == 1, down = st.down == 1, left = st.left == 1, right = st.right == 1, fire = st.fire == 1 }
        pending[#pending].joy = st   -- the port gets it after it has reached this point (it is behind the original): the next interval on both sides
    end
end
for _, a in ipairs(ADDRS) do ami.hook(a, sync_point) end

if CALLS then
    local table_c = ami.file_read("native/src/code_table.c", 0, ami.file_size("native/src/code_table.c"))
    local skip = { [PORT_ADDR] = true }
    for _, a in ipairs(ADDRS) do skip[a] = true end
    for text, name in table_c:gmatch("{ 0x(%x+), %(void %*%)([%w_]+) }") do
        local addr = tonumber(text, 16)
        if not skip[addr] and not call_names[addr] then
            call_names[addr] = name
            ami.hook(addr, function() call_counts[addr] = (call_counts[addr] or 0) + 1 end)
        end
    end
end

if STATE then
    -- the port plays to the level by itself: the fire button on a schedule until the level's per-frame update has run a few times
    port_call('function to_level() local n, f = 0, 0; port.hook(0x35d4, function() n = n + 1 end); while n < 5 and f < 6000 do f = f + 1; local k = f % 150; if k == 100 then port.joy{ fire = true } elseif k == 110 then port.joy{} end; port.step() end; port.unhook(0x35d4); port.joy{}; print(n, port.frame()) end')
    say("the original starts from the state %s (emulator frame %d); the port plays to the level: %s", STATE, ami.frame(), port_call("to_level()"))
    if STARTFN then
        peer:send("ram_in(524288)\n")
        peer:send(ami.read(0, 0x80000))
        wait_ready()
        port_call(("port.run_function(0x%s)"):format(STARTFN))
        say("the port runs the function $%s on the original's memory", STARTFN)
    end
else
    -- up to the start of the game: the fire presses that pass the intro, then nothing; the memory at the very first instruction of the game goes to the port
    ami.hook(0x7032, function() started = true; start_ram = ami.read(0, 0x80000) end)
    while not started do
        local f = ami.frame()
        if f >= 5200 and f % 150 == 0 then ami.joystick{ fire = true }; fire_until = f + 10 end
        if fire_until ~= 0 and f >= fire_until then ami.joystick(); fire_until = 0 end
        ami.wait(1)
    end
    ami.joystick()
    say("the game starts at emulator frame %d", ami.frame())
    peer:send("ram_in(524288)\n")
    peer:send(start_ram)
    wait_ready()
end

-- the window of the emulator's frame that is the port's picture (x, y of its first column and row, sx columns a pixel), x and y in the emulator's frame
local function cut(shot, win)
    local rows = {}
    for yy = 0, 255 do
        local parts = {}
        for xx = 0, 319 do
            local i = ((win.y + yy) * FRAME_W + win.x + xx * win.sx) * 3 + 1
            parts[#parts + 1] = (i >= 1 and i + 2 <= #shot) and shot:sub(i, i + 2) or "\0\0\0"
        end
        rows[#rows + 1] = table.concat(parts)
    end
    return table.concat(rows)
end

if CALLS then
    port_call('calls = {}')
    port_call('function count_calls(list) for a in list:gmatch("%d+") do a = tonumber(a); port.hook(a, function() calls[a] = (calls[a] or 0) + 1 end) end end')
    port_call('function calls_out() local t = {}; for a, c in pairs(calls) do t[#t + 1] = a .. "=" .. c end; calls = {}; print(table.concat(t, ",")) end')
    local list = {}
    for addr in pairs(call_names) do list[#list + 1] = tostring(addr) end
    port_call(('count_calls("%s")'):format(table.concat(list, ",")))
end

local shop_reported = false
local panels_shown, warned_window, picture_synced = false, false, false
local shifted_pictures, shifted_by = 0, { [1] = 0, [-1] = 0 }   -- pictures that equal the original's frame next to the usual one
-- where the port's 320 x 256 picture sits in the emulator's frame: given (window=X,Y,SX,LAG; level 1 from a state: 40,18,2,1), else found from the first pictures that have few enough lit pixels to match
local win
do
    local x, y, sx, lag = args:match("window=(%d+),(%d+),(%d+),(%-?%d+)")
    if x then win = { x = tonumber(x), y = tonumber(y), sx = tonumber(sx), lag = tonumber(lag) }
    elseif STATE then win = { x = 40, y = 18, sx = 2, lag = 1 } end
end
local function calibrate(port_pic, snap)
    local lit = {}
    for i = 0, 320 * 256 - 1 do if port_pic:sub(i * 3 + 1, i * 3 + 3) ~= "\0\0\0" then lit[#lit + 1] = { i % 320, i // 320 } end end
    if #lit == 0 or #lit > 4000 then return end
    local best = { n = -1 }
    for f = snap.frame - 8, snap.frame + 1 do
        local shot = ring[f]
        if shot then
            for sx = 1, 2 do
                for dx = 0, 200 do
                    for dy = -4, 30 do
                        local hit = 0
                        for _, q in ipairs(lit) do
                            local i = ((q[2] + dy) * FRAME_W + dx + q[1] * sx) * 3 + 1
                            if i >= 1 and i + 2 <= #shot and (shot:byte(i) ~= 0 or shot:byte(i + 1) ~= 0 or shot:byte(i + 2) ~= 0) then hit = hit + 1 end
                        end
                        if hit > best.n then best = { n = hit, dx = dx, dy = dy, sx = sx, frame = f } end
                    end
                end
            end
        end
    end
    if best.n >= math.max(1, #lit // 2) then
        win = { x = best.dx, y = best.dy, sx = best.sx, lag = best.frame - snap.frame }
        say("picture window in the emulator's frame: x %d y %d, %d columns a pixel; %d of the port's %d lit pixels coincide, with the emulator's frame %+d from the sync point",
            win.x, win.y, win.sx, best.n, #lit, win.lag)
    end
end

-- the bytes that differ in a member: a list of {first, last} runs, found by blocks of 64 bytes
local function different_bytes(a, b)
    local runs, size = {}, #a
    for block = 1, size, 64 do
        if a:sub(block, block + 63) ~= b:sub(block, block + 63) then
            local last = math.min(block + 63, size)
            for i = block, last do
                if a:byte(i) ~= b:byte(i) then
                    local run = runs[#runs]
                    if run and run[2] == i - 1 then run[2] = i else runs[#runs + 1] = { i, i } end
                end
            end
        end
    end
    return runs
end

-- A function has several addresses in the original (its routine, the thunk of the jump table): the port writes one of them where the original may have written another.
-- Longs that are two addresses of the same function are the same.
local alias_of = {}   -- original address -> the lowest address of the same function
do
    local table_c = ami.file_read("native/src/code_table.c", 0, ami.file_size("native/src/code_table.c"))
    local lowest = {}
    for text, name in table_c:gmatch("{ 0x(%x+), %(void %*%)([%w_]+) }") do
        local addr = tonumber(text, 16)
        if not lowest[name] or addr < lowest[name] then lowest[name] = addr end
    end
    for text, name in table_c:gmatch("{ 0x(%x+), %(void %*%)([%w_]+) }") do alias_of[tonumber(text, 16)] = lowest[name] end
end
local function long_at(img, i) local a, b, c, d = img:byte(i, i + 3); return d and ((a << 24) | (b << 16) | (c << 8) | d) or nil end
local function same_function(a, b, i)   -- the longs at byte i (1-based) of both: two addresses of one function?
    local x, y = long_at(a, i), long_at(b, i)
    return x and y and x ~= y and alias_of[x] and alias_of[x] == alias_of[y]
end

-- Baseline: at the first sync point every byte that differs is junk (what the original's memory held before the game, stale bits of pointers the port cannot hold the same
-- way); from then on a byte counts only if it did not differ at the first point.
local baseline   -- baseline[name] = set of byte indexes (1-based within the member) that differed at the first sync point
local function compare_memory(snap, port_ram)
    local first = baseline == nil
    if first then baseline = {} end
    local bad, shown = 0, 0
    for _, m in ipairs(members) do
        local addr, size, name = m[1], m[2], m[3]
        local a, b = snap.ram:sub(addr + 1, addr + size), port_ram:sub(addr + 1, addr + size)
        if not IGNORE[name] and a ~= b then
            local runs = different_bytes(a, b)
            do   -- (bytes that are part of two addresses of one function are not different)
                local kept = {}
                for _, run in ipairs(runs) do
                    for i = run[1], run[2] do
                        local alias = false
                        for start = i - 3, i do
                            if start >= 1 and same_function(a, b, start) then alias = true; break end
                        end
                        if not alias then kept[#kept + 1] = i end
                    end
                end
                runs = {}
                for _, i in ipairs(kept) do
                    local run = runs[#runs]
                    if run and run[2] == i - 1 then run[2] = i else runs[#runs + 1] = { i, i } end
                end
            end
            if first and not STRICT then
                local set = {}
                for _, run in ipairs(runs) do for i = run[1], run[2] do set[i] = true end end
                baseline[name] = set
            else
                local mask, k, count = baseline[name] or {}, nil, 0
                for _, run in ipairs(runs) do
                    for i = run[1], run[2] do
                        if not mask[i] then count = count + 1; k = k or i end
                    end
                end
                if count > 0 then
                    bad = bad + 1
                    if shown < 12 then
                        shown = shown + 1
                        say("   memory %-26s $%06x +%-5d (size %d, %d bytes differ): original %s port %s", name, addr, k - 1, size, count, hex(a, k, 8), hex(b, k, 8))
                        local from = math.max(1, k - 14)
                        say("      around it (from +%d): original %s  port %s", from - 1, hex(a, from, 28), hex(b, from, 28))
                    end
                end
            end
        end
    end
    if first and not STRICT then
        local junk = 0
        for _ in pairs(baseline) do junk = junk + 1 end
        say("baseline at the first sync point: %d members have bytes that differ (what the original's memory held before; stale bits); only other bytes are reported from here", junk)
        return 0
    end
    return bad
end

local function compare_picture(snap, port_pic)
    if not win then calibrate(port_pic, snap) end
    if not win then
        if PLAY and not warned_window then warned_window = true; say("play: the port's picture window in the emulator's frame is not known yet (the picture has to show something to be matched)") end
        return 0
    end
    local frame = ring[snap.frame + win.lag] or ring[snap.frame + 1]
    if not frame then return 0 end
    local original = cut(frame, win)
    local diffs, first, diff_image = ami.rgb_diff(original, port_pic)
    if diffs > 0 then
        -- the flip takes effect at a vertical blank: whether the original's frame is the one at the usual lag or the one next to it depends on where the interrupt fell
        for _, d in ipairs{ 1, -1 } do
            local f = ring[snap.frame + win.lag + d]
            if f then
                local near = cut(f, win)
                if ami.rgb_diff(near, port_pic) == 0 then
                    shifted_pictures = shifted_pictures + 1
                    shifted_by[d] = shifted_by[d] + 1
                    original, diffs, diff_image = near, 0, nil
                    break
                end
            end
        end
    end
    if PLAY or AUTO or REPLAY then   -- the three views: the original (the window), the port, the differences
        if not panels_shown then panels_shown = true; say("the three views: the original (the window), the port (first panel to its right), the differences (second panel)") end
        ami.panel(1, port_pic, 320, 256, 2, 1, 0)
        ami.panel(2, diff_image or ("\0"):rep(320 * 256 * 3), 320, 256, 2, 1, 0)
    end
    if diffs == 0 then
        if STATE and not picture_synced then picture_synced = true; say("the pictures agree from sync point %d on (until then the port's registers, which come from its own run, differ from the state's)", snap.n) end
        return 0
    end
    if STATE and not picture_synced and snap.n <= 60 then return 0 end   -- not yet: the colours and the scroll come from registers the game rewrites within a few frames
    say("   picture: %d pixels differ, the first at %d,%d", diffs, (first - 1) % 320, (first - 1) // 320)
    do   -- against the emulator's frames around it: is the original a frame ahead or behind?
        local near = {}
        for d = -3, 2 do
            local f = ring[snap.frame + win.lag + d]
            if f then near[#near + 1] = ("%+d:%d"):format(d, ami.rgb_diff(cut(f, win), port_pic)) end
        end
        say("   the port's picture against the emulator's frames (relative to the one compared): %s", table.concat(near, " "))
    end
    local shown, rows = 0, {}
    for i = 0, 320 * 256 - 1 do
        local a, b = original:sub(i * 3 + 1, i * 3 + 3), port_pic:sub(i * 3 + 1, i * 3 + 3)
        if a ~= b then
            local y, x = i // 320, i % 320
            local r = rows[y]
            if not r then r = { x, x, a, b }; rows[y] = r else r[2] = x end
            if shown < 6 then
                shown = shown + 1
                say("   pixel %3d,%3d original %02x%02x%02x port %02x%02x%02x", i % 320, i // 320, a:byte(1), a:byte(2), a:byte(3), b:byte(1), b:byte(2), b:byte(3))
            end
        end
    end
    local list = {}
    for y in pairs(rows) do list[#list + 1] = y end
    table.sort(list)
    say("   rows with differences: %d, from %d to %d", #list, list[1], list[#list])
    for k = 1, math.min(#list, 8) do
        local r = rows[list[k]]
        say("   row %3d: x %d..%d, first original %02x%02x%02x port %02x%02x%02x", list[k], r[1], r[2], r[3]:byte(1), r[3]:byte(2), r[3]:byte(3), r[4]:byte(1), r[4]:byte(2), r[4]:byte(3))
    end
    return diffs
end

local loads = {}   -- the original'"'"'s memory at the return of each of its disk loads, in order
if GATE then
    port_call("port.disk_gate(true)")
    ami.hook(0x921a, function()
        if ami.reg("a0") == 0x54e00 then shop_loaded = (ami.reg("d0") & 0xffff) == 1476 end
        local ret = ami.peek32(ami.reg("a7"))
        ami.hook(ret, function() ami.unhook(ret); loads[#loads + 1] = ami.read(0, 0x80000) end)
    end)
end
local function service_gate()   -- the port waits inside its loader: it goes on with the original's memory as its load ended
    while #loads == 0 do ami.wait(1) end
    local ram = table.remove(loads, 1)
    peer:send("ram_in(524288)\n")
    peer:send(ram)
    wait_ready()
    port_call("port.disk_release()")
    baseline = nil
    say("a disk load (emulator frame %d): the port took the original's memory at the end of its load and went on from there; the comparison starts again", ami.frame())
end
local desired_joy, port_joy   -- the stick the original has been given, and the one the port has
-- The original's interrupt decodes the stick into v_0a00 in the middle of a slow frame: the routines that read it before see the old stick, the ones after
-- the new.  The routines that read it are hooked on the original and the byte each saw is recorded in order, by the sync point before the interval;
-- the port is told the list and sets v_0a00 to each value as it enters the same routine.
local STICK_READERS = { 0x5de6, 0x68b0, 0x3b94, 0x3cdc, 0x3e2c, 0x4340, 0x6372 }
local reads = {}
for _, a in ipairs(STICK_READERS) do ami.hook(a, function() local r = reads[n]; if not r then r = {}; reads[n] = r end; r[#r + 1] = ("{0x%x,%d}"):format(a, ami.peek8(0xa00)) end) end
port_call("rseq, ridx = nil, 1; function rhook(a) local e = rseq and rseq[ridx]; if e and e[1] == a then port.poke_stick(e[2]); ridx = ridx + 1 end end")
for _, a in ipairs(STICK_READERS) do port_call(("port.hook(0x%x, rhook)"):format(a)) end
local function stick_bits(st) return (st.up == 1 and 1 or 0) | (st.down == 1 and 2 or 0) | (st.left == 1 and 4 or 0) | (st.right == 1 and 8 or 0) | (st.fire == 1 and 0x80 or 0) end
local function send_high()   -- the original's memory above 512 KB, which the port does not have (the map masks of bobs above the screen read it)
    peer:send("high_in(524288)\n")
    peer:send(ami.read(0x80000, 0x80000))
    wait_ready()
end
local function apply_joy(snap)
    if snap.n == 1 or snap.n % 200 == 0 then send_high() end
    local r = reads[snap.n - 1]
    reads[snap.n - 1] = nil
    port_call(("rseq, ridx = {%s}, 1"):format(r and table.concat(r, ",") or ""))
    local d = desired_joy
    if not d then return end
    if not r and port_joy and INLAG ~= "0" and (INLAG == "1" or snap.shop) and held_joy ~= d then   -- no steering call: the shop's stick check runs late
        held_joy = d
        return
    end
    held_joy = d
    if port_joy and stick_bits(port_joy) == stick_bits(d) then return end
    port_joy = d
    port_call(("joy(%d, %d, %d, %d, %d)"):format(d.up, d.down, d.left, d.right, d.fire))
end
local prev_orig_ram, prev_port_ram, last_orig_ram, last_port_ram
local different, compared, mem_bad, pic_bad = 0, 0, 0, 0
local done = false
while not done do
    -- two frames after the point: the emulator's frame after it is in the ring
    while #pending > 0 and ami.frame() >= pending[1].frame + 2 do
        local snap = table.remove(pending, 1)
        apply_joy(snap)
        for _, k in ipairs(snap.keys or {}) do port_call(("key(%d, %d)"):format(k[1], k[2])) end   -- before the port runs to the point: a key that ends the pause is what lets it get there
        local reply
        if port_entries >= snap.n then
            reply = ("synced %d 1"):format(0)   -- (the live stepping has parked it at this entry)
            local frame = port_call("print(port.frame())")
            reply = "synced " .. frame .. " 1"
        else
            reply = port_call(("sync(0x%x)"):format(PORT_ADDR))
            while reply == "gated" do
                service_gate()
                reply = port_call(("sync(0x%x)"):format(PORT_ADDR))
            end
            port_entries = snap.n
        end
        if not reply or not reply:match("^synced") then say("lockstep: the port did not reach sync point %d (%s)", snap.n, tostring(reply)); done = true; break end
        if snap.joy then desired_joy = snap.joy end
        if STATE and snap.n == 1 and args:match("dumpram=(%S+)") then ami.save_file(args:match("dumpram=(%S+)"), snap.ram) end
        local gap = last_snap_frame and snap.frame - last_snap_frame or 0
        last_snap_frame = snap.frame
        if RESYNC_GAP and gap > RESYNC_GAP and snap.ram then
            peer:send("ram_in(524288)\n")
            peer:send(snap.ram)
            wait_ready()
            baseline = nil
            say("sync point %d came %d frames after the one before (a disk load): the port takes the original's memory, the comparison starts again here", snap.n, gap)
        end
        if STATE and snap.n == 1 and not STARTFN then   -- the port is in the level, at the same program point: the original's memory in
            peer:send("ram_in(524288)\n")
            peer:send(snap.ram)
            wait_ready()
        end
        local problems = {}
        if COMPARE_MEM then
            local port_ram = port_binary("ram_out()")
            if TRACEMEM and snap.n >= TRACEMEM[3] and snap.n <= TRACEMEM[4] then
                say("tracemem sync %d: original %s port %s", snap.n, hex(snap.ram, TRACEMEM[1] + 1, TRACEMEM[2]), hex(port_ram, TRACEMEM[1] + 1, TRACEMEM[2]))
            end
            if #SHOW > 0 and snap.n <= 8 then
                for _, m in ipairs(members) do
                    for _, want in ipairs(SHOW) do
                        if m[3] == want then say("   sync %d %-16s original %s port %s", snap.n, want, hex(snap.ram, m[1] + 1, math.min(m[2], 8)), hex(port_ram, m[1] + 1, math.min(m[2], 8))) end
                    end
                end
            end
            local bad = compare_memory(snap, port_ram)
            prev_orig_ram, prev_port_ram, last_orig_ram, last_port_ram = last_orig_ram, last_port_ram, snap.ram, port_ram
            if bad > 0 then mem_bad = mem_bad + 1; problems[#problems + 1] = ("%d named members differ"):format(bad) end
        end
        if CALLS then
            local port_calls = {}
            for a, c in port_call("calls_out()"):gmatch("(%d+)=(%d+)") do port_calls[tonumber(a)] = tonumber(c) end
            if snap.n >= 2 then
                -- by function name: the code table has several addresses for a function (the original's routine, its thunk, the labels the code runs through: the most any of them was entered); the interrupt's own (irq_*, music_tick) are left out: they run at other points
                local o, p, names = {}, {}, {}
                for addr, name in pairs(call_names) do
                    names[name] = true
                    o[name] = math.max(o[name] or 0, snap.calls[addr] or 0)
                    p[name] = math.max(p[name] or 0, port_calls[addr] or 0)
                end
                local list = {}
                for name in pairs(names) do
                    if o[name] ~= p[name] and not name:match("^irq_") and not name:match("^music_") and not name:match("^sfx_") then
                        list[#list + 1] = ("%s original %d port %d"):format(name, o[name], p[name])
                    end
                end
                table.sort(list)
                if #list > 0 then
                    problems[#problems + 1] = ("%d functions were called a different number of times"):format(#list)
                    for i = 1, math.min(#list, 25) do say("   calls %s", list[i]) end
                end
            end
        end
        if COMPARE_PIC then
            local d = compare_picture(snap, port_binary("image_out()"))
            if d > 0 then pic_bad = pic_bad + 1; problems[#problems + 1] = ("%d picture pixels differ"):format(d) end
        end
        compared = compared + 1
        if #problems > 0 then
            different = different + 1
            say("sync %d (emulator frame %d, port frame %s): %s", snap.n, snap.frame, reply:match("synced (%d+)") or "?", table.concat(problems, "; "))
            if not CONTINUE then
                ami.pause()
                if HOLD then
                    LS = { loads = loads, prev = function() return prev_orig_ram, prev_port_ram end, peer = peer, wait_ready = wait_ready, orig_picture = function(d) d = d or 0; return cut(ring[snap.frame + win.lag + d] or ring[snap.frame + 1], win) end, port_call = port_call, port_binary = port_binary, snap = snap, hex = hex, ram16 = function(a) return ami.peek16(a) end, ram32 = function(a) return ami.peek32(a) end }
                    say("holding at sync %d: both sides stand (the emulator is paused, the port is stopped in swap_screens)", snap.n)
                    while true do ami.wait(1) end
                end
                done = true; break
            end
        end
        if compared % 100 == 0 then say("%d sync points compared, %d with differences (memory %d, picture %d)", compared, different, mem_bad, pic_bad) end
        if compared >= SYNCS then done = true; break end
    end
    -- the original runs code the port has no C for (the shop program, loaded from the disk at $54E00): it passes no sync point
    if not done and started and n >= 1 and ami.frame() - last_sync_frame > 300 and ami.reg("pc") >= 0x54e00 and ami.reg("pc") < 0x80000 and not shop_reported then
        shop_reported = true
        say("sync point %d was the last: for %d frames the original has run code at $%06x, from the disk, that the port has no C for (the shop, fn_054e00): the port skipped it", n, ami.frame() - last_sync_frame, ami.reg("pc"))
        different = different + 1
        if not CONTINUE then
            ami.pause()
            if HOLD then
                LS = { port_call = port_call, port_binary = port_binary, hex = hex, ram16 = function(a) return ami.peek16(a) end, ram32 = function(a) return ami.peek32(a) end }
                say("holding: the original is in the shop, the port is stopped in swap_screens")
                while true do ami.wait(1) end
            end
            done = true
        end
    end
    if snap_request then
        ami.snapshot_save(snap_request .. ".state")
        say("saved the original's state local/save/%s.state and memory captures/%s.ram at emulator frame %d (pc $%06x)", snap_request, snap_request, ami.frame(), ami.reg("pc"))
        snap_request = nil
    end
    if not done then
        if PLAY then read_keys() end
        if COMPARE_PIC then
            if NOSPR then ami.poke16(0xdff096, 0x0020) end
            ring[ami.frame()] = ami.frame_rgb(0, 0, FRAME_W, FRAME_H, 1)
            ring[ami.frame() - 16] = nil
        end
        if LIVE and started and port_entries == compared and not done then   -- the port is not parked ahead: one frame, and its picture on the panel
            if port_call(("live(0x%x)"):format(PORT_ADDR)) == "entry" then port_entries = port_entries + 1 end
            if GATE and #loads > 0 and port_call('print(port.disk_waiting() and "gated" or "run")') == "gated" then service_gate() end
            ami.panel(1, port_binary("image_out()"), 320, 256, 2, 1, 0)
        end
        ami.wait(1)
    end
end
say("lockstep over: %d sync points compared, %d with differences (memory %d, picture %d); %d pictures matched the original's frame next to the usual one (%d the next, %d the previous)", compared, different, mem_bad, pic_bad, shifted_pictures, shifted_by[1], shifted_by[-1])
peer:send("port.quit()\n")
ami.quit()
