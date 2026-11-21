#!/usr/bin/env python3
import pathlib, subprocess, sys
iso=pathlib.Path(sys.argv[1]); kernel=pathlib.Path(sys.argv[2]) if len(sys.argv)>2 else None
if not iso.is_file() or iso.stat().st_size<32768: raise SystemExit('[verify_iso] FAIL: ISO missing or too small')
if kernel: subprocess.run([sys.executable,str(pathlib.Path(__file__).with_name('verify_multiboot.py')),str(kernel)],check=True)
r=subprocess.run(['xorriso','-indev',str(iso),'-report_el_torito','plain'],text=True,capture_output=True)
if r.returncode: raise SystemExit('[verify_iso] FAIL: xorriso inspection failed')
if 'El Torito' not in r.stdout+r.stderr: raise SystemExit('[verify_iso] FAIL: El Torito boot entry missing')
l=subprocess.run(['xorriso','-indev',str(iso),'-find','/boot','-type','f','-exec','lsdl'],text=True,capture_output=True)
if l.returncode: raise SystemExit('[verify_iso] FAIL: cannot list /boot')
for item in ('alpos-kernel.elf','initramfs.cpio','grub.cfg'):
    if item not in l.stdout: raise SystemExit(f'[verify_iso] FAIL: {item} missing')
print('[verify_iso] PASS: El Torito, kernel, initramfs and GRUB config found')
