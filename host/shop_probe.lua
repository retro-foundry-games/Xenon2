-- The registers of the original at some addresses of the shop, under the seeded random player of host/trace_shop.lua (free-running, so the hooks are cheap).
--   ami9000_host host/shop_probe.lua --headless --script-args "frames=2200 seed=5 at=55016,55022,55026 [out=FILE]"
local args = ami.args or ""
local FRAMES = tonumber(args:match("frames=(%d+)") or "2200")
local seed = tonumber(args:match("seed=(%d+)") or "1")
local out = args:match("out=(%S+)") or "captures/shop-probe.txt"
local lines = {}
ami.machine{
    name = "Xenon2", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    content = "../local/xenon2.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_joyport_order = "2143" },
}
ami.boot()
ami.wait(1)
assert(ami.snapshot_load("shop-before.state"))
local base = ami.frame()
for a in (args:match("at=([%x,]+)") or ""):gmatch("%x+") do
    ami.hook(tonumber(a, 16), function()
        local r = {}
        for _, n in ipairs({ "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "a0", "a1", "a2", "a3", "a4", "a5", "a6" }) do r[#r + 1] = string.format("%s=%x", n, ami.reg(n)) end
        lines[#lines + 1] = string.format("frame %d pc=%x %s", ami.frame() - base, ami.reg("pc"), table.concat(r, " "))
    end)
end
local function random(n) seed = (seed * 1103515245 + 12345) % 2147483648; return (seed // 65536) % n end
local hold_until = 0
for f = 0, FRAMES do
    if f >= hold_until then
        local choice = random(10)
        local st = {}
        if choice == 0 then st.up = true elseif choice == 1 then st.down = true elseif choice == 2 then st.left = true elseif choice == 3 then st.right = true
        elseif choice <= 6 then st.fire = true end
        ami.joystick(st)
        hold_until = f + 3 + random(10)
    end
    ami.wait(1)
end
ami.save_file(out, table.concat(lines, "\n") .. "\n")
ami.quit()
