-- The port as a lockstep peer: commands on stdin (one a line), replies on stdout.  Run: xenon2-sdl-dev --lua dev/lua/peer.lua --disks DIR
--
--   joy UP DOWN LEFT RIGHT FIRE      the stick (0/1 each)
--   key RAW DOWN                     an Amiga raw key code
--   step                             one video frame            -> frame TICKS 320 256 NAME - + RGB (320*256*3 bytes)
--   image                            the picture as it is now   -> the same
--   sync ADDR [N]                    run until the function with the original address ADDR has been entered N times (default 1), the game standing inside it
--                                                               -> synced FRAME COUNT        (or `unsynced FRAME` after 20000 frames)
--   ram                              the memory (original byte order)  -> ram 524288 + the bytes
--   loadram LEN                      LEN (524288) bytes follow: the original's memory, put into the port before it starts   -> loaded
--   quit
local W, H = 320, 256
local synced = 0

local function reply_image(name)
    io.stdout:write(("frame %d %d %d %s -\n"):format(port.frame(), W, H, name or "port"))
    io.stdout:write(port.picture())
    io.stdout:flush()
end

for line in io.lines() do
    local cmd, rest = line:match("^(%S+)%s*(.*)$")
    if cmd == "joy" then
        local u, d, l, r, f = rest:match("(%d) (%d) (%d) (%d) (%d)")
        port.joy{ up = u == "1", down = d == "1", left = l == "1", right = r == "1", fire = f == "1" }
    elseif cmd == "key" then
        local raw, down = rest:match("(%d+) (%d)")
        port.key(tonumber(raw), down == "1")
    elseif cmd == "step" then
        port.step()
        reply_image()
    elseif cmd == "image" then
        reply_image()
    elseif cmd == "sync" then
        local addr, n = rest:match("(%S+)%s*(%d*)")
        addr, n = tonumber(addr), tonumber(n) or 1
        local count, ok = 0, false
        port.hook(addr, function() count = count + 1 end)
        port.stop_on(addr)
        for _ = 1, 20000 do
            local what = port.step()
            if what == "hook" then
                if count >= n then ok = true; break end
            end
        end
        port.stop_on(nil)
        port.unhook(addr)
        io.stdout:write(ok and ("synced %d %d\n"):format(port.frame(), count) or ("unsynced %d\n"):format(port.frame()))
        io.stdout:flush()
    elseif cmd == "ram" then
        local ram = port.ram()
        io.stdout:write(("ram %d\n"):format(#ram))
        io.stdout:write(ram)
        io.stdout:flush()
    elseif cmd == "loadram" then
        local len = tonumber(rest)
        port.load_ram(io.stdin:read(len))
        io.stdout:write("loaded\n"); io.stdout:flush()
    elseif cmd == "quit" then
        break
    else
        io.stdout:write("error unknown command\n"); io.stdout:flush()
    end
end
