/*
 * * pic.c ---- 8259A PIC ? x2APIC ??
 * *
 * * ???????QEMU `-machine pc` ??? 8259??? `-machine q35`
 * * ? IOAPIC+LAPICM0 ??? PIC ?????????APIC ?????
 * * ??????????
 * *
 * * PIC ?????
 * *   ICW1: ?? ICW4 | ?? | ????
 * *   ICW2: ??????? IRQ0 ?? 0x20
 * *   ICW3: ?? bit ???? IR ?????????????
 * *   ICW4: 8086 ?? | ??? EOI | ??? | ??????
 * *
 * * EOI ??????????????????????
 */
#include "x86.h"

#define PIC1    0x20
#define PIC1_CMD  PIC1
#define PIC1_DATA (PIC1 + 1)
#define PIC2    0xA0
#define PIC2_CMD  PIC2
#define PIC2_DATA (PIC2 + 1)

#define ICW1_ICW4       0x01
#define ICW1_INIT       0x10
#define ICW4_8086       0x01

#define APIC_REG(x)     (*(volatile uint32_t *)((uint64_t)(x) + KERNEL_VMA))

static uint64_t lapic_base = 0xFEE00000;     /* ?????? */

void pic_init(uint8_t off) {
 * ????????BIOS ??????
    uint8_t m1 = inb(PIC1_DATA), m2 = inb(PIC2_DATA);

    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();

    outb(PIC1_DATA, off);                    /* IRQ0..7  -> off..off+7 */
    io_wait();
    outb(PIC2_DATA, (uint8_t)(off + 8));     /* IRQ8..15 -> off+8..off+15 */
    io_wait();

    outb(PIC1_DATA, 1 << 2);                 /* ?? IR2 ??? */
    io_wait();
    outb(PIC2_DATA, 2);                      /* ??????? IR2 */
    io_wait();

    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

 * ???????? unmask
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    outb(PIC1_DATA, m1);
    outb(PIC2_DATA, m2);
}

void pic_eoi(uint8_t irq) {
    if (irq >= 8)
        outb(PIC2_CMD, 0x20);
    outb(PIC1_CMD, 0x20);
}

void pic_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t  cur  = inb(port);
    outb(port, cur | (1 << (irq & 7)));
}

void pic_unmask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t  cur  = inb(port);
    outb(port, cur & (uint8_t)~(1 << (irq & 7)));
 * ???????????????
    if (irq >= 8)
        outb(PIC1_DATA, (uint8_t)(inb(PIC1_DATA) & ~(1 << 2)));
}

/*
 * * apic_init ---- ?? local APIC ????
 * * APIC ?????? enabled?????/QEMU ????????
 * * spurious vector register ? bit 8 ????????????? 0xFF
 * * ??"????? ISR"??????????????
 */
void apic_init(void) {
    uint32_t *spur = (uint32_t *)(uint64_t)(lapic_base + KERNEL_VMA + 0xF0);
    *spur = (uint32_t)((*spur & 0xFFFFF0FFU) | 0x100U | 0xFFU);
}

uint32_t apic_id(void) {
    return APIC_REG(lapic_base + 0x20);      /* APIC ID */
}

void apic_eoi(void) {
    APIC_REG(lapic_base + 0xB0) = 0;         /* EOI ??????? */
}

/*
 * * lapic_start_timer ---- ? divide-by-16 ?????????????
 * * ???? APIC ??????????0x20000???????
 * * ????????? TSC ?????M0 ???freq_hz ????
 */
void lapic_start_timer(uint32_t freq_hz) {
    (void)freq_hz;
    APIC_REG(lapic_base + 0x3E0) = 3;        /* divide by 16 */
    APIC_REG(lapic_base + 0x380) = 0x10000;  /* one-shot */
    APIC_REG(lapic_base + 0x320) = 0x10000;  /* initial count */
}
