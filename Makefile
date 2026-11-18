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
BUILD := build
PYTHON ?= python3
GAME_BASE ?= C0000

ifeq ($(shell command -v $(CC) 2>/dev/null),)
.PHONY: all $(MAKECMDGOALS)
all $(MAKECMDGOALS):
	@nix develop $(CURDIR) --command $(MAKE) --no-print-directory $@
else

COMMON := -m68000 -ffreestanding -fno-tree-loop-distribute-patterns -fno-tree-forwprop -fno-ivopts -nostdlib -fdata-sections -ffunction-sections
GAME_CFLAGS := $(COMMON) -O2 -DMATCH_MEMORY_LAYOUT -I$(SRC)
WARN := -Wall -Wextra -Wno-unused-parameter
GAME_MODULES := game data
GAME_OBJS := $(GAME_MODULES:%=$(BUILD)/%.o)

.PHONY: all blob clean
all: blob

$(BUILD):
	mkdir -p $@
$(BUILD)/%.o: $(SRC)/%.c $(wildcard $(SRC)/*.h) | $(BUILD)
	$(CC) $(GAME_CFLAGS) $(WARN) -c $< -o $@
$(BUILD)/game: $(GAME_OBJS) $(SRC)/game.ld
	$(LD) -m amiga -T $(SRC)/game.ld -Map $(BUILD)/game.map -o $@ $(GAME_OBJS)
	$(NM) $@ > $(BUILD)/game.sym

# the code hunk and the function table, for the oracle
blob: $(BUILD)/game
	$(PYTHON) ../tools/build_blob.py $(GAME_BASE)

clean:
	rm -rf $(BUILD)
endif
