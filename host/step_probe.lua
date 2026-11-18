-- Exact executed-instruction trace of the cold run (menu -> level 1): free-runs to FROM, then single-steps to UNTIL with the same
-- inputs as run_level.lua (fire every 150 frames from 5200, disk 2 in after the game's disk check).  Hooks report an address that is
-- sometimes two bytes off while the machine free-runs; ami.step reports exact instruction addresses.
--   --script-args "from=9000 until=13000 out=captures/trace [ramat=F,..]"
-- Writes OUT.pcs: `pc count successor,successor,...` for every instruction that ran, and OUT-F.bin RAM dumps.
local args = ami.args or ""
local from = tonumber(args:match("from=(%d+)") or "9000")
local until_frame = tonumber(args:match("until=(%d+)") or "13000")
local out = args:match("out=(%S+)") or "captures/trace"
local ramat = {}
for n in (args:match("ramat=([%d,]+)") or ""):gmatch("%d+") do ramat[tonumber(n)] = true end
ami.machine{
    name = "Xenon2", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    content = "../local/xenon2.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_joyport_order = "2143" },
}
ami.boot()
local swapped, st, sf, last_read, nreads, fire_until = false, 0, 0, -1000, 0, 0
local reads = {}
ami.hook(0x921a, function()
    last_read = ami.frame(); nreads = nreads + 1
    reads[#reads + 1] = string.format("f%d rabs d0=%d d1=%d a0=%06x", ami.frame(), ami.reg("d0") & 0xffff, ami.reg("d1") & 0xffff, ami.reg("a0"))
end)
-- the inputs of one frame (the same in the free-running and the stepped part)
local function tick(f)
    if f >= 5200 and f % 150 == 0 then ami.joystick{ fire = true }; fire_until = f + 10 end
    if fire_until ~= 0 and f >= fire_until then ami.joystick(); fire_until = 0 end
    local id = ami.peek32(0x49602)
    if not swapped and nreads > 0 and f >= last_read + 60 and id ~= 0x58324220 then swapped = true; st = 1 end
    if st == 1 then ami.disk_eject(true); st = 2; sf = f
    elseif st == 2 and f >= sf + 30 then ami.disk_select(1); ami.disk_eject(false); st = 3 end
    if ramat[f] then ami.save_file(string.format("%s-%d.bin", out, f), ami.read(0, 0x80000)) end
end
while ami.frame() < from do tick(ami.frame()); ami.wait(1) end
local seen, n, frame, done = {}, 0, ami.frame(), false
local lines = {}
ami.step(function(pc)
    local f = ami.frame()
    if f ~= frame then frame = f; tick(f) end
    n = n + 1
    if pc >= 0xc40 and pc <= 0xc62 and not seen[pc] then
        seen[pc] = true
        lines[#lines + 1] = string.format("f%d pc=%06x bytes=%s a0=%08x a1=%08x d0=%08x sr=%04x", f, pc, ami.read(pc, 10):gsub(".", function(c) return string.format("%02x", c:byte()) end), ami.reg("a0"), ami.reg("a1"), ami.reg("d0"), ami.reg("sr"))
    end
    if n >= 400000 then
        ami.save_file("captures/step_probe.txt", table.concat(lines, "\n") .. "\n")
        done = true
        return false
    end
    return true
end)
while not done do ami.wait(1) end
ami.quit()
