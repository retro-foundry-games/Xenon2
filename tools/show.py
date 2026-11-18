"""Read the executed code of the game, annotated.

    uv run tools/show.py ADDR [ADDR ...]       the function that contains each address: instructions (with how often they ran),
                                               variables by name where game.symbols has them, callees by name where C exists
    uv run tools/show.py --todo [LO HI]        the functions (hex range LO..HI of entry addresses) that no C has yet, biggest first
    uv run tools/show.py --done                the functions that have C

analysis/ holds the data (tools/analyze.py made it from the trace of a run: menu to level 1).  A function has C when a comment
`orig $ADDR` (or `orig $A, $B`) is in native/src/*.c.  Absolute addresses of variables are named from native/src/game.symbols and
native/src/symbols/*.symbols.
"""
import glob, os, re, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def blocks():
    out, cur = {}, None
    for line in open(os.path.join(ROOT, 'analysis', 'listing.txt')):
        if line.startswith('; ----'):
            cur = int(line.split()[2], 16); out[cur] = []
        elif line.strip() and cur is not None:
            out[cur].append(line.rstrip('\n'))
    return out

def functions():
    out = {}
    for line in open(os.path.join(ROOT, 'analysis', 'functions.txt')):
        p = line.split()
        out[int(p[0], 16)] = (p[1], int(p[2]), p[3:])
    return out

def symbols():
    names = {}
    files = [os.path.join(ROOT, 'native/src/game.symbols')] + sorted(glob.glob(os.path.join(ROOT, 'native/src/symbols/*.symbols')))
    sizes = {'uint8_t': 1, 'int8_t': 1, 'uint16_t': 2, 'int16_t': 2, 'uint32_t': 4, 'int32_t': 4}
    types = {}
    for f in files:
        if not os.path.exists(f): continue
        for line in open(f):
            t = line.split(';')[0].split()
            if len(t) >= 3 and t[0] == 'type': types[t[1]] = int(t[2], 0)
        for line in open(f):
            t = line.split(';')[0].split()
            if len(t) >= 4 and t[0] in ('var', 'arr', 'tab'):
                ct = ' '.join(t[3:-1]) if t[0] != 'var' else ' '.join(t[3:])
                n = int(t[-1], 0) if t[0] != 'var' else 1
                es = 4 if ct.endswith('*') else sizes.get(ct, types.get(ct, 1))
                names[(int(t[2], 0), es * n)] = t[1]
    return names

def c_functions():
    done = {}
    for f in glob.glob(os.path.join(ROOT, 'native/src/*.c')) + glob.glob(os.path.join(ROOT, 'native/src/**/*.c'), recursive=True):
        text = open(f).read()
        for m in re.finditer(r'orig\s+((?:\$[0-9A-Fa-f]+(?:\s*(?:,|and)\s*)?)+)', text):
            for a in re.findall(r'\$([0-9A-Fa-f]+)', m.group(1)):
                done[int(a, 16)] = os.path.relpath(f, ROOT)
        for m in re.finditer(r'/\*\s*inlined:?\s*((?:\$[0-9A-Fa-f]+[ ,]*)+)', text):
            for a in re.findall(r'\$([0-9A-Fa-f]+)', m.group(1)): done[int(a, 16)] = os.path.relpath(f, ROOT)
    return done

def c_names():
    names = {}
    for f in glob.glob(os.path.join(ROOT, 'native/src/*.c')):
        text = open(f).read()
        for m in re.finditer(r'orig\s+\$([0-9A-Fa-f]+)(?:(?!\*/).)*?\*/\s*\n(?:static\s+)?[\w\s\*]+?\b(\w+)\s*\(', text, re.S):
            names[int(m.group(1), 16)] = m.group(2)
    return names

def annotate(line, syms, cn, funcs):
    notes = []
    for a in re.findall(r'\$([0-9a-f]{1,8})', line[40:]):
        v = int(a, 16)
        if v in cn: notes.append(cn[v] + '()')
        elif v in funcs: notes.append('fn_%06x' % v)
        for (base, size), name in syms.items():
            if base <= v < base + size:
                notes.append(name + ('+%d' % (v - base) if v != base else '')); break
    return line + ('   ; ' + ', '.join(notes) if notes else '')

def main():
    argv = sys.argv[1:]
    if not argv: print(__doc__); return
    bl, fn = blocks(), functions()
    if argv[0] in ('--todo', '--done'):
        done = c_functions()
        lo, hi = (int(argv[1], 16), int(argv[2], 16)) if len(argv) >= 3 else (0, 1 << 24)
        rows = [(fn[e][1], e) for e in sorted(fn) if lo <= e < hi and ((e in done) == (argv[0] == '--done'))]
        if argv[0] == '--todo': rows.sort(reverse=True)
        for n, e in rows: print('%06x %-8s %4d instr%s' % (e, fn[e][0], n, ('  ' + done[e]) if e in done else ''))
        print(len(rows), 'functions,', sum(n for n, _ in rows), 'instructions')
        return
    syms, cn = symbols(), c_names()
    shown = set()
    for a in argv:
        v = int(a, 16)
        entry = next((e for e, lines in bl.items() if any(int(l.split()[0], 16) == v for l in lines)), None)
        if entry is None or entry in shown:
            if entry is None: print('%06x: not in the executed code' % v)
            continue
        shown.add(entry)
        kind, size, callers = fn.get(entry, ('?', 0, []))
        print('; function %06x (%s, %d instructions, called from %s)%s' % (entry, kind, size, ' '.join(callers) or '-', '   [C: %s]' % c_functions()[entry] if entry in c_functions() else ''))
        for l in bl[entry]: print(annotate(l, syms, cn, fn))
        print()

main()
