#!/usr/bin/env python3
"""Create a minimal SVR4 newc CPIO archive from path=host-file arguments."""
import os, sys

def pad4(f): f.write(b'\0' * ((-f.tell()) & 3))
def entry(f, name, data, mode, ino):
    n=name.encode()+b'\0'
    fields=[ino,mode,0,0,1,0,len(data),0,0,0,0,len(n),0]
    f.write(b'070701'+b''.join(f'{x:08x}'.encode() for x in fields))
    f.write(n); pad4(f); f.write(data); pad4(f)
def main():
    if len(sys.argv)<2: raise SystemExit('usage: mknewc.py OUTPUT path=FILE [...]')
    out,*items=sys.argv[1:]
    with open(out,'wb') as f:
        entry(f,'.',b'',0o040755,1); entry(f,'bin',b'',0o040755,2)
        ino=3
        for item in items:
            if '=' not in item: raise SystemExit('expected archive-path=host-file: '+item)
            name,src=item.split('=',1)
            if name.startswith('/') or '..' in name.split('/'): raise SystemExit('unsafe archive path: '+name)
            with open(src,'rb') as sf: data=sf.read()
            entry(f,name,data,0o100755,ino); ino+=1
        entry(f,'TRAILER!!!',b'',0,ino)
    print(f'[newc] wrote {out} ({os.path.getsize(out)} bytes)')
if __name__=='__main__': main()
