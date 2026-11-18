# /// script
# dependencies = ["capstone"]
# ///
"""Disassemble a range of a binary image as 68000 (capstone).  usage: uv run tools/m68kdis.py FILE START_HEX LEN_HEX [BASE_HEX]"""
import sys
from capstone import Cs, CS_ARCH_M68K, CS_MODE_M68K_000
data = open(sys.argv[1], 'rb').read()
start, n = int(sys.argv[2], 16), int(sys.argv[3], 16)
base = int(sys.argv[4], 16) if len(sys.argv) > 4 else 0
md = Cs(CS_ARCH_M68K, CS_MODE_M68K_000)
for i in md.disasm(data[start:start + n], base + start):
    print('%06x  %-20s %s %s' % (i.address, i.bytes.hex(), i.mnemonic, i.op_str))
