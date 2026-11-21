-- title: the title / credits screen.  Not checked here (they wait for the vertical blank in $2026, which the oracle cannot give):
-- title_fade_in, title_fade_out, show_text_sequence, title_message, title_credits.
-- Differences in the track buffer ($45000-$49FFF) while a disk loads are the disk DMA, not the C.
return {
    { name = "clear_screen",       orig = 0x007eb6, args = {} },
    { name = "find_glyph",         orig = 0x007ef8, args = { "d1.b", "a2" }, ret = "d1" },
    { name = "draw_text_row",      orig = 0x007f12, args = { "a1" } },
    { name = "draw_text_row_at",   orig = 0x007f24, args = { "a0", "a1", "d7.w" } },
    { name = "draw_text_squeezed", orig = 0x007f64, args = { "a1" } },
    { name = "stars_update",       orig = 0x00854c, args = {} },
    { name = "stars_erase",        orig = 0x00861c, args = {} },
    { name = "title_restore",      orig = 0x00847e, args = {} },
    { name = "stars_init_dots",    orig = 0x007d9e, args = {} },
    { name = "title_stars_start",  orig = 0x008442, args = {} },
    -- never reached by the scenarios (calls 0): the stretched logo
    { name = "draw_logo_scaled",      orig = 0x008092, args = { "a0", "d0.w", "d1.w", "d2.w", "d3.w" } },
}
