#!/usr/bin/env python3
import pathlib, struct, sys
p=pathlib.Path(sys.argv[1]); d=p.read_bytes()
o=d.find(struct.pack('<I',0xE85250D6),0,min(len(d),32768))
if o<0: raise SystemExit('Multiboot2 header missing')
magic,arch,n,checksum=struct.unpack_from('<4I',d,o)
if n<16 or (magic+arch+n+checksum)&0xffffffff: raise SystemExit('bad Multiboot2 header/checksum')
end=o+n
pos=o+16; found=False
while pos+8<=end:
    typ,flags,size=struct.unpack_from('<HHI',d,pos)
    if size<8 or pos+size>end: raise SystemExit('invalid Multiboot2 tag')
    if typ==5:
        if size!=20: raise SystemExit('unexpected framebuffer request tag size')
        w,h,bpp=struct.unpack_from('<III',d,pos+8)
        if (w,h,bpp)!=(1024,768,32): raise SystemExit('wrong framebuffer request')
        found=True
    if typ==0: break
    pos=(pos+size+7)&~7
if not found: raise SystemExit('framebuffer request tag not found')
print('[verify_gui_header] PASS: Multiboot2 requests 1024x768x32 framebuffer')
