-- Lockstep of the original (this emulator) and the dev build of the port (dev/build/xenon2-sdl-dev, driven through its interactive Lua shell, dev/lua/shell.lua, on stdin/stdout): both run the game from its start (the port from
-- the original's memory at that moment); at every entry of swap_screens (the game passes it once a frame) the named members of the memory and the picture are compared.
-- The first difference pauses the emulator and the port is not stepped again, so both can be looked at (the emulator's --control socket); continue=1 goes on and lists them all.
--
--   ami9000_host host/lockstep.lua --headless --script-args "[syncs=N] [tolerant=1] [continue=1] [mem=0] [pic=0] [ignore=NAME,NAME] [joy=SYNC:DIRS,...] [show=NAME,NAME] [strict=1] [report=FILE]"
--     state=NAME    start from the emulator's savestate local/save/NAME.state (frame-8880: level 1 running) instead of the start of the game: the port plays to the level by itself
--                   (the fire button on the schedule of tools/verify_port.sh) and takes the original's memory at the first sync point
--     play=1        play it: ami9000_host host/lockstep.lua --script-args "play=1 state=frame-8880 tolerant=1"  (a window, not --headless): arrows = stick, Ctrl / Space / Return = fire;
--                   the keys are applied to both sides at the sync points; the port's picture and the difference are shown beside the original; the first difference pauses both
--     hold=1        stop at the first difference and stay there, both sides standing: with `ami9000_host --control PATH` a client (ami9000/host/ctl.py PATH 'LS.port_call("print(port.peek16(0x41c))")')
--                   asks the port and the original: LS.port_call(LUA) runs a line in the port's Lua shell, LS.port_binary(LUA) a call that answers BIN, LS.snap the original's ram at the point (LS.snap.ram), LS.ram16(addr) the original's live memory
--     calls=1       also compare the number of calls of every function between two sync points (finds where a difference starts)
--     syncs=N       how many synchronisation points to compare (default 300)
--     tolerant=1    also leave out what depends on how many interrupts the original took while its (slow) code ran: the music player, the sound requests (consumed by the interrupt), the hardware mirror, the random generator
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
local CONTINUE = args:match("continue=1") ~= nil
local STRICT = args:match("strict=1") ~= nil
local REPORT = args:match("report=(%S+)")
local PLAY = args:match("play=1") ~= nil   -- you play: arrows = stick, Ctrl / Space / Return = fire, in the emulator's window (without --headless); implies hold=1, no sync limit
local HOLD = PLAY or args:match("hold=1") ~= nil   -- at the first difference stay paused (with --control PATH: LS.port_call(LUA) runs Lua in the port, LS.snap is the original's memory and frame at the point)
local CALLS = args:match("calls=1") ~= nil   -- also compare how often each function of the game was called between two sync points (the code table of the port)
local STATE = args:match("state=(%S+)")   -- the original starts from local/save/NAME.state (a level running); the port plays itself to the level and takes the original's memory at the first sync point
local ROOT = ami.script_dir .. "/.."

-- members whose values depend on when the interrupts fall relative to the code, not on what the code does
local IGNORE = { vbl_count = true, v_0404 = true, exc_vectors_lo = true, exc_vectors_hi = true }
if args:match("tolerant=1") then
    for _, name in ipairs{ "music_accum", "music_channels", "hw_mirror", "random_a", "random_b", "sfx_channels", "sfx_request", "sfx_samples", "sfx_samples_ready", "v_0414", "v_0418" } do IGNORE[name] = true end
end
for name in (args:match("ignore=(%S+)") or ""):gmatch("[%w_]+") do IGNORE[name] = true end
local SHOW = {}
for name in (args:match("show=(%S+)") or ""):gmatch("[%w_]+") do SHOW[#SHOW + 1] = name end

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
local started = false
if STATE then
    ami.wait(1)
    assert(ami.snapshot_load(STATE .. ".state"), "cannot load " .. STATE)
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
    'function ram_in(n) port.load_ram(io.read(n)) end',
    -- run until the function with the original address `addr` has been entered once: "synced FRAME COUNT", or "unsynced FRAME" after 20000 frames
    'function sync(addr) local count, ok = 0, false; port.hook(addr, function() count = count + 1 end); port.stop_on(addr); for _ = 1, 20000 do if port.step() == "hook" and count >= 1 then ok = true; break end end; port.stop_on(nil); port.unhook(addr); print(ok and ("synced %d %d"):format(port.frame(), count) or ("unsynced %d"):format(port.frame())) end',
} do port_call(line) end
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
local pending, n = {}, 0
local FRAME_W, FRAME_H = 720, 287      -- the emulator's frame
local ring = {}                        -- ring[frame] = the emulator's whole frame at that frame (the last 16)

local stick, applied_stick = { up = 0, down = 0, left = 0, right = 0, fire = 0 }, { up = 0, down = 0, left = 0, right = 0, fire = 0 }
local KEY_STICK = { [273] = "up", [274] = "down", [275] = "right", [276] = "left", [305] = "fire", [306] = "fire", [32] = "fire", [13] = "fire" }   -- libretro key codes
local fire_keys = {}
local function read_keys()   -- what was pressed since the last call: the stick that is held now
    for _, e in ipairs(ami.input()) do
        if e[1] == "k" and KEY_STICK[e[2]] then
            local what = KEY_STICK[e[2]]
            if what == "fire" then fire_keys[e[2]] = e[3] == 1 and true or nil; stick.fire = next(fire_keys) and 1 or 0 else stick[what] = e[3] end
        end
    end
end
local call_names, call_counts = {}, {}   -- the functions of the code table: original address -> name; the calls since the last sync point on the original's side
local function sync_point()
    if not started then return end
    n = n + 1
    if n > SYNCS then return end
    local calls = call_counts
    call_counts = {}
    pending[#pending + 1] = { n = n, frame = ami.frame(), calls = calls, ram = (COMPARE_MEM or (STATE and n == 1)) and ami.read(0, 0x80000) or nil }
    local st = joys[n]
    if PLAY then   -- the stick as it is held now, on both sides from here on
        read_keys()
        local changed = false
        for k, v in pairs(stick) do if applied_stick[k] ~= v then changed = true end end
        if changed then
            st = { up = stick.up, down = stick.down, left = stick.left, right = stick.right, fire = stick.fire }
            for k, v in pairs(stick) do applied_stick[k] = v end
        end
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

local shifted_pictures, shifted_by = 0, { [1] = 0, [-1] = 0 }   -- pictures that equal the original's frame next to the usual one
local win
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
    if not win then return 0 end
    local frame = ring[snap.frame + win.lag] or ring[snap.frame + 1]
    if not frame then return 0 end
    local original = cut(frame, win)
    local diffs, first, diff_image = ami.rgb_diff(original, port_pic)
    if PLAY then
        ami.panel(1, port_pic, 320, 256, 2, 1, 0)
        if diff_image then ami.panel(2, diff_image, 320, 256, 2, 1, 0) end
    end
    if diffs == 0 then return 0 end
    -- the flip takes effect at a vertical blank: whether the original's frame is the one at the usual lag or the one next to it depends on where the interrupt fell
    for _, d in ipairs{ 1, -1 } do
        local f = ring[snap.frame + win.lag + d]
        if f and ami.rgb_diff(cut(f, win), port_pic) == 0 then shifted_pictures = shifted_pictures + 1; shifted_by[d] = shifted_by[d] + 1; return 0 end
    end
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
            rows[i // 320] = true
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
    return diffs
end

local different, compared, mem_bad, pic_bad = 0, 0, 0, 0
local done = false
while not done do
    -- two frames after the point: the emulator's frame after it is in the ring
    while #pending > 0 and ami.frame() >= pending[1].frame + 2 do
        local snap = table.remove(pending, 1)
        local reply = port_call(("sync(0x%x)"):format(PORT_ADDR))
        if not reply or not reply:match("^synced") then say("lockstep: the port did not reach sync point %d (%s)", snap.n, tostring(reply)); done = true; break end
        if snap.joy then
            local st = snap.joy
            port_call(("joy(%d, %d, %d, %d, %d)"):format(st.up, st.down, st.left, st.right, st.fire))
        end
        if STATE and snap.n == 1 and args:match("dumpram=(%S+)") then ami.save_file(args:match("dumpram=(%S+)"), snap.ram) end
        if STATE and snap.n == 1 then   -- the port is in the level, at the same program point: the original's memory in
            peer:send("ram_in(524288)\n")
            peer:send(snap.ram)
            wait_ready()
        end
        local problems = {}
        if COMPARE_MEM then
            local port_ram = port_binary("ram_out()")
            if #SHOW > 0 and snap.n <= 8 then
                for _, m in ipairs(members) do
                    for _, want in ipairs(SHOW) do
                        if m[3] == want then say("   sync %d %-16s original %s port %s", snap.n, want, hex(snap.ram, m[1] + 1, math.min(m[2], 8)), hex(port_ram, m[1] + 1, math.min(m[2], 8))) end
                    end
                end
            end
            local bad = compare_memory(snap, port_ram)
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
                    LS = { port_call = port_call, port_binary = port_binary, snap = snap, hex = hex, ram16 = function(a) return ami.peek16(a) end, ram32 = function(a) return ami.peek32(a) end }
                    say("holding at sync %d: both sides stand (the emulator is paused, the port is stopped in swap_screens)", snap.n)
                    while true do ami.wait(1) end
                end
                done = true; break
            end
        end
        if compared % 100 == 0 then say("%d sync points compared, %d with differences (memory %d, picture %d)", compared, different, mem_bad, pic_bad) end
        if compared >= SYNCS then done = true; break end
    end
    if not done then
        if PLAY then read_keys() end
        if COMPARE_PIC then
            ring[ami.frame()] = ami.frame_rgb(0, 0, FRAME_W, FRAME_H, 1)
            ring[ami.frame() - 16] = nil
        end
        ami.wait(1)
    end
end
say("lockstep over: %d sync points compared, %d with differences (memory %d, picture %d); %d pictures matched the original's frame next to the usual one (%d the next, %d the previous)", compared, different, mem_bad, pic_bad, shifted_pictures, shifted_by[1], shifted_by[-1])
peer:send("port.quit()\n")
ami.quit()
