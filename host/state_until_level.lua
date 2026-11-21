-- A savestate made later from another: fire is tapped every 60 frames until v_0e04 is LEVEL and the machine has run FRAMES more frames, then the state is saved.
--   ami9000_host host/state_until_level.lua --headless --script-args "state=frame-5694 level=4 frames=100 out=level4-start"
local args = ami.args or ""
local state = args:match("state=(%S+)")
local LEVEL = tonumber(args:match("level=(%d+)"))
local FRAMES = tonumber(args:match("frames=(%d+)") or "100")
local out = args:match("out=(%S+)")
ami.machine{
    name = "Xenon2", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    content = "../local/xenon2.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_joyport_order = "2143" },
}
ami.boot()
ami.wait(1)
assert(ami.snapshot_load(state .. ".state"), "cannot load " .. tostring(state))
local reached
for f = 1, 6000 do
    local k = f % 60
    if k == 10 then ami.joystick{ fire = true } elseif k == 20 then ami.joystick() end
    ami.wait(1)
    if not reached and ami.peek16(0xe04) == LEVEL and ami.peek32(0x54e04) == 0x7d120 then reached = f end
    if reached and f >= reached + FRAMES then break end
end
assert(reached, "the level was not reached")
ami.log(string.format("level %d reached after %d frames", LEVEL, reached))
assert(ami.snapshot_save(out .. ".state"))
ami.quit()
