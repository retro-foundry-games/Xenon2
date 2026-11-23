"""Replace the provisional callee names fn_ADDR by the names of the C functions that now have that `orig $ADDR`.

  python3 tools/rename_fns.py [--dry]

A placeholder declaration (an `extern`/prototype line that only declares fn_ADDR) is removed when the real function is declared in a header.
Prints the placeholders that have no C function (and the ones with more than one name)."""
import glob, os, re, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'native', 'src')

def definitions():
    defs = {}
    for f in sorted(glob.glob(os.path.join(SRC, '*.c'))):
        text = open(f).read()
        for m in re.finditer(r'orig\s+((?:\$[0-9A-Fa-f]+(?:\s*(?:,|and)\s*)?)+)(?:(?!\*/).)*?\*/\s*\n(static\s+)?(?:inline\s+)?[\w\s\*]+?\b(\w+)\s*\(', text, re.S):
            for a in re.findall(r'\$([0-9A-Fa-f]+)', m.group(1)):
                defs.setdefault(int(a, 16), []).append((m.group(3), bool(m.group(2)), os.path.basename(f)))
    return defs

def main():
    dry = '--dry' in sys.argv
    defs = definitions()
    files = sorted(glob.glob(os.path.join(SRC, '*.c')) + glob.glob(os.path.join(SRC, '*.h')))
    missing, ambiguous, count = {}, {}, 0
    for f in files:
        text = open(f).read()
        out_lines = []
        for line in text.split('\n'):
            ms = list(re.finditer(r'\bfn_([0-9a-f]{6})\b', line))
            if not ms:
                out_lines.append(line); continue
            new = line
            decl_only = bool(re.match(r'\s*(extern\s+)?[\w\s\*\(\),]*\bfn_[0-9a-f]{6}\s*\([^)]*\)\s*(__attribute__\(\(.*\)\)\s*)?;\s*(/\*.*\*/)?\s*$', line)) and not re.search(r'=|return|\bif\b', line)
            for m in ms:
                a = int(m.group(1), 16)
                cands = defs.get(a, [])
                nonstatic = [c for c in cands if not c[1]]
                same_file = [c for c in cands if c[2] == os.path.basename(f)]
                pick = (same_file or nonstatic or [None])[0]
                if not pick:
                    missing.setdefault(a, set()).add(os.path.basename(f)); continue
                if len({c[0] for c in (same_file or nonstatic)}) > 1:
                    ambiguous[a] = {c[0] for c in cands}
                if decl_only and f.endswith('.h'):
                    new = None; break          # drop the placeholder declaration of a header only (a call statement looks like a declaration: never drop lines of a .c)
                new = new.replace(m.group(0), pick[0]); count += 1
            if new is not None: out_lines.append(new)
        if not dry and '\n'.join(out_lines) != text: open(f, 'w').write('\n'.join(out_lines))
    print('replaced', count, 'uses')
    for a, fs in sorted(missing.items()): print('no C for fn_%06x (used in %s)' % (a, ', '.join(sorted(fs))))
    for a, n in ambiguous.items(): print('several names for $%06x: %s' % (a, n))
main()
