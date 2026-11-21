#include <stddef.h>
#include <stdint.h>
#include "vfs.h"
#include "descriptors.h"
#include "user.h"
#include "gui/gfx.h"

#define COM1 0x3F8
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint16_t *)0xB8000)
static size_t cursor_x,cursor_y;
static const uint8_t color=0x07;
static int cwd;
static inline void outb(uint16_t p,uint8_t v){__asm__ volatile("outb %0, %1"::"a"(v),"Nd"(p));}
static inline uint8_t inb(uint16_t p){uint8_t v;__asm__ volatile("inb %1, %0":"=a"(v):"Nd"(p));return v;}
static void serial_init(void){outb(COM1+1,0);outb(COM1+3,0x80);outb(COM1,1);outb(COM1+1,0);outb(COM1+3,3);outb(COM1+2,0xC7);outb(COM1+4,0x0B);}
static void serial_putc(char c){unsigned t=1000000;while(!(inb(COM1+5)&0x20)&&--t){}if(t)outb(COM1,(uint8_t)c);}
static void scroll(void){for(size_t y=1;y<VGA_HEIGHT;y++)for(size_t x=0;x<VGA_WIDTH;x++)VGA_MEMORY[(y-1)*VGA_WIDTH+x]=VGA_MEMORY[y*VGA_WIDTH+x];for(size_t x=0;x<VGA_WIDTH;x++)VGA_MEMORY[(VGA_HEIGHT-1)*VGA_WIDTH+x]=(uint16_t)(color<<8)|' ';cursor_y=VGA_HEIGHT-1;}
void console_putc(char c){serial_putc(c);if(gfx_available()){gfx_terminal_putc(c);return;}if(c=='\n'){cursor_x=0;++cursor_y;}else if(c=='\r')cursor_x=0;else if(c=='\b'){if(cursor_x)--cursor_x;VGA_MEMORY[cursor_y*VGA_WIDTH+cursor_x]=(uint16_t)(color<<8)|' ';}else{VGA_MEMORY[cursor_y*VGA_WIDTH+cursor_x]=(uint16_t)(color<<8)|(uint8_t)c;if(++cursor_x==VGA_WIDTH){cursor_x=0;++cursor_y;}}if(cursor_y>=VGA_HEIGHT)scroll();}
void console_write(const char*s,size_t n){for(size_t i=0;i<n;i++)console_putc(s[i]);}
static void puts(const char*s){while(*s)console_putc(*s++);}
static int eq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static void clear(void){if(gfx_available())gfx_clear_terminal();else{for(size_t i=0;i<VGA_WIDTH*VGA_HEIGHT;i++)VGA_MEMORY[i]=(uint16_t)(color<<8)|' ';cursor_x=cursor_y=0;}serial_putc(27);serial_putc('[');serial_putc('2');serial_putc('J');serial_putc(27);serial_putc('[');serial_putc('H');}
static int ps2_wait_input(void){for(unsigned t=0;t<1000000;t++){if(!(inb(0x64)&2))return 1;__asm__ volatile("pause");}return 0;}
static int ps2_wait_output(void){for(unsigned t=0;t<1000000;t++){if(inb(0x64)&1)return 1;__asm__ volatile("pause");}return 0;}
static void ps2_flush(void){for(unsigned t=0;t<64&&(inb(0x64)&1);t++)(void)inb(0x60);}
static int ps2_command(uint8_t c){if(!ps2_wait_input())return 0;outb(0x64,c);return 1;}
static int ps2_data(uint8_t d){if(!ps2_wait_input())return 0;outb(0x60,d);return 1;}
static int ps2_read(uint8_t *v){if(!ps2_wait_output())return 0;*v=inb(0x60);return 1;}
static int ps2_device_write(int mouse,uint8_t value){uint8_t r=0;for(int tries=0;tries<3;tries++){if(mouse&&!ps2_command(0xD4))return 0;if(!ps2_data(value)||!ps2_read(&r))return 0;if(r==0xFA)return 1;if(r!=0xFE)return 0;}return 0;}
static int mouse_present;
static void ps2_init(void){uint8_t cfg=0;ps2_command(0xAD);ps2_command(0xA7);ps2_flush();if(ps2_command(0x20)&&ps2_read(&cfg)){cfg=(uint8_t)((cfg|0x40)&~0x33);ps2_command(0x60);ps2_data(cfg);}ps2_command(0xAE);ps2_flush();/* Enable translated set-1 keyboard scanning. */ps2_device_write(0,0xF4);
 ps2_command(0xA8);if(ps2_command(0x20)&&ps2_read(&cfg)){cfg=(uint8_t)((cfg|0x40)&~0x20);ps2_command(0x60);ps2_data(cfg);}if(ps2_device_write(1,0xF6)&&ps2_device_write(1,0xF4))mouse_present=1;}
static int mouse_packet_pos;static uint8_t mouse_packet[3];static int mouse_px=24,mouse_py=24;
static void mouse_byte(uint8_t b){if(mouse_packet_pos==0){if(!(b&8))return;mouse_packet[0]=b;mouse_packet_pos=1;return;}mouse_packet[mouse_packet_pos++]=b;if(mouse_packet_pos==3){mouse_packet_pos=0;if(!(mouse_packet[0]&0xC0)){int dx=(int)(int8_t)mouse_packet[1],dy=(int)(int8_t)mouse_packet[2];mouse_px+=dx;mouse_py-=dy;gfx_mouse_move(mouse_px,mouse_py);}}}
static const char normal[128]={0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b','\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' '};
static const char shifted[128]={0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b','\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,'A','S','D','F','G','H','J','K','L',':','"','~',0,'|','Z','X','C','V','B','N','M','<','>','?',0,'*',0,' '};
static int shift_down;
static int input_char(void){for(;;){uint8_t st=inb(0x64);if(st&1){uint8_t b=inb(0x60);if(st&0x20){if(mouse_present)mouse_byte(b);continue;}if(b==0xE0)continue;if(b==0x2A||b==0x36){shift_down=1;continue;}if(b==0xAA||b==0xB6){shift_down=0;continue;}if(b&0x80)continue;if(b<128){char c=shift_down?shifted[b]:normal[b];if(c)return (unsigned char)c;}}if(inb(COM1+5)&1)return (int)(uint8_t)inb(COM1);__asm__ volatile("pause");}}
static void print_hex(uint32_t v){const char h[]="0123456789ABCDEF";for(int n=28;n>=0;n-=4)console_putc(h[(v>>n)&15]);}
static void print_path(int node){const struct vfs_node*n;int chain[16],count=0;while(node>=0&&count<16){chain[count++]=node;n=vfs_get(node);if(!n||n->parent<0)break;node=n->parent;}if(count==1){console_putc('/');return;}for(int i=count-2;i>=0;i--){n=vfs_get(chain[i]);console_putc('/');puts(n->name);}}
static void list_dir(int node){int c=vfs_child(node);while(c>=0){const struct vfs_node*n=vfs_get(c);if(n){puts(n->name);if(n->type==VFS_DIR)console_putc('/');console_putc(' ');}c=vfs_next(c);}console_putc('\n');}
static void print_dec(int v){char b[12];unsigned n=0;unsigned x=(unsigned)(v<0?-v:v);if(v<0)console_putc('-');do{b[n++]=(char)('0'+x%10);x/=10;}while(x&&n<sizeof(b));while(n)console_putc(b[--n]);}
static void run_user(const char *name,const char *args){int r=user_exec(name,args);if(r==-256)puts("exec: program not found or invalid ELF\n");else{puts("[exit ");print_dec(r);puts("]\n");}}
static void command(char *line,uint32_t magic,uint32_t mbi){char *arg=line;while(*arg&&*arg!=' ')arg++;if(*arg){*arg++=0;while(*arg==' ')arg++;}
 if(eq(line,"help")){puts("Commands: help about clear mem ls pwd cd mkdir touch cat exec reboot halt\nUser programs: /bin/echo, /bin/uname (try: echo hello)\n");}
 else if(eq(line,"about")){puts("alpOS x86_64 - graphical desktop + Ring 3 userland\nVFS: volatile RAM; ELF64 + int 0x80 syscalls\n");}
 else if(eq(line,"clear"))clear();
 else if(eq(line,"mem")){puts("Multiboot2 info: 0x");print_hex(mbi);puts(" magic: 0x");print_hex(magic);console_putc('\n');}
 else if(eq(line,"pwd")){print_path(cwd);console_putc('\n');}
 else if(eq(line,"ls")){int n=*arg?vfs_lookup(cwd,arg):cwd;const struct vfs_node*x=vfs_get(n);if(!x)puts("ls: path not found\n");else if(x->type==VFS_DIR)list_dir(n);else{puts(x->name);console_putc('\n');}}
 else if(eq(line,"cd")){int n=vfs_lookup(cwd,*arg?arg:"/");const struct vfs_node*x=vfs_get(n);if(!x||x->type!=VFS_DIR)puts("cd: not a directory\n");else cwd=n;}
 else if(eq(line,"mkdir")){if(!*arg)puts("usage: mkdir PATH\n");else if(vfs_mkdir(cwd,arg)<0)puts("mkdir: cannot create directory\n");}
 else if(eq(line,"touch")){if(!*arg)puts("usage: touch PATH\n");else if(vfs_create(cwd,arg)<0)puts("touch: cannot create file\n");}
 else if(eq(line,"cat")){char buf[VFS_FILE_MAX+1];int n=vfs_lookup(cwd,arg);int r=vfs_read(n,buf,sizeof(buf));if(r<0)puts("cat: cannot read file\n");else{puts(buf);if(!r||buf[r-1]!='\n')console_putc('\n');}}
 else if(eq(line,"exec")){char *prog=arg;while(*arg&&*arg!=' ')arg++;if(*arg){*arg++=0;while(*arg==' ')arg++;}if(!*prog)puts("usage: exec PROGRAM [ARGS...]\n");else run_user(prog,arg);}
 else if(eq(line,"reboot")){puts("Rebooting...\n");while(inb(0x64)&2){}outb(0x64,0xFE);}
 else if(eq(line,"halt")){puts("System halted.\n");for(;;)__asm__ volatile("cli; hlt");}
 else if(*line)run_user(line,arg);
}
void kernel_main(uint32_t magic,uint32_t mbi){serial_init();if(magic!=0x36D76289){puts("[alpOS] ERROR: invalid Multiboot2 magic\n");for(;;)__asm__ volatile("cli; hlt");}gfx_init(mbi);ps2_init();if(gfx_available())gfx_desktop();descriptors_init();vfs_init();user_init(mbi);cwd=vfs_root();clear();puts("alpOS graphical shell ready\nType 'help' to list commands.\n\n");char line[128];for(;;){size_t len=0;puts("alpos:");print_path(cwd);puts("$ ");for(;;){char c=(char)input_char();if(c=='\r'||c=='\n'){console_putc('\n');line[len]=0;break;}if((c=='\b'||c==0x7F)&&len){--len;console_putc('\b');}else if(c>=0x20&&c<=0x7E&&len+1<sizeof(line)){line[len++]=c;console_putc(c);}}command(line,magic,mbi);}}
