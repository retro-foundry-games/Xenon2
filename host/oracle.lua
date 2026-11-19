-- Check the C functions of native/src (built as a blob by `make blob`) against the original, on the machine as it runs.
--
-- Boots the original disks in a machine with 1 MB chip RAM (the C blob sits above the original's 512 KB), restores a savestate
-- (host/mkstate.lua), and hooks the original's entry of every function in host/cases/NAME.lua.  At each call (the first SAMPLES of them)
-- the state is saved, the original routine is run on it with ami.call, the memory is put back, the C function is run on the same
-- state with the arguments in the registers the original has them in, and the two are compared: the result register and all of
-- chip RAM (except the C code and the test's stack).  Then the state is put back, so the program goes on as if nothing had happened.
--
--   ami9000_host host/oracle.lua --headless --script-args "state=NAME [only=NAME[,NAME]] [samples=N] [stride=N] [until=FRAMES] [joy=F:DIRS,...] [swap=1] [build=build/NAME] [out=FILE]"
local args = ami.args or ""
local only = {}
for n in (args:match("only=([%w_,]+)") or ""):gmatch("[%w_]+") do only[n] = true end
local SAMPLES = tonumber(args:match("samples=(%d+)")) or 12
local UNTIL = tonumber(args:match("until=(%d+)")) or 2000
local STRIDE = tonumber(args:match("stride=(%d+)")) or 1   -- compare every STRIDE-th call, to spread the samples over the run
local STATE = args:match("state=(%S+)")
local OUT = args:match("out=(%S+)") or "captures/oracle.txt"
local DEBUG = args:match("debug=1")
local BUILD = args:match("build=(%S+)") or "build"   -- native/BUILD: where `make blob BUILD=...` left game.blob and game_syms.lua

ami.machine{
    name = "Xenon2 oracle", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    snapshots = "../local/save",
    content = "../local/xenon2.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_chipmem_size = 2, puae_joyport_order = "2143" },   -- 1 MB chip RAM for the C blob, the joystick in port 1
}
ami.boot()

local function read_file(name) return ami.file_read(name, 0, ami.file_size(name)) end
local syms = load(read_file("native/" .. BUILD .. "/game_syms.lua"))()
local blob = read_file("native/" .. BUILD .. "/game.blob")
local origs = load(read_file("native/" .. BUILD .. "/game_origs.lua"))()
-- a stored pointer to a C function is the original's address in the original: blob address -> the original addresses it stands for
local blob_to_orig = {}
for name, list in pairs(origs) do
    if syms[name] then for _, a in ipairs(list) do blob_to_orig[syms[name]] = blob_to_orig[syms[name]] or {}; blob_to_orig[syms[name]][a] = true end end
end
local waits = load(read_file("native/" .. BUILD .. "/game_waits.lua"))()
local case_names = {}
local cases = {}
for _, entry in ipairs(ami.dir("host/cases") or {}) do   -- one file for each module: host/cases/NAME.lua returns a list of cases
    if entry.name:match("%.lua$") then
        for _, case in ipairs(load(read_file("host/cases/" .. entry.name))()) do cases[#cases + 1] = case end
    end
end

local SCRATCH = 0xE8000   -- a stack for the test, above the memory that is compared
local CHIP = 0x100000     -- all of the machine's 1 MB chip RAM
local DATA_REGS = { "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "a0", "a1", "a2", "a3", "a4", "a5", "a6" }

local function reg_value(spec, regs)
    local name, width = spec:match("^(%w+)%.?(%w*)$")
    local v = regs[name]
    if width == "w" then v = v & 0xffff elseif width == "b" then v = v & 0xff end
    return v
end

-- the addresses at which two memory images differ (at most `limit`), each as {address = true}
local function differences(a, b, limit)
    local out, n = {}, 0
    for block = 1, #a, 4096 do
        if a:sub(block, block + 4095) ~= b:sub(block, block + 4095) then
            for i = block, math.min(block + 4095, #a) do
                if a:byte(i) ~= b:byte(i) then out[i - 1] = true; n = n + 1 end
            end
            if limit and n >= limit then break end
        end
    end
    return out
end

local log = {}
local function say(...) log[#log + 1] = string.format(...) end

local busy = false
local in_call   -- the comparison in progress (for the watchdog)
local function check(case)
    if DEBUG then say("hook %s frame %d busy=%s refire=%s done=%s", case.name, ami.frame(), tostring(busy), tostring(case.refire), tostring(case.done)) end
    if busy then return end   -- the hook fires inside the calls this function makes
    if case.refire then case.refire = false; return end   -- and once more as the program goes on after a comparison
    if case.where and not case.where(ami) then return end
    case.calls = (case.calls or 0) + 1
    if case.done >= SAMPLES or (case.calls - 1) % STRIDE ~= 0 then return end
    busy = true
    local regs = {}
    for _, r in ipairs(DATA_REGS) do regs[r] = ami.reg(r) end
    regs.a7, regs.sr, regs.pc = ami.reg("a7"), ami.reg("sr"), ami.reg("pc")
    local before = ami.read(0, CHIP)
    in_call = { case = case, before = before, regs = regs, frame = ami.frame() }
    local function fresh()
        local run = {}
        for k, v in pairs(regs) do run[k] = v end
        run.a7, run.sr = SCRATCH, case.sr or 0x2700
        return run
    end
    local function settle() if case.blitter and syms.blitter_wait then ami.call(syms.blitter_wait, { a7 = SCRATCH, sr = 0x2700 }) end end   -- (the blitter code waits for itself when there is no blitter_wait in the C)
    -- the routines that wait for the vertical blank return at once (a call made here cannot get across a frame): in the original, and below in the C
    local function patch_original()
        for _, a in ipairs(waits.orig) do if a ~= case.orig then ami.write(a, "\x4e\x75") end end
    end
    patch_original()
    local r1 = ami.call(case.orig, fresh())
    settle()
    local after_orig = ami.read(0, CHIP)
    ami.write(0, before)
    -- the original once more: what differs between the two runs is done by the chips and is left out of the comparison
    patch_original()
    ami.call(case.orig, fresh())
    settle()
    local noise = differences(after_orig, ami.read(0, CHIP))
    ami.write(0, before)
    for i, spec in ipairs(case.args) do   -- stack arguments: none (the arguments are in registers)
        ami.write(SCRATCH + 4 * (i - 1), string.pack(">I4", reg_value(spec, regs)))
    end
    ami.write(syms.base, blob)   -- the program may have used the memory the C code sits in
    patch_original()             -- (the same bytes of the original are patched in both runs, so that the memory compares)
    for _, n in ipairs(waits.c) do if syms[n] and n ~= (case.c or case.name) then ami.write(syms[n], "\x4e\x75") end end
    local r2 = ami.call(syms[case.c or case.name], fresh())
    settle()
    local after_c = ami.read(0, CHIP)
    ami.write(0, before)
    busy = false
    in_call = nil
    case.refire = true
    case.done = case.done + 1
    local problems = {}
    if case.ret then
        local want = reg_value(case.ret, r1)
        local got = r2.d0   -- the C function returns in d0, the original in `ret`
        local width = case.ret:match("%.(%w)")
        if width == "w" then got = got & 0xffff elseif width == "b" then got = got & 0xff end
        if want ~= got then problems[#problems + 1] = string.format("result %s: orig %x C %x", case.ret, want, got) end
    end
    if after_orig ~= after_c then
        local positions, shown = {}, 0
        for pos in pairs(differences(after_orig, after_c, 64)) do positions[#positions + 1] = pos end
        table.sort(positions)
        local dskpt = (ami.custom(0x20) << 16) | ami.custom(0x22)
        local dsklen = (ami.custom(0x24) & 0x3fff) * 2
        -- bytes that are part of a pointer: the C stored its function where the original stored the original's address
        local ok_bytes = {}
        for _, pos in ipairs(positions) do
            for w = pos - 3, pos do
                if w >= 0 and w % 2 == 0 and w + 4 <= CHIP then
                    local c = string.unpack(">I4", after_c, w + 1)
                    local o = string.unpack(">I4", after_orig, w + 1)
                    if blob_to_orig[c] and blob_to_orig[c][o] then for k = w, w + 3 do ok_bytes[k] = true end end
                end
            end
        end
        for _, pos in ipairs(positions) do
            if ok_bytes[pos] or (pos >= dskpt and pos < dskpt + dsklen) then goto continue end   -- (the disk DMA fills its buffer at its own pace)
            if pos >= 8 and not (pos >= syms.base and pos < syms.base + #blob) and not (pos >= SCRATCH - 0x1000 and pos < SCRATCH + 0x100) and not noise[pos] and shown < 12 then
                shown = shown + 1
                problems[#problems + 1] = string.format("memory $%05x orig %02x C %02x", pos, after_orig:byte(pos + 1), after_c:byte(pos + 1))
            end
            ::continue::
        end
    end
    if #problems == 0 then
        case.ok = (case.ok or 0) + 1
    else
        case.bad = (case.bad or 0) + 1
        local inputs = {}
        for _, spec in ipairs(case.args) do inputs[#inputs + 1] = string.format("%s=%x", spec, reg_value(spec, regs)) end
        say("DIFF %s (call %d, frame %d) %s: %s", case.name, case.calls, ami.frame(), table.concat(inputs, " "), table.concat(problems, "; "))
        local show_a, show_n = args:match("show=(%x+):(%d+)")
        if show_a and not case.shown then
            case.shown = true
            local a, n = tonumber(show_a, 16), tonumber(show_n)
            local function hex(img) return (img:sub(a + 1, a + n):gsub(".", function(c) return string.format("%02x", c:byte()) end)) end
            say("  at $%x: before %s", a, hex(before)); say("  at $%x: orig   %s", a, hex(after_orig)); say("  at $%x: C      %s", a, hex(after_c))
        end
    end
end

local function guarded(case)
    return function()
        local ok, err = pcall(check, case)
        if not ok then
            busy = false
            case.refire = false
            say("ERROR %s: %s", case.name, tostring(err))
        end
    end
end
local active = {}
for _, case in ipairs(cases) do
    if (next(only) == nil or only[case.name]) and syms[case.c or case.name] then
        case.done = 0
        active[#active + 1] = case
        ami.hook(case.orig, guarded(case))
    elseif not syms[case.c or case.name] then
        say("missing in the blob's game_syms.lua: %s", case.c or case.name)
    end
end

-- the joystick of --script-args joy=F:DIRS,... (frames from the start of the run)
local joys = {}
for f, dirs in (args:match("joy=(%S+)") or ""):gmatch("(%d+):([%a+]+)") do
    local st = {}
    for d in dirs:gmatch("%a+") do if d ~= "none" then st[d] = true end end
    joys[tonumber(f)] = st
end
-- swap=1: disk 2 replaces disk 1 after the game's disk check (for the runs that load the level)
local do_swap = args:match("swap=1")
local swapped, st, sf, last_read, nreads = false, 0, 0, -1000, 0
ami.hook(0x921a, function() last_read = ami.frame(); nreads = nreads + 1 end)
ami.wait(1)
assert(STATE and ami.snapshot_load(STATE), "cannot load the state " .. tostring(STATE))
ami.write(syms.base, blob)
local start = ami.frame()
while ami.frame() - start < UNTIL do
    ami.wait(1)
    local f = ami.frame() - start
    if in_call and ami.frame() - in_call.frame > 60 then
        -- the call has not come back (a routine that waits for something the call cannot get: an interrupt, the beam): put the machine back as it was at the hook
        local c = in_call
        ami.write(0, c.before)
        for k, v in pairs(c.regs) do ami.set_reg(k, v) end
        busy = false
        in_call = nil
        c.case.hung = true
        c.case.done = SAMPLES
        c.case.refire = true
        say("HUNG %s: the call did not return in 60 frames (not compared; it waits for something)", c.case.name)
    end
    if joys[f] then ami.joystick(joys[f]) end
    if DEBUG and f % 20 == 0 then say("f%d pc=%06x busy=%s", f, ami.reg("pc"), tostring(busy)) end
    if do_swap then
        local id = ami.peek32(0x49602)
        if not swapped and nreads > 0 and ami.frame() >= last_read + 60 and id ~= 0x58324220 then swapped = true; st = 1 end
        if st == 1 then ami.disk_eject(true); st = 2; sf = ami.frame()
        elseif st == 2 and ami.frame() >= sf + 30 then ami.disk_select(1); ami.disk_eject(false); st = 3 end
    end
end
say("--- %d frames; samples per function up to %d", UNTIL, SAMPLES)
for _, case in ipairs(active) do
    say("%-22s calls %5d  compared %3d  ok %3d  differ %3d", case.name, case.calls or 0, case.done, case.ok or 0, case.bad or 0)
end
ami.save_file(OUT, table.concat(log, "\n") .. "\n")
ami.quit(0)
