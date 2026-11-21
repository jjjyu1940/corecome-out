/*
 * * syscall.c - x86_64 system call dispatcher.
 * *
 * * ABI (custom, not Linux): the syscall instruction enters the kernel and
 * * syscall_entry saves the full frame; sys_dispatch receives (n, a0, a1, a2)
 * * via the normal C calling convention and writes the return value into RAX.
 * This lets us reuse every vfs / sched symbol from kern.h without
 * * rewriting the table for x86.
 * *
 * * The security model mirrors the upstream "third layer": every user pointer
 * * must pass u_ok(). On x86 this layer matters especially -- once syscall
 * * has switched CR3 to the kernel page table, the MMU no longer rejects
 * * accesses to kernel addresses, so only the range check stands between a
 * * malicious caller and arbitrary kernel memory.
 */
#include "x86.h"
#include "../../include/kern.h"

/*
 * * x86 syscall numbers. They need not line up with the upstream aarch64
 * * table; this port implements the subset it currently needs and appends
 * * new numbers at the end rather than inserting them, so existing user
 * * programs keep working.
 */
#define X86_PUTS    1
#define X86_WRITE   2
#define X86_READ    3
#define X86_OPEN    4
#define X86_CLOSE   5
#define X86_EXIT    6
#define X86_FORK    7
#define X86_EXEC    8
#define X86_WAIT    9
#define X86_PID     10
#define X86_PIPE    11
#define X86_DUP     12
#define X86_KILL    13

extern void console_putc(char c);

void sys_puts(const char *s) {
    if (!s) return;
    for (int i = 0; i < 1024; i++) {
        char c = s[i];
        if (!c) break;
        console_putc(c);
    }
}

/*
 * * u_ok - validate a user pointer: must lie inside the user window, must not
 * * wrap, and both endpoints must be mapped.
 * *
 * * x86 has no per-process aspace walker like the aarch64 port, so M0 just
 * * calls pg_user_pa directly. This means it cannot tell whether a process
 * * "really mapped" an interior page, and COW semantics need cooperation
 * * from pg_user_pa; this is a known gap, not a deliberate choice.
 */
static int u_ok(uint64_t p, uint64_t n) {
    if (n == 0) return 1;
    if (p < USER_BASE || p >= USER_TOP) return 0;
    if (p + n < p || p + n > USER_TOP) return 0;
    if (!pg_user_pa((uint64_t *)0, p)) return 0;
    if (n > 4096 && !pg_user_pa((uint64_t *)0, p + n - 1)) return 0;
    return 1;
}

uint64_t sys_dispatch(uint64_t n, uint64_t a0, uint64_t a1, uint64_t a2) {
    switch (n) {
    case X86_PUTS: {
        if (g_from_el0 && !u_ok(a0, 1)) return (uint64_t)-2;
        sys_puts((const char *)a0);
        return 0;
    }
    case X86_WRITE: {
        if (g_from_el0 && !u_ok(a1, a2)) return (uint64_t)-2;
        return (uint64_t)vfs_write((int)a0, (const void *)a1, a2);
    }
    case X86_READ: {
        if (g_from_el0 && !u_ok(a1, a2)) return (uint64_t)-2;
        return (uint64_t)vfs_read((int)a0, (void *)a1, a2);
    }
    case X86_OPEN: {
        if (g_from_el0 && !u_ok(a0, 1)) return (uint64_t)-2;
        return (uint64_t)vfs_open((const char *)a0, (int)a1);
    }
    case X86_CLOSE:
        return (uint64_t)vfs_close((int)a0);
    case X86_PID:
        return (uint64_t)(int64_t)sched_pid();
    case X86_EXIT:
        kthread_exit();
        return 0;
    case X86_PIPE: {
        int pair[2];
        if (g_from_el0 && !u_ok(a0, sizeof(pair))) return (uint64_t)-2;
        if (vfs_pipe(pair) != 0) return (uint64_t)-1;
        ((int *)a0)[0] = pair[0];
        ((int *)a0)[1] = pair[1];
        return 0;
    }
    case X86_DUP:
        return (uint64_t)vfs_dup((int)a0);
    default:
        return (uint64_t)-1;                          /* ENOSYS */
    }
}
