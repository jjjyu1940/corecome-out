#include <stdint.h>
#include <stddef.h>
#include "prototype_features.h"
#include "vfs.h"
#include "storage.h"
#include "fat32.h"
extern void console_putc(char);
extern void console_write(const char *, size_t);
static unsigned current_uid, current_gid;
static char current_name[16] = "root";
static struct { char name[16]; unsigned uid, gid; int used; } accounts[8];
static struct alpos_fat32_info mounted;
static int fat_mounted;
static void out(const char *s){while(*s)console_putc(*s++);}
static int eq(const char*a,const char*b){while(*a&&*b&&*a==*b){a++;b++;}return *a==*b;}
static int dec(const char*s,unsigned *v){unsigned n=0;if(!*s)return 0;while(*s){if(*s<'0'||*s>'9')return 0;n=n*10U+(unsigned)(*s++-'0');}*v=n;return 1;}
static void number(unsigned n){char b[12];unsigned k=0;do{b[k++]=(char)('0'+n%10);n/=10;}while(n&&k<sizeof(b));while(k)console_putc(b[--k]);}
void alpos_features_init(void){unsigned i;for(i=0;i<8;i++)accounts[i].used=0;accounts[0].used=1;accounts[0].uid=0;accounts[0].gid=0;accounts[0].name[0]='r';accounts[0].name[1]='o';accounts[0].name[2]='o';accounts[0].name[3]='t';accounts[0].name[4]=0;current_uid=current_gid=0;fat_mounted=0;storage_init();}
static int mount_fat(void){int i,n=storage_partition_count();for(i=0;i<n;i++){if(fat32_mount_partition((unsigned)i,&mounted)==0){fat_mounted=1;return i;}}return -1;}
static void list_fat(void){struct alpos_fat32_entry e;unsigned i;for(i=0;i<128;i++){if(fat32_root_entry(&mounted,i,&e)<0)break;out(e.name);if(e.attributes&0x10)out("/");out("  ");}out("\n");}
int alpos_feature_command(const char *cmd,char *arg){
 if(eq(cmd,"uname")){out("corecome-out 0.10 x86_64\n");return 1;}
 if(eq(cmd,"whoami")){out(current_name);out(" (uid=");number(current_uid);out(", gid=");number(current_gid);out(")\n");return 1;}
 if(eq(cmd,"users")){unsigned i;for(i=0;i<8;i++)if(accounts[i].used){out(accounts[i].name);out(" ");}out("\n");return 1;}
 if(eq(cmd,"useradd")){unsigned i; if(!arg||!*arg){out("usage: useradd NAME\n");return 1;}if(current_uid!=0){out("useradd: permission denied\n");return 1;}for(i=0;i<8;i++)if(accounts[i].used&&eq(accounts[i].name,arg)){out("useradd: already exists\n");return 1;}for(i=1;i<8&&accounts[i].used;i++);if(i==8){out("useradd: account table full\n");return 1;}accounts[i].used=1;accounts[i].uid=i;accounts[i].gid=i;{unsigned j=0;while(arg[j]&&j<14){accounts[i].name[j]=arg[j];j++;}accounts[i].name[j]=0;if(arg[j]){accounts[i].used=0;out("useradd: name too long\n");return 1;}}out("user added (prototype in-memory account)\n");return 1;}
 if(eq(cmd,"su")){unsigned i;if(!arg||!*arg){out("usage: su NAME\n");return 1;}for(i=0;i<8;i++)if(accounts[i].used&&eq(accounts[i].name,arg)){current_uid=accounts[i].uid;current_gid=accounts[i].gid;{unsigned j=0;while(arg[j]&&j<15){current_name[j]=arg[j];j++;}current_name[j]=0;}out("identity switched (prototype; no password authentication)\n");return 1;}out("su: unknown user\n");return 1;}
 if(eq(cmd,"ps")||eq(cmd,"top")){out("PID  USER  STATE  COMMAND\n1    root  RUN    kernel\n2    ");out(current_name);out("  RUN    shell\n3    -     IDLE   user-exec (single-task prototype)\n");if(eq(cmd,"top"))out("CPU: not measured; scheduler: not implemented\n");return 1;}
 if(eq(cmd,"chmod")){char *p=arg,*path,*mode;unsigned m=0;int node;const struct vfs_node *v;while(p&&*p==' ')p++;mode=p;while(p&&*p&&*p!=' ')p++;if(!p||!*p){out("usage: chmod OCTAL PATH\n");return 1;}*p++=0;while(*p==' ')p++;path=p;if(current_uid!=0){out("chmod: permission denied (prototype root-only)\n");return 1;}if(!dec(mode,&m)||m>0777){out("chmod: invalid mode\n");return 1;}node=vfs_lookup(vfs_root(),path);v=vfs_get(node);if(!v){out("chmod: path not found\n");return 1;}if(vfs_chmod(node,m)<0)out("chmod: failed\n");else out("mode updated in RAM VFS\n");return 1;}
 if(eq(cmd,"mount")){int p=mount_fat();if(p<0)out("mount: no supported FAT32 MBR partition found\n");else{out("mounted FAT32 partition /dev/hd0p");number((unsigned)p+1);out(" selected as active FAT32 volume (not a VFS path mount)\n");}return 1;}
 if(eq(cmd,"fat32")){if(!fat_mounted&&mount_fat()<0){out("fat32: no FAT32 partition\n");return 1;}list_fat();return 1;}
 if(eq(cmd,"catfat")){struct alpos_fat32_entry e;uint8_t buf[ALPOS_FAT32_FILE_MAX];unsigned i;int ok=0;if(!arg||!*arg){out("usage: catfat 8.3NAME\n");return 1;}if(!fat_mounted&&mount_fat()<0){out("catfat: no FAT32 partition\n");return 1;}for(i=0;i<128;i++){unsigned j;int same=1;if(fat32_root_entry(&mounted,i,&e)<0)break;for(j=0;arg[j]||e.name[j];j++){char a=arg[j],b=e.name[j];if(a>='a'&&a<='z')a=(char)(a-'a'+'A');if(a!=b){same=0;break;}}if(same){int n=fat32_read_file(&mounted,&e,buf,sizeof(buf));if(n<0)out("catfat: read failed\n");else{for(j=0;j<(unsigned)n;j++)console_putc((char)buf[j]);if(!n||buf[n-1]!='\n')console_putc('\n');}ok=1;break;}}if(!ok)out("catfat: file not found in root\n");return 1;}
 if(eq(cmd,"writefat")){char *name=arg,*data;unsigned n=0;while(name&&name[n]&&name[n]!=' ')n++;if(!name||!name[n]){out("usage: writefat 8.3NAME CONTENT (max 512 bytes)\n");return 1;}name[n++]=0;data=name+n; if(!fat_mounted&&mount_fat()<0){out("writefat: no FAT32 partition\n");return 1;}if(fat32_write_root_file(&mounted,name,(const uint8_t*)data,(uint32_t)__builtin_strlen(data))<0)out("writefat: failed; only root 8.3 files up to 512 bytes are supported\n");else out("writefat: sector and FAT updated\n");return 1;}
 return 0;
}
