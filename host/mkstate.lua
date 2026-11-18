-- Make the savestates the oracle starts from, in the 1 MB chip RAM machine: cold boot, the same inputs as run_level.lua, a state at each frame of snap=.
--   ami9000_host host/mkstate.lua --headless --script-args "snap=7400:menu,10000:level"       (frame:name -> local/save/xenon2-NAME.state)
local args = ami.args or ""
local snaps = {}
for f, name in (args:match("snap=(%S+)") or ""):gmatch("(%d+):(%w+)") do snaps[tonumber(f)] = name end
local last = 0
for f in pairs(snaps) do if f > last then last = f end end
ami.machine{
    name = "Xenon2 state", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    snapshots = "../local/save",
    content = "../local/xenon2.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_chipmem_size = 2, puae_joyport_order = "2143" },
}
ami.boot()
local swapped, st, sf, last_read, nreads, fire_until = false, 0, 0, -1000, 0, 0
ami.hook(0x921a, function() last_read = ami.frame(); nreads = nreads + 1 end)
while ami.frame() <= last do
    local f = ami.frame()
    if f >= 5200 and f % 150 == 0 then ami.joystick{ fire = true }; fire_until = f + 10 end
    if fire_until ~= 0 and f >= fire_until then ami.joystick(); fire_until = 0 end
    local id = ami.peek32(0x49602)
    if not swapped and nreads > 0 and f >= last_read + 60 and id ~= 0x58324220 then swapped = true; st = 1 end
    if st == 1 then ami.disk_eject(true); st = 2; sf = f
    elseif st == 2 and f >= sf + 30 then ami.disk_select(1); ami.disk_eject(false); st = 3 end
    if snaps[f] then
        print("state", f, snaps[f], ami.snapshot_save("xenon2-" .. snaps[f] .. ".state"))
    end
    ami.wait(1)
end
ami.quit()
