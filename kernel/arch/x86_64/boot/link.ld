OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(start)
KERNEL_VMA = 0xFFFFFF8000000000;
KERNEL_LMA = 0x00100000;
PHDRS { boot PT_LOAD FLAGS(5); bootdata PT_LOAD FLAGS(6); text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
SECTIONS {
 . = KERNEL_LMA;
 .boot ALIGN(4K) : AT(ADDR(.boot)) { KEEP(*(.boot)) } :boot
 .bootdata ALIGN(4K) : AT(ADDR(.bootdata)) { KEEP(*(.boot.data)) } :bootdata
 . += KERNEL_VMA;
 .text ALIGN(4K) : AT(ADDR(.text)-KERNEL_VMA) { *(.text .text.*) *(.rodata .rodata.*) } :text
 .data ALIGN(4K) : AT(ADDR(.data)-KERNEL_VMA) { *(.data .data.*) } :data
 .bss ALIGN(4K) (NOLOAD) : AT(ADDR(.bss)-KERNEL_VMA) { *(COMMON) *(.bss .bss.*) } :data
 .stack ALIGN(16) (NOLOAD) : { . += 64K; PROVIDE(__stack_top = .); } :data
 .syscall_stack ALIGN(16) (NOLOAD) : { . += 16K; PROVIDE(__syscall_stack_top = .); } :data
 /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}
