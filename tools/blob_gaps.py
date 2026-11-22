#!/usr/bin/env python3
"""The words of a level's blob that no member names but that the level's animation scripts and pointers reach (the scripts themselves, the pictures their steps show):
the words of the padding that hold them become u16 members of native/src/blobs/levelN.blob, so the loader converts them.

    tools/blob_gaps.py N=captures/levelN.ram ...        (RAM dumps of the original with level N loaded: host/ramdump.lua)

The stretch of padding that a picture lies in is taken whole (pictures sit side by side), so is the stretch of the tiles from the first to the one the map and the
cell tables use last; of the others only the script words."""
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LO = 0x54E40
HI = 0x80000
MARK = '; --- words reached by the scripts and pointers (tools/blob_gaps.py) ---\n'
JUMP = {0xE2A, 0x2C82}
SRC = (ROOT / 'native/src/guest_blob.h').read_text()


def table(name, lv):
    m = re.search(r'blob_%s_%s\[\] = \{(.*?)\n\};' % (name, lv), SRC, re.S)
    return [tuple(int(x, 16) if x.startswith('0x') else int(x) for x in re.findall(r'0x[0-9A-F]+|\d+', row)) for row in m.group(1).splitlines() if row.strip().startswith('{')]


def run(level, ram_path):
    ram = Path(ram_path).read_bytes()
    lv = {1: '1', 2: '2', 3: '3', 4: '4', 5: '5'}[level]
    pads = [(LO + o, LO + o + z) for o, z in table('pads', lv)]
    long = lambda a: struct.unpack('>I', ram[a:a + 4])[0]
    starts = [LO + o for o, _ in table('scripts', lv)]
    for o, c in table('script_pointers', lv):
        starts += [long(LO + o + 4 * k) for k in range(c)]
    seen, words, pics = set(), set(), set()

    def walk(a):
        while a not in seen and LO <= a < HI:
            seen.add(a)
            if long(a):
                if long(a) >= HI or long(a) & 1 or struct.unpack('>H', ram[a + 4:a + 6])[0] > 0x400:
                    return False
                words.update(range(a, a + 6))
                pics.add(long(a))
                a += 6
                continue
            code = long(a + 4)
            words.update(range(a, a + 8))
            a += 8
            if code in JUMP:
                words.update(range(a, a + 4))
                target = long(a)
                a += 4
                return walk(target)
            return code in (0xE30, 0x2C92)
        return True

    for s in starts:
        before = (set(words), set(pics))
        if not walk(s):
            words, pics = before
    word = lambda a: struct.unpack('>H', ram[a:a + 2])[0]
    spec_text = (ROOT / f'native/src/blobs/level{level}.blob').read_text()
    for m in re.finditer(r'^([0-9a-f]+) \w+ ptr:PathCmd(?: (\d+))?$', spec_text, re.M):
        for k in range(int(m.group(2) or 1)):
            start = long(int(m.group(1), 16) + 4 * k)
            if not LO <= start < HI:
                continue
            todo, ops = [start], set()
            while todo:
                pos = todo.pop()
                if pos in ops or not LO <= pos < HI:
                    continue
                ops.add(pos)
                op = word(pos)
                size = {0: 1, 2: 5, 4: 2, 6: 9, 8: 2, 10: 3}.get(op)
                if size is None:
                    break
                words.update(range(pos, pos + 2 * size))
                if op == 6:
                    todo += [start + o for o in (word(pos + 2 + 2 * i) for i in range(8)) if o < 0x8000]
                if op == 8:
                    todo.append(start + word(pos + 2))
                if op not in (0, 8):
                    todo.append(pos + 2 * size)
    tiles = long(0x54E08)
    spec = (ROOT / f'native/src/blobs/level{level}.blob').read_text()
    src = int(re.search(r'^([0-9a-f]+) data_src u16 (\d+)', spec, re.M).group(1), 16)
    count = int(re.search(r'^([0-9a-f]+) data_src u16 (\d+)', spec, re.M).group(2))
    top = max((struct.unpack('>H', ram[src + 2 * i:src + 2 * i + 2])[0] & 0x7FFF) for i in range(count))
    tile_area = (tiles, tiles + (top + 1) * 16)
    chunks = []
    for lo, hi in pads:
        a, b = max((lo + 1) & ~1, tile_area[0]), hi & ~1
        if a < b and (lo + 1) & ~1 < tile_area[1]:
            chunks.append((a, b))
    for lo, hi in pads:
        a, b = (lo + 1) & ~1, hi & ~1
        if any(a <= p < b or a <= p - 8 < b for p in pics):
            chunks.append((a, b))
            continue
        run_start = None
        for w in range(a, b + 2, 2):
            if w < b and w in words:
                run_start = w if run_start is None else run_start
            elif run_start is not None:
                chunks.append((run_start, w))
                run_start = None
    merged = []
    for a, b in sorted(set(chunks)):
        if merged and a <= merged[-1][1]:
            merged[-1] = (merged[-1][0], max(b, merged[-1][1]))
        else:
            merged.append((a, b))
    return merged


def main():
    for arg in sys.argv[1:]:
        n, path = arg.split('=', 1)
        chunks = run(int(n), path)
        spec = ROOT / f'native/src/blobs/level{n}.blob'
        text = spec.read_text()
        text = text.split(MARK)[0]
        text += MARK + ''.join(f'{a:x} words_{a:x} u16 {(b - a) // 2}\n' for a, b in chunks)
        spec.write_text(text)
        print(f'level {n}: {len(chunks)} stretches, {sum(b - a for a, b in chunks)} bytes')


main()
