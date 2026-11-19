-- pkg_i: the shop (scenario "shop": host/oracle.lua keeps these out of the level scenarios).
-- `input = true`: the oracle gives the shop a random player's stick and fire at every frame flip (host/oracle.lua), so that the loops that wait for the fire button end:
-- shop_stick, shop_sell, shop_buy and the whole shop (shop_main).  The interrupt handler shop_irq_tick cannot be called (it ends with rte): shop_sound_tick, which it calls, is a case.
-- A function that loops back to its own entry ($55264, $56912, $553F2) is hooked at every pass.
local S = "shop"
local function in_shop(ami) return ami.peek32(0x54e00) == 0x31fc00c7 end   -- (the level's code that replaces the shop at the same addresses is not the shop's)
local cases = {
    { name = "shop_draw_both",       orig = 0x0553f8, args = { "d0.w", "d1.w", "a0" }, scenario = S },
    { name = "shop_clear_columns",   orig = 0x0553f2, args = { "a1", "d0.w" }, ret = "a1", scenario = S },
    { name = "shop_copy_block",      orig = 0x055200, args = { "a0", "a1", "d0.w", "d1.w" }, scenario = S },
    { name = "shop_put_rows",        orig = 0x056912, args = { "a0", "a1", "d0.w" }, scenario = S },
    { name = "shop_lights_draw",     orig = 0x056e8e, args = {}, scenario = S },
    { name = "shop_text_newline",    orig = 0x056e44, args = {}, scenario = S },
    { name = "shop_text_char",       orig = 0x056d8c, args = { "d0.b" }, scenario = S },
    { name = "shop_records_tick",    orig = 0x056988, args = {}, scenario = S },
    { name = "shop_sparkle",         orig = 0x056c02, args = { "a1" }, scenario = S },
    { name = "shop_records_draw",    orig = 0x0569bc, args = {}, scenario = S },
    { name = "shop_keeper_blink",    orig = 0x056848, args = {}, scenario = S },
    { name = "shop_door_draw",       orig = 0x0568e6, args = {}, scenario = S },
    { name = "shop_room_draw",       orig = 0x05662a, args = {}, scenario = S },
    { name = "shop_room_update",     orig = 0x05661c, args = {}, scenario = S },
    { name = "shop_keeper_draw",     orig = 0x05648c, args = {}, scenario = S },
    { name = "shop_hold",            orig = 0x0564b2, args = { "d0.w" }, frames = 300, samples = 4, scenario = S },
    { name = "shop_wait",            orig = 0x0551e6, args = { "d0.w" }, frames = 600, samples = 3, scenario = S },
    { name = "shop_board_clear",     orig = 0x055360, args = {}, scenario = S },
    { name = "shop_text_home",       orig = 0x056ca0, args = {}, scenario = S },
    { name = "shop_cash_draw",       orig = 0x056cb2, args = {}, scenario = S },
    { name = "shop_print",           orig = 0x056cc0, args = { "a0" }, frames = 2000, samples = 4, scenario = S },
    { name = "shop_print_number",    orig = 0x056d56, args = { "d0.w" }, frames = 600, samples = 4, scenario = S },
    { name = "shop_cursor_draw",     orig = 0x056040, args = {}, scenario = S },
    { name = "shop_cursor_erase",    orig = 0x0560a6, args = {}, scenario = S },
    { name = "shop_can_buy",         orig = 0x05593e, args = {}, ret = "d0", scenario = S },
    { name = "shop_item_ca2",        orig = 0x0558e4, args = {}, scenario = S },
    { name = "shop_item_advice",     orig = 0x0558ec, args = {}, frames = 2000, samples = 2, scenario = S },
    { name = "shop_item_c90",        orig = 0x055930, args = {}, scenario = S },
    { name = "shop_item_cca",        orig = 0x055938, args = {}, scenario = S },
    { name = "shop_item_name",       orig = 0x055de8, args = { "d0.w", "d1.w" }, frames = 2000, samples = 3, scenario = S },
    { name = "shop_price_text",      orig = 0x055c60, args = { "a0" }, frames = 2000, samples = 3, scenario = S },
    { name = "shop_stock_update",    orig = 0x055424, args = {}, scenario = S },
    { name = "shop_stock_sold_out",  orig = 0x055512, args = {}, scenario = S },
    { name = "shop_stock_recheck",   orig = 0x055534, args = {}, scenario = S },
    { name = "shop_sell_list",       orig = 0x055afc, args = {}, scenario = S },
    { name = "shop_sell_show",       orig = 0x055c9a, args = { "a0" }, frames = 3000, samples = 2, scenario = S },
    { name = "shop_pillars_a",       orig = 0x05530c, args = {}, scenario = S },
    { name = "shop_pillars_b",       orig = 0x055324, args = {}, scenario = S },
    { name = "shop_delay",           orig = 0x05504e, args = {}, frames = 300, samples = 2, scenario = S },
    { name = "shop_slide_in",        orig = 0x055264, args = {}, frames = 900, samples = 2, scenario = S },
    { name = "shop_slide_out",       orig = 0x0552ba, args = {}, frames = 900, samples = 2, scenario = S },
    { name = "shop_keeper_in",       orig = 0x0564ce, args = {}, frames = 1200, samples = 2, scenario = S },
    { name = "shop_open",            orig = 0x056568, args = {}, frames = 900, samples = 2, scenario = S },
    { name = "shop_close",           orig = 0x0565c2, args = {}, frames = 900, samples = 2, scenario = S },
    { name = "shop_blank_screen",    orig = 0x055198, args = {}, frames = 100, samples = 2, scenario = S },
    { name = "shop_leave",           orig = 0x055016, args = {}, scenario = S },
    { name = "shop_end_of_broadcast", orig = 0x05505a, args = {}, frames = 6000, samples = 1, scenario = S },
    { name = "shop_sound_tick",      orig = 0x064bbc, args = {}, scenario = S },
    { name = "shop_sound_cmd",       orig = 0x0649c6, args = { "d0.w" }, scenario = S },
    { name = "shop_sound_stop",      orig = 0x0647ce, args = {}, scenario = S },
    { name = "shop_sound_reset",     orig = 0x064828, args = {}, scenario = S },
    { name = "shop_sound_tables",    orig = 0x06492a, args = {}, mutate = function(r) r.a3 = 0x6476e end, scenario = S },   -- (a3 = the player's base: the caller sets it)
    { name = "shop_sound_tables2",   orig = 0x064972, args = {}, mutate = function(r) r.a3 = 0x6476e end, scenario = S },
    { name = "shop_stick",           orig = 0x055f60, args = {}, input = true, frames = 3000, samples = 3, scenario = S },
    { name = "shop_sell",            orig = 0x055b48, args = {}, input = true, frames = 20000, samples = 1, scenario = S },
    { name = "shop_buy",             orig = 0x055594, args = {}, input = true, frames = 20000, samples = 1, scenario = S },
    { name = "shop_main",            orig = 0x054e00, args = {}, input = true, frames = 60000, samples = 1, scenario = S },
    { name = "shop_sound_delay",     orig = 0x064bb2, args = {}, scenario = S },
}

-- Fuzz cases: a function the program reaches rarely (or never in a run) is hooked at an address inside a function the program passes every frame (`hook`), the state it reads is set to
-- random values for the call (`setup`: put back afterwards; `mutate`: the registers), and the original and the C are compared as usual.
local function setup_with(fn)
    return function(ami, n)
        local saved = {}
        local P = {}
        function P.w8(a, v) saved[#saved + 1] = { a, 1, ami.peek8(a) }; ami.poke8(a, v) end
        function P.w16(a, v) saved[#saved + 1] = { a, 2, ami.peek16(a) }; ami.poke16(a, v) end
        function P.w32(a, v) saved[#saved + 1] = { a, 4, ami.peek32(a) }; ami.poke32(a, v) end
        local seed = (n * 2654435761 + 977) % 2147483648
        local function rnd(m) seed = (seed * 1103515245 + 12345) % 2147483648; return (seed // 65536) % m end
        fn(P, rnd, ami)
        return function()
            for i = #saved, 1, -1 do
                local a, size, v = table.unpack(saved[i])
                if size == 1 then ami.poke8(a, v) elseif size == 2 then ami.poke16(a, v) else ami.poke32(a, v) end
            end
        end
    end
end
local function mutate_with(fn)
    return function(regs, n)
        local seed = (n * 40503 + 31) % 2147483648
        local function rnd(m) seed = (seed * 1103515245 + 12345) % 2147483648; return (seed // 65536) % m end
        fn(regs, rnd)
    end
end
local RECS = 0x56ac2
local POOL = 0x3f07e
-- the ship's weapons and the counters can_buy asks about
local function ship(P, rnd, kind)
    P.w16(0xc9e, rnd(4)); P.w16(0xcc6, rnd(2) == 0 and 0x27 or rnd(0x28)); P.w16(0xc64, rnd(5)); P.w16(0xca2, rnd(2) * 0xaa); P.w16(0xca0, rnd(2) * 5)
    P.w16(0xcca, rnd(11)); P.w8(0xd83, rnd(2) * 255); P.w16(0xc90, rnd(2) * 0xdc)
    for i = 0, 6 do
        local slot = 0xc66 + 6 * i
        local obj = POOL + 98 * rnd(8)
        P.w16(slot, ({ 0, 0, 0x94, 0x44, 0x38, 0x98 })[rnd(6) + 1]); P.w32(slot + 2, obj)
        P.w16(obj + 0x44, rnd(2) == 0 and kind or 1 + rnd(25))
        local score = rnd(4)
        P.w16(obj + 0x42, score); P.w16(obj + 0x32, rnd(2) == 0 and score or rnd(4))
    end
end
local fuzz = {
    { name = "shop_can_buy_fuzz", c = "shop_can_buy", orig = 0x05593e, hook = 0x05663e, args = {}, ret = "d0", samples = 60,
      setup = setup_with(function(P, rnd)
          local kind = 1 + rnd(25)
          local rec = RECS + 0x10 * rnd(20)
          P.w32(0x55dc4, rec); P.w16(rec + 6, kind)
          ship(P, rnd, kind)
      end) },
    { name = "shop_stock_update_fuzz", c = "shop_stock_update", orig = 0x055424, hook = 0x056644, args = {}, samples = 40,
      setup = setup_with(function(P, rnd)
          P.w16(0x55422, rnd(2) * 0x13); P.w32(0xc9a, rnd(5) == 0 and rnd(100) or rnd(5000)); P.w16(0x9fa, ({ 0x200, 0x7d0, 0xfa0, 0x1f4 })[rnd(4) + 1]); P.w16(0xe04, ({ 1, 3, 5 })[rnd(3) + 1])
      end) },
    { name = "shop_stock_recheck_fuzz", c = "shop_stock_recheck", orig = 0x055534, hook = 0x056648, args = {}, samples = 40,
      setup = setup_with(function(P, rnd)
          P.w32(0xc9a, rnd(5000)); P.w16(0xe04, ({ 1, 3, 5 })[rnd(3) + 1])
          for i = 0, 19 do P.w16(RECS + 0x10 * i + 6, ({ 0, 1, 2, 5, 9, 14, 0x13, 0x19, 0x1a, 0x1b })[rnd(10) + 1]) end
      end) },
    { name = "shop_stock_sold_out_fuzz", c = "shop_stock_sold_out", orig = 0x055512, hook = 0x05664c, args = {}, samples = 3 },
    { name = "shop_sell_list_fuzz", c = "shop_sell_list", orig = 0x055afc, hook = 0x05664e, args = {}, samples = 20,
      setup = setup_with(function(P, rnd)
          -- the ship's object list ($AEA: the last object, each with the one before at $12): 0-23 objects of the pool, kinds at $44, the list ends with a type 0 object
          local n = rnd(24)
          local first = POOL + 98 * 30
          P.w32(0xaea, first)
          for i = 0, n do
              local o = first + 98 * i
              P.w16(o, i == n and 0 or (rnd(5) == 0 and 0x94 or 0x44))
              P.w16(o + 0x44, 1 + rnd(25)); P.w32(o + 0x12, first + 98 * (i + 1))
          end
      end) },
    { name = "shop_item_ca2_fuzz", c = "shop_item_ca2", orig = 0x0558e4, hook = 0x056652, args = {}, samples = 2 },
    { name = "shop_item_c90_fuzz", c = "shop_item_c90", orig = 0x055930, hook = 0x056658, args = {}, samples = 2 },
    { name = "shop_item_cca_fuzz", c = "shop_item_cca", orig = 0x055938, hook = 0x056670, args = {}, samples = 2 },
    { name = "shop_item_advice_fuzz", c = "shop_item_advice", orig = 0x0558ec, hook = 0x056674, args = {}, samples = 4, frames = 3000,
      setup = setup_with(function(P, rnd) P.w16(0xe04, 1 + rnd(5)); P.w16(0xcf2, ({ 4, 8, 0xc, 0x10, 0x14, 0x18 })[rnd(6) + 1]); P.w16(0x4f6, 0x37); P.w16(0x4f8, 0x76) end) },
    { name = "shop_text_char_fuzz", c = "shop_text_char", orig = 0x056d8c, hook = 0x056682, args = { "d0.b" }, samples = 16,
      setup = setup_with(function(P, rnd) P.w16(0x4f6, 0x37 + rnd(0x20)); P.w16(0x4f8, 0x76 + 7 * rnd(4)) end),
      mutate = mutate_with(function(r, rnd) r.d0 = rnd(6) == 0 and 0x0d or 0x20 + rnd(0x60) end) },
    { name = "shop_blank_screen_fuzz", c = "shop_blank_screen", orig = 0x055198, hook = 0x056e98, args = {}, samples = 2, frames = 200 },
    { name = "shop_slide_out_fuzz", c = "shop_slide_out", orig = 0x0552ba, hook = 0x056e9c, args = {}, samples = 1, frames = 900,
      setup = setup_with(function(P, rnd) P.w16(0x4ec, 0); P.w16(0x4ee, 0) end) },
    { name = "shop_sound_tick_fuzz", c = "shop_sound_tick", orig = 0x064bbc, hook = 0x064bc0, args = {}, samples = 60,
      setup = setup_with(function(P, rnd)
          for ch = 0, 3 do
              local base = 0x64d4c + 0x24 * ch
              for off = 0, 0x23 do P.w8(base + off, rnd(256)) end
              P.w8(base + 0x1a, rnd(4) == 0 and 0 or 255); P.w8(base + 0x12, rnd(3)); P.w8(base + 0x15, rnd(2) == 0 and 0 or 0x80 + rnd(4))
              P.w8(base + 0x1b, 1 + rnd(2)); P.w32(base + 0x1c, 0x65040 + rnd(64)); P.w16(base + 8, 0x40 + rnd(0x400)); P.w16(base + 0, rnd(8)); P.w16(base + 0xa, rnd(0x200)); P.w16(base + 0xc, rnd(0x200))
              P.w8(0x64dde + 12 * ch + 0xa, rnd(2)); P.w32(0x64dde + 12 * ch, 0x65120); P.w16(0x64dde + 12 * ch + 4, rnd(0x40)); P.w16(0x64dde + 12 * ch + 6, 0x100 + rnd(0x100)); P.w16(0x64dde + 12 * ch + 8, rnd(0x41))
          end
      end) },
    { name = "shop_sound_cmd_fuzz", c = "shop_sound_cmd", orig = 0x0649c6, hook = 0x064bc4, args = { "d0.w" }, samples = 60,
      mutate = mutate_with(function(r, rnd) r.d0 = (rnd(4) << 8) | (rnd(2) == 0 and rnd(0x1c) or (0x80 + rnd(4))) end) },
    { name = "shop_sound_tables_fuzz", c = "shop_sound_tables", orig = 0x06492a, hook = 0x064bd4, args = {}, samples = 4, mutate = function(r) r.a3 = 0x6476e end,
      setup = setup_with(function(P, rnd) P.w8(0x64856, 0) end) },
    { name = "shop_sound_tables2_fuzz", c = "shop_sound_tables2", orig = 0x064972, hook = 0x064bd8, args = {}, samples = 4, mutate = function(r) r.a3 = 0x6476e end,
      setup = setup_with(function(P, rnd) P.w8(0x649c4, 0) end) },
}
for _, c in ipairs(fuzz) do c.scenario = S; cases[#cases + 1] = c end
for _, c in ipairs(cases) do c.where = c.where or in_shop end
return cases
