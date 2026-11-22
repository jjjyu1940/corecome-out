#include <stdint.h>
#include <stddef.h>
#include "gfx.h"

#define MB2_TAG_END 0
#define MB2_TAG_FRAMEBUFFER 8
static volatile uint32_t *fb;
static uint32_t pitch,width,height,bpp;
static uint32_t tx=28,ty=112;
static int ready;
static void mouse_paint(void);
static const uint32_t C_TERM=0xff080d16,C_TERM_TEXT=0xffd7e2f1;
static void pixel(int x,int y,uint32_t c){if(!ready||x<0||y<0||(uint32_t)x>=width||(uint32_t)y>=height)return;volatile uint8_t *p=(volatile uint8_t*)fb+(uint32_t)y*pitch+(uint32_t)x*(bpp/8);p[0]=(uint8_t)c;p[1]=(uint8_t)(c>>8);p[2]=(uint8_t)(c>>16);if(bpp==32)p[3]=(uint8_t)(c>>24);}
static void fill(int x,int y,int w,int h,uint32_t c){for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++)pixel(x+xx,y+yy,c);}
static void line(int x,int y,int w,uint32_t c){fill(x,y,w,1,c);}
/* Compact 5x7 bitmap alphabet; lowercase is rendered as uppercase. */
static const uint8_t glyph[37][5]={
{0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},{0x3e,0x41,0x41,0x41,0x22},{0x7f,0x41,0x41,0x22,0x1c},{0x7f,0x49,0x49,0x49,0x41},{0x7f,0x09,0x09,0x09,0x01},{0x3e,0x41,0x49,0x49,0x7a},{0x7f,0x08,0x08,0x08,0x7f},{0x00,0x41,0x7f,0x41,0x00},{0x20,0x40,0x41,0x3f,0x01},{0x7f,0x08,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},{0x7f,0x02,0x0c,0x02,0x7f},{0x7f,0x04,0x08,0x10,0x7f},{0x3e,0x41,0x41,0x41,0x3e},{0x7f,0x09,0x09,0x09,0x06},{0x3e,0x41,0x51,0x21,0x5e},{0x7f,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7f,0x01,0x01},{0x3f,0x40,0x40,0x40,0x3f},{0x1f,0x20,0x40,0x20,0x1f},{0x3f,0x40,0x38,0x40,0x3f},{0x63,0x14,0x08,0x14,0x63},{0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},
{0x3e,0x51,0x49,0x45,0x3e},{0x00,0x42,0x7f,0x40,0x00},{0x62,0x51,0x49,0x49,0x46},{0x22,0x49,0x49,0x49,0x36},{0x18,0x14,0x12,0x7f,0x10},{0x2f,0x49,0x49,0x49,0x31},{0x3e,0x49,0x49,0x49,0x32},{0x01,0x71,0x09,0x05,0x03},{0x36,0x49,0x49,0x49,0x36},{0x26,0x49,0x49,0x49,0x3e},{0,0,0,0,0}};
static int glyph_index(char c){if(c>='a'&&c<='z')c=(char)(c-'a'+'A');if(c>='A'&&c<='Z')return c-'A';if(c>='0'&&c<='9')return 26+c-'0';return 36;}
static void text(int x,int y,const char*s,uint32_t color,int scale){while(*s){char c=*s++;if(c==' '){x+=6*scale;continue;}if(c=='-'){fill(x,y+3*scale,4*scale,scale,color);x+=6*scale;continue;}if(c==':'){fill(x+2*scale,y+2*scale,scale,scale,color);fill(x+2*scale,y+5*scale,scale,scale,color);x+=6*scale;continue;}if(c=='/'){for(int q=0;q<7;q++)fill(x+(4-q/2)*scale,y+q*scale,scale,scale,color);x+=6*scale;continue;}if(c=='\\'){for(int q=0;q<7;q++)fill(x+(q/2)*scale,y+q*scale,scale,scale,color);x+=6*scale;continue;}if(c=='.'){fill(x+2*scale,y+6*scale,scale,scale,color);x+=6*scale;continue;}if(c=='_'){fill(x,y+6*scale,5*scale,scale,color);x+=6*scale;continue;}if(c=='?'){fill(x+scale,y,3*scale,scale,color);fill(x+4*scale,y+scale,scale,2*scale,color);fill(x+2*scale,y+3*scale,2*scale,scale,color);fill(x+2*scale,y+6*scale,scale,scale,color);x+=6*scale;continue;}int k=glyph_index(c);for(int col=0;col<5;col++)for(int row=0;row<7;row++)if(glyph[k][col]&(1u<<row))fill(x+col*scale,y+row*scale,scale,scale,color);x+=6*scale;}}
void gfx_init(uint32_t mbi){uint32_t p=mbi; if(!p)return;uint32_t total=*(volatile uint32_t*)(uintptr_t)p;if(total<16||total>0x1000000)return;for(uint32_t off=8;off+8<=total;){uint32_t *tag=(uint32_t*)(uintptr_t)(p+off);uint32_t type=tag[0],size=tag[1];if(size<8||off+size>total)break;if(type==MB2_TAG_FRAMEBUFFER&&size>=32){uint64_t addr=*(uint64_t*)(uintptr_t)(p+off+8);pitch=*(uint32_t*)(uintptr_t)(p+off+16);width=*(uint32_t*)(uintptr_t)(p+off+20);height=*(uint32_t*)(uintptr_t)(p+off+24);bpp=*(uint8_t*)(uintptr_t)(p+off+28);uint8_t fbtype=*(uint8_t*)(uintptr_t)(p+off+29);if(addr<=0xffffffffu&&fbtype==1&&bpp==32&&width>=640&&height>=400&&pitch>=width*4){fb=(volatile uint32_t*)(uintptr_t)addr;ready=1;}}if(type==MB2_TAG_END)break;off=(off+size+7u)&~7u;}}
int gfx_available(void){return ready;}
void gfx_desktop(void){if(!ready)return;/* Prototype shell: terminal is the first and full-screen interface. */fill(0,0,(int)width,(int)height,C_TERM);text(12,8,"ALPOS - FULLSCREEN TERMINAL",0xff8fb8ee,1);line(8,22,(int)width-16,0xff34445c);tx=12;ty=32;mouse_paint();}
void gfx_clear_terminal(void){if(!ready)return;fill(0,24,(int)width,(int)height-24,C_TERM);tx=12;ty=32;text(12,8,"ALPOS - FULLSCREEN TERMINAL",0xff8fb8ee,1);line(8,22,(int)width-16,0xff34445c);}
void gfx_terminal_putc(char c){if(!ready)return;uint32_t left=12,right=width>20?width-8:width,bottom=height>12?height-8:height;if(c=='\n'){tx=left;ty+=12;}else if(c=='\r')tx=left;else if(c=='\b'){if(tx>=left+8)tx-=8;fill((int)tx,(int)ty,8,10,C_TERM);}else{char s[2]={c,0};text((int)tx,(int)ty,s,C_TERM_TEXT,1);tx+=8;}if(tx+8>right){tx=left;ty+=12;}if(ty+10>bottom){gfx_clear_terminal();}}

/* Software mouse cursor: restore the pixels under the previous cursor before moving. */
static int mouse_x=24, mouse_y=24, mouse_drawn;
static uint32_t mouse_saved[12*16];
static void mouse_restore(void){if(!mouse_drawn)return;for(int y=0;y<16;y++)for(int x=0;x<12;x++)pixel(mouse_x+x,mouse_y+y,mouse_saved[y*12+x]);mouse_drawn=0;}
static void mouse_paint(void){if(!ready)return;for(int y=0;y<16;y++)for(int x=0;x<12;x++)mouse_saved[y*12+x]=((mouse_x+x)>=0&&(mouse_y+y)>=0&&(uint32_t)(mouse_x+x)<width&&(uint32_t)(mouse_y+y)<height)?fb[((uint32_t)(mouse_y+y)*pitch/4)+(uint32_t)(mouse_x+x)]:0;for(int y=0;y<15;y++){int edge= y<8 ? y+1 : 8;for(int x=0;x<edge;x++){pixel(mouse_x+x,mouse_y+y,0xff05070a);if(x>0&&x<edge-1&&y>1)pixel(mouse_x+x,mouse_y+y,0xfff5f7fb);}}for(int y=9;y<16;y++)for(int x=4;x<9;x++)if(x<13-y/3)pixel(mouse_x+x,mouse_y+y,0xff05070a);mouse_drawn=1;}
void gfx_mouse_move(int x,int y){if(!ready)return;mouse_restore();if(x<0)x=0;if(y<0)y=0;if((uint32_t)x>=width)x=(int)width-1;if((uint32_t)y>=height)y=(int)height-1;mouse_x=x;mouse_y=y;mouse_paint();}
