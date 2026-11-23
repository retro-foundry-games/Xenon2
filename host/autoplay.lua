-- An autoplayer for the lockstep: at each sync point it decides the stick from the original's memory (object lists, the player) and holds fire.
-- Loaded by host/lockstep.lua (auto=1; the emulator's peek16/peek32) or by dev/lua/course.lua (the port's): `factory(peek16, peek32)` returns `function(n) -> {up=,down=,left=,right=,fire=}`.
--
-- It stays in the lower part of the screen, fires while an enemy is on screen, goes for a pickup and steps aside from an enemy shot that is about to hit it.
-- Otherwise it holds the centre of the screen.
local OBJ_SIZE = 98
local LIST_PLAYER, LIST_MISC, LIST_ENEMY, LIST_EFFECT, LIST_FRONT = 0, 1, 2, 3, 4
local SENTINELS = 0xa76            -- obj_list[5]: the head of list i is the forward link ($12) of the object at SENTINELS + i * 98
local PLAYER = 0x3f01c
local NOP = { [0xe48] = true, [0x3130] = true }       -- obj_nop (the jump-table thunk and the routine): an object that is not drawn is a marker

local peek16, peek32
local function s16(a) local v = peek16(a); return v >= 0x8000 and v - 0x10000 or v end

local function walk(list, visit)
    local sentinel = SENTINELS + list * OBJ_SIZE
    local o, guard = peek32(sentinel + 0x12), 0
    while o ~= 0 and o ~= sentinel and o >= 0x400 and o < 0x7ff00 and guard < 200 do
        visit(o)
        o = peek32(o + 0x12)
        guard = guard + 1
    end
end

local seed = 12345
local function random(n)   -- deterministic: the same run plays the same way
    seed = (seed * 1103515245 + 12345) % 2147483648
    return (seed // 65536) % n
end

local wander_x, wander_until = 160, 0
local last_scroll, stalled = -1, 0
local retreat_until, retreats = 0, 0   -- stuck (the scroll has stood still for long): back off for a while
local last_x, last_y = 0, 0   -- the direction held at the last decision

local KEEP_BACK = 20   -- px between the ship and an obstacle ahead

-- The tile map: the cell under screen pixel (x, y) is the word at level_init_src + map_row_offset + (row * 20 + column) * 2, row = (y + scroll_fine) / 16, column = x / 16
-- (map_tile_mask in objects.c): 0 is empty, any other word is a tile that stops the ship (a full one, or one with a mask of its own, taken as solid).
local cells, fine = {}, 0
local function read_map()   -- the 20 x 16 cells of the screen once a decision
    fine = peek16(0xcd6)
    local base = peek32(0x54e04) + s16(0xcce)
    for i = 0, 319 do cells[i] = peek16(base + i * 2) end
end
local function solid(x, y)
    if x < 0 or x > 319 then return true end
    if y < 0 then return false end
    local row = (y + fine) // 16
    if row > 15 then return false end
    return cells[row * 20 + x // 16] ~= 0
end

local REACH = 96   -- px ahead (up) that are looked at
local function clear_ahead(cx, py)   -- px the ship's column at cx can go up from py before a tile stops it (REACH at most)
    for d = 0, REACH - 1, 8 do
        local y = py - 14 - d
        if solid(cx - 14, y) or solid(cx, y) or solid(cx + 13, y) then return d end
    end
    return REACH
end

-- the ship's box (28 x 24) against the tiles at a position
local function box_free(x, y)
    for _, dx in ipairs { -14, 0, 13 } do
        for _, dy in ipairs { -14, -2, 9 } do
            if solid(x + dx, y + dy) then return false end
        end
    end
    return true
end

-- the positions the ship can reach (a grid of 8 px steps over the lower part of the screen, 4 neighbours), breadth first: the one with the longest open way up
-- (near the column wanted) is the goal; the answer is the first step towards it, { dx, dy } in grid steps, or nil when the ship is already there / there is no way
local GX0, GNX, GY0, GNY = 24, 35, 96, 13
local function find_way(px, py, want_x)
    local sx = math.max(0, math.min(GNX - 1, (px - GX0 + 4) // 8))
    local sy = math.max(0, math.min(GNY - 1, (py - GY0 + 4) // 8))
    local parent, queue, head = {}, { sx * 100 + sy }, 1
    parent[sx * 100 + sy] = -1
    local best, best_score
    while head <= #queue do
        local node = queue[head]; head = head + 1
        local gx, gy = node // 100, node % 100
        local x, y = GX0 + gx * 8, GY0 + gy * 8
        local score = clear_ahead(x, y) * 3 - math.abs(x - want_x) - math.abs(gx - sx) - math.abs(gy - sy)
        if not best_score or score > best_score then best, best_score = node, score end
        for _, d in ipairs { { 1, 0 }, { -1, 0 }, { 0, -1 }, { 0, 1 } } do
            local nx, ny = gx + d[1], gy + d[2]
            local key = nx * 100 + ny
            if nx >= 0 and nx < GNX and ny >= 0 and ny < GNY and not parent[key] and box_free(GX0 + nx * 8, GY0 + ny * 8) then
                parent[key] = node
                queue[#queue + 1] = key
            end
        end
    end
    if not best or best == sx * 100 + sy then return nil end
    local step = best
    while parent[step] ~= sx * 100 + sy do step = parent[step]; if step == -1 then return nil end end
    return { step // 100 - sx, step % 100 - sy }
end

local PICKUP = { [0x4634] = true, [0xf3e] = true }   -- pickup_update (the routine and the jump-table thunk)

local function decide(n)
    read_map()
    local px, py = s16(PLAYER + 0x20), s16(PLAYER + 0x24)
    local enemy, danger, pickup = false, nil, nil
    local ahead = nil   -- the gap (px) between the ship's top and the nearest thing above it that is in its way

    walk(LIST_ENEMY, function(o)
        if o == PLAYER or NOP[peek32(o + 6)] then return end
        local x, y = s16(o + 0x20), s16(o + 0x24)
        if y > 0 and y < py and x > 8 and x < 312 then enemy = true end
        local x0, x1, y1 = s16(o + 0x36), s16(o + 0x3a), s16(o + 0x3c)   -- the box x0 y0 x1 y1
        if x1 > x0 and x1 - x0 < 300 and x1 > px - 16 and x0 < px + 16 and y1 < py then
            local gap = (py - 14) - y1
            if not ahead or gap < ahead then ahead = gap end
        end
    end)
    for list = LIST_PLAYER, LIST_FRONT do
        walk(list, function(o)
            if not PICKUP[peek32(o + 2)] then return end
            local x, y = s16(o + 0x20), s16(o + 0x24)
            if y > 0 and y < 250 and x > 8 and x < 312 then
                if not pickup or math.abs(x - px) + math.abs(y - py) < math.abs(pickup.x - px) + math.abs(pickup.y - py) then pickup = { x = x, y = y } end
            end
        end)
    end
    walk(LIST_EFFECT, function(o)
        if peek16(o) ~= 0x14 then return end   -- an enemy shot (spawn_enemy_shot)
        local x, y = s16(o + 0x20), s16(o + 0x24)
        if y > py - 56 and y < py + 6 and math.abs(x - px) < 22 then
            if not danger or y > danger.y then danger = { x = x, y = y } end
        end
    end)

    -- the level scrolls all the time; when it has stood still for a few decision points the player is dead and the game waits for a new press of fire (a held button is no press): pulse it
    local scroll = peek16(0xcd0)
    if scroll == last_scroll then stalled = stalled + 1 else stalled = 0 end
    last_scroll = scroll
    if stalled == 40 then retreat_until = n + 30; retreats = retreats + 1; stalled = 5 end
    local st = { up = 0, down = 0, left = 0, right = 0, fire = (stalled >= 3) and (n % 2 == 0 and 0 or 1) or (enemy and 1 or 0) }
    local want_x
    if danger then
        want_x = danger.x <= px and px + 40 or px - 40       -- away from it
    elseif pickup and math.abs(pickup.x - px) < 90 and pickup.y > py - 120 then
        want_x = pickup.x
    else
        want_x = auto_target_x or 160   -- the centre of the screen, or the column ctl.py SOCK 'auto_target_x = 80' says
    end
    local free = clear_ahead(px, py)
    if want_x < px - 5 then st.left = 1 elseif want_x > px + 5 then st.right = 1 end
    if px < 20 then st.left, st.right = 0, 1 elseif px > 296 then st.left, st.right = 1, 0 end
    local way
    if free < 64 or not box_free(px + (st.right == 1 and 8 or st.left == 1 and -8 or 0), py) then   -- a wall ahead or beside: take the way the tiles allow
        way = find_way(px, py, want_x)
        if way then st.left, st.right = way[1] < 0 and 1 or 0, way[1] > 0 and 1 or 0 end
    end

    -- height: after a pickup when there is one (and no shot in the way), else the lower part of the screen
    local low, high = 150, 188
    if pickup and not danger then
        if pickup.y < py - 6 then st.up = 1 elseif pickup.y > py + 6 then st.down = 1 end
    elseif py > high then st.up = 1 elseif py < low then st.down = 1
    elseif random(12) == 0 then if random(2) == 0 then st.up = 1 else st.down = 1 end end
    if way then st.up, st.down = way[2] < 0 and 1 or 0, way[2] > 0 and 1 or 0 end
    if free < 16 and not way then st.up = 0 end   -- (not into a tile)
    if ahead and ahead < KEEP_BACK then st.up, st.down = 0, 1 end   -- hold back until the thing ahead is KEEP_BACK px away
    if n < retreat_until then   -- backing off: down, and to the side with more room (the other one each time)
        st.up, st.down = 0, 1
        if retreats % 2 == 0 then st.left, st.right = (px > 160) and 1 or 0, (px > 160) and 0 or 1 else st.left, st.right = (px > 160) and 0 or 1, (px > 160) and 1 or 0 end
    end
    -- a reversal goes through neutral for one sync interval: the emulator's stick reads neither direction for a while when two directions change in one update
    local x = st.left == 1 and -1 or st.right == 1 and 1 or 0
    local y = st.up == 1 and -1 or st.down == 1 and 1 or 0
    if x ~= 0 and x == -last_x then st.left, st.right, x = 0, 0, 0 end
    if y ~= 0 and y == -last_y then st.up, st.down, y = 0, 0, 0 end
    last_x, last_y = x, y
    if auto_trace and n % auto_trace == 0 then ami.log(string.format('auto %d: ship %d,%d want_x %d free %d ahead %s danger %s pickup %s -> %s%s%s%s%s', n, px, py, want_x, free, tostring(ahead), danger and (danger.x .. ',' .. danger.y) or '-', pickup and (pickup.x .. ',' .. pickup.y) or '-', st.left == 1 and 'L' or '', st.right == 1 and 'R' or '', st.up == 1 and 'U' or '', st.down == 1 and 'D' or '', st.fire == 1 and 'F' or '')) end
    return st
end

return function(p16, p32)
    peek16, peek32 = p16, p32
    return decide
end
