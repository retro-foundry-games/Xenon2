#!/usr/bin/env python3
"""tools/poison_gaps.py N=captures/NAME.ram OUT: copies a RAM dump with every byte that no member of the port names (the gaps of struct Xenon2 and the pads of the
level blob N) set to 0xA5 (the sample bank, $1BB80-$3B836, and the zero page stay).  The port played from the poisoned dump must behave as from the dump: a difference is a read of
memory that has no name."""
import re, sys, pathlib
root = pathlib.Path(__file__).resolve().parent.parent / 'native' / 'src'
level, src = sys.argv[1].split('=')
d = bytearray(open(src, 'rb').read())
cov = bytearray(0x80000)
for a, s in re.findall(r'\{ (0x[0-9a-f]+), (\d+), \d+, \d+,', (root / 'guest_members.h').read_text()):
    a = int(a, 16)
    cov[a:a + int(s)] = b'\1' * int(s)
text = (root / 'guest_blob.h').read_text()
pads = re.search(r'blob_pads_%s\[\] = \{.*?\n(.*?)\};' % level, text, re.S).group(1)
BLOB = 0x54E40
cov[BLOB:0x7d120] = b'\1' * (0x7d120 - BLOB)
for o, z in re.findall(r'\{ 0x([0-9A-F]+), (\d+) \}', pads):
    for i in range(int(o, 16) + BLOB, int(o, 16) + BLOB + int(z)):
        cov[i] = 0
n = 0
for i in range(0x400, 0x7d120):
    if not cov[i] and not (0x1bb80 <= i < 0x3b836):
        d[i] = 0xA5
        n += 1
open(sys.argv[2], 'wb').write(d)
print(n, 'bytes poisoned')
