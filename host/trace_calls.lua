-- The order in which the original enters the functions of analysis/functions.txt after the game starts ($7032), up to the Nth entry of swap_screens.
--   --script-args "calls=N swaps=K"
local args = ami.args or ""
local MAXCALLS = tonumber(args:match("calls=(%d+)") or "80")
local SWAPS = tonumber(args:match("swaps=(%d+)") or "1")
ami.machine{
    name = "Xenon2 calls", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    content = "../local/xenon2.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_joyport_order = "2143" },
}
ami.boot()
local started, log, swaps = false, {}, 0
local text = ami.file_read("analysis/functions.txt", 0, ami.file_size("analysis/functions.txt"))
for line in text:gmatch("[^\n]+") do
    local addr = tonumber(line:match("^(%x+)"), 16)
    if addr and addr ~= 0x7032 then
        ami.hook(addr, function()
            if started and #log < MAXCALLS then log[#log + 1] = string.format("%06x", addr) end
            if started and addr == 0x2034 then swaps = swaps + 1; print(string.format("swap_screens entry %d (frame %d): draw_screen %08x show_screen %08x copper_cur %08x", swaps, ami.frame(), ami.peek32(0x406), ami.peek32(0x40a), ami.peek32(0x700c))) end
        end)
    end
end
for _, a in ipairs{ 0x2026, 0x202e } do
    ami.hook(a, function()
        if not started then return end
        swaps = swaps + 1
        print(string.format("swap thunk $%x entry %d (frame %d): vbl_count %d v_0404 %d v_0410 %d", a, swaps, ami.frame(), ami.peek16(0x41c), ami.peek16(0x404), ami.peek16(0x410)))
    end)
end
ami.hook(0x7032, function() started = true; print(string.format("start: draw_screen %08x show_screen %08x copper_cur %08x", ami.peek32(0x406), ami.peek32(0x40a), ami.peek32(0x700c))) end)
local fire_until = 0
while not (started and (swaps >= SWAPS or #log >= MAXCALLS)) do
    local f = ami.frame()
    if not started then
        if f >= 5200 and f % 150 == 0 then ami.joystick{ fire = true }; fire_until = f + 10 end
        if fire_until ~= 0 and f >= fire_until then ami.joystick(); fire_until = 0 end
    end
    ami.wait(1)
end
print("original: " .. table.concat(log, " "))
ami.quit()
