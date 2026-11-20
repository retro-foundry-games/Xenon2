# Handoff (2026-11-20)

Xenon 2 Megablast (Amiga, Image Works 1989) ported to C following the skill in `~/git/retrofoundry/amiga_porting` (its `skill/SKILL.md`, and the patched emulator
`ami9000/`).  Read `README.md`, `DECOMPILING.md`, `PLAN.md` and the skill's sections 4, 4a, 5 first.  This file is where things stand and what to watch for.

## What exists

- `native/`: the port only (the C of the game, `src/pkg_*.c`, each function `/* orig $ADDR */`; the SDL3 machine in `src/sdl/`).  Covers the menu, level 1, the music
  player and (new) the shop (`pkg_i.c`, `pkg_i2.c`, functions `/* shop $ADDR */`).  Not ported: levels 2 and up (their code is on disk 2, never traced), the intro.
- `dev/`: the dev build of the port (`make -C dev` -> `dev/build/xenon2-sdl-dev`): the same C, run as a coroutine, with Lua 5.4 (`third_party/lua`) embedded.
  Global `port` (API at the top of `dev/src/dev.c`): step, run_until, hook by original address, ram/peek, picture, joy, key, save_state/load_state,
  run_function(ADDR) (start the game at a C function), sockets.  `dev/lua/shell.lua` is an interactive Lua shell (socket, or stdin/stdout with `--lua-args -`:
  how the lockstep drives the port); `dev/lua/course.lua` plots a course through level 1 (the port as its own simulator).
- `host/`: Lua for the emulator.  `lockstep.lua` runs the original and the dev build side by side (options at the top of the file).  `autoplay.lua` (a reactive
  autoplayer), `oracle.lua` + `cases/` (per-function original-vs-C check), `trace_steps.lua`, `trace_shop.lua` (exact instruction traces).
- `tools/`: `check.py` (the oracle driver; scenarios load, level, shop), `show.py` (annotated listings; `ANALYSIS_DIR=analysis-shop` for the shop), `analyze.py`,
  `gen_symbols.py`/`gen_records.py` (generated files, run by `make -C native`), `verify_port.sh` (headless acceptance).
- `analysis/` (level/base code from the cold run), `analysis-shop/` (the shop's code: 61 functions from 12 traces).

## Not in git: bring it over

`.gitignore` excludes `local/` and `captures/`; the emulator is a separate repo.
- `local/`: `system/kick34005.A500` (Kickstart 1.3, CRC32 c4f0f55f), `xenon2-disk1.adf`, `xenon2-disk2.adf` (the two-disk release; disk 1 is the Copylock key disk,
  disk 2 the game disk with boot block `X2B `), `xenon2.m3u`, and `local/save/*.state` (emulator savestates).  The ones the work uses: `frame-8880.state` (level 1
  running), `shop-before-512k.state` (just inside `shop_run`; `shop-before.state` is the same converted to 1 MB chip RAM for the oracle), `shop-entry.state`.
  Savestates can be remade: `host/lockstep.lua ... snap=ADDR:NAME`, `host/xenon2.lua` (F11 in the window).
- `captures/`: `level1.ram` (made by `lockstep.lua ... dumpram=captures/level1.ram`; `course.lua` needs it), `shop-before.ram`, `trace-shop-*.bin` (memory dumps the
  shop analysis decodes from), `input/*.txt` (recorded plays and the course; `input/level1-course.txt` is the plotted course), `trace-*.bin` (base-game dumps).
- Emulator: `~/git/retrofoundry/amiga_porting/ami9000/build/ami9000_host` (`ami9000/host/build_core.sh`; see `ami9000/host/README.md`).  The scripts expect it at
  `../amiga_porting/ami9000/build/ami9000_host` relative to this repo.  `pidrun.py` runs it (a pidfd runner).
- Toolchain: `nix develop native` (the flake gives gcc -m32, m68k-amigaos-gcc, SDL3); `uv` for Python (capstone is in the project); `nc` for the Lua shell.

## Commands

    make -C native sdl && native/build/sdl/xenon2-sdl --disks local        play the port (arrows = stick, Ctrl/Space/Return = fire)
    make -C dev                                                            the dev build
    make -C native blob BUILD=build/x && uv run tools/check.py --build build/x [--scenario shop] [NAMES]     the oracle
    tools/verify_port.sh                                                   headless acceptance (title -> menu -> level 1 loaded -> running)

    AMI=../amiga_porting/ami9000/build/ami9000_host
    # lockstep, level 1, unattended (a replay of recorded input, memory + picture, stops at the first difference)
    python3 ../amiga_porting/ami9000/pidrun.py -- $AMI host/lockstep.lua --headless --script-args "state=frame-8880 tolerant=1 replay=captures/input/FILE.txt hold=1"
    # lockstep, played in a window (original, port, differences; records your input to record=FILE)
    ... host/lockstep.lua --script-args "play=1 state=frame-8880 tolerant=1 record=captures/input/play-X.txt"      (+ --control SOCK to inspect a stop)
    # the shop, from the state before it
    ... host/lockstep.lua --script-args "play=1 state=shop-before-512k startfn=8ff2 tolerant=1"
    # the autoplayer / the plotted course
    ... host/lockstep.lua --script-args "auto=1 hold=1 state=frame-8880 tolerant=1"
    dev/build/xenon2-sdl-dev --lua dev/lua/course.lua --lua-args "syncs=3400 horizon=12 ew=15 sw=1 aw=6 pw=40 out=captures/input/level1-course.txt" --disks local
    ... host/lockstep.lua --headless --script-args "replay=captures/input/level1-course.txt hold=1 state=frame-8880 tolerant=1 syncs=3400"

A stop with `hold=1` and `--control SOCK`: `ami9000/host/ctl.py SOCK 'LS.port_call("print(port.peek16(0xcd0))")'` asks the port, `LS.snap.ram` is the original's
memory at the stop, `LS.hex(...)`.  `gdb -p` on the `xenon2-sdl-dev` process shows the port's raw state.

## State of the checks

- Per-function oracle: no function differs (level 1, base game, music; the shop: 52 functions, `shop_main` as a whole matches under a random player).
  Not reachable by the oracle: interrupt handlers, wait loops (checked inside callers / by the lockstep).
- Lockstep, level 1 from `frame-8880` (tolerant): clean for 2761 sync points of the recorded play `captures/input/play-20261119-170732.txt`, then a disk load (below).
  The unattended autoplayer ran 6000 sync points clean, but it mostly idled (see "the play checked").
- Lockstep, shop from `shop-before-512k` with `startfn=8ff2`: memory identical for 1500 sync points (the whole visit, no input); 10 sync points of picture differences
  during the fade-in (a few rows, one palette entry), not traced.  Nobody has played the shop through the lockstep yet with real input (buy, sell, leave): do that first.

## Open

1. **Disk loads in the lockstep.**  The original takes its real time (the shop program: 810 frames), the port's HLE loader takes the measured time since 2026-11-19
   (`sdl/disk.c`, counts the progress digit down) but frame-exact state (stars, loader internals, `map_image`'s first long, screens) still diverges after one.
   `resync=N` (default 120 with `startfn=`) copies the original's memory into the port after a gap and restarts the baseline; use `resync=120` for level runs too.
   Check `map_image` ($54E00, 4 bytes): after a reload the original has `31FC00C7`, the port 0.
2. **The shop**: unreached corners written from the listing: the player-2 picture (`shop_main` $54EA4), `shop_item_name`'s power text for kinds 1,8,9,A,D,E,12,13,
   `shop_slide_in`'s $4EE<0 branch; `shop_leave` is called with a stale d2 (0 passed); `refill_slot0(0,0)`.  Not oracle-compared alone: `shop_irq_tick`,
   `shop_wait`, `shop_end_of_broadcast`.  `shop_delay` is 26 frames in the port.  Not checked: that the shop's picture looks right in the SDL port window.
   The lockstep's `shop-entry.state` has a different layout than the 512 KB one (not converted).
3. **The play checked.**  The course planner reaches scroll `0x0BB8` of `0x11E7` (about 35% of level 1) at about sync 3300 and parks: the level only scrolls on while
   the ship is held high on the screen, and in the unit zone (`$D10..$A40`) a cautious plan hides at the bottom; a bold one dies (game over needs a fresh fire press at
   "READY PLAYER ONE", the autoplayer pulses fire when the scroll stands still).  The boss, later waves, the level end and the load of the next level are untested.
4. **Tolerant mode** leaves out what the original's interrupts make timing-dependent: music, sfx requests, `hw_mirror`, random generator, joystick bytes, key state
   (`tolerant=1`; plus `key_table`/`key_code` in play and replay).  A picture is accepted if it equals the original's frame next to the usual one (counted at the end;
   the usual offset is mis-calibrated, `window=X,Y,SX,LAG` defaults to 40,18,2,1 for level states).
5. **Savestate rework (asked, not done).**  `port.save_state` saves the raw custom-register and CIA blocks; you wanted a high-level state: only what the port owns
   (the game memory, mixer channels, disk in the drive, input, frame count, where the game is), the rest derived.  Also the port cannot start from an emulator `.state`
   (only memory, via `ram_in`); `startfn=` and the first-sync import are the two ways.
6. **Not pushed**: both repos have local commits only (Xenon2 has no remote yet; `amiga_porting` has its `origin`).  The skill has the lessons of this work in 4a.
7. **Starfield stops after a level reload** (seen in the lockstep play, 2026-11-20): when the port reloads level 1 (after the shop / `level_load`), the stars no longer animate.  Not investigated; start from `stars_init_dots`/`stars_update` use at level start and the state they leave (`stars`, `v_0cd8`).
8. **Shop input latency** (fixed in the lockstep 2026-11-20): `inlag=auto` gives the port the stick one sync later at sync points called from shop code; the shop replay `play-shop2.txt` from `frame-45589` is then clean to sync 758 with the random generator compared.  The port itself still acts a frame earlier than the original in the shop's stick check (HLE swap always waits a vertical blank): real play on the port is unaffected, only the comparison needed it.  Syncs 759-775 (first after the shop, before the level reload) differ in the `$54E00` level-header members: the shop's code is still there; not a bug.  `v_047c` (102 bytes) differs at later syncs: not looked at.

## Things that bit

- `pkill -f`/`pgrep -f` kill your own shell (the pattern matches the command); use `pgrep -x NAME` and kill by pid.  A windowed run started from a tool call dies with it:
  `setsid nohup ... < /dev/null & disown`.
- `port.joy{...}` takes booleans; `0` is true in Lua.  The lockstep's `joy(u,d,l,r,f)` helper converts.
- Opposite stick directions in one update read as neither in the emulator: the lockstep makes them exclusive, the autoplayer passes through neutral.
- Two addresses of one routine are different values to code that compares (`obj_nop` $3130 vs its jump-table entry $E48); level code stores the thunks, base code the routines.
- Records read through two views (`Object` vs `ObjMove`) must have the declared type the code uses; imports of a mid-game state find these (`V5E`, `ptr4a`, `void *` fn_hit).
- Hooks in the emulator: one per address, and they fire at labels inside routines, so call counts from hooks are hints.  Scripts in `$CLAUDE_JOB_DIR/tmp` cannot use
  `../local` paths: run Lua from the repo's `host/`.
- `ami.peer` can only start a process with pipes (hence the Lua shell on stdin/stdout); `ami.file_read` paths are relative to the repo root.

## Rules from the user (standing)

One emulator at a time; no screenshots or recordings; Python through `uv` with capstone; `native/` holds only the port (dev tooling, Lua, lockstep live outside it,
Lua in `third_party/`); comments state the constraint only (no backstory: that goes in the commit); the skill is updated with each lesson; commit trailers
`Co-Authored-By: Claude ...` / `Claude-Session: ...` as the tooling gives them.  Push only when asked.
9. **Level 2** (2026-11-20): `native/src/pkg_l2.c` has the entries (`$54E0C..$54E2C`: block collision, events, hooks, scroll, init, swap), the three gates and the first fight (`$56A4A`, `$56944`, boss `$56BE8`/`$5685A`/`$56ED0`), the turret and launcher markers, the walls of debris (`$56016`, `$5605A`, pieces `$566CE`), the block hit (`$56FA8`).  Level code is chosen by `v_0e04` (`level_scroll`, `level_hook`, ... in pkg_l2.c; `level1_*` for level 1).  `level2 $ADDR` tags give `code_table2` (tools/gen_records.py); while level 2's blob is loaded (`guest_blob_raw(2)`, set by `level_load` and `host_import_ram_buf`) level 1's members beyond `$54E40` are not converted and the blob is raw (`sh_r16`).  Code with no C resolves to a function that does nothing and is reported once on stderr (`the level's code at $... has no C`; the ones at `$0556DC`, `$056278`, `$060908` are stale level 1 pointers in free objects).  Lockstep from `level2-start` (made by `host/state_after.lua` from `frame-4464`, 150 frames after the fire press): 1000 syncs idle clean; the autoplayer (`auto=1`) 927 syncs clean until the game is over (hi-score entry, a timing difference of the name entry).  Not written: the markers' updates at `$5533E`, `$5550E`, `$5568C` (hook cases 2, 4, 5), the `$E0` enemy `$56144` and its shots (`$5623E`), the boss moves from `$56DD8` (they test bits of d0 the shot code leaves), `$56FEC` (blocks come back), `$56E56`.  `host/trace_level.lua` + `analysis-l2/` list the code a random player reaches; the random player stops at scroll `$09E0` (the three gates must be shot).  `host/state_pc.lua` prints where the original stands in a state.  Level 2 `v_0e04 == 2` only: levels 3 to 5 do nothing in the dispatch.  The oracle for level 2 is not set up (level scenario compares level 1: no differences).
