-- Ground truth for the hook addresses: single-step 400 instructions at frame 9000 and log, interleaved, the PC ami.step reports
-- and the addresses the hooks report.
ami.machine{
    name = "Xenon2", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    content = "../local/xenon2.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_joyport_order = "2143" },
}
ami.boot()
local swapped, st, sf, last_read, nreads = false, 0, 0, -1000, 0
ami.hook(0x921a, function() last_read = ami.frame(); nreads = nreads + 1 end)
local fire_until = 0
while ami.frame() < 9000 do
    local f = ami.frame()
    if f >= 5200 and f % 150 == 0 then ami.joystick{ fire = true }; fire_until = f + 10 end
    if fire_until ~= 0 and f >= fire_until then ami.joystick(); fire_until = 0 end
    local id = ami.peek32(0x49602)
    if not swapped and nreads > 0 and f >= last_read + 60 and id ~= 0x58324220 then swapped = true; st = 1 end
    if st == 1 then ami.disk_eject(true); st = 2; sf = f
    elseif st == 2 and f >= sf + 30 then ami.disk_select(1); ami.disk_eject(false); st = 3 end
    ami.wait(1)
end
local log = {}
for a = 0x19f00, 0x1b700, 2 do ami.hook(a, function() log[#log + 1] = string.format("H %06x", a) end) end
local n = 0
ami.step(function(pc)
    log[#log + 1] = string.format("S %06x", pc)
    n = n + 1
    if n >= 400 then
        ami.save_file("captures/hookstep.log", table.concat(log, "\n") .. "\n")
        ami.quit()
        return false
    end
    return true
end)
