-- Log every call of the game's own disk loader (rabs at $921a: d0 = first sector, d1 = count, a0 = destination)
-- and its result, starting from a snapshot, with fire pressed once.
--   --script-args "joy1 [load=NAME] fire=F [swap=F] frames=N [ram=PATH]"   (without load=, from a cold boot)
local args = ami.args or ""
local name = args:match("load=(%S+)")
local fire_at = tonumber(args:match("fire=(%d+)") or "10")
local frames = tonumber(args:match("frames=(%d+)") or "1500")
ami.machine{
    name = "Xenon2", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    content = "../local/xenon2.adf", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = args:match("joy1") and { puae_joyport_order = "2143" } or nil,
}
ami.boot()
ami.wait(1)
if name then assert(ami.snapshot_load(name .. ".state")) end
local base = ami.frame()
local function rd32(a) return ami.peek32(a) end
ami.hook(0x921a, function()
    print(string.format("f%d rabs d0=%d d1=%d a0=%06x caller=%06x", ami.frame() - base,
        ami.reg("d0") & 0xffff, ami.reg("d1") & 0xffff, ami.reg("a0"), rd32(ami.reg("a7"))))
end)
ami.hook(0x8fe8, function()
    print(string.format("f%d   boot block id at 49602 = %08x (X2B  = 58324220)", ami.frame() - base, ami.peek32(0x49602)))
end)
ami.hook(0x92b4, function() print(string.format("f%d   ok", ami.frame() - base)) end)
ami.hook(0x92bc, function() print(string.format("f%d   FAIL", ami.frame() - base)) end)
local swap_at = tonumber(args:match("swap=(%d+)") or "")
ami.wait(fire_at)
ami.joystick{ fire = true }; ami.wait(10); ami.joystick()
if swap_at then
    ami.wait(swap_at - fire_at - 10)
    ami.disk_eject(true); ami.wait(30); ami.disk_select(1); ami.disk_eject(false)
    print(string.format("f%d disk 2 in", ami.frame() - base))
    ami.wait(frames - swap_at - 30)
else
    ami.wait(frames)
end
local out = args:match("ram=(%S+)")
if out then ami.save_file(out, ami.read(0, 0x80000)) end

ami.quit()
