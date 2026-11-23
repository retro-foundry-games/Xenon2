-- Drops the port into a place of a level and plays there: the original's memory of the place (captures/NAME.ram, made from local/save/NAME.state by host/ramdump.lua after
-- host/scenario.lua made the state) is loaded as host/lockstep.lua loads a state's, then the ship moves left and right under fire for FRAMES frames.  A crash of the port is
-- the failure; a line is printed every 100 frames (scroll, boss and level flags, energy, lives).
--
--   host/ramdump.lua:  ami9000_host host/ramdump.lua --headless --script-args "joy1 load=l3-boss ram=captures/l3-boss.ram"
--   xenon2-sdl-dev --lua dev/lua/exercise.lua --lua-args "ram=captures/l3-boss.ram frames=3000" --disks local
local path = args:match("ram=(%S+)")
local frames = tonumber(args:match("frames=(%d+)") or "3000")
assert(path, "ram=PATH")
local SWAP = 0x2034
local OBJECTS_UPDATE = 0x35d4

-- to a level (any: the memory of the place replaces it), as dev/lua/course.lua does
local n, f = 0, 0
port.hook(OBJECTS_UPDATE, function() n = n + 1 end)
while n < 5 and f < 6000 do
    f = f + 1
    local k = f % 150
    if k == 100 then port.joy{ fire = true } elseif k == 110 then port.joy{} end
    port.step()
end
port.unhook(OBJECTS_UPDATE)
port.joy{}
port.run_until(SWAP, 20)
local fh = assert(io.open(path, "rb"))
port.load_ram(fh:read("a"))
fh:close()

local function s16(a) local v = port.peek16(a); return v >= 0x8000 and v - 0x10000 or v end
for i = 1, frames do
    local left = (i // 90) % 2 == 0
    port.joy{ left = left, right = not left, fire = (i % 4) < 2, up = (i // 400) % 3 == 0 }
    port.step()
    if i % 100 == 0 then
        print(string.format("frame %d picture %s level %d scroll_pos %x boss_dead %x level_over %x energy %d lives %d", i, port.picture_hash(), port.peek16(0xe04), port.peek16(0xcd0), port.peek8(0xa07),
            port.peek8(0xa6c), s16(0xcc6), s16(0xcca)))
    end
end
print("done: the port did not crash")
