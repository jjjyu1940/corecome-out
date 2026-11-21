/*
 * * gdt.c ---- GDT ? TSS ??
 * *
 * * 64-bit ? GDT ????????????? DPL0 ? 64-bit ?????
 * * ?? DPL3 ? 64-bit ???? sysret ?????? IST ? ring
 * * ?????? TSS
 * *
 * * ????? TSS ??????? ring0 ????????????CPU
 * * ??? TSS.RSP0 ?????????????????IST ??? 0
 * * ????? RSP0
 */
#include "x86.h"

#define TSS_N 1
static struct tss {
    uint32_t reserved0;
    uint64_t rsp[3];
    uint64_t reserved1;
    uint64_t ist[7];
    uint32_t reserved2;
    uint32_t iomap_base;
} __attribute__((packed)) tss[TSS_N];

uint64_t gdt[GDT_N];
struct gdt_ptr gdtp;

/*
 * * ? System V ABIGDTR base ???? -2 GiB..+2 GiB ????????
 * * ???????????gdt_flush ? lgdt ????
 */
static void gdt_pack(uint32_t slot, uint64_t base, uint64_t limit,
                     uint8_t type, uint8_t dpl, uint8_t l, uint8_t db) {
    gdt[slot] = 0;
    gdt[slot] |= (limit & 0xFFFFULL);
    gdt[slot] |= ((base & 0xFFFFULL) << 16);
    gdt[slot] |= ((base & 0xFF0000ULL) << 16);     /* bits 23:16 */
    gdt[slot] |= ((uint64_t)(type & 0xF) << 40);
    gdt[slot] |= ((uint64_t)(1) << 44);             /* S=1 ??/?? */
    gdt[slot] |= ((uint64_t)(dpl & 3) << 45);
    gdt[slot] |= ((uint64_t)(1) << 47);             /* P=1 */
    gdt[slot] |= (((limit >> 16) & 0xFULL) << 48);
    gdt[slot] |= ((uint64_t)(1) << 53);             /* AVL=1 */
    gdt[slot] |= ((uint64_t)(l & 1) << 53);         /* L=1 64-bit code */
    gdt[slot] |= ((uint64_t)(db & 1) << 54);
    gdt[slot] |= ((uint64_t)(1) << 55);             /* G=1 4K granularity */
    gdt[slot] |= ((base >> 24) & 0xFFULL) << 56;
}

static void tss_pack(uint32_t slot, uint64_t base, uint64_t limit) {
    gdt[slot] = 0;
    gdt[slot] |= (limit & 0xFFFFULL);
    gdt[slot] |= ((base & 0xFFFFULL) << 16);
    gdt[slot] |= ((base & 0xFF0000ULL) << 16);
    gdt[slot] |= ((uint64_t)(0x9) << 40);           /* type=9: 64-bit TSS */
    gdt[slot] |= ((uint64_t)(0) << 44);             /* S=0 ??? */
    gdt[slot] |= ((uint64_t)(0) << 45);             /* DPL=0 */
    gdt[slot] |= ((uint64_t)(1) << 47);             /* P=1 */
    gdt[slot] |= (((limit >> 16) & 0xFULL) << 48);
    gdt[slot] |= ((uint64_t)(0) << 53);
    gdt[slot] |= ((uint64_t)(0) << 54);
    gdt[slot] |= ((uint64_t)(0) << 55);
    gdt[slot] |= ((base >> 24) & 0xFFULL) << 56;
    gdt[slot] |= ((base >> 32) & 0xFFFFFFFFULL) << 32;
}

extern void gdt_flush(void);

void gdt_init(void) {
    for (int i = 0; i < GDT_N; i++) gdt[i] = 0;

    gdt_pack(1, 0, 0, 0xB, 0, 1, 0);     /* 0x08 kern code (L=1 D/B=0) */
    gdt_pack(2, 0, 0, 0x3, 0, 0, 1);     /* 0x10 kern data (L=0 D/B=1) */
    gdt_pack(3, 0, 0, 0xB, 3, 1, 0);     /* 0x18 user code */
    gdt_pack(4, 0, 0, 0x3, 3, 0, 1);     /* 0x20 user data */
    tss_pack(5, (uint64_t)&tss[0], sizeof(struct tss) - 1);

    gdtp.limit = GDT_N * 8 - 1;
    gdtp.base  = (uint64_t)gdt;

    gdt_flush();

    uint16_t tss_sel = 5 * 8;
    __asm volatile ("ltr %0" : : "r"(tss_sel));
}

/*
 * * tss_set_rsp0 ---- ?????? ring0 ???? TSS.RSP0
 * * ???????????????->?????? A ?????? B
 */
void tss_set_rsp0(uint64_t rsp0) {
    tss[0].rsp[0] = rsp0;
}
