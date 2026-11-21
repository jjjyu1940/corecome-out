/*
 * * x86.h -- alpOS x86_64 ????
 * *
 * * ??
 * *   - ????? 64-bit ???C ??? -mno-red-zone -mgeneral-regs-only
 * *   - ??? 0xFFFFFFFF80000000 ???????????????????
 * *     ?????????? 0..KERNEL_LOAD ????????????
 * *   - ???? IDT ??IRQ0..IRQ15 ? I/O APIC / 8259 ????????
 * *     PIC ??? + IRQ0 ?? tickIRQ1 ????
 * *   - ????? VGA ???? 0xB8000?????? COM1(0x3F8) ????
 * *     ???????????? QEMU ???????
 * *   - ?????? AArch64 ?? kern.h ??????struct taskfiletable
 * *     ?? arch/x86_64/sched.c ???????????????
 */

#ifndef X86_H
#define X86_H

#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int64_t  i64;

 * ---- ?? I/O ----
static inline void outb(uint16_t port, uint8_t v) {
    __asm volatile ("outb %0, %1" : : "a"(v), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void outw(uint16_t port, uint16_t v) {
    __asm volatile ("outw %0, %1" : : "a"(v), "Nd"(port));
}
static inline uint16_t inw(uint16_t port) {
    uint16_t v;
    __asm volatile ("inw %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void outl(uint16_t port, uint32_t v) {
    __asm volatile ("outl %0, %1" : : "a"(v), "Nd"(port));
}
static inline uint32_t inl(uint16_t port) {
    uint32_t v;
    __asm volatile ("inl %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void io_wait(void) { outb(0x80, 0); }

 * ---- MSR ----
#define MSR_IA32_EFER   0xC0000080
#define MSR_IA32_STAR   0xC0000081
#define MSR_IA32_LSTAR  0xC0000082
#define MSR_IA32_FMASK  0xC0000084
#define MSR_IA32_FSBASE 0xC0000100
#define MSR_IA32_GSBASE 0xC0000101
#define MSR_IA32_KRNGBS 0xC0000102

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}
static inline void wrmsr(uint32_t msr, uint64_t v) {
    uint32_t lo = (uint32_t)v, hi = (uint32_t)(v >> 32);
    __asm volatile ("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}

 * ---- CR ??? ----
static inline uint64_t read_cr0(void) {
    uint64_t v;
    __asm volatile ("mov %0, cr0" : "=r"(v));
    return v;
}
static inline void write_cr0(uint64_t v) {
    __asm volatile ("mov cr0, %0" : : "r"(v));
}
static inline uint64_t read_cr3(void) {
    uint64_t v;
    __asm volatile ("mov %0, cr3" : "=r"(v));
    return v;
}
static inline void write_cr3(uint64_t v) {
    __asm volatile ("mov cr3, %0" : : "r"(v));
}
static inline uint64_t read_cr4(void) {
    uint64_t v;
    __asm volatile ("mov %0, cr4" : "=r"(v));
    return v;
}
static inline void write_cr4(uint64_t v) {
    __asm volatile ("mov cr4, %0" : : "r"(v));
}

 * ---- ????? ----
static inline void sti(void) { __asm volatile ("sti"); }
static inline void cli(void) { __asm volatile ("cli"); }
static inline void hlt(void) { __asm volatile ("hlt"); }
static inline void invlpg(uint64_t va) {
    __asm volatile ("invlpg (%0)" : : "r"(va) : "memory");
}
static inline void cpuid(uint32_t leaf, uint32_t *a, uint32_t *b,
                         uint32_t *c, uint32_t *d) {
    __asm volatile ("cpuid"
        : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
        : "a"(leaf));
}

 * ---- ??? ----
#define VGA_MEM  0xB8000ULL
#define COM1     0x3F8
void panic(const char *msg);
void console_init(void);
void console_putc(char c);
void vga_init(void);
void vga_putc(char c);
void serial_init(uint16_t port);
void serial_putc(uint16_t port, char c);

 * ---- GDT ----
#define GDT_N 8
struct gdt_ptr { uint16_t limit; uint64_t base; } __attribute__((packed));
extern uint64_t gdt[GDT_N];
extern struct gdt_ptr gdtp;
void gdt_init(void);
void tss_set_rsp0(uint64_t rsp0);

 * ---- IDT ----
#define IDT_N 256
struct idt_ptr { uint16_t limit; uint64_t base; } __attribute__((packed));
struct idt_gate { uint16_t off_low; uint16_t sel; uint16_t ist:3, rsv:5, type:4,
                  s:1, dpl:2, p:1; uint16_t off_mid; uint32_t off_hi;
                  uint32_t zero; } __attribute__((packed));
extern struct idt_gate idt[IDT_N];
extern struct idt_ptr idtp;
void idt_init(void);
void idt_set(int n, void *handler, uint8_t ist, uint8_t dpl);
void irq_install(void);

 * ---- PIC / APIC ----
void pic_init(uint8_t off);
void pic_eoi(uint8_t irq);
void pic_mask(uint8_t irq);
void pic_unmask(uint8_t irq);
void apic_init(void);
uint32_t apic_id(void);
void apic_eoi(void);
void lapic_start_timer(uint32_t freq_hz);

 * ---- ?? ----
#define PTE_P   (1ULL << 0)
#define PTE_RW  (1ULL << 1)
#define PTE_US  (1ULL << 2)
#define PTE_PWT (1ULL << 3)
#define PTE_PCD (1ULL << 4)
#define PTE_A   (1ULL << 5)
#define PTE_D   (1ULL << 6)
#define PTE_PS  (1ULL << 7)
#define PTE_G   (1ULL << 8)
#define PTE_NX  (1ULL << 63)

#define KERNEL_VMA 0xFFFFFFFF80000000ULL
#define USER_BASE  0x400000ULL      /* ? 2 MiB ???????? 2 MiB ?? */
#define USER_TOP   0x600000ULL
#define KSTACK_SZ  (16 * 1024)

uint64_t *pgd_new(void);
void pgd_free(uint64_t *pgd);
void pgd_map(uint64_t *pgd, uint64_t v, uint64_t p, uint64_t flags);
uint64_t pgd_walk(uint64_t *pgd, uint64_t v, int create);
void pgd_switch(uint64_t *pgd);
uint64_t *pgd_current(void);
void paging_init(uint64_t phys_free, uint64_t phys_end);

 * ---- ???? ----
struct trap_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

void user_jump(uint64_t entry, uint64_t sp, uint64_t arg);
uint64_t sys_dispatch(uint64_t n, uint64_t a0, uint64_t a1, uint64_t a2);
void sys_puts(const char *s);

#endif
