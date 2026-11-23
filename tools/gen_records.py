"""Generate the SDL port's loader tables from the game's C: how a record from the disk image becomes a record of the host.

  python3 tools/gen_records.py

Reads the record types (`struct NAME { ... };`) of native/src/types_*.h and the functions of native/src/*.c that carry `/* orig $ADDR */`, writes
  native/src/sdl/records.c      guest_record_size(type), guest_swap_record(type, record): the 16- and 32-bit fields of a record in the byte order
                                of the host, the data pointers (addresses of the original's memory) as pointers into the game's memory block, the
                                function pointers (addresses of the original's code) as pointers to the C functions that have that `orig $ADDR`
  native/src/code_table.c       guest_code(address): the C function for an address of the original's code (0 if none)
Layout as the 32-bit host build gets it: -fpack-struct=2 (fields at most 2-byte aligned), pointers of 4 bytes.
"""
import glob, os, re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'native', 'src')
SCALARS = {'uint8_t': 1, 'int8_t': 1, 'char': 1, 'uint16_t': 2, 'int16_t': 2, 'uint32_t': 4, 'int32_t': 4, 'irq_fn': 4, 'unsigned': 4, 'int': 4}

def strip_comments(t):
    t = re.sub(r'/\*.*?\*/', '', t, flags=re.S)
    return re.sub(r'//[^\n]*', '', t)

def typedef_kinds():
    """typedef'd function pointers (kind fn) and scalars of the headers: name -> kind or size"""
    out = {}
    for f in glob.glob(os.path.join(SRC, '*.h')):
        text = strip_comments(open(f).read())
        for m in re.finditer(r'typedef\s+[\w\s\*]+?\(\s*\*\s*(\w+)\s*\)\s*\([^;]*\)\s*;', text):
            out[m.group(1)] = 'fn'
        for m in re.finditer(r'typedef\s+(?:const\s+)?(u?int(?:8|16|32)_t)\s+(\w+)\s*;', text):
            out[m.group(2)] = m.group(1)
    return out

TYPEDEFS = {}

def flatten_unions(t):
    """an anonymous `union { A; B; ... };` has the layout of its first member (the members are the same size): keep that one"""
    while True:
        m = re.search(r'\bunion\s*\{', t)
        if not m: return t
        depth, i = 1, m.end()
        while depth:
            depth += (t[i] == '{') - (t[i] == '}')
            i += 1
        body = t[m.end():i - 1]
        end = t.index(';', i) + 1
        first = re.match(r'\s*struct\s*\{', body)
        if first:
            d, j = 1, first.end()
            while d:
                d += (body[j] == '{') - (body[j] == '}')
                j += 1
            keep = body[first.end():j - 1]
        else:
            keep = body[:body.index(';') + 1]
        t = t[:m.start()] + keep + t[end:]

def parse_structs():
    structs = {}
    for f in [os.path.join(SRC, n + '.h') for n in ('objects', 'weapons', 'sound', 'title', 'types')]:
        text = flatten_unions(strip_comments(open(f).read()))
        for m in re.finditer(r'struct\s+(\w+)\s*\{(.*?)\}\s*(?:\w+\s*)?;', text, re.S):
            fields = []
            for stmt in m.group(2).split(';'):
                stmt = ' '.join(stmt.split())
                if not stmt: continue
                fields.append(stmt)
            structs[m.group(1)] = fields
    return structs

def layout(name, structs, memo):
    """-> (size, align, [(offset, kind, count, size)]) kind: 8 16 32 ptr fn rec:NAME"""
    if name in memo: return memo[name]
    off, items, align = 0, [], 1
    for stmt in structs[name]:
        m = re.match(r'void\s*\(\s*\*\s*(\w+)\s*\)\s*\(.*\)$', stmt)
        if m:   # a function pointer
            decls = [(m.group(1), 'fn', 4, 1)]
        else:
            m = re.match(r'(?:const\s+|volatile\s+)*(?:struct\s+)?([\w]+)\s*(\*?)\s*(.*)$', stmt)
            if not m: raise SystemExit('cannot parse field: %r in struct %s' % (stmt, name))
            base, star, rest = m.groups()
            decls = []
            for d in rest.split(','):
                d = d.strip()
                dm = re.match(r'(\*?)\s*(\w+)\s*(?:\[\s*(\w*)\s*\])?(?:\s*:\s*\d+)?$', d)
                if not dm: raise SystemExit('cannot parse declarator %r in struct %s' % (d, name))
                ptr = bool(star or dm.group(1))
                if base in TYPEDEFS and TYPEDEFS[base] == 'fn' and not ptr:
                    decls.append((dm.group(2), 'fn', 4, int(dm.group(3), 0) if dm.group(3) else 1)); continue
                if base in TYPEDEFS and TYPEDEFS[base] in SCALARS: base = TYPEDEFS[base]
                count = int(dm.group(3), 0) if dm.group(3) else (0 if '[' in d else 1)   # (a flexible array member has no elements of its own)
                if ptr: decls.append((dm.group(2), 'ptrv' if base == 'void' else 'ptr', 4, count))   # (a void pointer may be code or data)
                elif base in SCALARS:
                    decls.append((dm.group(2), {1: '8', 2: '16', 4: '32'}[SCALARS[base]] if base != 'irq_fn' else 'fn', SCALARS[base], count))
                elif base in structs:
                    sz, al, _ = layout(base, structs, memo)
                    decls.append((dm.group(2), 'rec:' + base, sz, count))
                else: raise SystemExit('unknown type %r in struct %s' % (base, name))
        for fname, kind, sz, count in decls:
            al = 1 if sz == 1 else 2
            if kind.startswith('rec:'): al = layout(kind[4:], structs, memo)[1]
            off = (off + al - 1) // al * al
            items.append((off, kind, count, sz))
            off += sz * count
            align = max(align, al)
    size = (off + align - 1) // align * align
    memo[name] = (size, align, items)
    return memo[name]

def code_functions(tag='orig'):
    funcs = {}
    for f in sorted(glob.glob(os.path.join(SRC, '*.c'))) + sorted(glob.glob(os.path.join(SRC, 'sdl', '*.c'))):   # (amiga/ is the Amiga build's: the port has its own high-level disk, drawing and sound)
        text = open(f).read()
        for m in re.finditer(tag + r'\s+((?:\$[0-9A-Fa-f]+(?:\s*(?:,|and)\s*)?)+)(?:(?!\*/).)*?\*/\s*\n(static\s+)?[\w\s\*]+?\b(\w+)\s*\(', text, re.S):
            if m.group(2): continue   # a static function cannot be named from another file
            for a in re.findall(r'\$([0-9A-Fa-f]+)', m.group(1)):
                funcs.setdefault(int(a, 16), m.group(3))
    return funcs

def main():
    TYPEDEFS.update(typedef_kinds())
    structs = parse_structs()
    memo = {}
    out = ['/* Generated by tools/gen_records.py: do not edit. */', '#include <stdint.h>', '#include <string.h>', '#include "game.h"', '#include "host.h"', '',
           'extern void *guest_code(uint32_t address);',
           'static void s16(uint8_t *p) { uint16_t v = (uint16_t)(p[0] << 8 | p[1]); memcpy(p, &v, 2); }',
           'static void s32(uint8_t *p) { uint32_t v = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; memcpy(p, &v, 4); }',
           'static void sptr(uint8_t *p) { uint32_t v = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; v = v ? (uint32_t)(uintptr_t)guest_ptr(v) : 0; memcpy(p, &v, 4); }',
           'extern uint32_t guest_code_address(const void *function);',
           'extern void *guest_code_stub(uint32_t address);   /* guest.c: guest_code(), or a function that does nothing for level code with no C */',
           'static void u16(uint8_t *p) { uint16_t v; memcpy(&v, p, 2); p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }',
           'static void u32(uint8_t *p) { uint32_t v; memcpy(&v, p, 4); p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }',
           'static void uptr(uint8_t *p) { uint32_t v; memcpy(&v, p, 4); v = v ? guest_address((void *)(uintptr_t)v) : 0; p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }',
           'static void sptrv(uint8_t *p) { uint32_t v = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; void *f = v ? guest_code(v) : 0; v = v ? (uint32_t)(uintptr_t)(f ? f : guest_ptr(v)) : 0; memcpy(p, &v, 4); }',
           'static void uptrv(uint8_t *p) { uint32_t v; memcpy(&v, p, 4); if (v) { uint32_t base = (uint32_t)(uintptr_t)guest_ptr(0); v = v - base < 0x80000u ? v - base : guest_code_address((void *)(uintptr_t)v); } p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }',
           'static void ufn(uint8_t *p) { uint32_t v; memcpy(&v, p, 4); v = v ? guest_code_address((void *)(uintptr_t)v) : 0; p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }',
           'static void sfn(uint8_t *p) { uint32_t v = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; v = v ? (uint32_t)(uintptr_t)guest_code_stub(v) : 0; memcpy(p, &v, 4); }', '']
    names = sorted(structs)
    for n in names: layout(n, structs, memo)
    out.append('unsigned guest_record_size(const char *type)\n{')
    for n in names:
        out.append('    if (!strcmp(type, "%s")) return %d;' % (n, memo[n][0]))
    out.append('    return 0;\n}\n')
    for n in names:
        size, align, items = memo[n]
        out.append('static void swap_%s(uint8_t *r)\n{\n    (void)r;' % n)
        for off, kind, count, sz in items:
            if kind == '8': continue
            for i in range(count):
                o = off + i * sz
                if kind == '16': out.append('    s16(r + %d);' % o)
                elif kind == '32': out.append('    s32(r + %d);' % o)
                elif kind == 'ptr': out.append('    sptr(r + %d);' % o)
                elif kind == 'ptrv': out.append('    sptrv(r + %d);' % o)
                elif kind == 'fn': out.append('    sfn(r + %d);' % o)
                else: out.append('    swap_%s(r + %d);' % (kind[4:], o))
        out.append('}\n')
        out.append('static void unswap_%s(uint8_t *r)\n{\n    (void)r;' % n)
        for off, kind, count, sz in items:
            if kind == '8': continue
            for i in range(count):
                o = off + i * sz
                if kind == '16': out.append('    u16(r + %d);' % o)
                elif kind == '32': out.append('    u32(r + %d);' % o)
                elif kind == 'ptr': out.append('    uptr(r + %d);' % o)
                elif kind == 'ptrv': out.append('    uptrv(r + %d);' % o)
                elif kind == 'fn': out.append('    ufn(r + %d);' % o)
                else: out.append('    unswap_%s(r + %d);' % (kind[4:], o))
        out.append('}\n')
    # swap_X may call swap_Y defined later: declare first
    decls = ['static void swap_%s(uint8_t *r);' % n for n in names] + ['static void unswap_%s(uint8_t *r);' % n for n in names]
    out = out[:out.index('unsigned guest_record_size(const char *type)\n{')] + decls + [''] + out[out.index('unsigned guest_record_size(const char *type)\n{'):]
    out.append('void guest_swap_record(const char *type, void *record)\n{')
    for n in names:
        out.append('    if (!strcmp(type, "%s")) { swap_%s(record); return; }' % (n, n))
    out.append('}')
    out.append('\nvoid guest_unswap_record(const char *type, void *record)\n{')
    for n in names:
        out.append('    if (!strcmp(type, "%s")) { unswap_%s(record); return; }' % (n, n))
    out.append('}')
    os.makedirs(os.path.join(SRC, 'sdl'), exist_ok=True)
    open(os.path.join(SRC, 'sdl', 'records.c'), 'w').write('\n'.join(out) + '\n')
    funcs = code_functions()
    funcs2 = code_functions('level2')   # the blob of level 2 ($54E00 on) has its own functions at the addresses of level 1's
    funcs3 = code_functions('level3')   # and so has level 3's
    funcs4 = code_functions('level4')   # and level 4's
    funcs5 = code_functions('level5')   # and level 5's
    tab = ['/* Generated by tools/gen_records.py: do not edit.  The C functions by the address of the original code they are. */', '#include <stdint.h>', '#include <stddef.h>', '']
    tab += ['extern void %s();' % n for n in sorted(set(funcs.values()) | set(funcs2.values()) | set(funcs3.values()) | set(funcs4.values()) | set(funcs5.values()))]
    tab += ['', 'static const struct { uint32_t address; void *function; } code_table[] = {']
    tab += ['    { 0x%x, (void *)%s },' % (a, funcs[a]) for a in sorted(funcs)]
    tab += ['};', '', 'static const struct { uint32_t address; void *function; } code_table2[] = {']
    tab += ['    { 0x%x, (void *)%s },' % (a, funcs2[a]) for a in sorted(funcs2)]
    tab += ['};', '', 'static const struct { uint32_t address; void *function; } code_table3[] = {']
    tab += ['    { 0x%x, (void *)%s },' % (a, funcs3[a]) for a in sorted(funcs3)]
    tab += ['};', '', 'static const struct { uint32_t address; void *function; } code_table4[] = {']
    tab += ['    { 0x%x, (void *)%s },' % (a, funcs4[a]) for a in sorted(funcs4)]
    tab += ['};', '', 'static const struct { uint32_t address; void *function; } code_table5[] = {']
    tab += ['    { 0x%x, (void *)%s },' % (a, funcs5[a]) for a in sorted(funcs5)]
    tab += ['};', '', '/* every address of the original\'s code the C function stands for (the dev build hooks functions by those addresses); returns how many */',
            'unsigned guest_code_entries(const void *function, uint32_t *out, unsigned max)', '{', '    unsigned n = 0;', '    for (unsigned i = 0; i < sizeof code_table / sizeof code_table[0] && n < max; i++)',
            '        if (code_table[i].function == function)', '            out[n++] = code_table[i].address;', '    return n;', '}', '', 'uint32_t guest_code_address(const void *function)', '{', '    for (unsigned i = 0; i < sizeof code_table / sizeof code_table[0]; i++)', '        if (code_table[i].function == function)', '            return code_table[i].address;', '    return 0;', '}', '', 'void *guest_code(uint32_t address)', '{', '    unsigned lo = 0, hi = sizeof code_table / sizeof code_table[0];', '    while (lo < hi) {', '        unsigned mid = (lo + hi) / 2;',
            '        if (code_table[mid].address < address) lo = mid + 1; else hi = mid;', '    }',
            '    return lo < sizeof code_table / sizeof code_table[0] && code_table[lo].address == address ? code_table[lo].function : 0;', '}']
    # (in both builds; a call of code with no C: in the 68000 build the original's code at that address, in the port nothing)
    tab = [l.replace("    return lo < sizeof code_table / sizeof code_table[0] && code_table[lo].address == address ? code_table[lo].function : 0;",
                     "    if (lo < sizeof code_table / sizeof code_table[0] && code_table[lo].address == address)\n        return code_table[lo].function;\n#ifdef GAME_SDL\n    return 0;\n#else\n    return (void *)(uintptr_t)address;   /* the original's code */\n#endif") for l in tab]
    text = '\n'.join(tab) + '\n'
    text = text.replace('    return n;\n}', '    for (unsigned i = 0; i < sizeof code_table2 / sizeof code_table2[0] && n < max; i++)\n        if (code_table2[i].function == function)\n            out[n++] = code_table2[i].address;\n    for (unsigned i = 0; i < sizeof code_table3 / sizeof code_table3[0] && n < max; i++)\n        if (code_table3[i].function == function)\n            out[n++] = code_table3[i].address;\n    for (unsigned i = 0; i < sizeof code_table4 / sizeof code_table4[0] && n < max; i++)\n        if (code_table4[i].function == function)\n            out[n++] = code_table4[i].address;\n    for (unsigned i = 0; i < sizeof code_table5 / sizeof code_table5[0] && n < max; i++)\n        if (code_table5[i].function == function)\n            out[n++] = code_table5[i].address;\n    return n;\n}', 1)
    text = text.replace('    return 0;\n}\n\nvoid *guest_code', '    for (unsigned i = 0; i < sizeof code_table2 / sizeof code_table2[0]; i++)\n        if (code_table2[i].function == function)\n            return code_table2[i].address;\n    for (unsigned i = 0; i < sizeof code_table3 / sizeof code_table3[0]; i++)\n        if (code_table3[i].function == function)\n            return code_table3[i].address;\n    for (unsigned i = 0; i < sizeof code_table4 / sizeof code_table4[0]; i++)\n        if (code_table4[i].function == function)\n            return code_table4[i].address;\n    for (unsigned i = 0; i < sizeof code_table5 / sizeof code_table5[0]; i++)\n        if (code_table5[i].function == function)\n            return code_table5[i].address;\n    return 0;\n}\n\nvoid *guest_code', 1)
    text = text.replace('void *guest_code(uint32_t address)\n{\n', 'void *guest_code(uint32_t address)\n{\n#ifdef GAME_SDL\n    if (address >= 0x54e00 && address < 0x74000 && guest_level_table() == 2) {   /* the blob of level 2 is loaded: its functions are at the level 1 addresses */\n        for (unsigned i = 0; i < sizeof code_table2 / sizeof code_table2[0]; i++)\n            if (code_table2[i].address == address)\n                return code_table2[i].function;\n        return 0;\n    }\n    if (address >= 0x54e00 && address < 0x74000 && guest_level_table() == 3) {   /* the blob of level 3 is loaded */\n        for (unsigned i = 0; i < sizeof code_table3 / sizeof code_table3[0]; i++)\n            if (code_table3[i].address == address)\n                return code_table3[i].function;\n        return 0;\n    }\n    if (address >= 0x54e00 && address < 0x74000 && guest_level_table() == 4) {   /* the blob of level 4 is loaded */\n        for (unsigned i = 0; i < sizeof code_table4 / sizeof code_table4[0]; i++)\n            if (code_table4[i].address == address)\n                return code_table4[i].function;\n        return 0;\n    }\n    if (address >= 0x54e00 && address < 0x74000 && guest_level_table() == 5) {   /* the blob of level 5 is loaded */\n        for (unsigned i = 0; i < sizeof code_table5 / sizeof code_table5[0]; i++)\n            if (code_table5[i].address == address)\n                return code_table5[i].function;\n        return 0;\n    }\n#endif\n', 1)
    text = text.replace('static const struct { uint32_t address; void *function; } code_table[] = {', 'int guest_level_table(void);   /* guest.c: the level (2, 3) while the blob loaded is that level\'s, else 0 */\nstatic const struct { uint32_t address; void *function; } code_table[] = {', 1)
    open(os.path.join(SRC, 'code_table.c'), 'w').write(text)
    old = os.path.join(SRC, 'sdl', 'code_table.c')
    if os.path.exists(old): os.remove(old)
    print(len(names), 'record types,', len(funcs), 'functions in the code table')

if __name__ == '__main__':
    main()
