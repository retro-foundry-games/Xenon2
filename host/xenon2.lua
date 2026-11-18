-- Xenon 2 Megablast (Image Works, 1989) from the original disk on a plain Kickstart 1.3 A500.
-- Needs local/system/kick34005.A500 (CRC32 c4f0f55f) and local/xenon2.adf.
local args = ami.args or ""
local shots = args:match("shots=(%S+)")
-- flags (--script-args "..."):
--   joy1          the stick in port 1 (the title and menu poll it)
--   bm            the single-disk Black Monks release (local/xenon2.adf) instead of the two disks (local/xenon2.m3u)
--   nokeys        no keyboard-as-joystick (arrows = stick, Ctrl = fire)
--   load=NAME     restore local/save/NAME.state before running
--   snap=F[,..]   save local/save/xenon2-F.state at frame F (counted from the start or from load=)
--   swap=F[,..]   with shots=: change to the next disk at frame F
--   shots=DIR     headless screenshots; fire=F,.. presses fire at frame F, shot=F,.. screenshots at frame F (counted from the start or from load=)
-- In the window F11 saves local/save/frame-N.state and PageDown changes to the next disk (disk 2 when the game asks for the game disk).
local load_name = args:match("load=(%S+)")

ami.machine{
    name = "Xenon2",
    model = "A500",
    system = "../local/system",
    save = "../local/save",
    files = "..",
    content = args:match("bm") and "../local/xenon2.adf" or "../local/xenon2.m3u",
    roms = { { "kick34005.A500", 0xc4f0f55f } },
    joystick_keys = not args:match("nokeys"),   -- arrows = stick, Ctrl = fire (the keys then do not reach the keyboard)
    opt = args:match("joy1") and { puae_joyport_order = "2143" } or nil,   -- joy1: the stick in port 1
}
ami.boot()
if load_name then
    ami.wait(1)
    assert(ami.snapshot_load(load_name .. ".state"), "cannot load " .. load_name)
end
local snaps = {}
for n in (args:match("snap=([%d,]+)") or ""):gmatch("%d+") do snaps[#snaps + 1] = tonumber(n) end
if #snaps > 0 then
    local base = ami.frame()
    for _, f in ipairs(snaps) do
        ami.wait(base + f - ami.frame())
        ami.snapshot_save(string.format("xenon2-%d.state", f))
    end
end

if shots then
    local base = ami.frame()
    local function shot(f)
        ami.wait(base + f - ami.frame())
        ami.screenshot(string.format("%s/xenon2-%d.ppm", shots, f))
    end
    local function fire(f)
        ami.wait(base + f - ami.frame())
        ami.joystick{ fire = true }; ami.wait(10); ami.joystick()
    end
    -- fire=F,... presses fire at those frames; shot=F,... screenshots
    local function swap(f)
        ami.wait(base + f - ami.frame())
        ami.disk_eject(true); ami.wait(30); ami.disk_select(1); ami.disk_eject(false)
        print("disk now", ami.disk_index(), "of", ami.disk_count())
    end
    local events = {}
    for n in (args:match("swap=([%d,]+)") or ""):gmatch("%d+") do events[#events + 1] = { tonumber(n), swap } end
    for n in (args:match("fire=([%d,]+)") or ""):gmatch("%d+") do events[#events + 1] = { tonumber(n), fire } end
    for n in (args:match("shot=([%d,]+)") or ""):gmatch("%d+") do events[#events + 1] = { tonumber(n), shot } end
    table.sort(events, function(a, b) return a[1] < b[1] end)
    for _, e in ipairs(events) do e[2](e[1]) end
end
