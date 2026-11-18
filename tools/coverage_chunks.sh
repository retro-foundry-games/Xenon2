#!/bin/sh
# Executed-instruction coverage of a cold run (boot to level 1) in 64 KB chunks, one emulator after the other:
# a single run that hooks all of chip RAM silently loses hooks.   usage: tools/coverage_chunks.sh UNTIL_FRAME OUT_DIR [FROM_FRAME]
# Hooks put in before the code was loaded miss it (the core); give FROM_FRAME to hook only once the code is in RAM.
UNTIL=${1:-12000}; OUT=${2:-captures/cov}; FROM=${3:-0}
mkdir -p "$OUT"
for lo in 0 10000 20000 30000 40000 50000 60000 70000; do
  hi=$(printf '%x' $((0x$lo + 0xfffe)))
  python3 ../amiga_porting/ami9000/pidrun.py --timeout 1800 -- ../amiga_porting/ami9000/build/ami9000_host host/run_level.lua \
    --headless --script-args "joy1 until=$UNTIL coverfrom=$FROM cover=$OUT/$lo.txt coverlo=$lo coverhi=$hi" > "$OUT/$lo.log" 2>&1
done
cat "$OUT"/*.txt | sort -u > "$OUT/all.txt"
echo finished > "$OUT/done"
