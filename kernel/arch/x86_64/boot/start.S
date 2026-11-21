/* alpOS x86_64 Multiboot2 entry: 32-bit protected mode -> long mode. */
.section .boot,"ax",@progbits
.code32
.global start
.type start,@function

.set MB2_MAGIC, 0xE85250D6
.set MB2_ARCH, 0
.set MB2_LENGTH, mb2_header_end - mb2_header
.set MB2_CHECKSUM, -(MB2_MAGIC + MB2_ARCH + MB2_LENGTH)
.set MULTIBOOT2_BOOT_MAGIC, 0x36D76289
.set STACK32_TOP, 0x90000
.set PML4_ADDR, 0x70000
.set PDPT_ADDR, 0x71000
.set PD_BASE, 0x72000

.align 8
mb2_header:
    .long MB2_MAGIC
    .long MB2_ARCH
    .long MB2_LENGTH
    .long MB2_CHECKSUM
    /* Required end tag. */
    .short 0
    .short 0
    .long 8
mb2_header_end:

start:
    cli
    cmpl $MULTIBOOT2_BOOT_MAGIC, %eax
    jne boot_fail
    movl %eax, boot_magic
    movl %ebx, boot_mbi
    movl $STACK32_TOP, %esp
    call setup_gdt32
    call setup_page_tables

    /* Enable PAE, long mode, then paging. */
    movl %cr4, %eax
    orl $(1 << 5), %eax
    movl %eax, %cr4
    movl $PML4_ADDR, %eax
    movl %eax, %cr3
    movl $0xC0000080, %ecx
    rdmsr
    orl $(1 << 8), %eax
    wrmsr
    movl %cr0, %eax
    orl $(1 << 31), %eax
    movl %eax, %cr0

    lgdt gdt64_ptr_low
    ljmp $0x08, $long_mode_start

boot_fail:
    movw $0x3F8, %dx
    movb $'!', %al
    outb %al, %dx
1:  hlt
    jmp 1b

setup_gdt32:
    lgdt gdt32_ptr
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw %ax, %fs
    movw %ax, %gs
    ret

/* Map physical 0..4GiB both identity and at KERNEL_VMA. */
setup_page_tables:
    movl $PML4_ADDR, %edi
    xorl %eax, %eax
    movl $(6 * 4096 / 4), %ecx
    rep stosl

    movl $PDPT_ADDR, %eax
    orl $3, %eax
    movl %eax, PML4_ADDR
    movl %eax, PML4_ADDR + (256 * 8)

    xorl %ebp, %ebp
2:
    /* ESI = this 1GiB page-directory physical address. */
    movl %ebp, %esi
    shll $12, %esi
    addl $PD_BASE, %esi
    movl %esi, %eax
    orl $3, %eax
    movl %eax, PDPT_ADDR(,%ebp,8)

    movl %esi, %edi
    movl %ebp, %eax
    shll $30, %eax
    orl $0x183, %eax
    xorl %ecx, %ecx
3:
    movl %eax, (%edi,%ecx,8)
    movl $0, 4(%edi,%ecx,8)
    addl $0x200000, %eax
    incl %ecx
    cmpl $512, %ecx
    jne 3b

    incl %ebp
    cmpl $4, %ebp
    jne 2b
    ret

.align 8
gdt32:
    .quad 0
    .quad 0x00CF9A000000FFFF
    .quad 0x00CF92000000FFFF
gdt32_end:
gdt32_ptr:
    .word gdt32_end - gdt32 - 1
    .long gdt32

.code64
.align 8
gdt64:
    .quad 0
    .quad 0x00AF9A000000FFFF /* ring 0, 64-bit code */
    .quad 0x00CF92000000FFFF /* ring 0 data */
gdt64_end:
gdt64_ptr_low:
    .word gdt64_end - gdt64 - 1
    .quad gdt64
gdt64_ptr_high:
    .word gdt64_end - gdt64 - 1
    .quad gdt64

long_mode_start:
    /* Move execution to the high-half alias before calling high-half C code. */
    movabs $KERNEL_VMA, %rax
    leaq long_mode_high(%rip), %rdx
    addq %rax, %rdx
    jmp *%rdx

long_mode_high:
    movabs $KERNEL_VMA, %rax
    addq %rax, gdt64_ptr_high+2(%rip)
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw %ax, %fs
    movw %ax, %gs
    lgdt gdt64_ptr_high(%rip)

    movabs $__stack_top, %rsp
    andq $-16, %rsp

    movl boot_magic(%rip), %edi
    movl boot_mbi(%rip), %esi
    movabs $kernel_main, %rax
    call *%rax
4:  cli
    hlt
    jmp 4b

.section .boot.data,"aw",@progbits
.align 4
boot_magic: .long 0
boot_mbi:   .long 0
