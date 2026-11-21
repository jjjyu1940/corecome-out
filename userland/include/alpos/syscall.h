#ifndef ALPOS_SYSCALL_H
#define ALPOS_SYSCALL_H
#include <stdint.h>
enum alpos_syscall_number { ALPOS_SYS_EXIT=0, ALPOS_SYS_WRITE=1 };
static inline long alpos_syscall3(long n,long a,long b,long c){long r;__asm__ volatile("int $0x80":"=a"(r):"a"(n),"D"(a),"S"(b),"d"(c):"memory","rcx","r11");return r;}
#endif
