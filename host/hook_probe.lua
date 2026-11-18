-- Does a hook installed at frame 0 keep firing across the disk swap?  Prints the hits on the vblank handler per 1000 frames.
ami.machine{
    name = "Xenon2", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    content = "../local/xenon2.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_joyport_order = "2143" },
}
ami.boot()
local swapped, st, sf = false, 0, 0
ami.hook(0x8fe8, function() if not swapped and ami.peek32(0x49602) ~= 0x58324220 then swapped = true; st = 1 end end)
local hits = {}
local function count() local k = ami.frame() // 1000; hits[k] = (hits[k] or 0) + 1 end
ami.hook(0x20c2, count)   -- level-3 handler of the level
ami.hook(0x2098, count)   -- level-3 handler of the menu
local fire_until = 0
while ami.frame() < 10000 do
    local f = ami.frame()
    if f >= 5200 and f % 150 == 0 then ami.joystick{ fire = true }; fire_until = f + 10 end
    if fire_until ~= 0 and f >= fire_until then ami.joystick(); fire_until = 0 end
    if st == 1 then ami.disk_eject(true); st = 2; sf = f
    elseif st == 2 and f >= sf + 30 then ami.disk_select(1); ami.disk_eject(false); st = 3; print("swap at", f) end
    ami.wait(1)
end
for k = 0, 9 do print(k * 1000, hits[k] or 0) end
ami.quit()
