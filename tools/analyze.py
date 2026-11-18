"""Functions, calls and data references of the executed code, from an exact trace (host/trace_steps.lua).

  uv run tools/analyze.py TRACE.pcs OUTDIR --from FRAME --ram LO:HI:FILE [--ram LO:HI:FILE ...]

TRACE.pcs: `pc count first_frame last_frame succ,succ,...` for every instruction that ran (host/trace_steps.lua).  Only instructions that
first ran at FRAME or later are analysed (the code of one program: the low RAM is overlaid by the intro, the game, the level blob).
Each --ram gives the dump to decode the addresses LO..HI (hex) from.  Writes
  OUTDIR/functions.txt  ENTRY kind size callers     kind: call, irq (reached by something that is not a branch), start
  OUTDIR/listing.txt    the instructions by function: address, bytes, text, and the count
  OUTDIR/refs.txt       absolute addresses the instructions read or write (data/variables), with the access sizes
  OUTDIR/unresolved.txt instructions no dump decodes consistently
"""
import sys, os, re, collections
from capstone import Cs, CS_ARCH_M68K, CS_MODE_M68K_000, CS_OP_IMM, CS_OP_MEM, CS_GRP_JUMP, CS_GRP_CALL, CS_GRP_RET, CS_GRP_BRANCH_RELATIVE
from capstone.m68k import M68K_AM_ABSOLUTE_DATA_SHORT, M68K_AM_ABSOLUTE_DATA_LONG

def read_trace(path):
    trace = {}
    for line in open(path):
        p = line.split()
        if not p: continue
        trace[int(p[0], 16)] = (int(p[1]), {int(x, 16) for x in p[4].split(',')} if len(p) > 4 else set(), int(p[2]), int(p[3]))
    return trace

def classify(i):
    m = i.mnemonic
    if m.startswith(('bsr', 'jsr')): return 'call'
    if m.startswith(('rts', 'rte', 'rtr')): return 'ret'
    if m.startswith(('bra', 'jmp')): return 'jump'
    if m.startswith('db'): return 'cond'
    if m.startswith('b') and m[:3] not in ('bch', 'bcl', 'bse', 'bts') and not m.startswith('bfi') and CS_GRP_JUMP in i.groups: return 'cond'
    if m.startswith(('trap', 'stop', 'reset', 'illegal')): return 'ret'
    return 'seq'

def static_targets(i):
    out = []
    for op in i.operands:
        if op.type == CS_OP_IMM: out.append(op.imm & 0xffffff)
        elif op.type == CS_OP_MEM and op.address_mode in (M68K_AM_ABSOLUTE_DATA_SHORT, M68K_AM_ABSOLUTE_DATA_LONG):
            out.append(op.imm & 0xffffff)
    return out

def main():
    argv = sys.argv[1:]
    trace_path, out = argv[0], argv[1]
    from_frame, rams = 0, []
    i = 2
    while i < len(argv):
        if argv[i] == '--from': from_frame = int(argv[i + 1]); i += 2
        elif argv[i] == '--ram':
            lo, hi, f = argv[i + 1].split(':', 2); rams.append((int(lo, 16), int(hi, 16), open(f, 'rb').read())); i += 2
        else: raise SystemExit('bad argument ' + argv[i])
    os.makedirs(out, exist_ok=True)
    trace = {pc: v for pc, v in read_trace(trace_path).items() if v[2] >= from_frame}
    md = Cs(CS_ARCH_M68K, CS_MODE_M68K_000); md.detail = True
    ins, unresolved = {}, []
    for pc in sorted(trace):
        got = None
        for lo, hi, ram in rams:
            if lo <= pc < hi:
                for i in md.disasm(ram[pc:pc + 12], pc, 1):
                    try: i.groups
                    except Exception: continue
                    got = i
        if got is None: unresolved.append(pc)
        else: ins[pc] = got
    kinds = {pc: classify(i) for pc, i in ins.items()}
    entries = {}   # entry -> kind
    callers = collections.defaultdict(set)
    body_succ = collections.defaultdict(set)
    for pc, i in ins.items():
        count, succ = trace[pc][:2]
        nxt, k = pc + i.size, kinds[pc]
        targets = set(static_targets(i)) if k in ('jump', 'cond', 'call') else set()
        for s in succ:
            if k == 'call' and s != nxt:
                entries.setdefault(s, 'call'); callers[s].add(pc)
                body_succ[pc].add(nxt) if nxt in trace else None      # the code after the call runs after the return
            elif k == 'ret':
                pass
            elif k == 'seq' and s != nxt:
                entries.setdefault(s, 'irq')            # an interrupt (or a trap) took the CPU somewhere
            elif k in ('jump', 'cond') and s not in targets and s != nxt:
                entries.setdefault(s, 'indirect')       # jmp (an), jump tables
                body_succ[pc].add(s)
            else:
                body_succ[pc].add(s)
    # indirect jumps are code-flow inside a function; calls through registers are in 'call' above.  The very first instruction is the start.
    first = min(trace) if trace else 0
    owner, order = {}, sorted(entries)
    for e in order:
        stack = [e]
        while stack:
            x = stack.pop()
            if x in owner or x not in ins: continue
            if x in entries and x != e and entries[x] == 'call': continue
            owner[x] = e
            stack.extend(body_succ[x])
    # instructions nothing owns (reached only by the entry we did not see): their own functions
    for pc in sorted(ins):
        if pc not in owner:
            entries.setdefault(pc, 'start'); owner[pc] = pc
            stack = [pc]
            while stack:
                x = stack.pop()
                for y in body_succ[x]:
                    if y in ins and y not in owner and not (y in entries and entries[y] == 'call'): owner[y] = pc; stack.append(y)
    size = collections.Counter(owner.values())
    with open(out + '/functions.txt', 'w') as f:
        for e in sorted(entries):
            if size[e]: f.write('%06x %-8s %4d  %s\n' % (e, entries[e], size[e], ' '.join('%06x' % c for c in sorted(callers[e])[:6])))
    with open(out + '/listing.txt', 'w') as f:
        last = None
        for pc in sorted(ins):
            if owner.get(pc) != last: f.write('\n; ---- %06x %s\n' % (owner[pc], entries.get(owner[pc], ''))); last = owner[pc]
            i = ins[pc]
            f.write('%06x  %-22s %-8s %-28s %d\n' % (pc, i.bytes.hex(), i.mnemonic, i.op_str, trace[pc][0]))
    refs = collections.defaultdict(collections.Counter)
    for pc, i in ins.items():
        if kinds[pc] in ('call', 'jump', 'cond'): continue
        for op in i.operands:
            if op.type == CS_OP_MEM and op.address_mode in (M68K_AM_ABSOLUTE_DATA_SHORT, M68K_AM_ABSOLUTE_DATA_LONG):
                sz = i.mnemonic.split('.')[-1] if '.' in i.mnemonic else 'x'
                refs[op.imm & 0xffffff][sz] += 1
    with open(out + '/refs.txt', 'w') as f:
        for a in sorted(refs): f.write('%06x %s\n' % (a, ' '.join('%s:%d' % kv for kv in sorted(refs[a].items()))))
    open(out + '/unresolved.txt', 'w').write('\n'.join('%06x' % a for a in unresolved) + '\n')
    n_funcs = sum(1 for e in entries if size[e])
    print('%d instructions (%d unresolved), %d functions (%d call, %d irq, %d indirect, %d start), %d data addresses' % (
        len(ins), len(unresolved), n_funcs, *[sum(1 for e in entries if size[e] and entries[e] == k) for k in ('call', 'irq', 'indirect', 'start')], len(refs)))

main()
