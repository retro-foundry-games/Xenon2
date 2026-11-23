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

local PICKUP = { [0x4634] = true, [0xf3e] = true }   -- pickup_update (the routine and the jump-table thunk)

local function decide(n)
    local px, py = s16(PLAYER + 0x20), s16(PLAYER + 0x24)
    local enemy, danger, pickup = false, nil, nil

    walk(LIST_ENEMY, function(o)
        if o == PLAYER or NOP[peek32(o + 6)] then return end
        local x, y = s16(o + 0x20), s16(o + 0x24)
        if y > 0 and y < py and x > 8 and x < 312 then enemy = true end
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
        want_x = 160   -- the centre of the screen
    end
    if want_x < px - 5 then st.left = 1 elseif want_x > px + 5 then st.right = 1 end
    if px < 20 then st.left, st.right = 0, 1 elseif px > 296 then st.left, st.right = 1, 0 end

    -- height: after a pickup when there is one (and no shot in the way), else the lower part of the screen
    local low, high = 150, 188
    if pickup and not danger then
        if pickup.y < py - 6 then st.up = 1 elseif pickup.y > py + 6 then st.down = 1 end
    elseif py > high then st.up = 1 elseif py < low then st.down = 1
    elseif random(12) == 0 then if random(2) == 0 then st.up = 1 else st.down = 1 end end
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
    return st
end

return function(p16, p32)
    peek16, peek32 = p16, p32
    return decide
end
