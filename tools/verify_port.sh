#!/bin/sh
# Headless acceptance of the SDL port: the title, the menu, the first level selected, loaded (disk 2 asked for) and run.
#   tools/verify_port.sh            (from the project root; needs local/xenon2-disk1.adf and -disk2.adf; make -C native sdl first)
# Counts the calls of the stages with gdb breakpoints over a scripted play (fire presses to pass the screens and start the game, then the stick and
# the fire button), and reads the game's variables at the end.  Exits 1 and says which stage did not happen.
BIN=native/build/sdl/xenon2-sdl
FRAMES=${1:-4200}
JOY=$(python3 -c "print(','.join('%d:fire,%d:none'%(100+150*i,110+150*i) for i in range(10)) + ',1700:fire+left,1800:fire+right,1900:fire+left,2000:fire+right,2100:fire+left,2200:fire+right,2300:fire+left,2400:fire+right,2500:fire+left,2600:fire+right')")
cat > /tmp/verify_gdb <<EOF2
set pagination off
break menu
break level_load
break files_insert
break level_start
break init_level_state
break update_all
break enemy_killed
break update_score_display
run
$(for b in 1 2 3 4 5 6 7 8; do echo "ignore $b 100000000"; done)
continue
info breakpoints
EOF2
OUT=$(timeout 600 gdb -batch -x /tmp/verify_gdb --args $BIN --disks local --headless --frames "$FRAMES" --joy "$JOY" --status "$((FRAMES-1))" 2>&1)
echo "$OUT" | grep -E "game:|has no C" | sed 's/  */ /g'
hits() { echo "$OUT" | grep -A1 "in $1 at" | grep -o "already hit [0-9]* time" | grep -o "[0-9]*" | head -1; }
fail=0
check() { n=$(hits "$1"); n=${n:-0}; if [ "$n" -ge "$3" ]; then echo "ok   $2: $1 called $n times"; else echo "FAIL $2: $1 called $n times (need $3)"; fail=1; fi; }
check menu "the menu is reached" 1
check level_load "the first level is selected and loaded" 1
check files_insert "the game disk is asked for and disk 2 inserted" 1
check init_level_state "the level is set up" 1
check update_all "the level runs (objects updated every frame)" 200
check update_score_display "the score display is updated" 1
SCROLL=$(echo "$OUT" | grep -o "scroll [0-9a-f]*" | tail -1)
echo "last status: $SCROLL"
if echo "$OUT" | grep -q "SIGSEGV\|SIGFPE\|SIGABRT"; then echo "FAIL the port crashed"; fail=1; fi
[ $fail = 0 ] && echo "PORT VERIFIED: title -> menu -> first level selected -> loaded -> running" || echo "PORT NOT VERIFIED"
exit $fail
