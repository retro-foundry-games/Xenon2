# The dev build of the port (xenon2-sdl-dev): the port's own sources (native/src, built with -finstrument-functions so that a script can hook the
# game's functions by their original addresses), the dev layer (src/dev.c: the game as a coroutine, Lua) and the vendored Lua (../third_party/lua).
#
#   make -C dev                  build dev/build/xenon2-sdl-dev
#   make -C dev lockstep         run the lockstep with the original (needs the disks and the Kickstart in local/; see host/lockstep.lua)
#
# The port itself is only native/: nothing here is part of it.  The toolchain is the flake's of native/; outside its shell every target runs again inside.

HERE   := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
ROOT   := $(abspath $(HERE)/..)
NATIVE := $(ROOT)/native
SRC    := $(NATIVE)/src
LUA    := $(ROOT)/third_party/lua
BUILD  := $(HERE)/build
PYTHON ?= python3

ifeq ($(IN_NIX_SHELL),)
.PHONY: all $(MAKECMDGOALS)
all $(MAKECMDGOALS):
	@nix develop $(NATIVE) --command $(MAKE) --no-print-directory -f $(lastword $(MAKEFILE_LIST)) $@
else

# the same flags as the port's (native/Makefile `sdl`), plus the instrumentation of the game's own C
GAME_CFLAGS := -m32 -fpack-struct=2 -std=gnu11 -O1 -g -fno-strict-aliasing -fno-pie -fno-stack-protector -ffunction-sections -fdata-sections -DGAME_SDL -I$(SRC) -I$(SRC)/sdl -Wall -Wno-unused-function \
               $(shell pkg-config --cflags sdl3)
GAME_MODULES := $(filter-out runtime data,$(notdir $(basename $(wildcard $(SRC)/*.c))))
GAME_OBJS  := $(GAME_MODULES:%=$(BUILD)/g_%.o)
HOST_SOURCES := $(filter-out $(SRC)/sdl/main.c,$(sort $(wildcard $(SRC)/sdl/*.c)))
HOST_OBJS  := $(patsubst $(SRC)/sdl/%.c,$(BUILD)/h_%.o,$(HOST_SOURCES))
LUA_OBJS   := $(patsubst $(LUA)/%.c,$(BUILD)/lua_%.o,$(wildcard $(LUA)/*.c))

all: $(BUILD)/xenon2-sdl-dev

# the port's generated files (symbols, struct, tables) come from native: make them first
$(SRC)/game_state.h:
	$(MAKE) -C $(NATIVE) generate

$(BUILD):
	mkdir -p $@
$(BUILD)/g_%.o: $(SRC)/%.c $(HERE)/Makefile $(SRC)/game_state.h $(wildcard $(SRC)/*.h) | $(BUILD)
	gcc $(GAME_CFLAGS) -finstrument-functions -c $< -o $@
# (the code table is what the hook looks functions up in: not instrumented)
$(BUILD)/g_code_table.o: $(SRC)/code_table.c $(SRC)/game_state.h | $(BUILD)
	gcc $(GAME_CFLAGS) -c $< -o $@
$(BUILD)/h_%.o: $(SRC)/sdl/%.c $(HERE)/Makefile $(SRC)/game_state.h $(wildcard $(SRC)/*.h $(SRC)/sdl/*.h $(SRC)/amiga/*.h) | $(BUILD)
	gcc $(GAME_CFLAGS) -c $< -o $@
# (main.c includes SDL's headers: without -fpack-struct=2, as in the port's own build)
$(BUILD)/h_main.o: $(SRC)/sdl/main.c $(wildcard $(SRC)/sdl/*.h $(SRC)/amiga/*.h) | $(BUILD)
	gcc $(filter-out -fpack-struct=2,$(GAME_CFLAGS)) -c $< -o $@
$(BUILD)/dev.o: $(HERE)/src/dev.c $(HERE)/Makefile $(wildcard $(SRC)/sdl/*.h) | $(BUILD)
	gcc -m32 -std=gnu11 -O1 -g -fno-pie -fno-stack-protector -Wall -I$(SRC)/sdl -I$(LUA) -c $< -o $@
$(BUILD)/lua_%.o: $(LUA)/%.c $(HERE)/Makefile | $(BUILD)
	gcc -m32 -std=gnu11 -O2 -fno-pie -fno-stack-protector -DLUA_USE_POSIX -I$(LUA) -c $< -o $@

$(BUILD)/xenon2-sdl-dev: $(GAME_OBJS) $(HOST_OBJS) $(BUILD)/h_main.o $(BUILD)/dev.o $(LUA_OBJS)
	gcc -m32 -no-pie -o $@ $^ $(shell pkg-config --libs sdl3) -lm -ldl -Wl,--gc-sections

clean:
	rm -rf $(BUILD)
.PHONY: all clean
endif
