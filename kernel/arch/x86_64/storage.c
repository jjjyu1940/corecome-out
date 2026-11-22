#include "storage.h"
#define ATA_DATA 0x1F0
#define ATA_SECCOUNT0 0x1F2
#define ATA_LBA0 0x1F3
#define ATA_LBA1 0x1F4
#define ATA_LBA2 0x1F5
#define ATA_HDDEVSEL 0x1F6
#define ATA_STATUS 0x1F7
#define ATA_CMD_READ 0x20
#define ATA_SR_ERR 0x01
#define ATA_SR_DRQ 0x08
#define ATA_SR_DF 0x20
#define ATA_SR_BSY 0x80
#define ATA_TIMEOUT 1000000U
static struct alpos_partition parts[ALPOS_PARTITION_MAX];
static int part_count, disk_found;
static inline void outb(uint16_t p,uint8_t v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline uint8_t inb(uint16_t p){uint8_t v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}
static inline uint16_t inw(uint16_t p){uint16_t v;__asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p));return v;}
static void delay400(void){(void)inb(0x3F6);(void)inb(0x3F6);(void)inb(0x3F6);(void)inb(0x3F6);}
static int wait_ready(void){unsigned i;for(i=0;i<ATA_TIMEOUT;i++){uint8_t s=inb(ATA_STATUS);if(s==0xFF)return 0;if(!(s&ATA_SR_BSY))return !(s&(ATA_SR_ERR|ATA_SR_DF));__asm__ volatile("pause");}return 0;}
static int read_lba0(uint16_t *buf){unsigned i;if(!wait_ready())return 0;outb(ATA_HDDEVSEL,0xE0);delay400();outb(ATA_SECCOUNT0,1);outb(ATA_LBA0,0);outb(ATA_LBA1,0);outb(ATA_LBA2,0);outb(ATA_STATUS,ATA_CMD_READ);for(i=0;i<ATA_TIMEOUT;i++){uint8_t s=inb(ATA_STATUS);if(s&(ATA_SR_ERR|ATA_SR_DF))return 0;if(s&ATA_SR_DRQ)break;__asm__ volatile("pause");}if(i==ATA_TIMEOUT)return 0;for(i=0;i<256;i++)buf[i]=inw(ATA_DATA);return 1;}
static uint32_t le32(const uint8_t*p){return(uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
void storage_init(void){uint16_t sector[256];const uint8_t*m=(const uint8_t*)sector;int i;part_count=0;disk_found=0;if(!read_lba0(sector))return;disk_found=1;if(m[510]!=0x55||m[511]!=0xAA)return;for(i=0;i<4;i++){const uint8_t*e=m+446+i*16;uint8_t type=e[4];uint32_t start=le32(e+8),count=le32(e+12);if(!type||!count)continue;parts[part_count].type=type;parts[part_count].bootable=(e[0]==0x80);parts[part_count].first_lba=start;parts[part_count].sectors=count;part_count++;}}
int storage_disk_present(void){return disk_found;}
int storage_partition_count(void){return part_count;}
const struct alpos_partition*storage_partition_get(int i){return(i<0||i>=part_count)?0:&parts[i];}
