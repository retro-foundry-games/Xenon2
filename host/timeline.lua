-- When do things happen in the cold run of the 1 MB machine (the one the oracle's states come from)?  Hits of chosen original routines per 500 frames.
local watch = { [0x35d4] = "update_all (level)", [0x921a] = "disk read", [0x7032] = "game start", [0x8f9a] = "game disk check", [0x5efe] = "player_update" }
ami.machine{
    name = "Xenon2 timeline", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    content = "../local/xenon2.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_chipmem_size = 2, puae_joyport_order = "2143" },
}
ami.boot()
local counts = {}
for a, n in pairs(watch) do ami.hook(a, function() local k = ami.frame() // 500 * 500; counts[k] = counts[k] or {}; counts[k][a] = (counts[k][a] or 0) + 1 end) end
local swapped, st, sf, last_read, nreads, fire_until = false, 0, 0, -1000, 0, 0
ami.hook(0x921a, function() last_read = ami.frame(); nreads = nreads + 1; local k = ami.frame() // 500 * 500; counts[k] = counts[k] or {}; counts[k][0x921a] = (counts[k][0x921a] or 0) + 1 end)
while ami.frame() < 14000 do
    local f = ami.frame()
    if f >= 5200 and f % 150 == 0 then ami.joystick{ fire = true }; fire_until = f + 10 end
    if fire_until ~= 0 and f >= fire_until then ami.joystick(); fire_until = 0 end
    local id = ami.peek32(0x49602)
    if not swapped and nreads > 0 and f >= last_read + 60 and id ~= 0x58324220 then swapped = true; st = 1; print("swap at", f) end
    if st == 1 then ami.disk_eject(true); st = 2; sf = f
    elseif st == 2 and f >= sf + 30 then ami.disk_select(1); ami.disk_eject(false); st = 3 end
    ami.wait(1)
end
for k = 0, 13500, 500 do
    local c = counts[k] or {}
    local parts = {}
    for a, n in pairs(watch) do if c[a] then parts[#parts + 1] = string.format("%s=%d", n, c[a]) end end
    print(k, table.concat(parts, " "))
end
ami.quit()
