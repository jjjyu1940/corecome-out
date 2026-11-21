/* alpOS x86_64 Multiboot2 entry. GRUB enters in 32-bit protected mode. */
.section .multiboot,"a"
.balign 8
mb2_header:
    .long 0xE85250D6              /* Multiboot2 header magic */
    .long 0                       /* i386 protected-mode entry */
    .long mb2_header_end - mb2_header
    .long -(0xE85250D6 + (mb2_header_end - mb2_header))
    .short 0                      /* end tag */
    .short 0
    .long 8
mb2_header_end:

.section .boot.text,"ax"
.code32
.global start
.type start, @function
start:
    cli
    cmpl $0x36D76289, %eax        /* Multiboot2 bootloader magic */
    jne boot_error
    movl %eax, boot_magic
    movl %ebx, boot_info
    movl $boot_stack_top, %esp

    /* Clear the three paging structures (PML4, PDPT, one page directory). */
    movl $boot_pml4, %edi
    xorl %eax, %eax
    movl $(4096 * 3 / 4), %ecx
    rep stosl

    movl $boot_pdpt, %eax
    orl $0x03, %eax
    movl %eax, boot_pml4
    movl $boot_pd, %eax
    orl $0x03, %eax
    movl %eax, boot_pdpt

    /* Identity-map the first 1 GiB with 2 MiB pages. */
    xorl %ecx, %ecx
1:
    movl %ecx, %eax
    shll $21, %eax
    orl $0x83, %eax              /* present | writable | huge page */
    movl %eax, boot_pd(,%ecx,8)
    movl $0, boot_pd+4(,%ecx,8)
    incl %ecx
    cmpl $512, %ecx
    jne 1b

    movl $boot_pml4, %eax
    movl %eax, %cr3
    movl %cr4, %eax
    orl $(1 << 5), %eax          /* CR4.PAE */
    movl %eax, %cr4

    movl $0xC0000080, %ecx       /* IA32_EFER */
    rdmsr
    orl $(1 << 8), %eax          /* EFER.LME */
    wrmsr

    lgdt gdt64_ptr
    movl %cr0, %eax
    orl $(1 << 31), %eax         /* CR0.PG; PE is already set by GRUB */
    movl %eax, %cr0
    ljmp $0x08, $long_mode_start

boot_error:
    movw $0x3F8, %dx
    movb $'!', %al
    outb %al, %dx
2:
    hlt
    jmp 2b

.code64
long_mode_start:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    xorw %ax, %ax
    movw %ax, %fs
    movw %ax, %gs

    movq $boot_stack_top, %rsp
    andq $-16, %rsp
    movl boot_magic, %edi
    movl boot_info, %esi
    call kernel_main
3:
    cli
    hlt
    jmp 3b

.section .boot.data,"aw"
.balign 8
gdt64:
    .quad 0
    .quad 0x00AF9A000000FFFF     /* 0x08: 64-bit ring-0 code */
    .quad 0x00CF92000000FFFF     /* 0x10: ring-0 data */
gdt64_end:
gdt64_ptr:
    .word gdt64_end - gdt64 - 1
    .long gdt64

.section .boot.bss,"aw",@nobits
.balign 4096
boot_pml4:  .skip 4096
boot_pdpt:  .skip 4096
boot_pd:    .skip 4096
.balign 16
boot_magic: .skip 4
boot_info:  .skip 4
.balign 16
boot_stack_bottom: .skip 16384
boot_stack_top:
