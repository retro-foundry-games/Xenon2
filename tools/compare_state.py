"""Compare the port's memory with the original's, member by member.

  uv run tools/compare_state.py PORT.bin ORIGINAL.bin [--all]

Both are 512 KB images in the original's byte order (the port: `xenon2-sdl --export-ram PATH:VBL`; the original: host/export_ram.lua).  Only the named
members (native/src/symbols/*.symbols) are compared: the rest of the block is padding or raw data the symbols do not name.  Prints the members that
differ, first bytes of each, and a count.  Volatile members (timers, the stack of the interrupts) can differ: judge by the names."""
import glob, os, re, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sizes = {'uint8_t': 1, 'int8_t': 1, 'uint16_t': 2, 'int16_t': 2, 'uint32_t': 4, 'int32_t': 4, 'irq_fn': 4}

def members():
    files = [os.path.join(ROOT, 'native/src/game.symbols')] + sorted(glob.glob(os.path.join(ROOT, 'native/src/symbols/*.symbols')))
    types = {}
    for f in files:
        if os.path.exists(f):
            for line in open(f):
                t = line.split(';')[0].split()
                if len(t) >= 3 and t[0] == 'type': types[t[1]] = int(t[2], 0)
    out = []
    for f in files:
        if not os.path.exists(f): continue
        for line in open(f):
            t = line.split(';')[0].split()
            if len(t) < 4 or t[0] not in ('var', 'arr', 'tab'): continue
            ct = ' '.join(t[3:-1]) if t[0] != 'var' else ' '.join(t[3:])
            n = int(t[-1], 0) if t[0] != 'var' else 1
            base = ct.replace('volatile ', '').strip()
            es = 4 if base.endswith('*') else sizes.get(base, types.get(base, 1))
            out.append((int(t[2], 0), es * n, t[1], os.path.basename(f)))
    return sorted(out)

def main():
    port, orig = open(sys.argv[1], 'rb').read(), open(sys.argv[2], 'rb').read()
    bad = 0
    ms = members()
    for addr, size, name, src in ms:
        a, b = port[addr:addr + size], orig[addr:addr + size]
        if a != b:
            bad += 1
            first = next(i for i in range(len(a)) if a[i] != b[i])
            print('%-28s $%06x +%-5d size %-6d port %s orig %s' % (name, addr, first, size, a[first:first + 8].hex(), b[first:first + 8].hex()))
    print(bad, 'of', len(ms), 'named members differ')

main()
