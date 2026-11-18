-- The functions of native/src that host/oracle.lua checks against the original: where the original has it, the registers its
-- arguments come in (".w": only the low word counts, ".l" or a bare name: the whole register), the register its result is in,
-- `blitter = true` when the function draws (the blitter goes on after it returns).  A function is in this list from the
-- moment it is decompiled:   { name = "NAME", orig = 0xADDR, args = { "d0.w", "a0" }, ret = "d0.w" },
return {
    { name = "random_next",  orig = 0x001f50, args = {},         ret = "d0" },
    { name = "random_below", orig = 0x001ff6, args = { "d0.w" }, ret = "d0.w" },
}
