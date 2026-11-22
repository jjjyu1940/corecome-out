#!/usr/bin/env python3
from pathlib import Path
import sys
root = Path(sys.argv[1] if len(sys.argv) > 1 else '.').resolve()
arch = root / 'kernel/arch/x86_64'
required = [arch/'main.c', arch/'fat32.c', arch/'fat32.h', arch/'storage.h', arch/'storage.c', root/'kernel/Makefile.x86']
missing = [str(p.relative_to(root)) for p in required if not p.is_file()]
if missing:
    raise SystemExit('Stage 1A and Stage 1B must be applied first; missing: ' + ', '.join(missing))
source = Path(__file__).resolve().parent / 'kernel/arch/x86_64'
for name in ('fat32.c', 'fat32.h'):
    (arch/name).write_bytes((source/name).read_bytes())

main_path = arch/'main.c'
s = main_path.read_text()
if '#include "fat32.h"' not in s:
    s = s.replace('#include "storage.h"', '#include "storage.h"\n#include "fat32.h"')
if 'static int alpos_fat32_mounted;' not in s:
    s = s.replace('static int cwd;\n', 'static int cwd;\nstatic int alpos_fat32_mounted;\nstatic struct alpos_fat32_info alpos_fat32_volume;\n')
helpers = r'''
static int alpos_name_equal(const char *a,const char *b){
    while(*a&&*b){char x=*a++,y=*b++;if(x>='a'&&x<='z')x=(char)(x-'a'+'A');if(y>='a'&&y<='z')y=(char)(y-'a'+'A');if(x!=y)return 0;}
    return *a==*b;
}
static int alpos_is_mnt(const char *p){return eq(p,"/mnt")||eq(p,"/mnt/");}
static int alpos_is_mnt_file(const char *p){return p[0]=='/'&&p[1]=='m'&&p[2]=='n'&&p[3]=='t'&&p[4]=='/';}
static void alpos_fat32_ls(void){
    unsigned i;struct alpos_fat32_entry e;int shown=0;
    for(i=0;i<512;i++){if(fat32_root_entry(&alpos_fat32_volume,i,&e)<0)break;puts(e.name);if(e.attributes&0x10)console_putc('/');puts("  ");shown++;}
    if(!shown)puts("(empty directory)\n");else console_putc('\n');
}
static int alpos_fat32_cat(const char *path){
    const char *name;unsigned i;struct alpos_fat32_entry e;uint8_t data[ALPOS_FAT32_FILE_MAX];int n;
    if(!alpos_is_mnt_file(path))return 0;
    name=path+5;if(!*name||name[0]=='/')return -1;
    for(i=0;i<512;i++){if(fat32_root_entry(&alpos_fat32_volume,i,&e)<0)break;if(alpos_name_equal(name,e.name)){
        if(e.attributes&0x10){puts("cat: is a directory\n");return -1;}
        if(e.size>sizeof(data)){puts("cat: file exceeds 4096-byte read limit\n");return -1;}
        n=fat32_read_file(&alpos_fat32_volume,&e,data,sizeof(data));if(n<0){puts("cat: FAT32 read error\n");return -1;}
        for(i=0;i<(unsigned)n;i++)console_putc((char)data[i]);if(!n||data[n-1]!='\n')console_putc('\n');return 1;
    }}
    puts("cat: file not found in mounted FAT32 root\n");return -1;
}
'''
if 'static void alpos_fat32_ls(void)' not in s:
    marker = 'static void command(char *line,uint32_t magic,uint32_t mbi)'
    if marker not in s: raise SystemExit('Cannot find command() in main.c')
    s = s.replace(marker, helpers + '\n' + marker)
old_mount = 'else if(eq(line,"mount")) puts("mount: block-device mounting is not available yet; root is the RAM initramfs VFS\\n");'
new_mount = r'''else if(eq(line,"mount")){
    int p=-1,r;struct alpos_fat32_info info;
    if(eq(arg,"/dev/hd0p1 /mnt"))p=0;else if(eq(arg,"/dev/hd0p2 /mnt"))p=1;else if(eq(arg,"/dev/hd0p3 /mnt"))p=2;else if(eq(arg,"/dev/hd0p4 /mnt"))p=3;
    if(p<0)puts("usage: mount /dev/hd0pN /mnt  (N=1..4)\n");
    else if(alpos_fat32_mounted)puts("mount: /mnt is already mounted; use umount /mnt first\n");
    else if(!storage_disk_present())puts("mount: no ATA primary-master disk detected\n");
    else if(p>=storage_partition_count())puts("mount: partition does not exist\n");
    else if((r=fat32_mount_partition((unsigned)p,&info))<0){puts("mount: FAT32 mount failed, error ");print_dec(r);console_putc('\n');}
    else{alpos_fat32_volume=info;alpos_fat32_mounted=1;puts("mounted read-only /dev/hd0p");print_dec(p+1);puts(" on /mnt (FAT32 root only)\n");}
 }
 else if(eq(line,"umount")){if(eq(arg,"/mnt")||eq(arg,"/mnt/")){if(alpos_fat32_mounted){alpos_fat32_mounted=0;puts("unmounted /mnt\n");}else puts("umount: /mnt is not mounted\n");}else puts("usage: umount /mnt\n");}'''
if old_mount in s:
    s=s.replace(old_mount,new_mount)
elif 'mounted read-only /dev/hd0p' not in s:
    # Existing Stage 1B may have replaced the mount command differently; fail rather than silently omit.
    raise SystemExit('Cannot find the Stage 1A placeholder mount command in main.c')
old_ls = 'else if(eq(line,"ls")){int n=*arg?vfs_lookup(cwd,arg):cwd;const struct vfs_node*x=vfs_get(n);if(!x)puts("ls: path not found\\n");else if(x->type==VFS_DIR)list_dir(n);else{puts(x->name);console_putc(\'\\n\');}}'
new_ls = 'else if(eq(line,"ls")){if(alpos_fat32_mounted&&alpos_is_mnt(arg))alpos_fat32_ls();else{int n=*arg?vfs_lookup(cwd,arg):cwd;const struct vfs_node*x=vfs_get(n);if(!x)puts("ls: path not found\\n");else if(x->type==VFS_DIR)list_dir(n);else{puts(x->name);console_putc(\'\\n\');}}}'
if old_ls in s:s=s.replace(old_ls,new_ls)
else: raise SystemExit('Cannot locate ls command branch; main.c does not match expected Stage 1B baseline')
old_cat = 'else if(eq(line,"cat")){char buf[VFS_FILE_MAX+1];int n=vfs_lookup(cwd,arg);int r=vfs_read(n,buf,sizeof(buf));if(r<0)puts("cat: cannot read file\\n");else{puts(buf);if(!r||buf[r-1]!=\'\\n\')console_putc(\'\\n\');}}'
new_cat = 'else if(eq(line,"cat")){if(alpos_fat32_mounted&&arg[0]==\'/\'&&arg[1]==\'m\'&&arg[2]==\'n\'&&arg[3]==\'t\'&&arg[4]==\'/\'){(void)alpos_fat32_cat(arg);}else{char buf[VFS_FILE_MAX+1];int n=vfs_lookup(cwd,arg);int r=vfs_read(n,buf,sizeof(buf));if(r<0)puts("cat: cannot read file\\n");else{puts(buf);if(!r||buf[r-1]!=\'\\n\')console_putc(\'\\n\');}}}'
if old_cat in s:s=s.replace(old_cat,new_cat)
else: raise SystemExit('Cannot locate cat command branch; main.c does not match expected Stage 1B baseline')
s=s.replace('partitions mount fat32 reboot halt','partitions mount umount fat32 reboot halt')
main_path.write_text(s)

mk_path=root/'kernel/Makefile.x86'
mk=mk_path.read_text()
if '$(BUILD)/fat32.o' not in mk:
    if '$(BUILD)/storage.o' not in mk: raise SystemExit('Storage Stage 1A object is not in Makefile.x86')
    mk=mk.replace('$(BUILD)/vfs.o:', '$(BUILD)/fat32.o: $(ARCH)/fat32.c $(ARCH)/fat32.h $(ARCH)/storage.h | $(BUILD)\n\t$(CC) $(CFLAGS) -c $< -o $@\n$(BUILD)/vfs.o:')
    mk=mk.replace('$(BUILD)/storage.o $(BUILD)/user.o','$(BUILD)/storage.o $(BUILD)/fat32.o $(BUILD)/user.o')
mk_path.write_text(mk)
print('Applied alpOS Storage Stage 1C: read-only FAT32 mount namespace at /mnt.')
