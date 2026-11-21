#!/usr/bin/env python3
import pathlib
import subprocess
import sys

iso = pathlib.Path(sys.argv[1])
kernel = pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else None
if not iso.is_file() or iso.stat().st_size < 32768:
    raise SystemExit('[verify_iso] FAIL: ISO missing or implausibly small')
if kernel is not None:
    subprocess.run([sys.executable, str(pathlib.Path(__file__).with_name('verify_multiboot.py')), str(kernel)], check=True)
result = subprocess.run(['xorriso', '-indev', str(iso), '-report_el_torito', 'plain'], text=True, capture_output=True)
if result.returncode:
    sys.stderr.write(result.stderr)
    raise SystemExit('[verify_iso] FAIL: xorriso could not inspect image')
report = result.stdout + result.stderr
if 'El Torito' not in report or 'boot' not in report.lower():
    raise SystemExit('[verify_iso] FAIL: no El Torito boot entry reported')
listing = subprocess.run(['xorriso', '-indev', str(iso), '-find', '/boot', '-type', 'f', '-exec', 'lsdl'], text=True, capture_output=True)
if listing.returncode:
    sys.stderr.write(listing.stderr)
    raise SystemExit('[verify_iso] FAIL: could not list ISO boot files')
for required in ('alpos-kernel.elf', 'grub.cfg'):
    if required not in listing.stdout:
        raise SystemExit(f'[verify_iso] FAIL: {required} missing from ISO')
print('[verify_iso] PASS: El Torito boot image and GRUB/kernel files are present')
