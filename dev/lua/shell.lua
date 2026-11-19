-- An interactive Lua shell on a unix socket, like the `lua` command's:
--
--   xenon2-sdl-dev --lua dev/lua/shell.lua --lua-args PATH --disks local
--   rlwrap socat - UNIX-CONNECT:PATH          (or: nc -U PATH)
--
-- `> ` prompt, `>> ` while a statement is unfinished, an expression's values are printed, errors come with a traceback; print() and io.write() go to the client.
-- The port is the global `port` (dev/src/dev.c); it moves only while a line runs, so it stands still between lines.  Globals and functions stay for the next lines.
local path = args ~= "" and args or "/tmp/xenon2-dev.sock"

local function handler(message)
    if type(message) ~= "string" then message = tostring(message) end
    return debug.traceback(message, 2)
end

-- one line (or the lines so far of a statement) at the Lua prompt; returns true if the statement is unfinished
local function eval(client, chunk)
    local say = function(text) port.write(client, text) end
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

local server = port.listen(path)
io.stderr:write("dev: Lua shell on ", path, "\n")
while true do
    local client = port.accept(server)
    local chunk = ""
    port.write(client, "> ")
    while true do
        local line = port.readline(client)
        if not line then break end
        chunk = chunk == "" and line or chunk .. "\n" .. line
        if eval(client, chunk) then
            port.write(client, ">> ")
        else
            chunk = ""
            port.write(client, "> ")
        end
    end
    port.close(client)
end
