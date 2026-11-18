-- Restore a snapshot and write chip RAM and the CPU registers (headless).
--   --script-args "joy1 load=NAME ram=PATH [frames=N]"
local args = ami.args or ""
local name = args:match("load=(%S+)")
local out = args:match("ram=(%S+)")
local frames = tonumber(args:match("frames=(%d+)") or "1")
ami.machine{
    name = "Xenon2", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    content = "../local/xenon2.adf", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = args:match("joy1") and { puae_joyport_order = "2143" } or nil,
}
ami.boot()
ami.wait(1)
assert(ami.snapshot_load(name .. ".state"))
ami.wait(frames)
ami.save_file(out, ami.read(0, 0x80000))
local regs = {}
for _, r in ipairs{ "pc", "sr", "d0","d1","d2","d3","d4","d5","d6","d7","a0","a1","a2","a3","a4","a5","a6","a7" } do
    regs[#regs + 1] = string.format("%s=%08x", r, ami.reg(r))
end
ami.save_file(out .. ".regs", table.concat(regs, "\n") .. "\n")
