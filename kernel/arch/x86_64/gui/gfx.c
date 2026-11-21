#include <stdint.h>
#include <stddef.h>
#include "gfx.h"

#define MB2_TAG_END 0
#define MB2_TAG_FRAMEBUFFER 8
static volatile uint32_t *fb;
static uint32_t pitch,width,height,bpp;
static uint32_t tx=28,ty=112;
static int ready;
static const uint32_t C_BG=0xff17243b,C_PANEL=0xffe8edf5,C_TITLE=0xff263b60,C_TASK=0xff101a2b,C_WHITE=0xffffffff,C_ACCENT=0xff4285d4,C_GREEN=0xff36a269,C_ORANGE=0xffffbd59,C_TERM=0xff080d16,C_TERM_TEXT=0xffd7e2f1;
static void pixel(int x,int y,uint32_t c){if(!ready||x<0||y<0||(uint32_t)x>=width||(uint32_t)y>=height)return;volatile uint8_t *p=(volatile uint8_t*)fb+(uint32_t)y*pitch+(uint32_t)x*(bpp/8);p[0]=(uint8_t)c;p[1]=(uint8_t)(c>>8);p[2]=(uint8_t)(c>>16);if(bpp==32)p[3]=(uint8_t)(c>>24);}
static void fill(int x,int y,int w,int h,uint32_t c){for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++)pixel(x+xx,y+yy,c);}
static void line(int x,int y,int w,uint32_t c){fill(x,y,w,1,c);}
/* Compact 5x7 bitmap alphabet; lowercase is rendered as uppercase. */
static const uint8_t glyph[37][5]={
{0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},{0x3e,0x41,0x41,0x41,0x22},{0x7f,0x41,0x41,0x22,0x1c},{0x7f,0x49,0x49,0x49,0x41},{0x7f,0x09,0x09,0x09,0x01},{0x3e,0x41,0x49,0x49,0x7a},{0x7f,0x08,0x08,0x08,0x7f},{0x00,0x41,0x7f,0x41,0x00},{0x20,0x40,0x41,0x3f,0x01},{0x7f,0x08,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},{0x7f,0x02,0x0c,0x02,0x7f},{0x7f,0x04,0x08,0x10,0x7f},{0x3e,0x41,0x41,0x41,0x3e},{0x7f,0x09,0x09,0x09,0x06},{0x3e,0x41,0x51,0x21,0x5e},{0x7f,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7f,0x01,0x01},{0x3f,0x40,0x40,0x40,0x3f},{0x1f,0x20,0x40,0x20,0x1f},{0x3f,0x40,0x38,0x40,0x3f},{0x63,0x14,0x08,0x14,0x63},{0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},
{0x3e,0x51,0x49,0x45,0x3e},{0x00,0x42,0x7f,0x40,0x00},{0x62,0x51,0x49,0x49,0x46},{0x22,0x49,0x49,0x49,0x36},{0x18,0x14,0x12,0x7f,0x10},{0x2f,0x49,0x49,0x49,0x31},{0x3e,0x49,0x49,0x49,0x32},{0x01,0x71,0x09,0x05,0x03},{0x36,0x49,0x49,0x49,0x36},{0x26,0x49,0x49,0x49,0x3e},{0,0,0,0,0}};
static int glyph_index(char c){if(c>='a'&&c<='z')c=(char)(c-'a'+'A');if(c>='A'&&c<='Z')return c-'A';if(c>='0'&&c<='9')return 26+c-'0';return 36;}
static void text(int x,int y,const char*s,uint32_t color,int scale){while(*s){char c=*s++;if(c==' '){x+=6*scale;continue;}if(c=='-'){fill(x,y+3*scale,4*scale,scale,color);x+=6*scale;continue;}if(c==':'){fill(x+2*scale,y+2*scale,scale,scale,color);fill(x+2*scale,y+5*scale,scale,scale,color);x+=6*scale;continue;}int k=glyph_index(c);for(int col=0;col<5;col++)for(int row=0;row<7;row++)if(glyph[k][col]&(1u<<row))fill(x+col*scale,y+row*scale,scale,scale,color);x+=6*scale;}}
static void icon(int x,int y,uint32_t color,const char*label){fill(x+7,y,30,25,color);fill(x+3,y+25,38,3,C_WHITE);fill(x,y+28,44,3,color);text(x-2,y+38,label,C_WHITE,1);}
void gfx_init(uint32_t mbi){uint32_t p=mbi; if(!p)return;uint32_t total=*(volatile uint32_t*)(uintptr_t)p;if(total<16||total>0x1000000)return;for(uint32_t off=8;off+8<=total;){uint32_t *tag=(uint32_t*)(uintptr_t)(p+off);uint32_t type=tag[0],size=tag[1];if(size<8||off+size>total)break;if(type==MB2_TAG_FRAMEBUFFER&&size>=32){uint64_t addr=*(uint64_t*)(uintptr_t)(p+off+8);pitch=*(uint32_t*)(uintptr_t)(p+off+16);width=*(uint32_t*)(uintptr_t)(p+off+20);height=*(uint32_t*)(uintptr_t)(p+off+24);bpp=*(uint8_t*)(uintptr_t)(p+off+28);uint8_t fbtype=*(uint8_t*)(uintptr_t)(p+off+29);if(addr<=0xffffffffu&&fbtype==1&&bpp==32&&width>=640&&height>=400&&pitch>=width*4){fb=(volatile uint32_t*)(uintptr_t)addr;ready=1;}}if(type==MB2_TAG_END)break;off=(off+size+7u)&~7u;}}
int gfx_available(void){return ready;}
void gfx_desktop(void){if(!ready)return;fill(0,0,(int)width,(int)height,C_BG);for(uint32_t y=0;y<height;y+=4)line(0,(int)y,(int)width,0xff1a2942);text(24,18,"ALPOS DESKTOP",C_WHITE,2);text(24,42,"X86 64 - GRAPHICAL SHELL",0xffb9c9df,1);icon(42,100,C_ACCENT,"FILES");icon(42,180,C_GREEN,"DISK");icon(42,260,C_ORANGE,"TERMINAL");int wx=(int)(width>900?190:105),wy=90,ww=(int)width-wx-35,wh=(int)height-150;if(ww<420)ww=(int)width-40;if(wh<220)wh=(int)height-115;fill(wx+5,wy+7,ww,wh,0xff070b12);fill(wx,wy,ww,wh,C_PANEL);fill(wx,wy,ww,30,C_TITLE);text(wx+12,wy+10,"ALPOS TERMINAL",C_WHITE,1);fill(wx+ww-58,wy+9,12,12,0xffd96b6b);fill(wx+ww-36,wy+9,12,12,0xffe8bd57);fill(wx+ww-14,wy+9,8,12,0xff6bc28a);fill(wx+8,wy+38,ww-16,wh-48,C_TERM);tx=(uint32_t)wx+20;ty=(uint32_t)wy+52;fill(0,(int)height-34,(int)width,34,C_TASK);fill(12,(int)height-27,82,22,C_ACCENT);text(25,(int)height-21,"START",C_WHITE,1);text(112,(int)height-21,"ALPOS - READY",0xffdce6f5,1);}
void gfx_clear_terminal(void){if(!ready)return;int wx=(int)(width>900?190:105),wy=90,ww=(int)width-wx-35,wh=(int)height-150;if(ww<420)ww=(int)width-40;if(wh<220)wh=(int)height-115;fill(wx+8,wy+38,ww-16,wh-48,C_TERM);tx=(uint32_t)wx+20;ty=(uint32_t)wy+52;}
void gfx_terminal_putc(char c){if(!ready)return;uint32_t left=(uint32_t)(width>900?210:125),right=width-45,bottom=height-70;if(c=='\n'){tx=left;ty+=12;}else if(c=='\r')tx=left;else if(c=='\b'){if(tx>=left+8)tx-=8;fill((int)tx,(int)ty,8,10,C_TERM);}else{char s[2]={c,0};text((int)tx,(int)ty,s,C_TERM_TEXT,1);tx+=8;}if(tx+8>right){tx=left;ty+=12;}if(ty+10>bottom){gfx_clear_terminal();}}
