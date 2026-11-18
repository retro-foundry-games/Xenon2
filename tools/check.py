"""Check functions of native/src against the original with the oracle (host/oracle.lua), one emulator run for all of them.

    uv run tools/check.py [--scenario menu|load|level|all] [--samples N] [NAME ...]

Builds the blob first (`make blob` in native/).  The functions are the ones in host/cases.lua (all, or the named).  A scenario is a
savestate (host/mkstate.lua) with a joystick script; each function is compared in the scenarios its case says (`scenario = "level"`;
default: all of them) and must show `differ 0` with `compared` above 0 in at least one.  A function that is not called in a scenario
shows `calls 0` there.  Run one emulator at a time.
"""
import os, re, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HOST = ROOT.parent / 'amiga_porting' / 'ami9000' / 'build' / 'ami9000_host'
SCENARIOS = {
    # the menu with the attract mode running, then the fire button: the game asks for its disk, disk 2 goes in, the level loads and runs
    'load': ('xenon2-menu.state', 2300, '40:fire,50:none', True),
    # level 1 running (the player is not controlled: nothing pressed, then some stick and fire)
    'level': ('xenon2-level.state', 900, '100:right,160:none,200:left+fire,260:none,300:up,340:down,380:fire,420:none', False),
}

def run(names, scenario, samples):
    state, until, joy, swap = SCENARIOS[scenario]
    out = ROOT / 'captures' / f'oracle-{scenario}.txt'
    out.unlink(missing_ok=True)
    args = f'state={state} until={until} joy={joy} samples={samples} stride=3 out={out}'
    if names: args += ' only=' + ','.join(names)
    if swap: args += ' swap=1'
    r = subprocess.run([str(HOST), str(ROOT / 'host' / 'oracle.lua'), '--headless', '--script-args', args], capture_output=True, text=True, cwd=ROOT)
    if not out.exists():
        print(f'[{scenario}] the run wrote no result\n' + (r.stdout + r.stderr)[-800:])
        return None
    return out.read_text()

def main():
    argv = sys.argv[1:]
    scenarios, samples, names = ['load', 'level'], 24, []
    while argv:
        a = argv.pop(0)
        if a == '--scenario':
            v = argv.pop(0); scenarios = list(SCENARIOS) if v == 'all' else [v]
        elif a == '--samples': samples = int(argv.pop(0))
        else: names.append(a)
    subprocess.run(['make', '-C', str(ROOT / 'native'), '--no-print-directory', 'blob'], check=True, capture_output=True)
    totals = {}
    for sc in scenarios:
        text = run(names, sc, samples)
        if text is None: return 2
        print(f'== {sc}')
        for line in text.splitlines():
            if line.startswith('---'): continue
            print('  ' + line)
            m = re.match(r'(\w+)\s+calls\s+(\d+)\s+compared\s+(\d+)\s+ok\s+(\d+)\s+differ\s+(\d+)', line)
            if m:
                t = totals.setdefault(m.group(1), [0, 0]); t[0] += int(m.group(3)); t[1] += int(m.group(5))
    bad = [n for n, (c, d) in totals.items() if d or not c]
    print('all compared functions match' if not bad else 'to look at (a difference, or never compared): ' + ' '.join(bad))
    return 1 if bad else 0

raise SystemExit(main())
