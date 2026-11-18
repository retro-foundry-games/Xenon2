-- Sample the PC once per frame over a cold run to the level (same inputs as run_level.lua); writes a histogram of PC pages.
--   --script-args "from=F until=F"
local args = ami.args or ""
local from = tonumber(args:match("from=(%d+)") or "9000")
local until_frame = tonumber(args:match("until=(%d+)") or "9500")
ami.machine{
    name = "Xenon2", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    content = "../local/xenon2.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_joyport_order = "2143" },
}
ami.boot()
local swapped, st, sf = false, 0, 0
ami.hook(0x8fe8, function() if not swapped and ami.peek32(0x49602) ~= 0x58324220 then swapped = true; st = 1 end end)
local fire_until, hist = 0, {}
while ami.frame() < until_frame do
    local f = ami.frame()
    if f >= 5200 and f % 150 == 0 then ami.joystick{ fire = true }; fire_until = f + 10 end
    if fire_until ~= 0 and f >= fire_until then ami.joystick(); fire_until = 0 end
    if st == 1 then ami.disk_eject(true); st = 2; sf = f
    elseif st == 2 and f >= sf + 30 then ami.disk_select(1); ami.disk_eject(false); st = 3 end
    if f >= from then
        local pc = ami.reg("pc")
        local k = string.format("%06x", pc & 0xfff000)
        hist[k] = (hist[k] or 0) + 1
    end
    ami.wait(1)
end
local keys = {}
for k in pairs(hist) do keys[#keys + 1] = k end
table.sort(keys)
for _, k in ipairs(keys) do print(k, hist[k]) end
