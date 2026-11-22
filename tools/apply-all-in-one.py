#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(sys.argv[1] if len(sys.argv)>1 else '.')
def get(p):
 q=root/p
 if not q.exists(): raise SystemExit('Missing: '+str(q))
 return q.read_text()
def put(p,s): (root/p).write_text(s)
def rep(s,a,b,label):
 if a not in s:
  if b in s:return s
  raise SystemExit('Source mismatch while patching '+label)
 return s.replace(a,b,1)
# storage API
p='kernel/arch/x86_64/storage.h';s=get(p);s=rep(s,'void storage_init(void);','int storage_read_sector(uint32_t lba, uint8_t *buffer);\nint storage_write_sector(uint32_t lba, const uint8_t *buffer);\nvoid storage_init(void);','storage header');put(p,s)
# build rules and link objects
p='kernel/Makefile.x86';s=get(p)
rules='''$(BUILD)/storage.o: $(ARCH)/storage.c $(ARCH)/storage.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/storage_io.o: $(ARCH)/storage_io.c $(ARCH)/storage.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/fat32.o: $(ARCH)/fat32.c $(ARCH)/fat32.h $(ARCH)/storage.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/prototype_features.o: $(ARCH)/prototype_features.c $(ARCH)/prototype_features.h $(ARCH)/vfs.h $(ARCH)/fat32.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
'''
if '$(BUILD)/storage.o:' not in s:s=rep(s,'$(BUILD)/descriptors.o: $(ARCH)/descriptors.c $(ARCH)/descriptors.h | $(BUILD)',rules+'$(BUILD)/descriptors.o: $(ARCH)/descriptors.c $(ARCH)/descriptors.h | $(BUILD)','Makefile compile rules')
s=rep(s,'$(BUILD)/descriptors.o $(ARCH)/boot/link.ld','$(BUILD)/descriptors.o $(BUILD)/storage.o $(BUILD)/storage_io.o $(BUILD)/fat32.o $(BUILD)/prototype_features.o $(ARCH)/boot/link.ld','Makefile prerequisites')
old='$(LD) -T $(ARCH)/boot/link.ld -nostdlib -z max-page-size=0x1000 -o $@ $(BUILD)/start.o $(BUILD)/interrupts.o $(BUILD)/main.o $(BUILD)/gfx.o $(BUILD)/vfs.o $(BUILD)/user.o $(BUILD)/descriptors.o'
s=rep(s,old,old+' $(BUILD)/storage.o $(BUILD)/storage_io.o $(BUILD)/fat32.o $(BUILD)/prototype_features.o','link command');put(p,s)
# FAT32 writer API
p='kernel/arch/x86_64/fat32.h';s=get(p);s=rep(s,'int fat32_read_file(const struct alpos_fat32_info *info, const struct alpos_fat32_entry *entry, uint8_t *out, uint32_t capacity);','int fat32_read_file(const struct alpos_fat32_info *info, const struct alpos_fat32_entry *entry, uint8_t *out, uint32_t capacity);\nint fat32_write_root_file(const struct alpos_fat32_info *info, const char *name, const uint8_t *data, uint32_t size);','FAT32 header');put(p,s)
p='kernel/arch/x86_64/fat32.c';s=get(p)
extra=r'''
/* Bounded prototype writer: root directory, 8.3 names, maximum 512 bytes. */
static void put16(uint8_t*p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void put32(uint8_t*p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static int fat_set_entry(const struct alpos_fat32_info*i,uint32_t c,uint32_t v){uint8_t b[512];uint32_t fs=(c*4U)/512U,off=(c*4U)%512U,k;if(fs>=i->sectors_per_fat)return -1;for(k=0;k<i->fat_count;k++){uint32_t l=i->partition_lba+i->reserved_sectors+k*i->sectors_per_fat+fs;if(!storage_read_sector(l,b))return -1;put32(b+off,(le32(b+off)&0xF0000000U)|(v&0x0FFFFFFFU));if(!storage_write_sector(l,b))return -1;}return 0;}
static int fat_find_free_cluster(const struct alpos_fat32_info*i,uint32_t*out){uint8_t b[512];uint32_t c,max=(i->total_sectors-i->reserved_sectors-(uint32_t)i->fat_count*i->sectors_per_fat)/i->sectors_per_cluster+2U;for(c=2;c<max;c++){uint32_t fs=(c*4U)/512U,off=(c*4U)%512U;if(fs>=i->sectors_per_fat)return -1;if(!storage_read_sector(i->partition_lba+i->reserved_sectors+fs,b))return -1;if((le32(b+off)&0x0FFFFFFFU)==0){*out=c;return 0;}}return -1;}
static int fat_name83(const char*s,uint8_t n[11]){unsigned i,j=0;int dot=0;for(i=0;i<11;i++)n[i]=' ';for(i=0;s[i];i++){char c=s[i];if(c=='.'){if(dot||j==0||j>8)return -1;dot=1;j=8;continue;}if(c>='a'&&c<='z')c=(char)(c-'a'+'A');if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='$'))return -1;if((!dot&&j>=8)||(dot&&j>=11))return -1;n[j++]=(uint8_t)c;}return j?0:-1;}
int fat32_write_root_file(const struct alpos_fat32_info*i,const char*name,const uint8_t*data,uint32_t size){uint8_t nm[11],b[512],dentry[32];uint32_t ds,max,cl,visited=0,slot_lba=0;unsigned s,e,slot=0;int found=0;if(!i||!name||(!data&&size)||size>512U||fat_name83(name,nm)<0)return -1;ds=i->partition_lba+i->reserved_sectors+(uint32_t)i->fat_count*i->sectors_per_fat;max=(i->total_sectors-i->reserved_sectors-(uint32_t)i->fat_count*i->sectors_per_fat)/i->sectors_per_cluster+2U;cl=i->root_cluster;
 while(cl>=2&&cl<max&&visited++<FAT32_MAX_CHAIN&&!found){uint32_t first=ds+(cl-2U)*i->sectors_per_cluster;for(s=0;s<i->sectors_per_cluster&&!found;s++){if(!storage_read_sector(first+s,b))return -1;for(e=0;e<16;e++){uint8_t*x=b+e*32U;if(x[0]==0||x[0]==0xE5){slot_lba=first+s;slot=e;found=1;break;}if(x[11]!=FAT32_ATTR_LFN&&!(x[11]&FAT32_ATTR_VOLUME)){unsigned z;for(z=0;z<11&&x[z]==nm[z];z++);if(z==11){slot_lba=first+s;slot=e;found=2;break;}}}}if(!found){uint32_t nx=0;if(next_cluster(i,cl,&nx)<0)return -1;if(nx>=FAT32_EOC){uint32_t nc;if(fat_find_free_cluster(i,&nc)<0||fat_set_entry(i,cl,nc)<0||fat_set_entry(i,nc,FAT32_EOC)<0)return -1;cl=nc;for(s=0;s<i->sectors_per_cluster;s++){unsigned z;for(z=0;z<512;z++)b[z]=0;if(!storage_write_sector(ds+(cl-2U)*i->sectors_per_cluster+s,b))return -1;}slot_lba=ds+(cl-2U)*i->sectors_per_cluster;slot=0;found=1;}else cl=nx;}}
 if(!found||!storage_read_sector(slot_lba,b))return -1;
 for(e=0;e<32;e++)dentry[e]=b[slot*32U+e];
 if(found==2){uint32_t old=((uint32_t)le16(dentry+20)<<16)|le16(dentry+26);if(le32(dentry+28)>512U)return -1;if(old>=2&&fat_set_entry(i,old,0)<0)return -1;}
 {uint32_t nc=0;if(size){if(fat_find_free_cluster(i,&nc)<0||fat_set_entry(i,nc,FAT32_EOC)<0)return -1;for(e=0;e<512;e++)b[e]=0;for(e=0;e<size;e++)b[e]=data[e];if(!storage_write_sector(ds+(nc-2U)*i->sectors_per_cluster,b))return -1;}for(e=0;e<11;e++)dentry[e]=nm[e];dentry[11]=0x20;put16(dentry+20,(uint16_t)(nc>>16));put16(dentry+26,(uint16_t)nc);put32(dentry+28,size);if(!storage_read_sector(slot_lba,b))return -1;for(e=0;e<32;e++)b[slot*32U+e]=dentry[e];if(!storage_write_sector(slot_lba,b))return -1;}return 0;}
'''
if 'int fat32_write_root_file(' not in s:s+=extra
put(p,s)
# VFS mode metadata
p='kernel/arch/x86_64/vfs.h';s=get(p);s=rep(s,'size_t size; char data[VFS_FILE_MAX];','size_t size; unsigned mode, uid, gid; char data[VFS_FILE_MAX];','VFS node fields');s=rep(s,'int vfs_write(int node,const char *data,size_t len);','int vfs_write(int node,const char *data,size_t len); int vfs_chmod(int node,unsigned mode);','VFS API');put(p,s)
p='kernel/arch/x86_64/vfs.c';s=get(p);s=rep(s,'nodes[id].size=0;return id;','nodes[id].size=0;nodes[id].mode=(type==VFS_DIR)?0755U:0644U;nodes[id].uid=0;nodes[id].gid=0;return id;','VFS defaults')
if 'int vfs_chmod(' not in s:s+='\nint vfs_chmod(int node,unsigned mode){if(node<0||node>=used||mode>0777U)return -1;nodes[node].mode=mode;return 0;}\n'
put(p,s)
# Shell integration
p='kernel/arch/x86_64/main.c';s=get(p);s=rep(s,'#include "gui/gfx.h"','#include "gui/gfx.h"\n#include "prototype_features.h"','main include');s=rep(s,' if(eq(line,"help")){puts("Commands: help about clear mem ls pwd cd mkdir touch cat exec reboot halt\\nUser programs: /bin/echo, /bin/uname (try: echo hello)\\n");}',' if(alpos_feature_command(line,arg))return;\n if(eq(line,"help")){puts("Commands: help about clear mem ls pwd cd mkdir touch cat exec uname whoami ps top useradd su users chmod mount fat32 writefat reboot halt\\n");}','shell command table');s=rep(s,'descriptors_init();vfs_init();user_init(mbi);','descriptors_init();vfs_init();user_init(mbi);alpos_features_init();','feature initialization');put(p,s)
print('Applied alpOS all-in-one prototype feature overlay.')
