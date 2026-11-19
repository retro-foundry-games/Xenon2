-- An interactive Lua shell on a unix socket, like the `lua` command's:
--
--   xenon2-sdl-dev --lua dev/lua/shell.lua --lua-args PATH --disks local
--   rlwrap nc -U PATH
--
-- With --lua-args - the shell is on stdin/stdout instead: that is how a program that starts the dev build as a process with pipes (the lockstep in the emulator,
-- host/lockstep.lua) drives the port.  The prompts are the globals shell_prompt and shell_prompt2 (a program that reads the replies sets them to something it can wait for).
--
-- `> ` prompt, `>> ` while a statement is unfinished, an expression's values are printed, errors come with a traceback; print() and io.write() go to the client.
-- The port is the global `port` (dev/src/dev.c); it moves only while a line runs, so it stands still between lines.  Globals and functions stay for the next lines.
local path = args ~= "" and args or "/tmp/xenon2-dev.sock"

local function handler(message)
    if type(message) ~= "string" then message = tostring(message) end
    return debug.traceback(message, 2)
end

-- one line (or the lines so far of a statement) at the Lua prompt; returns true if the statement is unfinished
local function eval(say, chunk)
    local real_print, real_write = print, io.write
    print = function(...)
        local t = table.pack(...)
        for i = 1, t.n do t[i] = tostring(t[i]) end
        say(table.concat(t, "\t") .. "\n")
    end
    io.write = function(...)
        local t = table.pack(...)
        for i = 1, t.n do t[i] = tostring(t[i]) end
        say(table.concat(t))
        return io.stdout
    end
    local unfinished = false
    local f, err = load("return " .. chunk, "=stdin")           -- as an expression: its values are shown
    if not f then
        local expression_error = err
        f, err = load(chunk, "=stdin")
        if not f and (err:sub(-5) == "<eof>" or expression_error:sub(-5) == "<eof>") then unfinished = true end
    end
    if unfinished then
        say("")
    elseif not f then
        say(err .. "\n")
    else
        local result = table.pack(xpcall(f, handler))
        if not result[1] then
            say(result[2] .. "\n")
        elseif result.n > 1 then
            print(table.unpack(result, 2, result.n))
        end
    end
    print, io.write = real_print, real_write
    return unfinished
end

shell_prompt, shell_prompt2 = "> ", ">> "

-- the shell over a connection: line(), say(text) and the end of the connection
local function shell(read_line, say)
    local chunk = ""
    say(shell_prompt)
    while true do
        local line = read_line()
        if not line then break end
        chunk = chunk == "" and line or chunk .. "\n" .. line
        if eval(say, chunk) then
            say(shell_prompt2)
        else
            chunk = ""
            say(shell_prompt)
        end
    end
end

if path == "-" then
    shell(function() return io.stdin:read("l") end, function(text) io.stdout:write(text); io.stdout:flush() end)
    return
end
local server = port.listen(path)
io.stderr:write("dev: Lua shell on ", path, "\n")
while true do
    local client = port.accept(server)
    shell(function() return port.readline(client) end, function(text) port.write(client, text) end)
    port.close(client)
end
