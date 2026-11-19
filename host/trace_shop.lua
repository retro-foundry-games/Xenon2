-- Exact executed-instruction trace of the shop: from the original's state before the shop (local/save/shop-before.state, made by host/lockstep.lua snap=8ff2:shop-before),
-- single-stepped for FRAMES frames while a seeded random player (the stick and fire, a press every few frames) works the shop's menus.
--   ami9000_host host/trace_shop.lua --headless --script-args "frames=6000 out=captures/trace-shop [seed=N] [state=shop-before] [ramat=F,..] [poke=ADDR:SIZE:VALUE,..]"
-- Writes OUT.pcs (`pc count first_frame last_frame successor,...`, as host/trace_steps.lua) and OUT-F.bin memory dumps at the frames of ramat (frames counted from the state).
local args = ami.args or ""
local FRAMES = tonumber(args:match("frames=(%d+)") or "6000")
local out = args:match("out=(%S+)") or "captures/trace-shop"
local seed = tonumber(args:match("seed=(%d+)") or "1")
local state = args:match("state=(%S+)") or "shop-before"
local ramat = {}
for n in (args:match("ramat=([%d,]+)") or ""):gmatch("%d+") do ramat[tonumber(n)] = true end
ami.machine{
    name = "Xenon2", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    content = "../local/xenon2.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_joyport_order = "2143" },
}
ami.boot()
ami.wait(1)
assert(ami.snapshot_load(state .. ".state"), "cannot load " .. state)
local base = ami.frame()
-- poke=ADDR:SIZE:VALUE,... (hex address, size b/w/l, decimal value): variables set before the run (the cash, the setting: e04 = 5 and 8fec = 255 reach the other paths)
for addr, size, value in (args:match("poke=(%S+)") or ""):gmatch("(%x+):([bwl]):(%d+)") do
    local a, v = tonumber(addr, 16), tonumber(value)
    if size == "b" then ami.poke8(a, v) elseif size == "w" then ami.poke16(a, v) else ami.poke32(a, v) end
end

local function random(n) seed = (seed * 1103515245 + 12345) % 2147483648; return (seed // 65536) % n end
local hold_until, held = 0, nil
local function tick(f)   -- f counted from the state
    if f >= hold_until then
        local choice = random(10)
        local st = {}
        if choice == 0 then st.up = true elseif choice == 1 then st.down = true elseif choice == 2 then st.left = true elseif choice == 3 then st.right = true
        elseif choice <= 6 then st.fire = true end   -- (fire is the commonest: it chooses)
        ami.joystick(st)
        hold_until = f + 3 + random(10)
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
