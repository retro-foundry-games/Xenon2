-- RAM of the original when the game's own frame counter ($41C) reaches VBL, for comparing with the port (tools/compare_state.py).
-- Cold boot; the fire presses of run_level.lua only until the game starts ($7032), after that no input, as in the port without --joy.
--   --script-args "vbl=V out=PATH"
local args = ami.args or ""
local VBL = tonumber(args:match("vbl=(%d+)") or "100")
local OUT = args:match("out=(%S+)") or "captures/orig_state.bin"
ami.machine{
    name = "Xenon2", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    content = "../local/xenon2.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_joyport_order = "2143" },
}
ami.boot()
local started, fire_until = false, 0
ami.hook(0x7032, function() started = true end)
while true do
    local f = ami.frame()
    if not started then
        if f >= 5200 and f % 150 == 0 then ami.joystick{ fire = true }; fire_until = f + 10 end
        if fire_until ~= 0 and f >= fire_until then ami.joystick(); fire_until = 0 end
    elseif fire_until ~= 0 then ami.joystick(); fire_until = 0 end
    if started and ami.peek16(0x41c) >= VBL then break end
    if f > 20000 then break end
    ami.wait(1)
end
ami.save_file(OUT, ami.read(0, 0x80000))
print("exported at frame", ami.frame(), "vbl", ami.peek16(0x41c))
ami.quit()
