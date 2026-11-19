# Xenon 2 Megablast (Image Works, 1989)

Port set-up using the `amiga-porting` skill (`~/git/retrofoundry/amiga_porting`).

## Disks

The game is two disks; the single-disk Black Monks release is not enough.

- Disk 1: `Copylock Amiga Key Disk` (PNA crack), boots, then asks for the game disk.
- Disk 2: game disk, boot block `X2B ` at offset 2 (not a DOS boot block).
- The game's own raw MFM loader ($921a, `rabs`: d0 = first sector, d1 = count, a0 = destination) reads DF0 only.
  At $8fde it requires `X2B ` in sector 0, otherwise it shows INSERT GAME DISK and retries.
- So disk 2 has to replace disk 1 in DF0: PageDown in the window (host disk control), or `ami.disk_eject/select`.
- `local/xenon2.adf` is the Black Monks single-disk release (flag `bm`); it never gets past INSERT GAME DISK.

## Files (git-ignored `local/`)

- `local/system/kick34005.A500`: Kickstart 1.3 (CRC32 c4f0f55f)
- `local/xenon2-disk1.adf`, `local/xenon2-disk2.adf`, `local/xenon2.m3u`
- `local/save/frame-8134.state` loading from disk, `frame-8693.state` level loaded (waits for fire),
  `frame-8880.state` level running. Older states `6998/7637/8973` are the single-disk release (`bm`),
  `6159/6538` are pre-menu and menu with both disks.

## Run

```
AMI=../amiga_porting/ami9000/build/ami9000_host
python3 ../amiga_porting/ami9000/pidrun.py -- $AMI host/xenon2.lua --script-args joy1          # window
python3 ../amiga_porting/ami9000/pidrun.py -- $AMI host/xenon2.lua --script-args "joy1 load=frame-8880"
```

The title and menu poll the joystick in port 1 (`joy1`); arrows = stick, Ctrl = fire. Flags are listed at the top
of `host/xenon2.lua`. `host/ramdump.lua` dumps chip RAM and registers from a state, `host/rabs_trace.lua` logs the
game's disk reads.

## The port

`native/` is the reconstruction (the C of the game proper: menu, level 1 and the music player, in `src/pkg_*.c`, each function with `/* orig $ADDR */`),
built in the reproducible shell of `native/flake.nix` (`nix develop`, or just `make`, which re-enters it):

    make -C native blob BUILD=build/x     the game for the 68000 as a blob at $C0000 (what the oracle runs against the original)
    make -C native sdl                    the SDL3 port: native/build/sdl/xenon2-sdl
    make -C native sdl-run                build and run it (disks from ../local)

    native/build/sdl/xenon2-sdl --disks local                 arrows = joystick, Ctrl / Space / Return = fire, Esc = quit
    ... --headless --frames N --joy "100:fire,110:none,..."   scripted, no window; --status F prints registers and game variables
    ... --export-ram PATH:VBL                                  the memory in the original's byte order (tools/compare_state.py)

The port runs the game's C from its start-up (`game_entry`, orig `$400`) on the main thread.  The Amiga is the `native/src/sdl/` machine: `guest.c` (the
memory is `struct Xenon`, loaded from disk 1 at `$70C00`), `files.c`/`disk.c` (the ADFs; the game's disk loader is replaced; disk 2 goes in after the game
has asked for it twice), `display.c` (the picture from the copper list and registers), `blitter.c` (the blit at the `BLTSIZE` write), `sound.c` (Paula
from the channel registers), `main.c` (the window, the 50 Hz loop, `HOST_IDLE`, the interrupts).

Checking against the original (one emulator at a time):

    uv run tools/check.py --build build/x [NAME ...]       per function, original vs C, two scenarios (menu to level; level played)

Routines that wait for the vertical blank or the disk cannot be called by the oracle (a watchdog reports them as HUNG); interrupt handlers neither.
Code that never ran in the recorded run has no C: the port logs `the original's code at $XXXXXX has no C` when it is reached.

    tools/verify_port.sh                                   headless acceptance: menu -> first level selected -> loaded -> running (exit 0 = verified)

## Lockstep (dev build)

`dev/` is the dev build of the port (`make -C dev` -> `dev/build/xenon2-sdl-dev`): the same game C and SDL machine, run as a coroutine, with Lua 5.4
(`third_party/lua`) embedded.  The dev build only hosts Lua: the global `port` (API at the top of `dev/src/dev.c`) steps the game, runs to or hooks a function by its
original address, reads the memory in the original's byte order and the picture, gives keys and the stick, and has raw sockets.  Interactivity is Lua's: `dev/lua/shell.lua`
serves an interactive Lua shell on a unix socket.

    dev/build/xenon2-sdl-dev --lua dev/lua/shell.lua --lua-args /tmp/x2.sock --disks local &
    rlwrap nc -U /tmp/x2.sock                  > port.run(30)   > port.peek16(0x41c)   > port.run_until(0x2034, 20)

`host/lockstep.lua` runs the original in the emulator with the dev build as its peer and drives the port through this same Lua shell (`--lua-args -`: the shell on the peer's
stdin/stdout; lines of Lua in, the prompt as the end of a reply, `BIN LEN` and bytes for the memory and the picture); at every entry of
`swap_screens` (once a frame) the named members of the memory and the picture are compared.  The emulator's own `--control PATH` is the Lua shell of that script.

    python3 ../amiga_porting/ami9000/pidrun.py -- $AMI host/lockstep.lua --headless --script-args "syncs=150 tolerant=1 continue=1"

From a savestate with the level running: `state=frame-8880` loads the emulator's `local/save/frame-8880.state`, the port plays itself to the level (fire on the schedule of
`tools/verify_port.sh`) and takes the original's memory at the first sync point; the comparison is byte by byte against what already differed at that point.

    python3 ../amiga_porting/ami9000/pidrun.py -- $AMI host/lockstep.lua --headless --script-args "state=frame-8880 syncs=200 tolerant=1 continue=1"

To play it (a window; arrows = stick, Ctrl / Space / Return = fire; the port's picture and the difference are shown beside the original, and the first difference pauses both):

    python3 ../amiga_porting/ami9000/pidrun.py -- $AMI host/lockstep.lua --script-args "play=1 state=frame-8880 tolerant=1"

`hold=1` stops at the first difference and stays there (with the emulator's `--control PATH`: `LS.port_call(LUA)` asks the port, `LS.snap.ram` is the original's memory at the point); `calls=1` also compares the number of calls of each function (a hint where a difference starts, not exact: the original's hooks fire at labels inside functions); `joy=SYNC:DIRS,...` gives both sides the same stick from sync point SYNC on.
Last full run: `state=frame-8880 syncs=1500 tolerant=1` with the stick and fire on a schedule: 1500 sync points, no differences in memory or picture.

Options are listed at the top of the script.  `tolerant=1` leaves out what depends on interrupt timing (music player, hardware mirror, random generator).
`dev/members.lua` is generated by `tools/gen_symbols.py`.  `host/trace_calls.lua` prints the original's call order.
