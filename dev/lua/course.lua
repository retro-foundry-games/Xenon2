-- Plots a course through level 1: the port is its own simulator.  From the original's memory at the start of the level (captures/level1.ram, made by
-- host/lockstep.lua dumpram=), at each sync point every stick option is tried for a few intervals from a saved state, the best one (progress of the scroll and
-- score up, energy not lost) is kept, the state is restored and the chosen stick is played for one interval.  The sticks are written as
-- captures/input/level1-course.txt ("SYNC joy up down left right fire"), which host/lockstep.lua replays (replay=) against the original, headless or in the window.
--
--   xenon2-sdl-dev --lua dev/lua/course.lua --lua-args "syncs=3000 horizon=5 out=captures/input/level1-course.txt" --disks local
local count = tonumber(args:match("syncs=(%d+)") or "3000")
local horizon = tonumber(args:match("horizon=(%d+)") or "5")
local out_path = args:match("out=(%S+)") or "captures/input/level1-course.txt"
local W_ENERGY = tonumber(args:match("ew=([%d.]+)") or "40")   -- value of an energy cell lost
local W_SCORE = tonumber(args:match("sw=([%d.]+)") or "0.2")     -- of a point of score
local W_ALIGN = tonumber(args:match("aw=([%d.]+)") or "3")       -- of an enemy the ship is under (three at most)
local W_PROGRESS = tonumber(args:match("pw=([%d.]+)") or "8")    -- of a unit of scroll (the level only scrolls on while the ship is high on the screen)
local STATE_FILE = "/dev/shm/x2-course.state"
local SWAP = 0x2034

local function s16(a) local v = port.peek16(a); return v >= 0x8000 and v - 0x10000 or v end
local function score() local v = 0; for i = 0, 6 do v = v * 10 + port.peek8(0xd7c + i) end; return v end

-- to the level, then the original's memory: as host/lockstep.lua does it
local n, f = 0, 0
port.hook(0x35d4, function() n = n + 1 end)
while n < 5 and f < 6000 do f = f + 1; local k = f % 150; if k == 100 then port.joy{ fire = true } elseif k == 110 then port.joy{} end; port.step() end
port.unhook(0x35d4)
port.joy{}
port.run_until(SWAP, 20)
local fh = assert(io.open("captures/level1.ram", "rb"))
port.load_ram(fh:read("a"))
fh:close()

local function stick(dx, dy, fire) return { left = dx < 0 and 1 or 0, right = dx > 0 and 1 or 0, up = dy < 0 and 1 or 0, down = dy > 0 and 1 or 0, fire = fire } end
local function interval(st) port.joy{ up = st.up == 1, down = st.down == 1, left = st.left == 1, right = st.right == 1, fire = st.fire == 1 }; port.run_until(SWAP, 20) end

-- enemies (list 2, drawn, on screen, above the ship) that the ship is under: its shots go straight up
local function aligned_enemies(px, py)
    local sentinel, count = 0xa76 + 2 * 98, 0
    local o, guard = port.peek32(sentinel + 0x12), 0
    while o ~= 0 and o ~= sentinel and o >= 0x400 and o < 0x7ff00 and guard < 200 do
        local draw = port.peek32(o + 6)
        if draw ~= 0xe48 and draw ~= 0x3130 then
            local x, y = s16(o + 0x20), s16(o + 0x24)
            if y > -16 and y < py and x > -8 and x < 328 and math.abs(x - px) < 12 then count = count + 1 end
        end
        o = port.peek32(o + 0x12); guard = guard + 1
    end
    return count
end

local function measure()
    local x, y = s16(0x3f01c + 0x20), s16(0x3f01c + 0x24)
    return { scroll = port.peek16(0xcd0), energy = port.peek16(0xcc6), score = score(), x = x, y = y, aligned = aligned_enemies(x, y) }
end

-- how good the horizon was: scroll progress and score up, energy lost very bad, and not hugging the bottom or the sides (where it cannot dodge)
local function value(before, after)
    local v = W_ENERGY * math.min(0, after.energy - before.energy)
    local progress = before.scroll - after.scroll
    if progress < 0 or progress > 1000 then progress = 0 end
    v = v + W_PROGRESS * progress
    v = v + W_SCORE * (after.score - before.score) + W_ALIGN * math.min(after.aligned, 3)
    if after.y > 150 then v = v - 0.3 * (after.y - 150) elseif after.y < 70 then v = v - 0.3 * (70 - after.y) end   -- (neither the bottom nor the top: no room to dodge)
    v = v - 0.05 * math.abs(after.x - 160)        -- the middle: room to dodge, and the scroll can stop at the sides
    return v
end

local debug_loss, prev_energy = args:match("debug=1") ~= nil, 39
local lines, last_x, last_y, last_scroll, stalled, last_st = {}, 0, 0, -1, 0, nil
local start_time = os.clock()
for i = 1, count do
    local scroll = port.peek16(0xcd0)
    stalled = (scroll == last_scroll) and stalled + 1 or 0
    last_scroll = scroll
    local fire = (stalled >= 3 and i % 2 == 0) and 0 or 1

    port.save_state(STATE_FILE)
    local before = measure()
    local best, best_value = nil, -math.huge
    -- a reversal goes through neutral (the emulator's stick reads neither direction when two directions change in one update)
    local xs = last_x ~= 0 and { last_x, 0 } or { -1, 0, 1 }
    local ys = last_y ~= 0 and { last_y, 0 } or { -1, 0, 1 }
    for _, dx in ipairs(xs) do
        for _, dy in ipairs(ys) do
            for _ = 1, horizon do interval(stick(dx, dy, 1)) end
            local v = value(before, measure())
            if v > best_value then best, best_value = { dx, dy }, v end
            port.load_state(STATE_FILE)
        end
    end
    if debug_loss and before.energy < prev_energy then
        local px, py = before.x, before.y
        local near = {}
        for list = 0, 4 do
            local sentinel = 0xa76 + list * 98
            local o, guard = port.peek32(sentinel + 0x12), 0
            while o ~= 0 and o ~= sentinel and o >= 0x400 and o < 0x7ff00 and guard < 200 do
                local x, y = s16(o + 0x20), s16(o + 0x24)
                if math.abs(x - px) < 60 and math.abs(y - py) < 60 then near[#near + 1] = ("l%d t%04x (%d,%d) draw %x"):format(list, port.peek16(o), x, y, port.peek32(o + 6)) end
                o = port.peek32(o + 0x12); guard = guard + 1
            end
        end
        io.stderr:write(("sync %d energy %d -> %d at %d,%d scroll %04x chose %d,%d; near: %s\n"):format(i, prev_energy, before.energy, px, py, before.scroll, best[1], best[2], table.concat(near, " | ")))
    end
    prev_energy = before.energy
    local st = stick(best[1], best[2], fire)
    last_x, last_y = best[1], best[2]
    interval(st)
    local key = ("%d %d %d %d %d"):format(st.up, st.down, st.left, st.right, st.fire)
    if key ~= last_st then lines[#lines + 1] = ("%d joy %s\n"):format(i, key); last_st = key end
    if i % 50 == 0 then
        local m = measure()
        io.stderr:write(("sync %4d scroll %04x energy %2d score %7d at %3d,%3d  (%.0f s)\n"):format(i, m.scroll, m.energy, m.score, m.x, m.y, os.clock() - start_time))
        local o = io.open(out_path, "w"); o:write(table.concat(lines)); o:close()
    end
end
local o = io.open(out_path, "w"); o:write(table.concat(lines)); o:close()
io.stderr:write(("course written to %s (%d changes)\n"):format(out_path, #lines))
