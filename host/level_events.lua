-- The scroll events of the level of a state: "scroll arg" of the records of the two lists (the first list calls level_event, the second level_hook).
--   ami9000_host host/level_events.lua --headless --script-args "state=frame-8880"
local args = ami.args or ""
local state = args:match("state=(%S+)")
ami.machine{
    name = "Xenon2", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    content = "../local/xenon2.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_joyport_order = "2143" },
}
ami.boot()
ami.wait(1)
assert(ami.snapshot_load(state .. ".state"), "cannot load " .. tostring(state))
local function list(name, pointer)
    local rec = ami.peek32(pointer)
    local out = {}
    while true do
        local scroll = ami.peek16(rec)
        if scroll >= 0x8000 then break end
        out[#out + 1] = string.format("%x:%x", scroll, ami.peek16(rec + 2))
        rec = rec + 14
    end
    ami.log(string.format("level %d %s: %s", ami.peek16(0xe04), name, table.concat(out, " ")))
end
list("events_a", 0x54e38)
list("events_b", 0x54e14)
ami.log(string.format("scroll_pos %x scroll_y %x", ami.peek16(0xcd0), ami.peek16(0xce6)))
ami.quit()
