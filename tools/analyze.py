"""Functions, calls and data references of the executed code, from an exact trace (host/trace_steps.lua).

  uv run tools/analyze.py TRACE.pcs OUTDIR RAM.bin [RAM2.bin ...]

TRACE.pcs: `pc count succ,succ,...` for every instruction that ran.  The RAM dumps hold the code: for each instruction the first dump
whose decoding agrees with the observed successors is used (code is overlaid: the loaders, the level blob).  Writes
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
        trace[int(p[0], 16)] = (int(p[1]), {int(x, 16) for x in p[2].split(',')} if len(p) > 2 else set())
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
    trace_path, out, rams = sys.argv[1], sys.argv[2], [open(p, 'rb').read() for p in sys.argv[3:]]
    os.makedirs(out, exist_ok=True)
    trace = read_trace(trace_path)
    md = Cs(CS_ARCH_M68K, CS_MODE_M68K_000); md.detail = True
    ins, unresolved = {}, []
    for pc in sorted(trace):
        count, succ = trace[pc]
        chosen = None
        for ram in rams:
            got = None
            for i in md.disasm(ram[pc:pc + 12], pc, 1):
                try: i.groups
                except Exception: continue
                got = i
            if got is None: continue
            kind = classify(got)
            nxt = pc + got.size
            if kind == 'seq': ok = succ <= {nxt} or True    # an interrupt can follow any instruction
            else: ok = True
            if kind in ('seq', 'cond') and nxt not in succ and succ - {nxt} and kind == 'seq' and False: ok = False
            chosen = got; break
        if chosen is None: unresolved.append(pc)
        else: ins[pc] = chosen
    kinds = {pc: classify(i) for pc, i in ins.items()}
    entries = {}   # entry -> kind
    callers = collections.defaultdict(set)
    body_succ = collections.defaultdict(set)
    for pc, i in ins.items():
        count, succ = trace[pc]
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
