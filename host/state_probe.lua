-- What is running in a savestate: the vertical-blank vector, the frame counter ($41C), the level scroll ($CD0), the screens shown, over N frames.
--   --script-args "state=NAME frames=N"
local args = ami.args or ""
local STATE = args:match("state=(%S+)")
local frames = tonumber(args:match("frames=(%d+)") or "200")
ami.machine{
    name = "Xenon2 probe", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    snapshots = "../local/save", content = "../local/xenon2.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_chipmem_size = 2, puae_joyport_order = "2143" },
}
ami.boot()
ami.wait(1)
assert(ami.snapshot_load(STATE))
local joys = {}
for f, dirs in (args:match("joy=(%S+)") or ""):gmatch("(%d+):([%a+]+)") do
    local st = {}
    for d in dirs:gmatch("%a+") do if d ~= "none" then st[d] = true end end
    joys[tonumber(f)] = st
end
local step = math.max(1, frames // 12)
for i = 0, frames do
    if joys[i] then ami.joystick(joys[i]) end
    if i % step == 0 then
        print(string.format("+%d vec6c=%08x vbl=%04x scroll=%04x show=%08x objs=%08x", i, ami.peek32(0x6c), ami.peek16(0x41c), ami.peek16(0xcd0), ami.peek32(0x40a), ami.peek32(0xa76)))
    end
    ami.wait(1)
end
ami.quit()
