/*
 * * main.c ---- x86_64 ????
 * *
 * * ???? x86 ??????"??????? core/ ??"?????
 * *   1. ? Multiboot2 ?????????????? mmap ? module ??
 * *   2. ?? init ??? -> GDT -> IDT -> PIC -> paging -> ???
 * *   3. ? initrd ??????????? vfs_init ?? ramfs ??
 * *   4. ?? kernel_main ????????
 * *   5. panic/????
 * *
 * * ?? phys_free/phys_end ????????? core/ ?????????
 * * mm ????????"????????"?????????????
 * * ? 128 MiB ??????????????????????
 */
#include "x86.h"
#include "../../include/kern.h"

#define PHYS_END  (128ULL * 1024 * 1024)      /* M0 ????? mmap ?? */

static uint64_t initrd_phys = 0;
static uint64_t initrd_len  = 0;

static void mb2_parse(uint64_t phys) {
    uint32_t total = *(uint32_t *)phys;
    uint8_t *p = (uint8_t *)phys + 8;
    uint8_t *end = (uint8_t *)phys + total;
    while (p < end) {
        uint32_t t = *(uint32_t *)p;
        uint32_t l = *(uint32_t *)(p + 4);
        if (t == 4) {                        /* mmap */
            uint8_t *e = p + 8;
            while (e < p + l) {
                uint64_t base = *(uint64_t *)e;
                uint64_t len  = *(uint64_t *)(e + 8);
                uint32_t ty   = *(uint32_t *)(e + 16);
                if (ty == 1 && base + len > PHYS_END)
                    (void)0;                  /* ?? PHYS_END ?? */
                e += 24;
            }
        } else if (t == 3 && !initrd_phys) { /* module = initrd */
            initrd_phys = *(uint32_t *)(p + 8);
            initrd_len  = *(uint32_t *)(p + 12) - initrd_phys;
        }
        p += (l + 7) & ~7ULL;
        if (l == 0) break;
    }
}

void kernel_main(uint64_t magic, uint64_t mbi_phys, uint64_t initrd,
                 uint32_t initrd_len_arg) {
    (void)magic; (void)initrd; (void)initrd_len_arg;

    console_init();
    serial_putc(COM1, 'A'); serial_putc(COM1, '\n');
    kputs("[alpOS/x86_64] booting\n");

    mb2_parse(mbi_phys);
    if (initrd_phys)
        kputs("[alpOS/x86_64] initrd present\n");

    gdt_init();
    idt_init();
    pic_init(0x20);
    paging_init(0x200000, PHYS_END);
    irq_install();

    pic_unmask(0);                           /* ????? */

    sti();
    kputs("[alpOS/x86_64] entering core kernel\n");

    /*
 * * ?? core/ ???x86 ???????? kernel_main_arch ???
 * * ??? start.S ?????????????????? core/system.c
 * * ???????M0 ??????? CI ??
     */
    extern void alpos_main(void);
    alpos_main();

    cli();
    kputs("[alpOS/x86_64] halted\n");
    for (;;) hlt();
}
