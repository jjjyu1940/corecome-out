#!/usr/bin/env python3
"""Static guardrails for the current fixed-layout Ring 3 bring-up."""
import pathlib, re, subprocess, sys
root=pathlib.Path(__file__).resolve().parents[2]
start=(root/'kernel/arch/x86_64/boot/start.S').read_text()
link=(root/'kernel/arch/x86_64/boot/link.ld').read_text()
vma_match=re.search(r'KERNEL_VMA\s*=\s*(0x[0-9A-Fa-f]+)',link)
if not vma_match: raise SystemExit('KERNEL_VMA missing from linker script')
vma=int(vma_match.group(1),16)
index=(vma>>39)&0x1ff
if f'PML4_ADDR+({index}*8)' not in start:
    raise SystemExit(f'high-half PML4 mapping missing: KERNEL_VMA requires PML4[{index}]')
if 'orl $0x83,%eax' not in start or '0x183' in start:
    raise SystemExit('identity huge pages must be supervisor-only (P|RW|PS)')
for token in ('0x76000|7','0x77000|7','PDPT_ADDR+8'):
    if token not in start: raise SystemExit(f'user page-table mapping missing: {token}')
elf=pathlib.Path(sys.argv[1])
symbols=subprocess.run(['nm',str(elf)],check=True,text=True,capture_output=True).stdout
for sym in ('gdt_tss_desc','__syscall_stack_top','enter_user','isr80_stub','syscall_dispatch','user_saved_rsp'):
    if not re.search(r'\b'+re.escape(sym)+r'$',symbols,re.M): raise SystemExit(f'kernel symbol missing: {sym}')
ph=subprocess.run(['readelf','-l',str(elf)],check=True,text=True,capture_output=True).stdout
if 'RWE' in ph: raise SystemExit('unexpected RWX load segment')
print(f'[verify_ring3_layout] PASS: high-half PML4[{index}], supervisor kernel map, user PTs, TSS/syscall symbols, RX/RW segments')
