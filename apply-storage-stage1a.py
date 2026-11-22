#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(sys.argv[1] if len(sys.argv)>1 else ".").resolve()
main=root/"kernel/arch/x86_64/main.c"
mk=root/"kernel/Makefile.x86"
if not main.is_file() or not mk.is_file():
    raise SystemExit("Run from the alpOS repository root, or pass its path.")
s=main.read_text()
if '#include "storage.h"' not in s:
    s=s.replace('#include "gui/gfx.h"','#include "gui/gfx.h"\n#include "storage.h"')
needle=' else if(eq(line,"reboot"))'
insert=''' else if(eq(line,"partitions")){
    if(!storage_disk_present()) puts("partitions: no ATA primary-master disk detected\\n");
    else if(storage_partition_count()==0) puts("partitions: disk detected; no MBR partitions (GPT is not supported yet)\\n");
    else {
        int i;
        puts("Index  Type  Start-LBA  Sectors  Boot\\n");
        for(i=0;i<storage_partition_count();i++){
            const struct alpos_partition *p=storage_partition_get(i);
            puts("/dev/hd0p"); print_dec(i+1); puts("  0x");
            console_putc("0123456789ABCDEF"[p->type>>4]);
            console_putc("0123456789ABCDEF"[p->type&15]);
            puts("  "); print_dec((int)p->first_lba); puts("  ");
            print_dec((int)p->sectors); puts(p->bootable?"  yes\\n":"  no\\n");
        }
    }
 }
 else if(eq(line,"mount")) puts("mount: block-device mounting is not available yet; root is the RAM initramfs VFS\\n");
'''
if 'eq(line,"partitions")' not in s:
    if needle not in s: raise SystemExit("main.c command insertion point not found")
    s=s.replace(needle,insert+needle)
s=s.replace('void kernel_main(uint32_t magic,uint32_t mbi){serial_init();','void kernel_main(uint32_t magic,uint32_t mbi){serial_init();storage_init();')
s=s.replace('Commands: help about clear mem ls pwd cd mkdir touch cat exec reboot halt','Commands: help about clear mem ls pwd cd mkdir touch cat exec partitions mount reboot halt')
main.write_text(s)
m=mk.read_text()
if '$(BUILD)/storage.o' not in m:
    m=m.replace('$(BUILD)/vfs.o: $(ARCH)/vfs.c $(ARCH)/vfs.h | $(BUILD)',
'''$(BUILD)/storage.o: $(ARCH)/storage.c $(ARCH)/storage.h | $(BUILD)
\t$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/vfs.o: $(ARCH)/vfs.c $(ARCH)/vfs.h | $(BUILD)''')
    m=m.replace('$(BUILD)/vfs.o $(BUILD)/user.o','$(BUILD)/vfs.o $(BUILD)/storage.o $(BUILD)/user.o')
mk.write_text(m)
print("Applied storage discovery stage 1A.")
