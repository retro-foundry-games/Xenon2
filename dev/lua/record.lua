-- Records the port's picture (the 320 x 200 of the game's window) and sound as raw files, frame by frame at 50 frames a second:
--   xenon2-sdl-dev --lua dev/lua/record.lua --lua-args "frames=3000 out=captures/intro" --disks local
-- writes OUT.rgb (320 x 200, rgb24) and OUT.pcm (s16le, 44100 Hz, stereo); video.sh in this folder joins them into an MP4.
-- A "fire=F,F,..." option taps the fire button at those frames (the title stands until a key).
local frames = tonumber(args:match("frames=(%d+)") or "1500")
local out = args:match("out=(%S+)") or "captures/record"
local fire = {}
for f in (args:match("fire=(%S+)") or ""):gmatch("%d+") do fire[tonumber(f)] = true end
local TOP, ROWS, W = 14, 200, 320
local video, sound = assert(io.open(out .. ".rgb", "wb")), assert(io.open(out .. ".pcm", "wb"))
for f = 1, frames do
    if fire[f] then port.joy{ fire = true } elseif fire[f - 4] then port.joy{} end
    port.step()
    local picture = port.picture()
    video:write(picture:sub(TOP * W * 3 + 1, (TOP + ROWS) * W * 3))
    sound:write(port.audio())
end
video:close()
sound:close()
print(("recorded %d frames to %s.rgb and %s.pcm"):format(frames, out, out))
