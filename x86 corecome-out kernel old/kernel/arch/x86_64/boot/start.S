 /* */
 * * start.S ---- x86_64 ???? + ?????
 * *
 * * ????? GRUB Multiboot2???? Multiboot2 ???? ELF ????
 * *   eax == MULTIBOOT2_BOOTLOADER_MAGIC
 * *   ebx == ?? multiboot_tag ???????
 * *
 * * ??
 * *   1. ? 32-bit ??????????? flat 4 GiB GDT? PAE
 * *   2. ??? 2 MiB huge pagePTE_PS?? 0..4 MiB ??????
 * *      0..4 GiB ??? PML4[0] / PML4[256]??"???? + ?????"
 * *   3. ? CR4.PAE? CR3? EFER.LME? CR0.PG?????? CS.L=1
 * *      ?????? 64-bit ????? long_mode_start
 * *   4. long_mode_start ??? 64-bit GDT ? TSS????? __stack_top
 * *      ?? call kernel_main(???, mbi_phys, initrd_phys, initrd_len)
 * *
 * * ?????????????? 0..4 MiB + ?????kernel_main ?
 * * ?? paging_init() ??????????? initrd ???? ramfs
 */
        .section .boot, "ax"
        .code32
        .global start

MULTIBOOT2_MAGIC = 0x36D76289
STACK32_TOP      = 0x90000 /* ??? IVT ??????? */

start:
 * Multiboot2 ???? eax????????? ISO ??????
        cmp    $MULTIBOOT2_MAGIC, %eax
        jne    no_multiboot

 * ???????????????
        movl   $STACK32_TOP, %esp
        pushl  %ebx /* ?? multiboot info ???? */
        pushl  %eax

 * ???IDT ???
        cli
        call   boot_gdt
        call   boot_paging

 * ????
        movl   %cr0, %eax
        orl    $(1 << 31), %eax /* PG */
        movl   %eax, %cr0

 * ???? CS.L??? m16:32 ??????
        lgdtl  gdt64_ptr
        pushl  $8
        pushl  $long_mode_start
        lret

no_multiboot:
 * ????????????? COM1????
        movw   $0x3F8, %dx
        movb   $'!', %al
        outb   %al, %dx
1:      hlt
        jmp    1b

 /* */
 * * boot_gdt ---- 32-bit flat ?cs=0x08 ? 4 GiB ??ds=0x10 ?
 * * 4 GiB ?????????????? 64-bit GDT ?????
 * * ??????
         */
boot_gdt:
        lgdtl  gdt32_ptr
        movl   $0x10, %eax
        movw   %ax, %ds
        movw   %ax, %es
        movw   %ax, %fs
        movw   %ax, %gs
        movw   %ax, %ss
        ret

        .align 8
gdt32:
        .quad 0
        .quad 0x00CF9A000000FFFF /* code: P=1 DPL=0 S=1 type=0xA L=0 D=1 */
        .quad 0x00CF92000000FFFF /* data: P=1 DPL=0 S=1 type=0x2 W=1 */
gdt32_ptr:
        .word  . - gdt32 - 1
        .long  gdt32

 /* */
 * * boot_paging ---- ?? PML4[0]=0..4GiB ??PML4[256]=0..4GiB ??
 * * ? 2 MiB huge page????????? 4 GiB
         */
boot_paging:
 * ??????? 0x70000 ????????
        movl   $0x70000, %edi
        movl   $0x8000, %ecx
        xorl   %eax, %eax
        rep    stosb

 * PML4[0] = PDPT_lowPML4[256] = PDPT_high
        movl   $0x70000, %eax
        movl   $0x71000, %ebx
        orl    $3, %eax
        movl   %eax, 0(%ebx) /* PDPT_low */
        movl   $0x72000, %eax
        orl    $3, %eax
        movl   %eax, 2048(%ebx) /* PDPT_high = PML4[256] */

 * ?? PDPT ?? 2048 ? 2 MiB PD?? 0..4 GiB
        movl   $0x71000, %edi
        movl   $0x1000, %ecx
        movl   $0x183, %eax /* P|RW|US|PS */
        call   fill_pd

        movl   $0x72000, %edi
        movl   $0x1000, %ecx
        movl   $0x183, %eax
        call   fill_pd

        movl   $0x70000, %eax
        movl   %eax, %cr3

        movl   %cr4, %eax
        orl    $(1 << 5), %eax /* PAE */
        orl    $(1 << 7), %eax /* PGE */
        movl   %eax, %cr4

        movl   $0xC0000080, %ecx /* IA32_EFER */
        rdmsr
        orl    $(1 << 8), %eax /* LME */
        wrmsr
        ret

fill_pd:
        movl   %ecx, %edx
        xorl   %ecx, %ecx
1:      movl   %eax, (%edi, %ecx, 8)
        addl   $0x200000, %eax
        incl   %ecx
        cmpl   %edx, %ecx
        jne    1b
        ret

 /* */
 * * 64-bit GDTnull / 64-bit ???? / 64-bit ???? / ????32
 * * / ????64 / user code64 / user data64 / TSS
         */
        .code64
        .align 8
gdt64:
        .quad 0
        .quad 0x00209A0000000000 /* 0x08 kern code */
        .quad 0x0000920000000000 /* 0x10 kern data */
        .quad 0x0020FA0000000000 /* 0x18 user code32 */
        .quad 0x0000F20000000000 /* 0x20 user data64 */
        .quad 0x0020FA0000000000 /* 0x28 user code64 */
        .quad 0x0000F2000000000000 /* 0x30 user data64 */
        .quad 0 /* 0x38 TSS???? */
        .quad 0
gdt64_ptr:
        .word  8 * 8 - 1
        .long  gdt64

 /* */
 * * long_mode_start ---- ????????????
 * * ????rsp ?? multiboot info?????????????
 * * mbi ????????????? C
         */
long_mode_start:
        movabs $gdt64, %rax
        lgdtq  (gdt64_ptr - gdt64)(%rax)

        movabs $KERNEL_VMA, %rax
        addq   %rax, gdt64_ptr
        lgdtq  gdt64_ptr
        subq   %rax, gdt64_ptr

        movw   $0x10, %ax
        movw   %ax, %ds
        movw   %ax, %es
        movw   %ax, %fs
        movw   %ax, %gs
        movw   %ax, %ss

 * ? Multiboot2 tag MB2_TAG_TYPE_BOOT_LOADER_NAME ??????
 * * ???? mbi ??? initrd ???? C
        movq   $KERNEL_VMA, %rbx
        popq   %rdi /* ??? */
        popq   %rsi /* mbi ???? */
        addq   %rbx, %rsi

        xorq   %rcx, %rcx
        xorq   %rdx, %rdx
 * ?? tagsize(8) + type(4) + ...???? tag_total_size ??
        movq   %rsi, %r8
        addq   $8, %r8
1:      movl   (%r8), %eax /* size */
        movl   4(%r8), %ecx /* type */
        cmp    $0, %eax
        je     2f
        cmp    $3, %ecx /* MB2_TAG_TYPE_MODULE */
        je     3f
        addq   %rax, %r8
        addq   $7, %r8
        andq   $~7, %r8
        jmp    1b
3: /* module: mod_start mod_end + ???mod_end-mod_start ??? */
        movl   8(%r8), %ecx
        movl   12(%r8), %edx
        subl   %ecx, %edx
        addq   %rbx, %rcx /* ????? */
        movq   %rcx, %rdx /* initrd ???? */
        movl   %edx, %ecx /* initrd ?? */
2:
 * ????????????????
        movabs $__stack_top, %rsp
        subq   $16, %rsp
        andq   $~15, %rsp
        call   kernel_main
        cli
        hlt
loop:   jmp    loop
