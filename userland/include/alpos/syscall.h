#ifndef ALPOS_SYSCALL_H
#define ALPOS_SYSCALL_H
#include <stdint.h>
/*
 * alpOS x86_64 userspace ABI draft.
 * NOT IMPLEMENTED BY THE CURRENT KERNEL YET.
 * int $0x80; rax=number, rdi/rsi/rdx/r10/r8/r9=args, rax=return.
 */
enum alpos_syscall_number {
    ALPOS_SYS_EXIT = 0,
    ALPOS_SYS_WRITE = 1,
    ALPOS_SYS_READ = 2,
    ALPOS_SYS_OPEN = 3,
    ALPOS_SYS_CLOSE = 4,
    ALPOS_SYS_GETCWD = 5,
    ALPOS_SYS_UNAME = 6
};
static inline long alpos_syscall0(long n) {
    long r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n) : "memory", "rcx", "r11");
    return r;
}
static inline long alpos_syscall1(long n, long a) {
    long r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "D"(a) : "memory", "rcx", "r11");
    return r;
}
static inline long alpos_syscall3(long n, long a, long b, long c) {
    long r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "D"(a), "S"(b), "d"(c) : "memory", "rcx", "r11");
    return r;
}
#endif
