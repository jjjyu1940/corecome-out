.section .text
.code64
.global isr_fault
.type isr_fault,@function
isr_fault:
 cli
 cld
 andq $-16,%rsp
 call exception_panic
1: hlt
 jmp 1b

.global isr80_stub
.type isr80_stub,@function
isr80_stub:
 cld
 pushq %r15
 pushq %r14
 pushq %r13
 pushq %r12
 pushq %r11
 pushq %r10
 pushq %r9
 pushq %r8
 pushq %rbp
 pushq %rdi
 pushq %rsi
 pushq %rdx
 pushq %rbx
 pushq %rax
 movq %rsp,%rdi
 subq $8,%rsp              /* SysV call-site alignment */
 call syscall_dispatch
 addq $8,%rsp
 cmpq $-4096,%rax
 je 2f
 movq %rax,(%rsp)
 popq %rax
 popq %rbx
 popq %rdx
 popq %rsi
 popq %rdi
 popq %rbp
 popq %r8
 popq %r9
 popq %r10
 popq %r11
 popq %r12
 popq %r13
 popq %r14
 popq %r15
 iretq
2:
 movq user_saved_rsp(%rip),%rsp
 jmp user_return

.global enter_user
.type enter_user,@function
/* void enter_user(uint64_t entry, uint64_t user_rsp); */
enter_user:
 pushq %rbx
 pushq %rbp
 pushq %r12
 pushq %r13
 pushq %r14
 pushq %r15
 movq %rsp,user_saved_rsp(%rip)
 pushq $0x1b                 /* SS: user data */
 pushq %rsi                  /* RSP */
 pushfq
 andq $~0x200,(%rsp)         /* keep IRQs disabled until PIC/APIC setup */
 pushq $0x23                 /* CS: user code */
 pushq %rdi                  /* RIP */
 iretq
user_return:
 popq %r15
 popq %r14
 popq %r13
 popq %r12
 popq %rbp
 popq %rbx
 movl user_exit_status(%rip),%eax
 ret
.size enter_user,.-enter_user
.section .bss
.align 8
.global user_saved_rsp
user_saved_rsp: .quad 0
.section .note.GNU-stack,"",@progbits
