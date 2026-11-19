# Decompiling Xenon 2 (how it is done here)

The goal is C source that builds for the Amiga and for SDL3 and behaves like the original **byte for byte**; the original is the
authority and nothing is "fixed" because it looks wrong.  The method is the `amiga-porting` skill
(`~/git/retrofoundry/amiga_porting/skill/SKILL.md`, read it first); this file is how it is applied to this game.

## What is decompiled

The **game proper**: the program that disk 1 (tracks 80-159, raw, loaded at `$400`) starts at frame ~7000 with `jmp $400` (`bra $7032`):
its initialisation, the menu, the check for the game disk, the load of level 1 (the 229 sectors of disk 2 from sector 11 to `$54E00`,
the level's own code and data), the level loop, the music player, the interrupt handlers.  Not the intro, the title or the copy-protection
stages before it (`$64194`, `$7D896`): the port starts at `$400` with the image in memory.

Only **executed code** (a recorded run from the menu to level 1, `analysis/`): 3963 instructions in 201 functions.  A branch to code that
never ran ends the C function with `/* goes on at $ADDR */`; it is filled in when a later run reaches it.

## Tools (run from the project root; Python through `uv run`, capstone is in the project)

    uv run tools/show.py --todo [LO HI]     functions (entry addresses, hex) that have no C yet, biggest first
    uv run tools/show.py ADDR [ADDR ...]    a function's instructions, annotated: how often each ran, the names of variables
                                            (symbol files) and of functions that have C; `fn_ADDR` marks a callee without C yet
    uv run tools/m68kdis.py captures/trace-10000.bin ADDR LEN   disassemble RAM (code that did not run; unexecuted branches)
    uv run tools/check.py [--build build/NAME] [--scenario load|level] [NAME ...]   the oracle (below)
    make -C native blob BUILD=build/NAME    build the C for the 68000 (also regenerates game_state.h, game.ld, data.c from the symbols)

RAM dumps of the original are in `captures/trace-*.bin` (`trace-7100.bin`: the game image just after it loaded; `trace-9000.bin` .. `trace-12000.bin`:
the level running with its blob at `$54E00`; 512 KB each).  Read tables and strings from them.  `analysis/` has `functions.txt`, `listing.txt`
(every executed instruction by function) and `refs.txt` (absolute addresses referenced).

## Rules (from the skill)

- C only, no inline assembly, stdint types.  **68000 semantics**: keep the widths (`add.w` is 16-bit arithmetic on the low word and leaves
  the high word alone; `lsr.l`; `divu.w` is a 32/16 divide whose remainder is in the high word), signed (`blt/bgt/bge/ble`) and unsigned
  (`bcs/bcc/bhi/bls`) comparisons exactly.  Register arguments become parameters; a result the caller uses comes back as the return value
  (results in several registers: a struct or an out pointer; results in the condition codes: return the flag).  A tiny helper that cannot be
  a C function (results in registers, jumps out) is written into its caller (say `/* inlined: $12BDA */`).
- **Variables are one struct.**  Every variable, table and buffer at an absolute address is a line in a symbol file; never write an
  address number in the code.  `uv run`-free generators make `game_state.h` (the struct, `PAD()` between members), `game.ld` (the names at
  their addresses for the 68000 build) and `data.c` (constant tables copied from the RAM dump).  Never edit generated files.
  Format (see `tools/gen_symbols.py`):

      var NAME 0xADDR uint16_t              ; comment
      arr NAME 0xADDR uint8_t COUNT         ; a buffer or an array (a screen, a list of records: arr NAME 0xADDR struct_or_type COUNT)
      tab NAME 0xADDR uint16_t COUNT        ; constant data (a table the code only reads): its values come from the RAM dump
      type NAME SIZE                        ; a record type, `struct NAME` is in native/src/types_<module>.h (sizeof must match)

  Types: `uint8_t int8_t uint16_t int16_t uint32_t int32_t`, `T *` (4 bytes), a declared record type.  Put `volatile` in front of the type (`var tick 0x0c30 volatile uint16_t`) for what an interrupt changes.  Name by what it is for; a provisional name is `v_` + the address (`v_0cd0`).  **An address that the
  code uses as a pointer is declared with a pointer type, and an address constant in the code is the name of what is at that address**
  (the SDL build keeps all memory in one struct, so pointers are translated when it loads; a longword that is an address and is typed
  `uint32_t` breaks it).  Records: `struct` with fields at the original offsets, `-fpack-struct=2`; no unions: one struct for each variant.
- **Before adding a symbol, search all symbol files for its address** (`grep -i 0x0cd0 native/src/game.symbols native/src/symbols/*.symbols`):
  one address has one name; someone else may have added it.  `make blob` fails on an overlap or a duplicate name and names the files.
- Every function carries `/* orig $ADDR: what it does */` before it (`orig $A, $B and $C` for several entries).  Name functions by what
  they do; a provisional name is `fn_` + the address.  A function of another package that has no C yet is called as `fn_ADDR` (declare
  it `extern` in your header; the lead renames it when its owner names it: `uv run tools/show.py` shows names).
- The hardware: `native/src/amiga/hw.h` (`custom->bltcon0 = ...`, `CIAA_PRA`).  C that programs the blitter, the copper, the disk or
  audio chips stays faithful C (it runs on the Amiga and in the oracle) and carries `/* HLE: blitter */` (or `copper`, `disk`, `audio`)
  in its comment and is declared `HLE_WEAK` (hw.h; `HLE_WEAK void blit_text(...) { ... }`): the SDL port links high-level replacements over such functions, it does not emulate the chips.  Waiting for the
  blitter: `while (custom->dmaconr & DMAF_BLTBUSY) ;` as the original does.
- The blitter is started only with `BLTSIZE_WRITE(value)` (hw.h; the SDL host performs the blit at once from the registers).  The other blitter registers are plain writes.
- The registers with set/clear semantics (`dmacon`, `intena`, `intreq`, `adkcon`) are written only with `SETCLR_WRITE(dmacon, value)` (hw.h): the SDL port applies the write.  Palette registers
  (`custom->color[n]`), `bplcon0`, `diwstrt`... are written directly; the picture is made from the copper list the game leaves in memory (`cop1lc`, a list of
  MOVE/WAIT pairs that mostly sets the bitplane pointers) and from these registers.
- A loop that waits for something an interrupt handler or a chip changes (the vertical blank flag, a disk-done flag) is written
  `while (!flag) HOST_IDLE();` (hw.h): nothing on the Amiga, the SDL host runs the interrupts that are due.  The vectors are the array
  `irq_vectors` (core.symbols, `irq_fn`, levels 1-7 at $64..$7C): code that stores a handler address there assigns the C function.
- Interrupt handlers (end with `rte`) are `void irq_...(void)` functions; the oracle cannot call them (it calls routines that end with
  `rts`); translate carefully and list them in your report as unchecked.
- Self-modifying code (a `move.w #$4E75, addr` over a routine's first word, a branch patched into a slot) is a function pointer or a
  flag in C; say so in a comment and look at the dumps before and after.  Unexecuted ends of a routine are `/* goes on at $ADDR */`.

## Files, and working beside others

Several people work in this tree at once.  Each has a **module name** (`pkg_a`, ...) and owns only:

    native/src/<module>.c, <module>.h          the code and its header (prototypes of its functions; `extern` of fn_ADDR callees)
    native/src/types_<module>.h                its record types (included into game_state.h by the generator)
    native/src/symbols/<module>.symbols        its variables, tables and types
    host/cases/<module>.lua                    its oracle cases

Never edit another module's files; reading them (names, types, symbols) is how you reuse a name.  Do not commit (the lead does), do not
change tools, the Makefile or generated files; if a tool is wrong, say so in your report.  Keep your symbol file valid at all times (an
overlap breaks everyone's build): run `make -C native generate` after editing it.  Build into your own directory: `BUILD=build/<module>`.

## Checking against the original

1. `make -C native blob BUILD=build/<module>` builds your C into a blob for `$C0000` (RAM above the original's 512 KB) and fails on a C error.
2. Add a case for each function to `host/cases/<module>.lua`:
   `{ name = "function_name", orig = 0xADDR, args = { "d0.w", "a0" }, ret = "d0.w" },` (`args`: the registers the arguments come in, in the
   order of the C parameters, `.w`/`.b` for the low part; `ret`: the register the caller reads the result from; `blitter = true` when the
   function draws).  `where = function(ami) ... end` restricts the calls compared (see `host/oracle.lua`).
3. `uv run tools/check.py --build build/<module> NAME ...` runs one emulator over two scenarios (`load`: from the menu the fire button
   starts the game, disk 2 goes in, level 1 loads; `level`: level 1 played with some stick and fire), hooks the original's entry of each
   function, and at the first N calls runs the original and the C on the same state and compares the result register and all of chip RAM.
   It must say `differ 0` and `compared` above 0 for every function (in at least one scenario).  A `DIFF` names the registers or the memory
   bytes that differ: that is your error.  **One emulator at a time per run; no screenshots, no recordings.**
4. A function the oracle cannot call (interrupt handlers, entries in the middle of another, results only in the condition codes, loops
   that wait for the vertical blank) is checked inside its caller or by the later lockstep run; say which in your report.
5. A function that is not called in either scenario (`calls 0`) is not checked: write it from the listing with extra care and report it.

## Lessons

- The function list is the executed code of one run; the game takes branches the run did not.  Read the disassembly of the whole function
  (`uv run tools/m68kdis.py`), write all of it you can read, and mark the rest `/* goes on at $ADDR */`.
- A routine that is one `rts` may be patched at run time; do not treat it as empty without looking at the dumps.
- Data in the code area (the random generator's state at `$1FEE`, tables after a `rts`) is data: give it symbols.
- `captures/` and `local/` are not in git; `analysis/` is.
