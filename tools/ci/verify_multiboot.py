#!/usr/bin/env python3
import pathlib
import struct
import sys

path = pathlib.Path(sys.argv[1])
data = path.read_bytes()
if len(data) < 64 or data[:4] != b'\x7fELF' or data[4] != 2 or data[5] != 1:
    raise SystemExit('not a little-endian ELF64 image')
if struct.unpack_from('<H', data, 18)[0] != 62:
    raise SystemExit('ELF machine is not x86_64')
magic = struct.pack('<I', 0xE85250D6)
limit = min(len(data), 32768)
off = data.find(magic, 0, limit)
if off < 0:
    raise SystemExit('Multiboot2 header not found in first 32 KiB')
header_length = struct.unpack_from('<I', data, off + 8)[0]
checksum = struct.unpack_from('<I', data, off + 12)[0]
if header_length < 16 or off + header_length > limit:
    raise SystemExit('invalid Multiboot2 header length')
if (0xE85250D6 + struct.unpack_from('<I', data, off + 4)[0] + header_length + checksum) & 0xFFFFFFFF:
    raise SystemExit('invalid Multiboot2 header checksum')
print(f'[verify_multiboot] PASS: x86_64 ELF64, Multiboot2 header at file offset {off}')
