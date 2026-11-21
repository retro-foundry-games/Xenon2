-- sound: the music and sound-effect player ($19F9E-$1B6CE)
return {
    { name = "music_start",           orig = 0x019f9e, args = { "d0.w" } },
    { name = "music_tick",            orig = 0x019fac, args = {} },
    { name = "music_silence",         orig = 0x019fba, args = {} },
    { name = "music_resume",          orig = 0x019fc8, args = {} },
    { name = "sfx_play",              orig = 0x019fd6, args = { "d0.w" } },
    { name = "sfx_stop_all",          orig = 0x019fe4, args = {} },
    { name = "music_fade_out",        orig = 0x019ff2, args = {} },
    { name = "sound_init_instruments", orig = 0x01b1b6, args = {} },
    { name = "sound_init_sfx_samples", orig = 0x01b216, args = {} },
    { name = "audio_delay",           orig = 0x01b536, args = {} },
    { name = "sfx_tick",              orig = 0x01b540, args = {} },
}
