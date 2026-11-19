-- Lockstep of the original (this emulator) and the dev build of the port (dev/build/xenon2-sdl-dev, its control protocol on stdin/stdout): both run the game from its start (the port from
-- the original's memory at that moment); at every entry of swap_screens (the game passes it once a frame) the named members of the memory and the picture are compared.
-- The first difference pauses the emulator and the port is not stepped again, so both can be looked at (--control); continue=1 goes on and lists them all.
--
--   ami9000_host host/lockstep.lua --headless --script-args "[syncs=N] [tolerant=1] [continue=1] [mem=0] [pic=0] [ignore=NAME,NAME] [joy=SYNC:DIRS,...] [show=NAME,NAME] [strict=1] [report=FILE]"
--     syncs=N       how many synchronisation points to compare (default 300)
--     tolerant=1    also leave out what depends on how many interrupts the original took while its (slow) code ran: the music player, the hardware mirror, the random generator
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
local SYNCS = tonumber(args:match("syncs=(%d+)") or "300")
local ADDRS = {}
for a in (args:match("addr=(%S+)") or "2026,202e"):gmatch("%x+") do ADDRS[#ADDRS + 1] = tonumber(a, 16) end
local PORT_ADDR = tonumber(args:match("%f[%w]port=(%x+)") or "2034", 16)
local COMPARE_PIC = args:match("pic=0") == nil
local COMPARE_MEM = args:match("mem=0") == nil
local CONTINUE = args:match("continue=1") ~= nil
local STRICT = args:match("strict=1") ~= nil
local REPORT = args:match("report=(%S+)")
local ROOT = ami.script_dir .. "/.."

-- members whose values depend on when the interrupts fall relative to the code, not on what the code does
local IGNORE = { vbl_count = true, v_0404 = true, exc_vectors_lo = true, exc_vectors_hi = true }
if args:match("tolerant=1") then
    for _, name in ipairs{ "music_accum", "music_channels", "hw_mirror", "random_a", "random_b", "sfx_channels" } do IGNORE[name] = true end
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
    peers = { port = { ROOT .. "/dev/build/xenon2-sdl-dev", "--control", "-", "--disks", ROOT .. "/local" } },
}
ami.boot()
local peer = ami.peer("port")
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
local started, start_ram = false, nil
local fire_until = 0
local pending, n = {}, 0
local FRAME_W, FRAME_H = 720, 287      -- the emulator's frame
local ring = {}                        -- ring[frame] = the emulator's whole frame at that frame (the last 16)

local function sync_point()
    if not started then return end
    n = n + 1
    if n > SYNCS then return end
    pending[#pending + 1] = { n = n, frame = ami.frame(), ram = COMPARE_MEM and ami.read(0, 0x80000) or nil }
    local st = joys[n]
    if st then
        ami.joystick{ up = st.up == 1, down = st.down == 1, left = st.left == 1, right = st.right == 1, fire = st.fire == 1 }
        peer:send(("joy %d %d %d %d %d\n"):format(st.up, st.down, st.left, st.right, st.fire))
    end
end
for _, a in ipairs(ADDRS) do ami.hook(a, sync_point) end

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
peer:send("loadram 524288\n")
peer:send(start_ram)
assert(peer:readline() == "loaded", "the port did not take the memory")

local function read_exact(count)
    local data = peer:read(count)
    assert(data and #data == count, "the peer ended")
    return data
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

local baseline
local function compare_memory(snap, port_ram)
    local first = baseline == nil
    if first then baseline = {} end
    local bad, shown = 0, 0
    for _, m in ipairs(members) do
        local addr, size, name = m[1], m[2], m[3]
        local a, b = snap.ram:sub(addr + 1, addr + size), port_ram:sub(addr + 1, addr + size)
        if not IGNORE[name] and a ~= b then
            if first then baseline[name] = a .. b end
            if STRICT or (not first and baseline[name] ~= a .. b) then
                bad = bad + 1
                if shown < 12 then
                    shown = shown + 1
                    local k = 1
                    while a:byte(k) == b:byte(k) do k = k + 1 end
                    say("   memory %-26s $%06x +%-5d (size %d): original %s port %s", name, addr, k - 1, size, hex(a, k, 8), hex(b, k, 8))
                end
            end
        end
    end
    if first then
        local junk = 0
        for _ in pairs(baseline) do junk = junk + 1 end
        say("baseline at the first sync point: %d members already differ (what the original's memory held before the game); only changes are reported from here", junk)
        if not STRICT then return 0 end
    end
    return bad
end

local function compare_picture(snap, port_pic)
    if not win then calibrate(port_pic, snap) end
    if not win then return 0 end
    local frame = ring[snap.frame + win.lag] or ring[snap.frame + 1]
    if not frame then return 0 end
    local original = cut(frame, win)
    local diffs, first = ami.rgb_diff(original, port_pic)
    if diffs == 0 then return 0 end
    say("   picture: %d pixels differ, the first at %d,%d", diffs, (first - 1) % 320, (first - 1) // 320)
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
        peer:send(("sync 0x%x 1\n"):format(PORT_ADDR))
        local reply = peer:readline()
        if not reply or not reply:match("^synced") then say("lockstep: the port did not reach sync point %d (%s)", snap.n, tostring(reply)); done = true; break end
        local problems = {}
        if COMPARE_MEM then
            peer:send("ram\n")
            local port_ram = read_exact(tonumber(peer:readline():match("ram (%d+)")))
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
        if COMPARE_PIC then
            peer:send("image\n")
            peer:readline()
            local d = compare_picture(snap, read_exact(320 * 256 * 3))
            if d > 0 then pic_bad = pic_bad + 1; problems[#problems + 1] = ("%d picture pixels differ"):format(d) end
        end
        compared = compared + 1
        if #problems > 0 then
            different = different + 1
            say("sync %d (emulator frame %d, port frame %s): %s", snap.n, snap.frame, reply:match("synced (%d+)") or "?", table.concat(problems, "; "))
            if not CONTINUE then ami.pause(); done = true; break end
        end
        if compared % 100 == 0 then say("%d sync points compared, %d with differences (memory %d, picture %d)", compared, different, mem_bad, pic_bad) end
        if compared >= SYNCS then done = true; break end
    end
    if not done then
        if COMPARE_PIC then
            ring[ami.frame()] = ami.frame_rgb(0, 0, FRAME_W, FRAME_H, 1)
            ring[ami.frame() - 16] = nil
        end
        ami.wait(1)
    end
end
say("lockstep over: %d sync points compared, %d with differences (memory %d, picture %d)", compared, different, mem_bad, pic_bad)
peer:send("quit\n")
ami.quit()
