-- A savestate of the original at a place in a level, made from a state of that level (SCENARIOS below names the places): scroll_y is the place the game puts the
-- player back at after a death, the death is announced (player_died), and the game's own respawn clears the level objects and puts the map and the player there.  The
-- machine then runs FRAMES frames (the fire button is tapped to dismiss the "ready" message, which lasts about 170 frames; the energy is kept full) and the state is saved.
--   ami9000_host host/scenario.lua --headless --script-args "scenario=l1-boss"
--   ami9000_host host/scenario.lua --headless --script-args "state=frame-8880 scroll=b40 out=NAME [frames=200] [probe=1]"
-- probe=1 prints, every 25 frames, the kinds of object that are there and the level's flags.
local args = ami.args or ""
-- the state of the level it starts from and the scroll_y (a level scrolls down from $11EA to 0; the boss events and zones are in the lists of
-- host/level_events.lua and in level*_scroll)
local SCENARIOS = {
    ["l1-boss"] = { state = "frame-8880",   scroll = 0x9e0 },
    ["l2-boss"] = { state = "level2-start", scroll = 0xb30 },
    ["l3-boss"] = { state = "frame-2280",   scroll = 0xb20 },
    ["l4-boss"] = { state = "frame-1197",   scroll = 0x9e0 },
    ["l5-boss"] = { state = "frame-2094",   scroll = 0x1c0 },
}
local scenario = SCENARIOS[args:match("scenario=(%S+)") or ""] or {}
local state = args:match("state=(%S+)") or scenario.state
local SCROLL = tonumber(args:match("scroll=(%x+)") or "", 16) or scenario.scroll
local FRAMES = tonumber(args:match("frames=(%d+)") or "") or scenario.frames or 240
local out = args:match("out=(%S+)") or args:match("scenario=(%S+)")
local PROBE = args:match("probe=1") ~= nil
assert(state and SCROLL and out, "scenario=NAME, or state=, scroll= and out=")
ami.machine{
    name = "Xenon2", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    content = "../local/xenon2.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_joyport_order = "2143" },
}
ami.boot()
ami.wait(1)
assert(ami.snapshot_load(state .. ".state"), "cannot load " .. tostring(state))
local OBJECTS, OBJECT_SIZE, OBJECT_COUNT = 0x3f07e, 98, 159
local function kinds()
    local seen, order = {}, {}
    for i = 0, OBJECT_COUNT - 1 do
        local type = ami.peek16(OBJECTS + i * OBJECT_SIZE)
        if type ~= 0 then
            if not seen[type] then seen[type] = 0; order[#order + 1] = type end
            seen[type] = seen[type] + 1
        end
    end
    table.sort(order)
    local text = {}
    for _, type in ipairs(order) do text[#text + 1] = string.format("%x%s", type, seen[type] > 1 and ("x" .. seen[type]) or "") end
    return table.concat(text, " ")
end
local function keep_alive()
    ami.poke16(0xcca, 5)        -- player_lives
    ami.poke16(0xcc6, 0x27)     -- player_energy
end
ami.poke16(0xce6, SCROLL)       -- scroll_y
keep_alive()
ami.poke8(0xa02, 0xff)          -- player_died
for f = 1, FRAMES do
    if f % 40 == 10 then ami.joystick{ fire = true } elseif f % 40 == 20 then ami.joystick() end
    keep_alive()
    ami.wait(1)
    if PROBE and f % 25 == 0 then
        ami.log(string.format("frame %d scroll_pos %x boss_dead %x level_over %x: %s", f, ami.peek16(0xcd0), ami.peek8(0xa07), ami.peek8(0xa6c), kinds()))
    end
end
ami.log(string.format("level %d: scroll_pos %x scroll_y %x", ami.peek16(0xe04), ami.peek16(0xcd0), ami.peek16(0xce6)))
assert(ami.snapshot_save(out .. ".state"))
ami.quit()
