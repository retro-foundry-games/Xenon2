"""Check functions of native/src against the original with the oracle (host/oracle.lua), one emulator run for all of them.

    uv run tools/check.py [--build build/NAME] [--scenario load|level|all] [--samples N] [NAME ...]

Builds the blob first (`make blob` in native/).  The functions are the ones in host/cases/NAME.lua (all, or the named).  A scenario is a
savestate (host/mkstate.lua) with a joystick script; each function is compared in the scenarios its case says (`scenario = "level"`;
default: all of them) and must show `differ 0` with `compared` above 0 in at least one.  A function that is not called in a scenario
shows `calls 0` there.  Run one emulator at a time.
"""
import os, re, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HOST = Path(os.environ.get('AMI_HOST', ROOT.parent / 'amiga_porting' / 'ami9000' / 'build' / 'ami9000_host'))
def joystick_script(frames, first_fire=20):
    """Fire for 10 frames every 150 frames (the game waits for it at the start of a level and ends its screens with it), and between the
    fires the stick moves in a cycle of directions, so that the player moves and shoots."""
    moves = ['right', 'left+fire', 'up', 'down+fire', 'right+up', 'left', 'right+fire', 'down']
    out = []
    for i, k in enumerate(range(0, frames, 150)):
        out += [f'{k + first_fire}:fire', f'{k + first_fire + 10}:none', f'{k + 60}:{moves[i % len(moves)]}', f'{k + 110}:none']
    return ','.join(sorted(out, key=lambda e: int(e.split(':')[0])))

SCENARIOS = {
    # the menu with the attract mode running, then the fire button: the game asks for its disk, disk 2 goes in, the level loads and is played
    'load': ('xenon2-menu.state', 4200, joystick_script(4200, 40), True),
    # level 1 as it stands (loaded, waiting for fire), then played
    'level': ('xenon2-level.state', 1800, joystick_script(1800), False),
}

def run(names, scenario, samples, build):
    state, until, joy, swap = SCENARIOS[scenario]
    out = ROOT / 'captures' / f'oracle-{build.replace("/", "-")}-{scenario}.txt'
    out.unlink(missing_ok=True)
    args = f'state={state} until={until} joy={joy} samples={samples} stride=3 build={build} out={out}'
    if names: args += ' only=' + ','.join(names)
    if swap: args += ' swap=1'
    r = subprocess.run([str(HOST), str(ROOT / 'host' / 'oracle.lua'), '--headless', '--script-args', args], capture_output=True, text=True, cwd=ROOT)
    if not out.exists():
        print(f'[{scenario}] the run wrote no result\n' + (r.stdout + r.stderr)[-800:])
        return None
    return out.read_text()

def main():
    argv = sys.argv[1:]
    scenarios, samples, names, build = ['load', 'level'], 24, [], 'build'
    while argv:
        a = argv.pop(0)
        if a == '--scenario':
            v = argv.pop(0); scenarios = list(SCENARIOS) if v == 'all' else [v]
        elif a == '--samples': samples = int(argv.pop(0))
        elif a == '--build': build = argv.pop(0)   # build/NAME: an own build directory beside the others
        else: names.append(a)
    m = subprocess.run(['make', '-C', str(ROOT / 'native'), '--no-print-directory', 'blob', f'BUILD={build}'], capture_output=True, text=True)
    if m.returncode:
        print((m.stdout + m.stderr)[-3000:]); return 2
    totals = {}
    for sc in scenarios:
        text = run(names, sc, samples, build)
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
