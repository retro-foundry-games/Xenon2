"""Which custom-chip registers do the instructions at the given addresses touch?  usage: custom_refs.py RAM COVERAGE_TXT"""
import re, sys, collections
from capstone import Cs, CS_ARCH_M68K, CS_MODE_M68K_000
ram = open(sys.argv[1], 'rb').read()
addrs = sorted(int(x, 16) for x in open(sys.argv[2]).read().split())
md = Cs(CS_ARCH_M68K, CS_MODE_M68K_000)
names = {}
for line in open(sys.argv[3]) if len(sys.argv) > 3 else []:
    a, n = line.split()[:2]; names[int(a, 16)] = n
cnt = collections.Counter()
for a in addrs:
    for i in md.disasm(ram[a:a + 10], a, 1):
        for m in re.finditer(r'\$(df[f0-9a-f]{3}|bf[de][0-9a-f]{3})\b', i.op_str):
            cnt[m.group(1)] += 1
for k, v in sorted(cnt.items()): print(k, v)
