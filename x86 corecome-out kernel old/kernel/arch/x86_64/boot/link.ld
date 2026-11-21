/*
 * * link.ld - alpOS x86_64 kernel linker script.
 * *
 * * Layout:
 * *   0x100000   kernel physical entry. GRUB Multiboot2 loads the ELF at the
 * *              p_vaddr, which is also -Ttext's base.
 * *   0xFFFFFF... high-half window: the first 1 GiB of physical memory is
 * *              identity-mapped into the high half, so all kernel text and
 * *              data live there and page-table pages are also reachable.
 * *   .boot     only the stage-2 32-bit entry and the temporary GDT; freed
 * *             after the long-mode switch.
 */
OUTPUT_FORMAT("elf64-x86-64");
ENTRY(start);

KERNEL_VMA = 0xFFFFFF8000000000;
KERNEL_LMA = 0x100000;

SECTIONS {
    . = KERNEL_LMA;

    .boot ALIGN(4K) : AT(ADDR(.boot)) {
        *(.boot)
        *(.boot.text)
        *(.boot.data)
    }

    . += KERNEL_VMA;

    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VMA) {
        *(.text*)
        *(.rodata*)
    }

    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VMA) {
        *(.data*)
        *(.bss .bss.* COMMON)
        *(COMMON)
    }

    .stack ALIGN(16) (NOLOAD) : {
        . = . + 64 * 1024;
        PROVIDE(__stack_top = .);
    }

    /DISCARD/ : { *(.note*) *(.comment*) *(.eh_frame*) }
}
