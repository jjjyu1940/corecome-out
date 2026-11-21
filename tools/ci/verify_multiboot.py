import pathlib, struct, sys
p=pathlib.Path(sys.argv[1]); d=p.read_bytes()
if len(d)<64 or d[:4]!=b'\x7fELF' or d[4]!=2 or d[5]!=1: raise SystemExit('not little-endian ELF64')
if struct.unpack_from('<H',d,18)[0]!=62: raise SystemExit('ELF machine is not x86_64')
m=struct.pack('<I',0xE85250D6); off=d.find(m,0,min(len(d),32768))
if off<0: raise SystemExit('Multiboot2 header not found in first 32 KiB')
n=struct.unpack_from('<I',d,off+8)[0]; c=struct.unpack_from('<I',d,off+12)[0]
if n<16 or off+n>min(len(d),32768): raise SystemExit('invalid Multiboot2 header length')
if (0xE85250D6+struct.unpack_from('<I',d,off+4)[0]+n+c)&0xffffffff: raise SystemExit('invalid Multiboot2 header checksum')
print(f'[verify_multiboot] PASS: x86_64 ELF64, Multiboot2 header at offset {off}')
