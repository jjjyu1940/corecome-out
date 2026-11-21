#include <stddef.h>
#include <stdint.h>
#define COM1 0x3F8
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint16_t *)0xB8000)
static size_t cursor_x, cursor_y;
static const uint8_t color = 0x07;
static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(p)); }
static inline uint8_t inb(uint16_t p) { uint8_t v; __asm__ volatile("inb %1, %0":"=a"(v):"Nd"(p)); return v; }
static void serial_init(void) { outb(COM1+1,0); outb(COM1+3,0x80); outb(COM1,1); outb(COM1+1,0); outb(COM1+3,3); outb(COM1+2,0xC7); outb(COM1+4,0x0B); }
static void serial_putc(char c) { unsigned t=1000000; while (!(inb(COM1+5)&0x20) && --t) {} if(t) outb(COM1,(uint8_t)c); }
static void scroll(void) { for(size_t y=1;y<VGA_HEIGHT;y++) for(size_t x=0;x<VGA_WIDTH;x++) VGA_MEMORY[(y-1)*VGA_WIDTH+x]=VGA_MEMORY[y*VGA_WIDTH+x]; for(size_t x=0;x<VGA_WIDTH;x++) VGA_MEMORY[(VGA_HEIGHT-1)*VGA_WIDTH+x]=(uint16_t)(color<<8)|' '; cursor_y=VGA_HEIGHT-1; }
static void putc(char c) { serial_putc(c); if(c=='\n'){cursor_x=0;++cursor_y;} else if(c=='\r') cursor_x=0; else if(c=='\b'){if(cursor_x)--cursor_x; VGA_MEMORY[cursor_y*VGA_WIDTH+cursor_x]=(uint16_t)(color<<8)|' ';} else {VGA_MEMORY[cursor_y*VGA_WIDTH+cursor_x]=(uint16_t)(color<<8)|(uint8_t)c; if(++cursor_x==VGA_WIDTH){cursor_x=0;++cursor_y;}} if(cursor_y>=VGA_HEIGHT)scroll(); }
static void puts(const char*s){while(*s)putc(*s++);}
static int eq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static void clear(void){for(size_t i=0;i<VGA_WIDTH*VGA_HEIGHT;i++)VGA_MEMORY[i]=(uint16_t)(color<<8)|' ';cursor_x=cursor_y=0;serial_putc(27);serial_putc('[');serial_putc('2');serial_putc('J');serial_putc(27);serial_putc('[');serial_putc('H');}
static char getc_serial(void){while(!(inb(COM1+5)&1))__asm__ volatile("pause");return (char)inb(COM1);}
static void command(const char*s,uint32_t magic,uint32_t mbi){if(eq(s,"help"))puts("Commands: help, about, clear, mem, reboot, halt\n");else if(eq(s,"about"))puts("alpOS x86_64 - minimal bootable kernel\nMultiboot2 boot completed; serial shell ready.\n");else if(eq(s,"clear"))clear();else if(eq(s,"mem")){puts("Boot protocol: Multiboot2\nBoot info: ");for(int n=28;n>=0;n-=4){const char h[]="0123456789ABCDEF";putc(h[(mbi>>n)&15]);}puts("\nBoot magic: ");for(int n=28;n>=0;n-=4){const char h[]="0123456789ABCDEF";putc(h[(magic>>n)&15]);}putc('\n');}else if(eq(s,"reboot")){puts("Rebooting...\n");while(inb(0x64)&2){}outb(0x64,0xFE);}else if(eq(s,"halt")){puts("System halted.\n");for(;;)__asm__ volatile("cli; hlt");}else if(*s)puts("Unknown command. Type 'help'.\n");}
void kernel_main(uint32_t magic,uint32_t mbi){serial_init();if(magic!=0x36D76289){puts("[alpOS] ERROR: invalid Multiboot2 magic\n");for(;;)__asm__ volatile("cli; hlt");}clear();puts("========================================\n              alpOS x86_64              \n========================================\nBooted via GRUB / Multiboot2.\nType 'help' to list commands.\n[alpOS] shell ready\n\n");char line[96];for(;;){size_t len=0;puts("alpos> ");for(;;){char c=getc_serial();if(c=='\r'||c=='\n'){putc('\n');line[len]=0;break;}if((c=='\b'||c==0x7F)&&len){--len;putc('\b');}else if(c>=0x20&&c<=0x7E&&len+1<sizeof(line)){line[len++]=c;putc(c);}}command(line,magic,mbi);}}
