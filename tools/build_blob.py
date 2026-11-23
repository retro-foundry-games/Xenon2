"""Turn native/build/game (an Amiga hunk executable linked at BASE) into native/build/game.blob and game_syms.lua.

    python3 tools/build_blob.py [BASE_HEX]        (run from native/, `make blob` does)
"""
import os, re, struct, sys
from pathlib import Path

base = int(sys.argv[1], 16) if len(sys.argv) > 1 else 0xC0000
build = Path(os.environ.get('BUILD', 'build'))
data = (build / 'game').read_bytes()
pos, hunks = 0, []
def u32(): 
    global pos
    v = struct.unpack('>I', data[pos:pos + 4])[0]; pos += 4; return v
assert u32() == 0x3F3                      # HUNK_HEADER
while (n := u32()) != 0: pos += n * 4      # resident library names
table, first, last = u32(), u32(), u32()
sizes = [u32() & 0x3FFFFFFF for _ in range(last - first + 1)]
code = None
for size in sizes:
    t = u32() & 0x3FFFFFFF
    assert t in (0x3E9, 0x3EA, 0x3EB), hex(t)   # CODE, DATA, BSS
    if t == 0x3EB: u32(); body = b'\0' * (size * 4)
    else:
        n = u32(); body = data[pos:pos + n * 4]; pos += n * 4
    while True:
        e = u32() & 0x3FFFFFFF
        if e == 0x3F2: break                     # HUNK_END
        if e == 0x3EC:                           # RELOC32: (count, hunk, offsets...)* 0
            while (cnt := u32()) != 0: pos += (cnt + 1) * 4
        elif e == 0x3F0:                         # SYMBOL: (words, name..., value)* 0
            while (n := u32() & 0xFFFFFF) != 0: pos += (n + 1) * 4
        elif e == 0x3F1:                         # DEBUG: words
            n = u32(); pos += n * 4
        else: raise SystemExit('hunk type %x unsupported' % e)
    hunks.append((t, body))
code = hunks[0][1]
(build / 'game.blob').write_bytes(code)
funcs = {m.group(3).lstrip('_'): int(m.group(1), 16) + base
         for m in re.finditer(r'^([0-9a-f]{8}) ([tT]) (\S+)$', (build / 'game.sym').read_text(), re.M)
         if not m.group(3).startswith('__')}
(build / 'game_syms.lua').write_text('return {\n  base = 0x%x,\n  size = %d,\n%s}\n' % (
    base, len(code), ''.join('  %s = 0x%x,\n' % (k, v) for k, v in sorted(funcs.items()))))
print('game: %d bytes at $%x, %d functions' % (len(code), base, len(funcs)))

# which C function is which function of the original (the `orig $ADDR` comments): for the oracle, which accepts a stored pointer to the C function where the original
# stores the original's address
import glob
origs = {}
for f in sorted(glob.glob('src/*.c') + ['src/amiga/disk.c']):
    text = open(f).read()
    for m in re.finditer(r'orig\s+((?:\$[0-9A-Fa-f]+(?:\s*(?:,|and)\s*)?)+)(?:(?!\*/).)*?\*/\s*\n(?:static\s+)?(?:inline\s+)?[\w\s\*]+?\b(\w+)\s*\(', text, re.S):
        for a in re.findall(r'\$([0-9A-Fa-f]+)', m.group(1)):
            origs.setdefault(m.group(2), []).append(int(a, 16))
(build / 'game_origs.lua').write_text('return {\n%s}\n' % ''.join('  %s = { %s },\n' % (k, ', '.join('0x%x' % a for a in v)) for k, v in sorted(origs.items())))

# the routines that wait for the vertical blank (they contain HOST_IDLE): the oracle makes them return at once, in the original and in the C
waits_c, waits_orig = set(), set()
hdr = re.compile(r'(?:/\*.*?orig\s+((?:\$[0-9A-Fa-f]+(?:\s*(?:,|and)\s*)?)+)(?:(?!\*/).)*?\*/\s*)?\n(?:static\s+)?(?:inline\s+)?(?:const\s+)?[\w\*]+[\w\s\*]*?\b(\w+)\s*\([^;{)]*\)\s*\{', re.S)
for f in sorted(glob.glob('src/*.c') + ['src/amiga/disk.c']):
    text = open(f).read()
    ms = list(hdr.finditer(text))
    for i, m in enumerate(ms):
        body = text[m.end():ms[i + 1].start() if i + 1 < len(ms) else len(text)]
        if 'HOST_IDLE' in body:
            waits_c.add(m.group(2))
            for a in re.findall(r'\$([0-9A-Fa-f]+)', m.group(1) or ''): waits_orig.add(int(a, 16))
(build / 'game_waits.lua').write_text('return {\n  c = { %s },\n  orig = { %s },\n}\n' % (', '.join('"%s"' % n for n in sorted(waits_c)), ', '.join('0x%x' % a for a in sorted(waits_orig))))
