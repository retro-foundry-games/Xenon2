-- A smoke test of the dev build: step, hook by the original's address, read memory and the picture.
local swaps = 0
port.hook(0x2034, function() swaps = swaps + 1 end)       -- swap_screens
for f = 1, 400 do
    if f % 150 == 100 then port.joy{ fire = true } elseif f % 150 == 110 then port.joy{} end
    port.step()
    if f % 100 == 0 then
        print(("frame %d  vbl_count %d  swap_screens entered %d times  picture hash %08x"):format(port.frame(), port.peek16(0x41c), swaps, port.picture_hash()))
    end
end
local ok = port.run_until(0x2034, 50)
print("run_until swap_screens:", ok, "frame", port.frame())
print("ram bytes:", #port.ram(), " picture bytes:", #port.picture())
