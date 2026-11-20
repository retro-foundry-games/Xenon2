-- Exact executed-instruction trace of a level: from an original's state where the level is loaded and waits for the fire button (a savestate of the player's), single-stepped for FRAMES
-- frames under a seeded random player that keeps the ship high on the screen and firing (the level only scrolls on then) and has its energy refilled every frame.
--   ami9000_host host/trace_level.lua --headless --script-args "state=frame-4464 frames=7000 out=captures/trace-l2-1 [seed=N] [ramat=F,..] [poke=ADDR:SIZE:VALUE,..]"
-- Writes OUT.pcs (`pc count first_frame last_frame successor,...`, as host/trace_steps.lua) and OUT-F.bin memory dumps at the frames of ramat (frames counted from the state).
local args = ami.args or ""
local FRAMES = tonumber(args:match("frames=(%d+)") or "7000")
local out = args:match("out=(%S+)") or "captures/trace-level"
local seed = tonumber(args:match("seed=(%d+)") or "1")
local state = args:match("state=(%S+)")
local ramat = {}
for n in (args:match("ramat=([%d,]+)") or ""):gmatch("%d+") do ramat[tonumber(n)] = true end
ami.machine{
    name = "Xenon2", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    content = "../local/xenon2.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_joyport_order = "2143" },
}
ami.boot()
ami.wait(1)
assert(ami.snapshot_load(state .. ".state"), "cannot load " .. tostring(state))
local base = ami.frame()
for addr, size, value in (args:match("poke=(%S+)") or ""):gmatch("(%x+):([bwl]):(%d+)") do
    local a, v = tonumber(addr, 16), tonumber(value)
    if size == "b" then ami.poke8(a, v) elseif size == "w" then ami.poke16(a, v) else ami.poke32(a, v) end
end

local function random(n) seed = (seed * 1103515245 + 12345) % 2147483648; return (seed // 65536) % n end
local hold_until = 0
local function tick(f)   -- f counted from the state
    ami.poke16(0xcc6, 0x27)   -- (the energy)
    if f >= 5 and f < 12 then ami.joystick{ fire = true }; return end   -- (the fire button starts the level)
    if f == 12 then ami.joystick() end
    if f >= 20 and f >= hold_until then
        local choice = random(10)
        local st = {}
        if choice <= 3 then st.up = true elseif choice == 4 then st.left = true elseif choice == 5 then st.right = true elseif choice == 6 then st.up = true; st.left = true elseif choice == 7 then st.up = true; st.right = true end
        if random(10) < 7 then st.fire = true end
        ami.joystick(st)
        hold_until = f + 4 + random(20)
    end
    if ramat[f] then ami.save_file(string.format("%s-%d.bin", out, f), ami.read(0, 0x80000)) end
end

local count, succ, firstf, lastf = {}, {}, {}, {}
local prev, frame = nil, 0
local done = false
ami.step(function(pc)
    local f = ami.frame() - base
    if f ~= frame then
        frame = f
        tick(f)
        if f >= FRAMES then
            local keys = {}
            for k in pairs(count) do keys[#keys + 1] = k end
            table.sort(keys)
            local lines = {}
            for _, k in ipairs(keys) do
                local nx = {}
                for t in pairs(succ[k] or {}) do nx[#nx + 1] = string.format("%06x", t) end
                table.sort(nx)
                lines[#lines + 1] = string.format("%06x %d %d %d %s", k, count[k], firstf[k], lastf[k], table.concat(nx, ","))
            end
            ami.save_file(out .. ".pcs", table.concat(lines, "\n") .. "\n")
            done = true
            return false
        end
    end
    count[pc] = (count[pc] or 0) + 1
    if not firstf[pc] then firstf[pc] = f end
    lastf[pc] = f
    if prev then local s = succ[prev]; if not s then s = {}; succ[prev] = s end; s[pc] = true end
    prev = pc
    return true
end)
while not done do ami.wait(1) end
ami.quit()
