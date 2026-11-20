-- Where the original is in a savestate: the PCs of the first instructions after loading it and the return addresses on the stack.
--   ami9000_host host/state_pc.lua --headless --script-args "state=frame-4464 [n=2000]"
local args = ami.args or ""
local state = args:match("state=(%S+)")
local N = tonumber(args:match("n=(%d+)") or "2000")
ami.machine{
    name = "Xenon2", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    content = "../local/xenon2.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_joyport_order = "2143" },
}
ami.boot()
ami.wait(1)
assert(ami.snapshot_load(state .. ".state"), "cannot load " .. tostring(state))
ami.log("loaded, stepping")
local seen, order, count = {}, {}, 0
local finished = false
local function body(pc)
    count = count + 1
    if not seen[pc] then seen[pc] = true; order[#order + 1] = pc end
    if count >= N then
        local sp = ami.reg("a7")
        local out = {}
        for i = 1, math.min(#order, 40) do out[#out + 1] = string.format("%06x", order[i]) end
        ami.log("pcs: " .. table.concat(out, " "))
        if sp then
            local st = {}
            for i = 0, 15 do st[#st + 1] = string.format("%08x", ami.peek32(sp + 4 * i)) end
            ami.log("a7=" .. string.format("%x", sp) .. " stack: " .. table.concat(st, " "))
        end
        finished = true
        return false
    end
    return true
end
ami.step(function(pc)
    local ok, r = pcall(body, pc)
    if not ok then ami.log('error: ' .. tostring(r)); finished = true; return false end
    return r
end)
while not finished do ami.wait(1) end
ami.quit()
