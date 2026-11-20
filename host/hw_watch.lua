-- Logs every change of the display registers (value, the PC that wrote it) frame by frame from a savestate.
--   --script-args "state=NAME frames=N"
local args = ami.args or ""
local STATE = args:match("state=(%S+)")
local frames = tonumber(args:match("frames=(%d+)") or "1200")
ami.machine{
    name = "Xenon2 watch", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    snapshots = "../local/save",
    content = "../local/xenon2.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_joyport_order = "2143" },
}
ami.boot()
ami.wait(1)
assert(ami.snapshot_load(STATE .. ".state"))
local names = { [0x8e] = "diwstrt", [0x90] = "diwstop", [0x92] = "ddfstrt", [0x94] = "ddfstop", [0x96] = "dmacon", [0x100] = "bplcon0", [0x102] = "bplcon1",
    [0x104] = "bplcon2", [0x108] = "bpl1mod", [0x10a] = "bpl2mod", [0x80] = "cop1lch", [0x82] = "cop1lcl" }
for i = 0, 5 do names[0xe0 + 4 * i] = "bpl" .. (i + 1) .. "pth"; names[0xe2 + 4 * i] = "bpl" .. (i + 1) .. "ptl" end
for i = 0, 7 do names[0x120 + 4 * i] = "spr" .. i .. "pth"; names[0x122 + 4 * i] = "spr" .. i .. "ptl"; names[0x140 + 8 * i] = "spr" .. i .. "pos"; names[0x142 + 8 * i] = "spr" .. i .. "ctl" end
for i = 16, 31 do names[0x180 + 2 * i] = "color" .. i end
local keys = {}
for k in pairs(names) do keys[#keys + 1] = k end
table.sort(keys)
local last = {}
for f = 1, frames do
    for _, k in ipairs(keys) do
        local v, pc = ami.custom(k)
        local sig = tostring(v) .. "/" .. tostring(pc)
        if last[k] ~= sig then
            last[k] = sig
            ami.log(string.format("frame %4d %-8s = %s  pc %s", f, names[k], v and string.format("%04x", v) or "nil", pc and string.format("%06x", pc) or "nil"))
        end
    end
    ami.wait(1)
end
ami.quit()
