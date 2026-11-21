local n = 0
port.hook(0x2034, function() n = n + 1; print(("swap_screens entry %d (frame %d): vbl_count %d last_flip_vbl %d min_vbl_per_frame %d"):format(n, port.frame(), port.peek16(0x41c), port.peek16(0x404), port.peek16(0x410))) end)
port.run(40)
