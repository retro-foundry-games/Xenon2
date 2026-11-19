# Lua 5.4.9 (third party)

The source of Lua 5.4.9 from https://www.lua.org/ftp/lua-5.4.9.tar.gz (the files of `src/` except the stand-alone interpreter `lua.c` and the compiler
`luac.c`), unchanged.  MIT license: see `LICENSE`.  It is compiled into the dev build of the port (`make -C dev`), which embeds it as the
script language for driving the port (`dev/`).
