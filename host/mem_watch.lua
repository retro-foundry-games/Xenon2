-- Who writes a chip RAM word: single-steps from a savestate (after N free-running frames) and logs every change of the watched words with the PC that ran.
--   --script-args "state=NAME from=FRAME steps=N watch=ADDR,ADDR"
local args = ami.args or ""
local STATE = args:match("state=(%S+)")
local from = tonumber(args:match("from=(%d+)") or "1140")
local steps = tonumber(args:match("steps=(%d+)") or "400000")
local watch = {}
for a in (args:match("watch=([%x,]+)") or ""):gmatch("%x+") do watch[#watch + 1] = tonumber(a, 16) end
ami.machine{ name = "Xenon2 memwatch", model = "A500", system = "../local/system", save = "../local/save", files = "..", snapshots = "../local/save",
    content = "../local/xenon2.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } }, opt = { puae_joyport_order = "2143" } }
ami.boot()
ami.wait(1)
assert(ami.snapshot_load(STATE .. ".state"))
ami.wait(from)
local last = {}
for i, a in ipairs(watch) do last[i] = ami.peek16(a) end
local n, prev_pc = 0, 0
ami.log(string.format("start at frame %d, watching %d words", ami.frame(), #watch))
ami.step(function(pc)
    for i, a in ipairs(watch) do
        local v = ami.peek16(a)
        if v ~= last[i] then
            ami.log(string.format("frame %d step %d: %06x %04x -> %04x  written by the instruction at %06x", ami.frame(), n, a, last[i], v, prev_pc))
            last[i] = v
        end
    end
    prev_pc = pc
    n = n + 1
    return n < steps
end)
ami.quit()
