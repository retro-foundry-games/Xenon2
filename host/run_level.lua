-- Cold boot to the first level, scripted: fire every 150 frames from 5200 on; the first time the game's disk check
-- ($8fe8, "X2B " at $49602) fails, disk 2 replaces disk 1.  Logs the game's disk reads ($921a) and screenshots.
--   --script-args "joy1 until=N [ram=PATH] [shots=DIR every=N] [cover=PATH coverfrom=F coverlo=HEX coverhi=HEX] [ramat=F,.. ramprefix=P]"
-- (no 'load=': a snapshot restore puts disk 1 back in DF0)
local args = ami.args or ""
local until_frame = tonumber(args:match("until=(%d+)") or "16000")
local ram_out = args:match("ram=(%S+)")
local shots = args:match("shots=(%S+)")
local every = tonumber(args:match("every=(%d+)") or "1000")
local cover = args:match("cover=(%S+)")
local cover_lo = tonumber(args:match("coverlo=(%x+)") or "0", 16)
local cover_hi = tonumber(args:match("coverhi=(%x+)") or "7fffe", 16)
local cover_from = tonumber(args:match("coverfrom=(%d+)") or "0")
ami.machine{
    name = "Xenon2", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    content = "../local/xenon2.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_joyport_order = "2143" },
}
ami.boot()
local swapped, swap_state, swap_frames = false, 0, 0
local reads = {}
local last_read = -1000
local function log_read()
    last_read = ami.frame()
    reads[#reads + 1] = string.format("f%d rabs d0=%d d1=%d a0=%06x caller=%06x", ami.frame(),
        ami.reg("d0") & 0xffff, ami.reg("d1") & 0xffff, ami.reg("a0"), ami.peek32(ami.reg("a7")))
end
ami.hook(0x921a, log_read)
local ramat = {}
for n in (args:match("ramat=([%d,]+)") or ""):gmatch("%d+") do ramat[tonumber(n)] = true end
local ram_prefix = args:match("ramprefix=(%S+)") or "captures/ram"
local covered, order = {}, {}
-- the address comes from the closure: ami.reg("pc") inside a hook is the CPU's prefetched PC, not the instruction's address
local function cover_hook(a)
    return function()
        if not covered[a] then covered[a] = true; order[#order + 1] = a end
        if a == 0x921a then log_read() end
    end
end
local cover_on = false
local fire_until = 0
while ami.frame() < until_frame do
    local f = ami.frame()
    if cover and not cover_on and f >= cover_from then
        for a = cover_lo, cover_hi, 2 do ami.hook(a, cover_hook(a)) end
        cover_on = true
    end
    if f >= 5200 and f % 150 == 0 then ami.joystick{ fire = true }; fire_until = f + 10 end
    if fire_until ~= 0 and f >= fire_until then ami.joystick(); fire_until = 0 end
    -- the game's disk check read sector 0 of disk 1 (not "X2B " at $49602): disk 2 replaces it
    local id = ami.peek32(0x49602)
    if not swapped and #reads > 0 and f >= last_read + 60 and id ~= 0x58324220 then swapped = true; swap_state = 1 end
    if swap_state == 1 then ami.disk_eject(true); swap_state = 2; swap_frames = f
    elseif swap_state == 2 and f >= swap_frames + 30 then
        ami.disk_select(1); ami.disk_eject(false); swap_state = 3
        print(string.format("f%d disk 2 in", f))
    end
    if ramat[f] then ami.save_file(string.format("%s-%d.bin", ram_prefix, f), ami.read(0, 0x80000)) end
    if shots and f % every == 0 then ami.screenshot(string.format("%s/run-%d.ppm", shots, f)) end
    ami.wait(1)
end
if ram_out then ami.save_file(ram_out, ami.read(0, 0x80000)) end
ami.save_file((ram_out or "captures/run") .. ".reads", table.concat(reads, "\n") .. "\n")
if cover then
    table.sort(order)
    local out = {}
    for _, pc in ipairs(order) do out[#out + 1] = string.format("%06x", pc) end
    ami.save_file(cover, table.concat(out, "\n") .. "\n")
end

ami.quit()
