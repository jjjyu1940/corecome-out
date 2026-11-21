#!/usr/bin/env python3
from pathlib import Path
import sys
p=Path(sys.argv[1]); s=p.read_text()
checks=['ps2_device_write(1,0xF4)','mouse_byte','gfx_mouse_move','input_char']
missing=[x for x in checks if x not in s]
if missing: raise SystemExit('input support missing: '+', '.join(missing))
print('[verify_input] PASS: PS/2 keyboard polling and mouse packet handling present')
