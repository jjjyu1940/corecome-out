/*
 * syscall.S —— 用户态系统调用门。
 *
 * AArch64 约定（与 kernel/arch/aarch64/exception/vectors.S + syscall/syscall.c 一致）：
 *   svc #0x80
 *   x8  = 调用号
 *   x0  = 参数 1 / 返回值
 *   x1  = 参数 2
 *   x2  = 参数 3
 *   返回值 < 0 视为错误：内核一律返回无符号的 (u64)-1 / -2 这类值，
 *   由包装器检查最高位并写 errno。
 *
 * 所有包装器返回 long：成功返回结果，失败返回 -1 并把 errno 填上。
 * 纯传参型（exit/fork 返回值不需要区分错误）另外给一份。
 */

    .arch armv8-a
    .section .text
    .global syscall_raw
    .global sys_puts
    .global sys_ticks
    .global sys_write
    .global sys_read
    .global sys_open
    .global sys_close
    .global sys_dup
    .global sys_dup2
    .global sys_fcntl
    .global sys_seek
    .global sys_fstat
    .global sys_pipe
    .global sys_mkdir
    .global sys_unlink
    .global sys_rename
    .global sys_access
    .global sys_stat
    .global sys_chdir
    .global sys_getcwd
    .global sys_umask
    .global sys_getuid
    .global sys_getgid
    .global sys_mount
    .global sys_fork
    .global sys_exec
    .global sys_wait
    .global sys_getpid
    .global sys_getppid
    .global sys_gettid
    .global sys_kill
    .global sys_exit
    .global sys_sigaction
    .global sys_sigprocmask
    .global sys_ioctl
    .global sys_mmap
    .global sys_munmap
    .global sys_getpgid
    .global sys_setpgid
    .global sys_yield
    .global sys_sleep
    .global sys_sigreturn

/*
 * syscall_raw(nr, a0, a1, a2) —— 裸门，不碰 errno。
 * 给那些必须原样拿回 x0 的场景（fork 的 0、exec 后不返回）。
 */
syscall_raw:
    mov  x8, x0
    mov  x0, x1
    mov  x1, x2
    mov  x2, x3
    svc  #0x80
    ret

/*
 * _chkerr:  x0 是内核返回值（无符号）。
 *   若 (signed)x0 < 0 则把它写进 errno 并返 -1，否则原样返 x0。
 *   约定：内核侧返回的负数是 errno 的负值，如 -2 = EFAULT。
 */
_chkerr:
    cmp  x0, #0
    b.ge 1f
    neg  w1, w0
    adrp x2, errno
    str  w1, [x2, #:lo12:errno]
    mov  x0, #-1
1:  ret

/* ---- 0/1/2 参、无 errno 副作用的简单门 ---- */
sys_puts:
    mov  x8, #1
    svc  #0x80
    ret

sys_ticks:
    mov  x8, #2
    svc  #0x80
    ret

sys_yield:
    mov  x8, #8
    svc  #0x80
    ret

sys_sleep:
    mov  x8, #24
    svc  #0x80
    ret

sys_close:
    mov  x8, #7
    svc  #0x80
    b    _chkerr

sys_mkdir:
    mov  x8, #20
    svc  #0x80
    b    _chkerr

sys_unlink:
    mov  x8, #21
    svc  #0x80
    b    _chkerr

sys_rename:
    mov  x8, #26
    svc  #0x80
    b    _chkerr

sys_chdir:
    mov  x8, #33
    svc  #0x80
    b    _chkerr

sys_umask:
    mov  x8, #35
    svc  #0x80
    ret

sys_getuid:
    mov  x8, #36
    svc  #0x80
    ret

sys_getgid:
    mov  x8, #37
    svc  #0x80
    ret

sys_getpid:
    mov  x8, #14
    svc  #0x80
    ret

sys_getppid:
    mov  x8, #15
    svc  #0x80
    ret

sys_gettid:
    mov  x8, #17
    svc  #0x80
    ret

sys_kill:
    mov  x8, #18
    svc  #0x80
    b    _chkerr

sys_access:
    mov  x8, #29
    svc  #0x80
    b    _chkerr

sys_stat:
    mov  x8, #22
    svc  #0x80
    b    _chkerr

sys_write:
    mov  x8, #4
    svc  #0x80
    b    _chkerr

sys_read:
    mov  x8, #6
    svc  #0x80
    b    _chkerr

sys_open:
    mov  x8, #5
    svc  #0x80
    b    _chkerr

sys_seek:
    mov  x8, #25
    svc  #0x80
    b    _chkerr

sys_pipe:
    mov  x8, #28
    svc  #0x80
    b    _chkerr

sys_dup:
    mov  x8, #27
    svc  #0x80
    b    _chkerr

sys_getpgid:
    mov  x8, #45
    svc  #0x80
    b    _chkerr

sys_setpgid:
    mov  x8, #46
    svc  #0x80
    b    _chkerr

/* ---- 3 参：x2 被内核吃，自己塞进去 ---- */
sys_dup2:
    mov  x8, #42
    svc  #0x80
    b    _chkerr

sys_fcntl:
    mov  x8, #43
    svc  #0x80
    b    _chkerr

sys_getcwd:
    mov  x8, #34
    svc  #0x80
    b    _chkerr

sys_mount:
    mov  x8, #48
    svc  #0x80
    b    _chkerr

sys_sigaction:
    mov  x8, #38
    svc  #0x80
    b    _chkerr

sys_sigprocmask:
    mov  x8, #39
    svc  #0x80
    b    _chkerr

sys_ioctl:
    mov  x8, #47
    svc  #0x80
    b    _chkerr

sys_munmap:
    mov  x8, #41
    svc  #0x80
    b    _chkerr

/* ---- exec / fork / exit：不检查错误 ---- */
sys_exec:
    mov  x8, #19
    svc  #0x80
    ret

sys_fork:
    mov  x8, #11
    svc  #0x80
    ret

sys_exit:
    mov  x8, #9
    svc  #0x80
    b    _chkerr
    ret

sys_wait:
    mov  x8, #13
    svc  #0x80
    ret

sys_fstat:
    mov  x8, #44
    svc  #0x80
    b    _chkerr

sys_mmap:
    mov  x8, #40
    svc  #0x80
    b    _chkerr

sys_sigreturn:
    mov  x8, #32
    svc  #0x80
    ret
