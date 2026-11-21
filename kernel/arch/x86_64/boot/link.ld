OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(start)

KERNEL_VMA = 0xFFFFFF8000000000;
KERNEL_LMA = 0x00100000;

SECTIONS
{
    . = KERNEL_LMA;
    .boot ALIGN(4K) : AT(ADDR(.boot)) {
        KEEP(*(.boot))
        *(.boot.*)
    }

    . += KERNEL_VMA;
    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VMA) {
        *(.text .text.*)
        *(.rodata .rodata.*)
    }
    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VMA) {
        *(.data .data.*)
        *(.bss .bss.* COMMON)
    }
    .stack ALIGN(16) (NOLOAD) : {
        . += 64K;
        PROVIDE(__stack_top = .);
    }
    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}
