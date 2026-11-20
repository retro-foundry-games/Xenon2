-- A savestate made later from another: fire is held for frames 5-11 (a level waiting for the fire button starts), the machine runs FRAMES frames, and the state is saved.
--   ami9000_host host/state_after.lua --headless --script-args "state=frame-4464 frames=200 out=level2-start"
local args = ami.args or ""
local state = args:match("state=(%S+)")
local FRAMES = tonumber(args:match("frames=(%d+)") or "200")
local out = args:match("out=(%S+)")
ami.machine{
    name = "Xenon2", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    content = "../local/xenon2.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_joyport_order = "2143" },
}
ami.boot()
ami.wait(1)
assert(ami.snapshot_load(state .. ".state"), "cannot load " .. tostring(state))
local base = ami.frame()
for f = 1, FRAMES do
    if f == 5 then ami.joystick{ fire = true } elseif f == 12 then ami.joystick() end
    ami.wait(1)
end
assert(ami.snapshot_save(out .. ".state"))
ami.quit()
