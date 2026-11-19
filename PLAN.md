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

- Decompiled: 200+ functions of the executed code (eight packages, integrated: `make -C native blob`, `make -C native sdl`).
- Oracle (`tools/check.py`): about 140 functions compared with no difference in the two scenarios; the rest cannot be called (they wait for the
  vertical blank or the disk, or are interrupt handlers), or never ran.  The only differences left are the star routines during disk loading
  (the disk DMA writes MFM data into the screen memory at different moments in the two runs).
- The port runs headless from the title through the menu, the disk-2 request and the load into level 1: enemies move, the player is hit, shots kill
  enemies and the score rises (see `--status-every`).  Not yet confirmed by looking at it in the window.
- No C (never ran in the recorded run; the port logs it): the stretched-logo animation of the title ($8092), the level-1 spawner $55E2C,
  several weapon routines ($3B4A-$4340, $54E00, $561DA, $56278), the keyboard interrupt handler ($0FAC).
