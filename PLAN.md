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
| draw | `$0000-$17FF` | ~440 | low-RAM routines: blitter helpers, text, object lists |
| mainloop | `$1800-$27FF` + the start-up flow of `$400` (`$7032`, `$70F6`, `$763A` ...) | ~470 | the start, the main loop, random, interrupts |
| objects | `$2800-$3FFF` | ~455 | |
| weapons | `$4000-$5FFF` | ~490 | weapons, pickups, the player |
| level1 to level5 | the level blob `$54E00-$6FFFF`, one file per level | | the levels' code |
| play | `$6000-$6FFF` | ~540 | |
| title | `$7000-$87FF` | ~410 | |
| menus | `$8800-$FFFF` | ~600 | the menu, the disk check, loader |
| sound | `$19000-$1FFFF` | ~560 | the music player |

Then: the SDL machine layer (`native/src/sdl/`: guest memory, files, irq, HLE display, sound), the port build, the run.

## Status (this session)

- Decompiled: the executed code and the routines the recorded run never reached (the title logo scaler, the level-1 spawner, turrets, boss, weapons, the
  keyboard handler, the pool-exhaustion path): about 250 functions in eight modules, integrated (`make -C native blob`, `make -C native sdl`).
- Oracle (`tools/check.py`): 456 comparisons over 129 functions, no difference (the oracle waits for the blitter before its snapshot and ignores
  the disk DMA buffer).  Not compared: functions the oracle cannot call (they wait for the vertical blank or the disk, or are interrupt handlers) and
  functions the two scenarios never reach (many weapon, boss and turret routines: written carefully from the disassembly, unchecked).
- Port: checked by the lockstep against the original (`host/lockstep.lua`), headless or played in a window.
- The shop (`shop_main`, shop.c and shop_sound.c): ported and checked by the oracle in the `shop` scenario, see DECOMPILING.md.  The shop's end-of-broadcast path (`v_8fec`) and the
  weapon-less corners listed in the commit messages are written from the dump.
