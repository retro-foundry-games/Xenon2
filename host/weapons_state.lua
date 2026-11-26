-- A savestate of the original with weapons given by the original's own routines: the state of a level plus NAME[:LEVEL],... (as xenon2-sdl-dev --weapons);
-- a side weapon named twice is on both sides.  The lockstep starts from the saved state.
--   ami9000_host host/weapons_state.lua --headless --script-args "state=l2-start out=l2-weapons weapons=laser:2,laser:2,flamer:2"
local args = ami.args or ""
local state, out, list = args:match("state=(%S+)"), args:match("out=(%S+)"), args:match("weapons=(%S+)")
assert(state and out and list, "state=, out= and weapons=")
local GIVE = {
    forward = 0x4afc, double = 0x4ba8, side = 0x4c64, bomb = 0x4d28, homing = 0x4dc6, flamer = 0x4e74, drone = 0x4f66,
    electro = 0x5014, mine1 = 0x50ac, mine2 = 0x5158, laser = 0x538c, cannon = 0x543a, missile = 0x52e0, rear = 0x5206,
}
local TYPE = {
    forward = 0x94, double = 0xc0, side = 0x98, bomb = 0x28, homing = 0x48, flamer = 0xb0, drone = 0x2c, electro = 0x4c,
    mine1 = 0x38, mine2 = 0x3c, laser = 0x20, cannon = 0x34, missile = 0x90, rear = 0x44,
}
local PLAYER, SLOTS, SCRATCH = 0x3f01c, 0xc66, 0x7e000
ami.machine{
    name = "Xenon2", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    content = "../local/xenon2.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_joyport_order = "2143" },
}
ami.boot()
ami.wait(1)
assert(ami.snapshot_load(state .. ".state"), "cannot load " .. tostring(state))
local function slot_objects(type)
    local o = {}
    for i = 0, 6 do
        if ami.peek16(SLOTS + i * 6) == type then o[i] = ami.peek32(SLOTS + i * 6 + 2) end
    end
    return o
end
local done = false
ami.hook(0x3554, function()   -- (scroll_events: the game stands between two frames' work; ami.call works inside a hook only)
    if done then return end
    done = true
    for name, level in list:gmatch("(%w+):?(%d*)") do
        assert(GIVE[name], "no weapon " .. name)
        local before = slot_objects(TYPE[name])
        ami.call(GIVE[name], { a0 = PLAYER, a1 = 0, a7 = SCRATCH, sr = 0x2700 })
        local weapon
        for i, obj in pairs(slot_objects(TYPE[name])) do if obj ~= before[i] then weapon = obj end end
        if weapon then
            local max = ami.peek16(weapon + 50)
            local want = tonumber(level) or 0
            ami.poke16(weapon + 66, want < max and want or max)
            ami.log(string.format("%s: object $%x power %d (max %d)", name, weapon, ami.peek16(weapon + 66), max))
        else
            ami.log("no free slot for " .. name)
        end
    end
end)
for _ = 1, 600 do
    if done then break end
    ami.wait(1)
end
assert(done, "the level did not run")
ami.wait(1)
assert(ami.snapshot_save(out .. ".state"))
ami.quit()
