-- An autoplayer for the lockstep: at each sync point it decides the stick from the original's memory (object lists, the player) and holds fire.
-- Loaded by host/lockstep.lua (auto=1) and run in the emulator's Lua state: `return function(n) -> {up=,down=,left=,right=,fire=}`.
--
-- It stays in the lower part of the screen, goes under the nearest enemy that is on screen, and steps aside from an enemy shot that is about to hit it.
-- Where there is nothing to shoot it wanders (a deterministic pseudo-random walk of n), so the run reaches more of the game than a fixed schedule.
local OBJ_SIZE = 98
local LIST_PLAYER, LIST_MISC, LIST_ENEMY, LIST_EFFECT, LIST_FRONT = 0, 1, 2, 3, 4
local SENTINELS = 0xa76            -- obj_list[5]: the head of list i is the forward link ($12) of the object at SENTINELS + i * 98
local PLAYER = 0x3f01c
local NOP = { [0xe48] = true, [0x3130] = true }       -- obj_nop (the jump-table thunk and the routine): an object that is not drawn is a marker

local function s16(a) local v = ami.peek16(a); return v >= 0x8000 and v - 0x10000 or v end

local function walk(list, visit)
    local sentinel = SENTINELS + list * OBJ_SIZE
    local o, guard = ami.peek32(sentinel + 0x12), 0
    while o ~= 0 and o ~= sentinel and o >= 0x400 and o < 0x7ff00 and guard < 200 do
        visit(o)
        o = ami.peek32(o + 0x12)
        guard = guard + 1
    end
end

local seed = 12345
local function random(n)   -- deterministic: the same run plays the same way
    seed = (seed * 1103515245 + 12345) % 2147483648
    return (seed // 65536) % n
end

local wander_x, wander_until = 160, 0

return function(n)
    local px, py = s16(PLAYER + 0x20), s16(PLAYER + 0x24)
    local target, danger = nil, nil

    walk(LIST_ENEMY, function(o)
        if o == PLAYER or NOP[ami.peek32(o + 6)] then return end
        local x, y = s16(o + 0x20), s16(o + 0x24)
        if y > 0 and y < py and x > 8 and x < 312 then
            if not target or math.abs(x - px) + (py - y) // 4 < math.abs(target.x - px) + (py - target.y) // 4 then target = { x = x, y = y } end
        end
    end)
    walk(LIST_EFFECT, function(o)
        if ami.peek16(o) ~= 0x14 then return end   -- an enemy shot (spawn_enemy_shot)
        local x, y = s16(o + 0x20), s16(o + 0x24)
        if y > py - 56 and y < py + 6 and math.abs(x - px) < 22 then
            if not danger or y > danger.y then danger = { x = x, y = y } end
        end
    end)

    local st = { up = 0, down = 0, left = 0, right = 0, fire = 1 }
    local want_x
    if danger then
        want_x = danger.x <= px and px + 40 or px - 40       -- away from it
    elseif target then
        want_x = target.x
    else
        if n >= wander_until then wander_x = 24 + random(270); wander_until = n + 20 + random(40) end
        want_x = wander_x
    end
    if want_x < px - 5 then st.left = 1 elseif want_x > px + 5 then st.right = 1 end
    if px < 20 then st.left, st.right = 0, 1 elseif px > 296 then st.left, st.right = 1, 0 end

    -- height: the lower part of the screen, with a little drift
    local low, high = 150, 188
    if py > high then st.up = 1 elseif py < low then st.down = 1
    elseif random(12) == 0 then if random(2) == 0 then st.up = 1 else st.down = 1 end end
    return st
end
