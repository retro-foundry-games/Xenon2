"""Static analysis of the executed code of a run.

  uv run tools/analyze.py RAM.bin COVERAGE.txt OUTDIR [--ram2 OTHER.bin ...]

COVERAGE.txt: one hex address per line (the instructions that ran).  RAM.bin: chip RAM with the code in place.  Writes
  OUTDIR/functions.txt   entries (targets of bsr/jsr, vectors) with the number of covered instructions each reaches
  OUTDIR/refs.txt        absolute addresses the executed instructions read or write: ADDR kind sizes count
  OUTDIR/listing.txt     the executed instructions (address, bytes, text), marked with their function entry
"""
import sys, re, collections
from capstone import Cs, CS_ARCH_M68K, CS_MODE_M68K_000, CS_OP_MEM, CS_OP_IMM, CS_GRP_JUMP, CS_GRP_CALL, CS_GRP_RET, CS_GRP_BRANCH_RELATIVE
from capstone.m68k import M68K_AM_ABSOLUTE_DATA_SHORT, M68K_AM_ABSOLUTE_DATA_LONG, M68K_AM_PCI_DISP, M68K_AM_PCI_INDEX_8_BIT_DISP, M68K_AM_PCI_INDEX_BASE_DISP

def load(argv):
    ram = open(argv[1], 'rb').read()
    cov = sorted(int(x, 16) for x in open(argv[2]).read().split() if len(x) > 1)
    return ram, cov

def main():
    ram, cov = load(sys.argv)
    out = sys.argv[3]
    import os; os.makedirs(out, exist_ok=True)
    md = Cs(CS_ARCH_M68K, CS_MODE_M68K_000); md.detail = True
    ins = {}
    # the core's hooks also fire on the extension words of an instruction: an address inside an instruction that
    # starts lower is not a start.  Walk upward and skip those.
    bad, skip_until = [], 0
    for a in cov:
        if a < skip_until: continue
        for i in md.disasm(ram[a:a + 12], a, 1):
            skip_until = a + i.size
            try:
                i.groups
            except Exception:
                continue
            ins[a] = i
        if a not in ins: bad.append(a)
    if bad: print(len(bad), 'covered addresses do not decode in this RAM (code replaced later?), first:', ' '.join('%06x' % b for b in bad[:8]))
    covset = set(ins)
    calls, jumps, refs = set(), collections.defaultdict(set), collections.defaultdict(lambda: [collections.Counter(), collections.Counter()])
    succ = collections.defaultdict(set)
    for a, i in ins.items():
        nxt = a + i.size
        is_ret = CS_GRP_RET in i.groups
        is_jump = CS_GRP_JUMP in i.groups or CS_GRP_BRANCH_RELATIVE in i.groups
        is_call = CS_GRP_CALL in i.groups
        target = None
        for op in i.operands:
            if op.type == CS_OP_IMM and (is_jump or is_call) and i.mnemonic[0] in 'bj' :
                target = op.imm & 0xffffff
            elif op.type == CS_OP_MEM:
                am = op.address_mode
                if am in (M68K_AM_ABSOLUTE_DATA_SHORT, M68K_AM_ABSOLUTE_DATA_LONG):
                    ad = op.imm & 0xffffff
                    if (is_jump or is_call) and i.mnemonic in ('jmp', 'jsr'):
                        target = ad
                    else:
                        kind = 'w' if op.mem.disp is not None and False else None
                        sz = i.mnemonic.split('.')[-1] if '.' in i.mnemonic else 'x'
                        refs[ad][0][sz] += 1
        if i.mnemonic.startswith('bsr') or i.mnemonic.startswith('jsr'):
            if target is not None: calls.add(target)
            if nxt in covset: succ[a].add(nxt)
        elif i.mnemonic.startswith('bra') or i.mnemonic.startswith('jmp'):
            if target is not None and target in covset: succ[a].add(target)
        elif i.mnemonic.startswith('b') and target is not None and is_jump:   # conditional (and dbra: handled below)
            succ[a].add(nxt) ; 
            if target in covset: succ[a].add(target)
        elif i.mnemonic.startswith('db'):
            succ[a].add(nxt)
            for op in i.operands:
                if op.type == CS_OP_IMM and op.imm in covset: succ[a].add(op.imm & 0xffffff)
        elif not is_ret and i.mnemonic not in ('rte', 'rtr', 'stop'):
            if nxt in covset: succ[a].add(nxt)
    # noise: the 68000 prefetches words after a taken branch and the hooks fire on them; such addresses form components
    # of one or two instructions that nothing calls.  Real indirect entries start a chain of several.
    parent = {a: a for a in ins}
    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]; x = parent[x]
        return x
    for a in list(succ):
        for b in succ[a]:
            if b in parent: parent[find(a)] = find(b)
    size = collections.Counter(find(a) for a in ins)
    dropped = [a for a in ins if size[find(a)] < 3 and a not in calls]
    for a in dropped:
        del ins[a]; succ.pop(a, None)
    covset = set(ins)
    print(len(dropped), 'isolated addresses dropped as prefetch noise')
    # entries: call targets that ran, and anything nothing flows into
    entries = sorted(t for t in calls if t in covset)
    flows_in = set(b for a in succ for b in succ[a])
    for a in cov:
        if a not in flows_in and a not in entries: entries.append(a)
    entries = sorted(set(entries))
    # reach (not through calls)
    owner = {}
    for e in entries:
        stack = [e]
        while stack:
            x = stack.pop()
            if x in owner and owner[x] != e and x in entries and x != e: continue
            if x in owner: continue
            owner[x] = e
            for y in succ[x]:
                if y in entries and y != e: continue
                stack.append(y)
    with open(out + '/functions.txt', 'w') as f:
        cnt = collections.Counter(owner.values())
        for e in entries: f.write('%06x %4d%s\n' % (e, cnt[e], ' called' if e in calls else ''))
    with open(out + '/listing.txt', 'w') as f:
        last = None
        for a in sorted(ins):
            i = ins[a]
            if owner.get(a) != last or (last is not None and a not in owner):
                f.write('\n')
            last = owner.get(a)
            mark = '*' if a in entries else ' '
            f.write('%06x%s %-22s %s %s\n' % (a, mark, i.bytes.hex(), i.mnemonic, i.op_str))
    with open(out + '/refs.txt', 'w') as f:
        for ad in sorted(refs):
            c = refs[ad][0]
            f.write('%06x %s\n' % (ad, ' '.join('%s:%d' % kv for kv in sorted(c.items()))))
    print(len(ins), 'instructions,', len(entries), 'entries,', len(refs), 'absolute data addresses')

main()
