/* Multiboot2 entry: request a 32-bit RGB framebuffer, then enter long mode. */
.section .boot,"ax",@progbits
.code32
.global start
.type start,@function
.set MB2_MAGIC, 0xE85250D6
.set MB2_ARCH, 0
.set MB2_LENGTH, mb2_header_end - mb2_header
.set MB2_CHECKSUM, -(MB2_MAGIC + MB2_ARCH + MB2_LENGTH)
.set BOOT_MAGIC, 0x36D76289
.set STACK32_TOP, 0x90000
.set PML4_ADDR, 0x70000
.set PDPT_ADDR, 0x71000
.set PD_BASE, 0x72000
.align 8
mb2_header:
.long MB2_MAGIC, MB2_ARCH, MB2_LENGTH, MB2_CHECKSUM
/* Framebuffer header tag: preferred 1024x768, 32bpp. */
.short 5, 0
.long 20
.long 1024, 768, 32
.align 8
.short 0, 0
.long 8
mb2_header_end:
start:
 cli
 cld
 cmpl $BOOT_MAGIC,%eax
 jne boot_fail
 movl %eax,boot_magic
 movl %ebx,boot_mbi
 movl $STACK32_TOP,%esp
 call setup_gdt32
 call setup_page_tables
 movl %cr4,%eax
 orl $(1<<5),%eax
 movl %eax,%cr4
 movl $PML4_ADDR,%eax
 movl %eax,%cr3
 movl $0xC0000080,%ecx
 rdmsr
 orl $(1<<8),%eax
 wrmsr
 movl %cr0,%eax
 orl $((1<<31)|(1<<16)),%eax
 movl %eax,%cr0
 lgdt gdt64_ptr_low
 ljmp $0x08,$long_mode_start
boot_fail:
 movw $0x3F8,%dx
 movb $'!',%al
 outb %al,%dx
1: hlt
 jmp 1b
setup_gdt32:
 lgdt gdt32_ptr
 movw $0x10,%ax
 movw %ax,%ds
 movw %ax,%es
 movw %ax,%ss
 movw %ax,%fs
 movw %ax,%gs
 ret
setup_page_tables:
 movl $PML4_ADDR,%edi
 xorl %eax,%eax
 movl $(8*4096/4),%ecx
 rep stosl
 movl $(PDPT_ADDR|7),%eax
 movl %eax,PML4_ADDR
 movl $(PDPT_ADDR|3),%eax
 movl %eax,PML4_ADDR+(511*8)
 xorl %ebp,%ebp
2:
 movl %ebp,%esi
 shll $12,%esi
 addl $PD_BASE,%esi
 movl %esi,%eax
 orl $3,%eax
 movl %eax,PDPT_ADDR(,%ebp,8)
 movl %esi,%edi
 movl %ebp,%eax
 shll $30,%eax
 orl $0x83,%eax
 xorl %ecx,%ecx
3:
 movl %eax,(%edi,%ecx,8)
 movl $0,4(%edi,%ecx,8)
 addl $0x200000,%eax
 incl %ecx
 cmpl $512,%ecx
 jne 3b
 incl %ebp
 cmpl $4,%ebp
 jne 2b
 movl $(0x76000|7),%eax
 movl %eax,PD_BASE+(2*8)
 movl $(0x77000|7),%eax
 movl %eax,(PD_BASE+4096)+(511*8)
 orl $4,PDPT_ADDR
 orl $4,PDPT_ADDR+8
 ret
.align 8
gdt32:
.quad 0
.quad 0x00CF9A000000FFFF
.quad 0x00CF92000000FFFF
gdt32_end:
gdt32_ptr:
.word gdt32_end-gdt32-1
.long gdt32
.code64
.section .boot.data,"aw",@progbits
.align 8
gdt64:
.quad 0
.quad 0x00AF9A000000FFFF
.quad 0x00CF92000000FFFF
.quad 0x00CFF2000000FFFF
.quad 0x00AFFA000000FFFF
.global gdt_tss_desc
gdt_tss_desc:
.quad 0,0
gdt64_end:
gdt64_ptr_low:
.word gdt64_end-gdt64-1
.quad gdt64
gdt64_ptr_high:
.word gdt64_end-gdt64-1
.quad gdt64
.section .boot,"ax",@progbits
long_mode_start:
 movabs $0xFFFFFF8000000000,%rax
 leaq long_mode_high(%rip),%rdx
 addq %rax,%rdx
 jmp *%rdx
long_mode_high:
 cld
 movabs $0xFFFFFF8000000000,%rax
 addq %rax,gdt64_ptr_high+2(%rip)
 movw $0x10,%ax
 movw %ax,%ds
 movw %ax,%es
 movw %ax,%ss
 movw %ax,%fs
 movw %ax,%gs
 lgdt gdt64_ptr_high(%rip)
 movabs $__stack_top,%rsp
 andq $-16,%rsp
 movl boot_magic(%rip),%edi
 movl boot_mbi(%rip),%esi
 movabs $kernel_main,%rax
 call *%rax
4: cli
 hlt
 jmp 4b
.section .boot.data,"aw",@progbits
.align 4
boot_magic: .long 0
boot_mbi: .long 0
