-- The custom chip registers as the original last wrote them (value and the PC that wrote it), from a savestate.
--   --script-args "state=NAME frames=N"
local args = ami.args or ""
local STATE = args:match("state=(%S+)")
local frames = tonumber(args:match("frames=(%d+)") or "5")
ami.machine{
    name = "Xenon2 probe", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    snapshots = "../local/save",
    content = "../local/xenon2.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_chipmem_size = 2, puae_joyport_order = "2143" },
}
ami.boot()
ami.wait(1)
assert(ami.snapshot_load(STATE))
ami.wait(frames)
local names = {
  [0x02]="dmaconr", [0x1c]="intenar", [0x1e]="intreqr", [0x20]="dskpth", [0x22]="dsklen?", [0x80]="cop1lch", [0x82]="cop1lcl", [0x84]="cop2lch", [0x86]="cop2lcl",
  [0x8e]="diwstrt", [0x90]="diwstop", [0x92]="ddfstrt", [0x94]="ddfstop", [0x96]="dmacon", [0x9a]="intena", [0x9c]="intreq", [0x9e]="adkcon",
  [0x100]="bplcon0", [0x102]="bplcon1", [0x104]="bplcon2", [0x108]="bpl1mod", [0x10a]="bpl2mod",
}
for i = 0, 5 do names[0xe0 + 4 * i] = "bpl" .. (i + 1) .. "pth"; names[0xe2 + 4 * i] = "bpl" .. (i + 1) .. "ptl" end
for i = 0, 7 do names[0x120 + 4 * i] = "spr" .. i .. "pth"; names[0x122 + 4 * i] = "spr" .. i .. "ptl" end
for i = 0, 3 do
  names[0xa0 + 16 * i] = "aud" .. i .. "lch"; names[0xa2 + 16 * i] = "aud" .. i .. "lcl"; names[0xa4 + 16 * i] = "aud" .. i .. "len"
  names[0xa6 + 16 * i] = "aud" .. i .. "per"; names[0xa8 + 16 * i] = "aud" .. i .. "vol"
end
for i = 0, 31 do names[0x180 + 2 * i] = "color" .. i end
local keys = {}
for k in pairs(names) do keys[#keys + 1] = k end
table.sort(keys)
local out = {}
for _, k in ipairs(keys) do
    local ok, v, pc = pcall(ami.custom, k)
    if ok then out[#out + 1] = string.format("%03x %-9s %04x  by pc=%06x", k, names[k], v, pc) else out[#out + 1] = string.format("%03x %-9s n/a", k, names[k]) end
end
ami.save_file(args:match("out=(%S+)") or "captures/hw_probe.txt", table.concat(out, "\n") .. "\n")
ami.quit()
