#!/usr/bin/env python3
"""Wrap a raw MIPS binary linked at 0x80010000 into a PS-X EXE."""
import struct
import sys

raw = open(sys.argv[1], 'rb').read()
raw += b'\0' * (-len(raw) % 0x800)
load = 0x80010000
hdr = bytearray(0x800)
hdr[0:8] = b'PS-X EXE'
struct.pack_into('<I', hdr, 0x10, load)        # pc0
struct.pack_into('<I', hdr, 0x14, 0)           # gp0
struct.pack_into('<I', hdr, 0x18, load)        # t_addr
struct.pack_into('<I', hdr, 0x1c, len(raw))    # t_size
struct.pack_into('<I', hdr, 0x30, 0x801ffff0)  # s_addr
struct.pack_into('<I', hdr, 0x34, 0)           # s_size
marker = b'Sony Computer Entertainment Inc. for North America area'
hdr[0x4c:0x4c + len(marker)] = marker
open(sys.argv[2], 'wb').write(bytes(hdr) + raw)
