#!/usr/bin/env python3
"""Verify newc archive entries and the embedded static ELF64 executables."""
import pathlib, struct, sys
archive=pathlib.Path(sys.argv[1]).read_bytes()
off=0; entries={}
while off+110<=len(archive):
    h=archive[off:off+110]
    if h[:6]!=b'070701': raise SystemExit(f'bad newc magic at {off}')
    size=int(h[54:62],16); namesz=int(h[94:102],16)
    if not namesz or namesz>4096: raise SystemExit('invalid newc name size')
    name_start=off+110; data_start=(name_start+namesz+3)&~3
    if name_start+namesz>len(archive) or archive[name_start+namesz-1]!=0: raise SystemExit('bad newc filename')
    name=archive[name_start:name_start+namesz-1].decode()
    if data_start+size>len(archive): raise SystemExit(f'truncated newc entry: {name}')
    if name=='TRAILER!!!': break
    entries[name]=archive[data_start:data_start+size]
    off=(data_start+size+3)&~3
for name in ('bin/echo','bin/uname'):
    blob=entries.get(name)
    if blob is None: raise SystemExit(f'missing {name} in initramfs')
    if len(blob)<64 or blob[:6]!=b'\x7fELF\x02\x01': raise SystemExit(f'{name}: not ELF64 little endian')
    etype,machine=struct.unpack_from('<HH',blob,16)
    entry=struct.unpack_from('<Q',blob,24)[0]
    phoff=struct.unpack_from('<Q',blob,32)[0]
    phentsize,phnum=struct.unpack_from('<HH',blob,54)
    if etype!=2 or machine!=62 or phentsize!=56 or phnum>32: raise SystemExit(f'{name}: unsupported ELF header')
    if phoff+phentsize*phnum>len(blob): raise SystemExit(f'{name}: truncated program headers')
    executable=False
    for i in range(phnum):
        p=phoff+i*phentsize
        ptype,flags=struct.unpack_from('<II',blob,p)
        poff,pva=struct.unpack_from('<QQ',blob,p+8)
        filesz,memsz=struct.unpack_from('<QQ',blob,p+32)
        if ptype!=1: continue
        if poff+filesz>len(blob) or filesz>memsz: raise SystemExit(f'{name}: invalid PT_LOAD bounds')
        if pva<0x400000 or pva+memsz>0x600000: raise SystemExit(f'{name}: PT_LOAD outside supported user window')
        if flags&1 and pva<=entry<pva+memsz: executable=True
    if not executable: raise SystemExit(f'{name}: entry is not in executable PT_LOAD')
print('[verify_userland] PASS: newc archive contains valid /bin/echo and /bin/uname ELF64 executables')
