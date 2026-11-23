# Xenon 2: the C of the game, built with m68k-amigaos-gcc (the toolchain is in the flake of this directory).
#
#   make blob     the game as a blob at $(GAME_BASE) for the checks against the original (build/game.blob, build/game_syms.lua)
#   make sdl      the SDL3 port (src/sdl/), build/sdl/xenon2-sdl
#   make clean
#
# Outside the flake's shell every target is run again inside it.

CC  := m68k-amigaos-gcc
LD  := m68k-amigaos-ld
NM  := m68k-amigaos-nm
SRC := src
# an agent working beside others builds in its own directory: make blob BUILD=build/NAME
BUILD ?= build
PYTHON ?= python3
GAME_BASE ?= C0000

ifeq ($(shell command -v $(CC) 2>/dev/null),)
.PHONY: all $(MAKECMDGOALS)
all $(MAKECMDGOALS):
	@nix develop $(CURDIR) --command $(MAKE) --no-print-directory $@
else

COMMON := -m68000 -ffreestanding -fno-tree-loop-distribute-patterns -fno-tree-forwprop -fno-ivopts -nostdlib -fdata-sections -ffunction-sections
GAME_CFLAGS := $(COMMON) -O2 -I$(SRC)
WARN := -Wall -Wextra -Wno-unused-parameter
# every module in src/ (data.c is generated)
GAME_MODULES := $(filter-out runtime,$(notdir $(basename $(wildcard $(SRC)/*.c))))
GAME_OBJS := $(GAME_MODULES:%=$(BUILD)/%.o) $(BUILD)/runtime.o

.PHONY: all blob clean generate sdl sdl-run
all: blob

# game.h, game.ld, guest_members.h and data.c are made from the symbol files (tools/gen_symbols.py; the tables are read from XENON_RAM)
XENON_RAM ?= ../captures/trace-10000.bin
SYMBOL_FILES := $(wildcard $(SRC)/game.symbols $(SRC)/symbols/*.symbols)
$(SRC)/game.h $(SRC)/game.ld $(SRC)/guest_members.h $(SRC)/data.c: $(SYMBOL_FILES) $(SRC)/level.h ../tools/gen_symbols.py
	$(PYTHON) ../tools/gen_symbols.py --ram $(XENON_RAM)
generate: $(SRC)/game.h
# the layout of the level blobs (tools/gen_blobs.py)
$(SRC)/level.h $(SRC)/guest_blob.h $(SRC)/level2.h $(SRC)/level3.h $(SRC)/level4.h $(SRC)/level5.h: $(wildcard $(SRC)/blobs/*.blob) ../tools/gen_blobs.py
	$(PYTHON) ../tools/gen_blobs.py

$(BUILD):
	mkdir -p $@
$(BUILD)/%.o: $(SRC)/%.c $(SRC)/game.h $(wildcard $(SRC)/*.h) | $(BUILD)
	$(CC) $(GAME_CFLAGS) $(WARN) -c $< -o $@
$(BUILD)/runtime.o: $(SRC)/amiga/runtime.c | $(BUILD)
	$(CC) $(GAME_CFLAGS) -fno-builtin -c $< -o $@
$(BUILD)/game: $(GAME_OBJS) $(SRC)/game.ld
	$(LD) -m amiga -T $(SRC)/game.ld -Map $(BUILD)/game.map -o $@ $(GAME_OBJS)
	$(NM) $@ > $(BUILD)/game.sym

# the code hunk and the function table, for the oracle
blob: $(BUILD)/game
	BUILD=$(BUILD) $(PYTHON) ../tools/build_blob.py $(GAME_BASE)

# the SDL3 port: the game's C compiled for the host as 32-bit (the 68000's pointers are 4 bytes) with the machine of src/sdl/ around it
SDL_BUILD ?= build/sdl
SDL_CFLAGS := -m32 -fpack-struct=2 -std=gnu11 -O1 -g -fno-strict-aliasing -DGAME_SDL -I$(SRC) -I$(SRC)/sdl -Wall -Wno-unused-function $(shell pkg-config --cflags sdl3)
# (the tables are struct members in the SDL build, loaded from the disk image: no data.c)
SDL_GAME_MODULES := $(filter-out runtime data,$(GAME_MODULES))
SDL_HOST_SOURCES := $(sort $(wildcard $(SRC)/sdl/*.c) $(SRC)/sdl/records.c)
SDL_OBJS := $(SDL_GAME_MODULES:%=$(SDL_BUILD)/g_%.o) $(patsubst $(SRC)/sdl/%.c,$(SDL_BUILD)/h_%.o,$(SDL_HOST_SOURCES))
$(SDL_BUILD)/g_%.o: $(SRC)/%.c $(SRC)/game.h $(wildcard $(SRC)/*.h) | $(SDL_BUILD)
	gcc $(SDL_CFLAGS) -c $< -o $@
# (the files that include SDL's headers are built without -fpack-struct=2, which the game's structs need and SDL's must not have)
$(SDL_BUILD)/h_main.o: $(SRC)/sdl/main.c $(wildcard $(SRC)/sdl/*.h $(SRC)/amiga/*.h) | $(SDL_BUILD)
	gcc $(filter-out -fpack-struct=2,$(SDL_CFLAGS)) -c $< -o $@
$(SDL_BUILD)/h_%.o: $(SRC)/sdl/%.c $(SRC)/game.h $(wildcard $(SRC)/*.h $(SRC)/sdl/*.h $(SRC)/amiga/*.h) | $(SDL_BUILD)
	gcc $(SDL_CFLAGS) -c $< -o $@
$(SDL_BUILD):
	mkdir -p $@
# the loader tables of the port: how records of the disk image become records of the host, and the C function of each address of the original's code
$(SRC)/sdl/records.c $(SRC)/code_table.c: $(wildcard $(SRC)/types_*.h $(filter-out $(SRC)/code_table.c $(SRC)/data.c,$(wildcard $(SRC)/*.c))) ../tools/gen_records.py
	$(PYTHON) ../tools/gen_records.py
sdl: $(SDL_BUILD)/xenon2-sdl
$(SDL_BUILD)/xenon2-sdl: $(SDL_OBJS)
	gcc -m32 -o $@ $^ $(shell pkg-config --libs sdl3) -lm
sdl-run: sdl
	$(SDL_BUILD)/xenon2-sdl --disks ../local

clean:
	rm -rf $(BUILD)
endif
