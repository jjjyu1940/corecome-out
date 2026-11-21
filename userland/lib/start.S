.section .text
.global _start
.type _start,@function
.extern main
_start:
    xor %rbp, %rbp
    mov (%rsp), %rdi          /* argc */
    lea 8(%rsp), %rsi         /* argv */
    and $-16, %rsp
    call main
    mov %rax, %rdi
    mov $0, %rax
    int $0x80
1:  pause
    jmp 1b
.size _start, .-_start
.section .note.GNU-stack,"",@progbits
