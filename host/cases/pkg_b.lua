-- pkg_b.  Not here: box_overlap ($200A, result in the Z flag), swap_screens* ($2026, $202E: wait for the beam), irq_vblank,
-- irq_vblank_hooked and game_entry (never return / end with rte).
return {
    { name = "scroll_pos_update", orig = 0x001f22, args = { "d0.w" } },
    { name = "stars_scroll",      orig = 0x0022da, args = { "d0.w" } },
    { name = "stars_draw",        orig = 0x00233a, args = {} },
    { name = "obj_free_pop",      orig = 0x002418, args = {}, ret = "a0" },
}
