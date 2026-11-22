#include <stdint.h>
#include "storage.h"
#define D 0x1F0
#define SC 0x1F2
#define L0 0x1F3
#define L1 0x1F4
#define L2 0x1F5
#define DH 0x1F6
#define ST 0x1F7
#define CMD 0x1F7
#define BSY 0x80
#define DRQ 0x08
#define ERR 0x01
#define DF 0x20
static inline void ob(uint16_t p,uint8_t v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline uint8_t ib(uint16_t p){uint8_t v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}
static inline uint16_t iw(uint16_t p){uint16_t v;__asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p));return v;}
static inline void ow(uint16_t p,uint16_t v){__asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p));}
static int wait_drq(void){unsigned i;for(i=0;i<1000000;i++){uint8_t s=ib(ST);if(s==0xff||s&(ERR|DF))return 0;if(!(s&BSY)&&(s&DRQ))return 1;__asm__ volatile("pause");}return 0;}
static int io_sector(uint32_t lba,uint8_t *buf,int write){unsigned i;if(!buf||lba>=0x10000000U)return 0;ob(DH,(uint8_t)(0xE0|((lba>>24)&0x0f)));(void)ib(0x3f6);(void)ib(0x3f6);(void)ib(0x3f6);(void)ib(0x3f6);ob(SC,1);ob(L0,(uint8_t)lba);ob(L1,(uint8_t)(lba>>8));ob(L2,(uint8_t)(lba>>16));ob(CMD,write?0x30:0x20);if(!wait_drq())return 0;if(write){for(i=0;i<256;i++)ow(D,((uint16_t)buf[i*2])|((uint16_t)buf[i*2+1]<<8));__asm__ volatile("outb %%al,$0x80"::"a"(0));for(i=0;i<1000000;i++){uint8_t s=ib(ST);if(s==0xff||s&(ERR|DF))return 0;if(!(s&BSY))return 1;__asm__ volatile("pause");}return 0;}for(i=0;i<256;i++){uint16_t v=iw(D);buf[i*2]=(uint8_t)v;buf[i*2+1]=(uint8_t)(v>>8);}return 1;}
int storage_read_sector(uint32_t lba,uint8_t *buffer){return io_sector(lba,buffer,0);}
int storage_write_sector(uint32_t lba,const uint8_t *buffer){return io_sector(lba,(uint8_t *)(uintptr_t)buffer,1);}
