-- try the peer's sync from a Lua script alone (no emulator): the 10th entry of swap_screens, its picture and memory
local count = 0
port.hook(0x2034, function() count = count + 1 end)
port.stop_on(0x2034)
while count < 10 do local w = port.step() end
print("stopped in swap_screens, entry", count, "frame", port.frame(), "vbl", port.peek16(0x41c), "picture", ("%08x"):format(port.picture_hash()))
