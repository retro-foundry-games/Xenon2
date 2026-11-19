-- the first calls of the game, in order (compare with the original's: host/trace_calls.lua)
port.trace(tonumber(os.getenv("CALLS") or "80"))
port.hook(0x2034, function() print("swap_screens entered, frame", port.frame()); port.quit() end)
port.run(10)
