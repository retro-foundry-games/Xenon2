-- Records a scripted run of the port to an MP4 (the picture is the 320 x 200 of the game's window, 50 frames a second, with the sound):
--   xenon2-sdl-dev --lua dev/lua/record.lua --lua-args "frames=2223 [fire=F,F,...]" --record captures/intro.mp4 --disks local
-- The "fire" frames tap the fire button (the title stands until a key).  A played game needs no script: xenon2-sdl-dev --record
local frames = tonumber(args:match("frames=(%d+)") or "1500")
local fire = {}
for f in (args:match("fire=(%S+)") or ""):gmatch("%d+") do fire[tonumber(f)] = true end
for f = 1, frames do
    if fire[f] then port.joy{ fire = true } elseif fire[f - 4] then port.joy{} end
    port.step()
    port.record_frame()
end
port.quit()
