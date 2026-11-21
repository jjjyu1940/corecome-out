#include <stdint.h>
#include "descriptors.h"

struct __attribute__((packed)) tss64 {
    uint32_t reserved0;
    uint64_t rsp0, rsp1, rsp2;
    uint64_t reserved1;
    uint64_t ist1, ist2, ist3, ist4, ist5, ist6, ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
};
struct __attribute__((packed)) idt_gate {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t attributes;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
};
struct __attribute__((packed)) idtr { uint16_t limit; uint64_t base; };
static struct tss64 tss;
static struct idt_gate idt[256];
extern uint64_t gdt_tss_desc[2];
extern void isr_fault(void);
extern void isr80_stub(void);
extern char __syscall_stack_top[];

static void set_gate(unsigned n, void (*handler)(void), uint8_t attr) {
    uint64_t a = (uint64_t)handler;
    idt[n].offset_low = (uint16_t)a;
    idt[n].selector = 0x08;
    idt[n].ist = 0;
    idt[n].attributes = attr;
    idt[n].offset_mid = (uint16_t)(a >> 16);
    idt[n].offset_high = (uint32_t)(a >> 32);
    idt[n].zero = 0;
}

void descriptors_init(void) {
    uint64_t base;
    struct idtr idtr;
    unsigned i;
    for (i = 0; i < sizeof(tss); ++i) ((uint8_t *)&tss)[i] = 0;
    tss.rsp0 = (uint64_t)__syscall_stack_top;
    tss.iomap_base = (uint16_t)sizeof(tss);
    base = (uint64_t)&tss;
    gdt_tss_desc[0] = ((uint64_t)(sizeof(tss)-1) & 0xffffULL)
                    | ((base & 0xffffffULL) << 16)
                    | (0x89ULL << 40)
                    | (((base >> 24) & 0xffULL) << 56);
    gdt_tss_desc[1] = base >> 32;
    for (i = 0; i < 256; ++i) set_gate(i, isr_fault, 0x8e);
    set_gate(0x80, isr80_stub, 0xee); /* present, DPL3, interrupt gate */
    idtr.limit = (uint16_t)(sizeof(idt)-1);
    idtr.base = (uint64_t)&idt[0];
    __asm__ volatile("lidt %0" : : "m"(idtr));
    { uint16_t sel = 0x28; __asm__ volatile("ltr %0" : : "r"(sel)); }
}

static inline uint8_t port_in(uint16_t p) { uint8_t v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline void port_out(uint16_t p,uint8_t v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
void exception_panic(void) {
    static const char msg[] = "\n[alpOS] CPU exception: kernel halted\n";
    unsigned i;
    for (i = 0; msg[i]; ++i) {
        unsigned spin = 100000;
        while (!(port_in(0x3fd) & 0x20) && --spin) { }
        if (spin) port_out(0x3f8, (uint8_t)msg[i]);
    }
}
