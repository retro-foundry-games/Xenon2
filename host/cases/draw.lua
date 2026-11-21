-- draw: $0000-$17FF.  draw_bob_or_first ($1368) patches the object's routine pointer to the C function's address: not comparable.
-- draw_number ($1636) was not called in the recorded run.
return {
    { name = "free_object",        orig = 0x000fee, args = { "a0" } },
    { name = "draw_bob_masked",    orig = 0x00128e, args = { "a0" } },
    { name = "draw_bob_masked_at", orig = 0x00129a, args = { "d0.w", "d1.w", "a0" } },
    { name = "draw_bob_or",        orig = 0x001370, args = { "a0" } },
    { name = "draw_bob_or_at",     orig = 0x00137c, args = { "d0.w", "d1.w", "a0" } },
    { name = "draw_text",          orig = 0x0015ce, args = { "a0", "a3" } },
    { name = "draw_number",        orig = 0x001636, args = { "d0", "a1" } },
    { name = "redraw_energy_bar",  orig = 0x0016be, args = {} },
    { name = "draw_status_digit",  orig = 0x00176e, args = {} },
    { name = "draw_background",    orig = 0x0017d6, args = {} },
    -- never reached by the scenarios (calls 0)
    { name = "draw_tile_grid",        orig = 0x0018b6, args = { "a1", "d0.w", "d1.w", "d2.w", "d3.w" }, blitter = true },
    { name = "draw_tile_grid_flash",  orig = 0x001bd0, args = { "a1", "d0.w", "d1.w", "d2.w", "d3.w" }, blitter = true },
    { name = "draw_bob_behind_map",   orig = 0x001438, args = { "a0" }, blitter = true },
}
