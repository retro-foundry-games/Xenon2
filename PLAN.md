# Xenon 2 port: plan and status

Goal: an SDL3 port, built by the rules of the `amiga-porting` skill, that implements the menu, the selection of the first level, its loading
and its running.  Method and tools: [DECOMPILING.md](DECOMPILING.md).  Setup and disks: [README.md](README.md).

## Done

- Setup: two disks + Kickstart 1.3 under ami9000, floppy swap in the host, scripted cold run (`host/run_level.lua`), exact step trace
  (`host/trace_steps.lua`), RAM dumps, `analysis/` (3963 executed instructions in 201 functions of the game proper).
- Build: flake shell, `tools/gen_symbols.py` (symbols -> struct, linker script, tables), m68k blob at `$C0000`, runtime helpers.
- Oracle: `host/oracle.lua`, `tools/check.py` (original vs C on the same machine state, scenarios `load` and `level`).
- Written and checked: `random_next`, `random_below` (`native/src/game.c`).

## Packages (decompiling; entry-address ranges of `uv run tools/show.py --todo`)

| module | functions at | instructions | what is probably there |
| --- | --- | --- | --- |
| pkg_a | `$0000-$17FF` | ~440 | low-RAM routines: blitter helpers, text, object lists |
| pkg_b | `$1800-$27FF` + the start-up flow of `$400` (`$7032`, `$70F6`, `$763A` ...) | ~470 | the start, the main loop, random, interrupts |
| pkg_c | `$2800-$3FFF` | ~455 | |
| pkg_d | `$4000-$5FFF`, the level blob `$54000-$6FFFF`, `$64000` | ~490 | level code (the blob), enemies |
| pkg_e | `$6000-$6FFF` | ~540 | |
| pkg_f | `$7000-$87FF` | ~410 | |
| pkg_g | `$8800-$FFFF` | ~600 | the menu, the disk check, loader |
| pkg_h | `$19000-$1FFFF` | ~560 | the music player |

Then: the SDL machine layer (`native/src/sdl/`: guest memory, files, irq, HLE display, sound), the port build, the run.

## Status (this session)

- Decompiled: the executed code and the routines the recorded run never reached (the title logo scaler, the level-1 spawner, turrets, boss, weapons, the
  keyboard handler, the pool-exhaustion path): about 250 functions in eight modules, integrated (`make -C native blob`, `make -C native sdl`).
- Oracle (`tools/check.py`): about 450 comparisons with no difference.  Left: `sfx_tick` twice (screen bytes the blitter was still writing in the
  original), and functions that cannot be called by the oracle (they wait for the vertical blank or the disk, or are interrupt handlers) or that the two
  scenarios never reach (many of the weapon and boss routines: written carefully from the disassembly, unchecked).
- Port: `tools/verify_port.sh` runs the port headless from the title through the menu, the first level selected, loaded (disk 2 asked for and
  inserted) and running, and checks each stage.  Enemies move, shots kill them, the score rises.  Not looked at in the window by the author.
- No C: `fn_054e00`, the shop program loaded from disk at $54E00 (not in the captures).
