OUTPUT_FORMAT(elf64-x86-64)
ENTRY(start)

PHDRS
{
    text PT_LOAD FLAGS(5); /* read + execute */
    data PT_LOAD FLAGS(6); /* read + write */
}

SECTIONS
{
    . = 0x00100000;

    .multiboot ALIGN(8) : { KEEP(*(.multiboot)) } :text
    .boot ALIGN(0x1000) : { *(.boot.text) *(.boot.data) } :text
    .text ALIGN(0x1000) : { *(.text .text.*) } :text
    .rodata ALIGN(0x1000) : { *(.rodata .rodata.*) } :text
    .data ALIGN(0x1000) : { *(.data .data.*) } :data
    .bss ALIGN(0x1000) (NOLOAD) :
    {
        *(COMMON)
        *(.boot.bss)
        *(.bss .bss.*)
    } :data

    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}
